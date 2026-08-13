#include "appcontroller.h"
#include "apipaths.h"
#include "database.h"
#include "librarysyncworker.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>
#include <tuple>

namespace {
// How many rows are handed to a view at a time. The cache is local, so this
// only bounds how much the QML views have to keep alive.
constexpr int kCachePageSize = 200;
// A library changes rarely; re-walking it more often than this buys nothing.
constexpr int kCacheMaxAgeSeconds = 6 * 60 * 60;
// Apple's maximum page size for the plain library collections.
constexpr int kApiPageSize = 100;
// recently-added rejects anything above 25 ("Value must be an integer less
// than or equal to 25").
constexpr int kRecentlyAddedPageSize = 25;
// That view only ever needs the newest slice, so the walk stops early.
constexpr int kRecentlyAddedLimit = 100;
const QLatin1String kRecentlyAdded("recently-added");
const QLatin1String kSyncTagPrefix("sync-");
const QLatin1String kRatingsTagPrefix("ratings-");
const QLatin1String kFavoriteAddTagPrefix("favorite-add-");
const QLatin1String kFavoriteRemoveTagPrefix("favorite-remove-");
const QLatin1String kLibraryAddTagPrefix("library-add-");
const QLatin1String kLibraryRemoveTagPrefix("library-remove-");
const QLatin1String kReplayResolveTagPrefix("replay-resolve-");
// Apple caps batch id lookups; keep well inside it.
constexpr int kRatingsBatchSize = 100;
} // namespace

namespace {
MediaItem
demoItem(const char *id, const char *type, const char *title, const char *subtitle, const char *album, const char *artwork, qint64 duration = 0)
{
    MediaItem item;
    item.id = QString::fromLatin1(id);
    item.catalogId = item.id;
    item.type = QString::fromLatin1(type);
    item.title = QString::fromLatin1(title);
    item.subtitle = QString::fromLatin1(subtitle);
    item.album = QString::fromLatin1(album);
    item.streamable = item.isSong();
    item.artworkUrl = QStringLiteral("qrc:/qt/qml/io/github/timpalpant/kadenza/data/demo/") + QString::fromLatin1(artwork);
    item.durationMs = duration;
    return item;
}

// Album, playlist and artist responses all wrap their children in a
// relationship; a plain song list arrives at the top level. Both detail pages
// and play-from-grid need the same unwrapping.
void extractCollection(const QJsonObject &root, const QString &requestedType, QJsonArray &data, QString &next)
{
    const auto array = root.value(QStringLiteral("data")).toArray();
    if (array.isEmpty())
        return;
    const auto resource = array.first().toObject();
    const QString type = resource.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("songs") || type == QStringLiteral("library-songs")) {
        data = array;
        next = root.value(QStringLiteral("next")).toString();
        return;
    }
    const QString relationshipName = requestedType.contains(QStringLiteral("artist")) || requestedType == QStringLiteral("record-labels")
        ? QStringLiteral("albums")
        : (requestedType == QStringLiteral("curators") || requestedType == QStringLiteral("apple-curators")
           || requestedType == QStringLiteral("activities"))
            ? QStringLiteral("playlists")
            : QStringLiteral("tracks");
    const auto relationship = resource.value(QStringLiteral("relationships")).toObject().value(relationshipName).toObject();
    data = relationship.value(QStringLiteral("data")).toArray();
    next = relationship.value(QStringLiteral("next")).toString();
}
} // namespace

AppController *AppController::s_instance = nullptr;

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_api(this)
    , m_player(this)
    , m_recent(this)
    , m_heavyRotation(this)
    , m_recentTracks(this)
    , m_recentlyAdded(this)
    , m_songs(this)
    , m_albums(this)
    , m_artists(this)
    , m_playlists(this)
    , m_search(this)
    , m_searchSongs(this)
    , m_searchAlbums(this)
    , m_searchArtists(this)
    , m_searchPlaylists(this)
    , m_detailTracks(this)
    , m_stations(this)
    , m_liveStations(this)
    , m_personalStation(this)
    , m_chartSongs(this)
    , m_chartAlbums(this)
    , m_chartPlaylists(this)
    , m_artistTopSongs(this)
    , m_artistAlbums(this)
    , m_artistSingles(this)
    , m_artistSimilar(this)
    , m_artistLatest(this)
    , m_replayTopSongs(this)
    , m_replayTopAlbums(this)
    , m_replayTopArtists(this)
    , m_playlistFolder(this)
    , m_searchCurators(this)
    , m_searchActivities(this)
    , m_pendingPlay(this)
{
    m_demo = qEnvironmentVariableIsSet("KADENZA_DEMO");
    m_sidebarWidth = KConfigGroup(KSharedConfig::openConfig(), "General").readEntry("SidebarWidth", 0);
    const KConfigGroup general(KSharedConfig::openConfig(), "General");
    m_searchHistory = general.readEntry("SearchHistory", QStringList{});
    m_lastPage = general.readEntry("LastPage", QStringLiteral("home"));
    m_sidebarCollapsed = general.readEntry("SidebarCollapsed", false);
    m_artworkSize = qBound(7, general.readEntry("ArtworkSize", 11), 20);
    connect(&m_player, &PlayerController::tokensChanged, this, [this](const QString &developer, const QString &user, const QString &storefront) {
        const bool wasAuthenticated = authenticated();
        m_api.setTokens(developer, user, storefront);
        // Signing out clears the tokens; the cached library belongs to the
        // account that just went away.
        if (developer.isEmpty() && user.isEmpty()) {
            m_cache.clear();
            m_syncs.clear();
            for (auto *model : allModels())
                model->clear();
            Q_EMIT syncingChanged();
        }
        if (wasAuthenticated != authenticated())
            Q_EMIT authenticatedChanged();
        if (authenticated()) {
            // The sidecar guesses the storefront from MusicKit and falls
            // back to "us" if it is not populated yet, which would silently
            // return US catalog results to everyone else. Ask Apple.
            m_api.get(QStringLiteral("storefront"), QStringLiteral("/v1/me/storefront"));
            refreshHome();
        }
    });
    connect(&m_player, &PlayerController::authenticatedChanged, this, &AppController::authenticatedChanged);
    connect(&m_player, &PlayerController::errorChanged, this, &AppController::errorChanged);
    connect(&m_api, &ApiClient::succeeded, this, &AppController::handleSuccess);
    connect(&m_api, &ApiClient::failed, this, &AppController::handleFailure);
    connect(&m_api, &ApiClient::succeededRaw, this, [this](const QString &tag, const QByteArray &body) {
        // Raw replies are only ever library pages.
        if (tag.startsWith(kSyncTagPrefix))
            handleSyncPayload(tag.mid(kSyncTagPrefix.size()), body);
    });
    if (m_demo) {
        loadDemoData();
        m_player.setDemoState();
    } else {
        // A cache failure is not fatal: every path falls back to the network.
        Database::instance().open();
        m_player.start();
    }

    // The library walk decodes and stores its pages here, so a sync never
    // competes with painting for the GUI thread.
    m_syncWorker = new LibrarySyncWorker;
    m_syncWorker->moveToThread(&m_syncThread);
    connect(&m_syncThread, &QThread::finished, m_syncWorker, &QObject::deleteLater);
    connect(this, &AppController::ingestPageRequested, m_syncWorker, &LibrarySyncWorker::ingestPage);
    connect(this, &AppController::finishSyncRequested, m_syncWorker, &LibrarySyncWorker::finishSync);
    connect(m_syncWorker, &LibrarySyncWorker::pageIngested, this, &AppController::handleSyncPageIngested);
    connect(m_syncWorker, &LibrarySyncWorker::syncFinished, this, &AppController::handleSyncFinished);
    connect(m_syncWorker, &LibrarySyncWorker::pageFailed, this, [this](const QString &kind, qint64 epoch, const QString &message) {
        if (const auto sync = m_syncs.constFind(kind); sync == m_syncs.constEnd() || sync->epoch != epoch)
            return;
        qWarning() << "kadenza: library page rejected" << kind << message;
        abandonSync(kind);
    });
    m_syncThread.start();
}

AppController::~AppController()
{
    if (!m_syncThread.isRunning())
        return;
    // Close the worker's SQLite handle on the worker's own thread; a
    // QSqlDatabase outliving its thread warns at shutdown.
    QMetaObject::invokeMethod(m_syncWorker, &LibrarySyncWorker::releaseDatabase, Qt::BlockingQueuedConnection);
    m_syncThread.quit();
    m_syncThread.wait();
}

AppController *AppController::create(QQmlEngine *, QJSEngine *)
{
    return s_instance;
}
void AppController::setInstance(AppController *instance)
{
    s_instance = instance;
}
PlayerController *AppController::player()
{
    return &m_player;
}
MediaModel *AppController::recentModel()
{
    return &m_recent;
}
MediaModel *AppController::heavyRotationModel()
{
    return &m_heavyRotation;
}
MediaModel *AppController::recentTracksModel()
{
    return &m_recentTracks;
}
QVariantList AppController::recommendations() const
{
    return m_recommendations;
}
MediaModel *AppController::recentlyAddedModel()
{
    return &m_recentlyAdded;
}
MediaModel *AppController::songsModel()
{
    return &m_songs;
}
MediaModel *AppController::albumsModel()
{
    return &m_albums;
}
MediaModel *AppController::artistsModel()
{
    return &m_artists;
}
MediaModel *AppController::playlistsModel()
{
    return &m_playlists;
}
MediaModel *AppController::searchModel()
{
    return &m_search;
}
MediaModel *AppController::searchSongsModel()
{
    return &m_searchSongs;
}
MediaModel *AppController::searchAlbumsModel()
{
    return &m_searchAlbums;
}
MediaModel *AppController::searchArtistsModel()
{
    return &m_searchArtists;
}
MediaModel *AppController::searchPlaylistsModel()
{
    return &m_searchPlaylists;
}
MediaModel *AppController::detailTracksModel()
{
    return &m_detailTracks;
}
MediaModel *AppController::stationsModel()
{
    return &m_stations;
}
MediaModel *AppController::liveStationsModel()
{
    return &m_liveStations;
}
MediaModel *AppController::personalStationModel()
{
    return &m_personalStation;
}
QVariantList AppController::stationGenres() const
{
    return m_stationGenres;
}
MediaModel *AppController::chartSongsModel()
{
    return &m_chartSongs;
}
MediaModel *AppController::chartAlbumsModel()
{
    return &m_chartAlbums;
}
MediaModel *AppController::chartPlaylistsModel()
{
    return &m_chartPlaylists;
}
QVariantList AppController::genres() const
{
    return m_genres;
}
MediaModel *AppController::artistTopSongsModel()
{
    return &m_artistTopSongs;
}
MediaModel *AppController::artistAlbumsModel()
{
    return &m_artistAlbums;
}
MediaModel *AppController::artistSinglesModel()
{
    return &m_artistSingles;
}
MediaModel *AppController::artistSimilarModel()
{
    return &m_artistSimilar;
}
MediaModel *AppController::artistLatestModel()
{
    return &m_artistLatest;
}
MediaModel *AppController::replayTopSongsModel()
{
    return &m_replayTopSongs;
}
MediaModel *AppController::replayTopAlbumsModel()
{
    return &m_replayTopAlbums;
}
MediaModel *AppController::replayTopArtistsModel()
{
    return &m_replayTopArtists;
}
MediaModel *AppController::playlistFolderModel()
{
    return &m_playlistFolder;
}
QString AppController::playlistFolderTitle() const
{
    return m_playlistFolderTitle;
}
QString AppController::playlistFolderId() const
{
    return m_playlistFolderId;
}
MediaModel *AppController::searchCuratorsModel()
{
    return &m_searchCurators;
}
MediaModel *AppController::searchActivitiesModel()
{
    return &m_searchActivities;
}
QString AppController::detailCuratorId() const
{
    return m_detailCuratorId;
}
QString AppController::detailCuratorName() const
{
    return m_detailCuratorName;
}
QString AppController::detailRecordLabelId() const
{
    return m_detailRecordLabelId;
}
QString AppController::detailRecordLabelName() const
{
    return m_detailRecordLabelName;
}
QString AppController::detailTitle() const
{
    return m_detailTitle;
}
QString AppController::detailSubtitle() const
{
    return m_detailSubtitle;
}
QString AppController::detailArtwork() const
{
    return m_detailArtwork;
}
QString AppController::detailType() const
{
    return m_detailType;
}
int AppController::detailRating() const
{
    return m_detailRating;
}
bool AppController::detailRatable() const
{
    return ratableType(m_detailType) && !m_detailRatingId.isEmpty();
}

// Albums and playlists can be collected; artists and stations cannot.
bool AppController::detailCollectable() const
{
    return (m_detailType.contains(QStringLiteral("albums")) || m_detailType.contains(QStringLiteral("playlists")))
           && !(m_detailInLibrary ? m_detailId : m_detailCatalogId).isEmpty();
}

bool AppController::detailInLibrary() const
{
    return m_detailInLibrary;
}

void AppController::toggleDetailInLibrary()
{
    if (m_demo || !detailCollectable())
        return;
    const bool wanted = !m_detailInLibrary;
    // Removing works on the library copy, adding on the catalog resource.
    setInLibrary(wanted ? m_detailCatalogId : m_detailId, m_detailType, wanted);
    m_cache.setInLibrary(m_detailCatalogId, wanted);
    // Reflect the choice straight away; the request only confirms it.
    m_detailInLibrary = wanted;
    Q_EMIT detailLibraryChanged();
}

bool AppController::ratableType(const QString &type)
{
    QString resource = type;
    resource.remove(QStringLiteral("library-"));
    return resource == QStringLiteral("songs") || resource == QStringLiteral("albums") || resource == QStringLiteral("playlists")
           || resource == QStringLiteral("stations");
}
bool AppController::authenticated() const
{
    return m_demo || (m_api.authenticated() && m_player.authenticated());
}
int AppController::sidebarWidth() const
{
    return m_sidebarWidth;
}
void AppController::setSidebarWidth(int width)
{
    if (m_sidebarWidth == width)
        return;
    m_sidebarWidth = width;
    auto config = KSharedConfig::openConfig();
    KConfigGroup(config, "General").writeEntry("SidebarWidth", width);
    config->sync();
    Q_EMIT sidebarWidthChanged();
}
bool AppController::sidebarCollapsed() const
{
    return m_sidebarCollapsed;
}
void AppController::setSidebarCollapsed(bool collapsed)
{
    if (m_sidebarCollapsed == collapsed)
        return;
    m_sidebarCollapsed = collapsed;
    auto config = KSharedConfig::openConfig();
    KConfigGroup(config, "General").writeEntry("SidebarCollapsed", collapsed);
    config->sync();
    Q_EMIT sidebarCollapsedChanged();
}
int AppController::artworkSize() const
{
    return m_artworkSize;
}
void AppController::setArtworkSize(int gridUnits)
{
    const int clamped = qBound(7, gridUnits, 20);
    if (m_artworkSize == clamped)
        return;
    m_artworkSize = clamped;
    auto config = KSharedConfig::openConfig();
    KConfigGroup(config, "General").writeEntry("ArtworkSize", clamped);
    config->sync();
    Q_EMIT artworkSizeChanged();
}
bool AppController::loading() const
{
    return m_loadingCount > 0;
}
bool AppController::syncing() const
{
    return !m_syncs.isEmpty();
}
QString AppController::error() const
{
    return m_error.isEmpty() ? m_player.error() : m_error;
}
QString AppController::message() const
{
    return m_message;
}
QStringList AppController::searchHistory() const
{
    return m_searchHistory;
}
QStringList AppController::searchHints() const
{
    return m_searchHints;
}
bool AppController::searchLibrary() const
{
    return m_searchLibrary;
}
void AppController::setSearchLibrary(bool libraryOnly)
{
    if (m_searchLibrary == libraryOnly)
        return;
    m_searchLibrary = libraryOnly;
    Q_EMIT searchLibraryChanged();
    // Re-run the current term against the other source straight away.
    if (!m_searchTerm.isEmpty())
        search(m_searchTerm);
}
QString AppController::lastPage() const
{
    return m_lastPage;
}
void AppController::setLastPage(const QString &page)
{
    if (page.isEmpty() || page == QStringLiteral("login") || page == QStringLiteral("detail") || m_lastPage == page)
        return;
    m_lastPage = page;
    auto config = KSharedConfig::openConfig();
    KConfigGroup(config, "General").writeEntry("LastPage", page);
    config->sync();
    Q_EMIT lastPageChanged();
}

void AppController::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    Q_EMIT errorChanged();
}
void AppController::setMessage(const QString &message)
{
    if (m_message == message)
        return;
    m_message = message;
    Q_EMIT messageChanged();
}
void AppController::clearMessage()
{
    setMessage({});
}
void AppController::clearError()
{
    setError({});
    m_player.clearError();
}

void AppController::requestModel(const QString &tag, const QString &path, MediaModel *model, bool append)
{
    // Demo mode is populated locally; firing tokenless requests would only
    // replace the fixture data with an error.
    if (m_demo || !authenticated() || path.isEmpty())
        return;
    model->setLoading(true);
    model->setError({});
    m_pendingModels.insert(tag, model);
    if (append)
        m_appendRequests.insert(tag);
    ++m_loadingCount;
    Q_EMIT loadingChanged();
    m_api.get(tag, path);
}

void AppController::refreshHome()
{
    if (m_demo)
        return;
    requestModel(QStringLiteral("recent"), QStringLiteral("/v1/me/recent/played?types=albums,playlists&limit=10"), &m_recent);
    requestModel(QStringLiteral("heavy"), QStringLiteral("/v1/me/history/heavy-rotation?limit=10&types=albums,playlists"), &m_heavyRotation);
    // This endpoint rejects a limit above 10.
    requestModel(QStringLiteral("recent-tracks"), QStringLiteral("/v1/me/recent/played/tracks?limit=10"), &m_recentTracks);
    m_api.get(QStringLiteral("recommendations"), QStringLiteral("/v1/me/recommendations?limit=10"));
}

MediaModel *AppController::modelForKind(const QString &kind)
{
    if (kind == QStringLiteral("recently-added"))
        return &m_recentlyAdded;
    if (kind == QStringLiteral("songs"))
        return &m_songs;
    if (kind == QStringLiteral("albums"))
        return &m_albums;
    if (kind == QStringLiteral("artists"))
        return &m_artists;
    if (kind == QStringLiteral("playlists"))
        return &m_playlists;
    return nullptr;
}

QString AppController::cacheKindFor(const QString &uiKind)
{
    return LibraryCache::isKnownKind(uiKind) ? uiKind : QString();
}

LibraryCache::Sort AppController::sortFor(const QString &uiKind)
{
    // Apple returns recently-added newest-first already, but sorting on the
    // stored timestamp is explicit and survives pages arriving out of order.
    return uiKind == kRecentlyAdded ? LibraryCache::DateAddedDesc : LibraryCache::LibraryOrder;
}

QString AppController::libraryPathFor(const QString &cacheKind)
{
    // Apple has a dedicated recently-added resource. /v1/me/library/albums is
    // documented as alphabetical with no sort parameter, so deriving Recently
    // Added from it would mean walking the entire library first.
    if (cacheKind == kRecentlyAdded)
        return QStringLiteral("/v1/me/library/recently-added?limit=%1&include=catalog").arg(kRecentlyAddedPageSize);
    return QStringLiteral("/v1/me/library/%1?limit=%2&include=catalog").arg(cacheKind).arg(kApiPageSize);
}

void AppController::fillFromCache(const QString &uiKind)
{
    auto *model = modelForKind(uiKind);
    const QString cacheKind = cacheKindFor(uiKind);
    if (!model || cacheKind.isEmpty())
        return;
    auto items = m_cache.items(cacheKind, sortFor(uiKind), kCachePageSize);
    if (items.isEmpty())
        return;
    model->replaceItems(std::move(items));
}

void AppController::loadLibrary(const QString &kind, bool refresh)
{
    if (m_demo)
        return;
    auto *model = modelForKind(kind);
    const QString cacheKind = cacheKindFor(kind);
    if (!model || cacheKind.isEmpty())
        return;

    // Paint from disk first. This is what makes Recently Added open immediately
    // instead of after a walk over every page of the album library.
    if (refresh || model->rowCount() == 0)
        fillFromCache(kind);

    // The mirror is trusted across launches. Re-walking it on the first visit
    // of every launch meant 42 pages of albums before the page settled, which
    // is precisely what kCacheMaxAgeSeconds exists to avoid. The Refresh
    // action still forces a walk.
    if (refresh || m_cache.count(cacheKind) == 0 || m_cache.isStale(cacheKind, kCacheMaxAgeSeconds))
        startLibrarySync(cacheKind);
}

void AppController::loadMore(const QString &kind)
{
    if (m_demo)
        return;
    auto *model = modelForKind(kind);
    const QString cacheKind = cacheKindFor(kind);
    if (!model || cacheKind.isEmpty())
        return;
    // Scrolling pages out of the local cache, so it never waits on Apple.
    model->appendItems(m_cache.items(cacheKind, sortFor(kind), kCachePageSize, model->rowCount()));
}

void AppController::startLibrarySync(const QString &cacheKind)
{
    if (m_demo || !authenticated() || cacheKind.isEmpty())
        return;
    if (m_syncs.contains(cacheKind))
        return;

    LibrarySync sync;
    sync.epoch = m_cache.beginSync(cacheKind);
    sync.position = 0;
    sync.bootstrapping = m_cache.count(cacheKind) == 0;
    m_syncs.insert(cacheKind, sync);

    // Only show a spinner when there is nothing cached to look at meanwhile.
    if (sync.bootstrapping) {
        if (auto *model = modelForKind(cacheKind))
            model->setLoading(true);
    }
    Q_EMIT syncingChanged();

    m_api.getRaw(kSyncTagPrefix + cacheKind, libraryPathFor(cacheKind));
}

void AppController::handleSyncPayload(const QString &cacheKind, const QByteArray &payload)
{
    auto sync = m_syncs.constFind(cacheKind);
    if (sync == m_syncs.constEnd())
        return;
    // Decoding and writing happen on the worker; the reply comes back as
    // handleSyncPageIngested().
    Q_EMIT ingestPageRequested(cacheKind, sync->epoch, sync->position, payload);
}

void AppController::handleSyncPageIngested(const QString &cacheKind, qint64 epoch, int itemCount, const QString &next)
{
    auto sync = m_syncs.find(cacheKind);
    // A sync abandoned or restarted while this page was with the worker must
    // not resume paging, nor finish an epoch that is no longer the current one.
    if (sync == m_syncs.end() || sync->epoch != epoch)
        return;

    sync->position += itemCount;

    const bool enough = cacheKind == kRecentlyAdded && sync->position >= kRecentlyAddedLimit;
    if (!next.isEmpty() && !enough) {
        // On a first-ever sync there is nothing else to show, so let each page
        // land in the view as it arrives instead of leaving the page empty.
        if (sync->bootstrapping)
            refreshCachedModels(cacheKind);
        m_api.getRaw(kSyncTagPrefix + cacheKind, ApiPaths::withLibraryIncludes(next));
        return;
    }

    Q_EMIT finishSyncRequested(cacheKind, epoch);
}

void AppController::handleSyncFinished(const QString &cacheKind, qint64 epoch)
{
    auto sync = m_syncs.find(cacheKind);
    if (sync == m_syncs.end() || sync->epoch != epoch)
        return;
    m_syncs.erase(sync);
    refreshCachedModels(cacheKind);
    // Library rows carry their ratings in the cache; refresh them once per sync
    // rather than on every page of the walk.
    if (auto *model = modelForKind(cacheKind))
        requestRatingsFor(model);
    Q_EMIT syncingChanged();
}

void AppController::abandonSync(const QString &cacheKind)
{
    // Dropped without calling finishSync(), so the previous epoch's rows
    // survive and a partial sync cannot look like a shrunken library.
    m_syncs.remove(cacheKind);
    if (auto *model = modelForKind(cacheKind))
        model->setLoading(false);
    Q_EMIT syncingChanged();
}

void AppController::handleRecommendations(const QJsonDocument &document)
{
    qDeleteAll(m_recommendationModels);
    m_recommendationModels.clear();
    m_recommendations.clear();

    for (const auto &value : document.object().value(QStringLiteral("data")).toArray()) {
        const auto recommendation = value.toObject();
        const auto attributes = recommendation.value(QStringLiteral("attributes")).toObject();
        const QString title = attributes.value(QStringLiteral("title")).toObject().value(QStringLiteral("stringForDisplay")).toString();
        const auto contents = recommendation.value(QStringLiteral("relationships"))
                                  .toObject()
                                  .value(QStringLiteral("contents"))
                                  .toObject()
                                  .value(QStringLiteral("data"))
                                  .toArray();
        // Apple nests groups of recommendations inside recommendations; only the
        // leaves carry playable content.
        if (title.isEmpty() || contents.isEmpty())
            continue;
        auto *model = new MediaModel(this);
        model->replace(contents);
        m_recommendationModels.push_back(model);
        requestRatingsFor(model);
        m_recommendations.push_back(
            QVariantMap{{QStringLiteral("title"), title}, {QStringLiteral("model"), QVariant::fromValue(static_cast<QObject *>(model))}});
    }
    Q_EMIT recommendationsChanged();
}

void AppController::handleArtistDetail(const QJsonDocument &document)
{
    const auto data = document.object().value(QStringLiteral("data")).toArray();
    if (data.isEmpty())
        return;
    const auto resource = data.first().toObject();
    const auto attributes = resource.value(QStringLiteral("attributes")).toObject();
    if (m_detailTitle.isEmpty()) {
        m_detailTitle = attributes.value(QStringLiteral("name")).toString();
        Q_EMIT detailChanged();
    }
    if (m_detailArtwork.isEmpty()) {
        m_detailArtwork = MediaItem::artwork(attributes.value(QStringLiteral("artwork")).toObject().value(QStringLiteral("url")).toString());
        Q_EMIT detailChanged();
    }

    const auto views = resource.value(QStringLiteral("views")).toObject();
    const auto fill = [&views](const QString &name, MediaModel *model) {
        model->replace(views.value(name).toObject().value(QStringLiteral("data")).toArray());
        model->setLoading(false);
    };
    fill(QStringLiteral("latest-release"), &m_artistLatest);
    fill(QStringLiteral("top-songs"), &m_artistTopSongs);
    requestRatingsFor(&m_artistTopSongs);
    fill(QStringLiteral("full-albums"), &m_artistAlbums);
    requestRatingsFor(&m_artistAlbums);
    fill(QStringLiteral("singles"), &m_artistSingles);
    requestRatingsFor(&m_artistSingles);
    fill(QStringLiteral("similar-artists"), &m_artistSimilar);
    // The albums grid the artist page already had keeps working for callers
    // that fall back to the library relationship.
    m_detailTracks.replaceItems({});
    setError({});
}

void AppController::handleReplay(const QJsonDocument &document)
{
    if (qEnvironmentVariableIsSet("KADENZA_TRACE_REPLAY"))
        qWarning().noquote() << "kadenza: replay response" << document.toJson(QJsonDocument::Compact);
    const auto data = document.object().value(QStringLiteral("data")).toArray();
    if (data.isEmpty()) {
        // No eligible year yet (a brand new account, for instance): leave the
        // shelves empty rather than stuck loading forever.
        m_replayTopSongs.setLoading(false);
        m_replayTopAlbums.setLoading(false);
        m_replayTopArtists.setLoading(false);
        return;
    }
    const auto resource = data.first().toObject();
    const auto views = resource.value(QStringLiteral("views")).toObject();
    // Unlike every other `views`/`relationships` shelf in this API, each row
    // here is not the song/album/artist itself but a *PeriodSummaries
    // wrapper (per Apple's own dictionary names) with the actual resource one
    // level deeper, behind a singular relationship named after it — e.g. an
    // AlbumPeriodSummaries row's real Albums resource sits at
    // relationships.album.data[0], not at the row's own attributes.
    const auto unwrap = [&views](const QString &viewName, const QString &relationshipName) {
        QJsonArray unwrapped;
        for (const auto &row : views.value(viewName).toObject().value(QStringLiteral("data")).toArray()) {
            const auto nested
                = row.toObject().value(QStringLiteral("relationships")).toObject().value(relationshipName).toObject().value(QStringLiteral("data")).toArray();
            if (!nested.isEmpty())
                unwrapped.append(nested.first());
        }
        return unwrapped;
    };
    // Ratings for songs are deliberately not requested here: when the shelf is
    // bare (see fillReplayShelf) the model is still empty at this point and the
    // batched resolve that will populate it finishes later, asynchronously —
    // fetching ratings after the fact would need its own completion tracking
    // for what is only a small cosmetic gap (no love/dislike icons on this one
    // shelf), so it is left alone rather than adding that machinery.
    fillReplayShelf(unwrap(QStringLiteral("top-songs"), QStringLiteral("song")), QStringLiteral("songs"), &m_replayTopSongs);
    fillReplayShelf(unwrap(QStringLiteral("top-albums"), QStringLiteral("album")), QStringLiteral("albums"), &m_replayTopAlbums);
    requestRatingsFor(&m_replayTopAlbums);
    fillReplayShelf(unwrap(QStringLiteral("top-artists"), QStringLiteral("artist")), QStringLiteral("artists"), &m_replayTopArtists);
    setError({});
}

void AppController::fillReplayShelf(const QJsonArray &items, const QString &catalogType, MediaModel *model)
{
    // Confirmed against a real account: album and artist period-summary rows
    // embed the full resource, but song rows are a bare id/type/href
    // reference with no attributes at all. Detected rather than hardcoded to
    // "songs only", in case that varies by account or region.
    const bool bare = !items.isEmpty() && !items.first().toObject().contains(QStringLiteral("attributes"));
    if (!bare) {
        model->replace(items);
        model->setLoading(false);
        return;
    }
    QStringList ids;
    for (const auto &value : items) {
        const QString id = value.toObject().value(QStringLiteral("id")).toString();
        if (!id.isEmpty())
            ids.push_back(id);
    }
    if (ids.isEmpty()) {
        model->setLoading(false);
        return;
    }
    for (int start = 0; start < ids.size(); start += kRatingsBatchSize) {
        const auto batch = ids.mid(start, kRatingsBatchSize);
        requestModel(kReplayResolveTagPrefix + catalogType + QLatin1Char('-') + QString::number(start),
                     QStringLiteral("/v1/catalog/%1?ids[%2]=%3").arg(m_api.storefront(), catalogType, batch.join(QLatin1Char(','))), model,
                     start > 0);
    }
}

void AppController::handleFolderContents(const QJsonDocument &document)
{
    const auto data = document.object().value(QStringLiteral("data")).toArray();
    if (data.isEmpty()) {
        m_playlistFolder.setLoading(false);
        return;
    }
    const auto resource = data.first().toObject();
    m_playlistFolderId = resource.value(QStringLiteral("id")).toString();
    m_playlistFolderTitle = resource.value(QStringLiteral("attributes")).toObject().value(QStringLiteral("name")).toString();
    Q_EMIT playlistFolderChanged();
    const auto children
        = resource.value(QStringLiteral("relationships")).toObject().value(QStringLiteral("children")).toObject().value(QStringLiteral("data")).toArray();
    m_playlistFolder.replace(children);
    m_playlistFolder.setLoading(false);
    setError({});
}

void AppController::refreshCachedModels(const QString &cacheKind)
{
    auto *model = modelForKind(cacheKind);
    if (!model)
        return;
    // Keep however much the user had already scrolled into view.
    const int wanted = qMax(kCachePageSize, model->rowCount());
    model->replaceItems(m_cache.items(cacheKind, sortFor(cacheKind), wanted));
}

void AppController::search(const QString &term)
{
    if (m_demo)
        return;
    const QString trimmed = term.trimmed();
    if (trimmed.isEmpty()) {
        m_searchTerm.clear();
        if (!m_searchHints.isEmpty()) {
            m_searchHints.clear();
            Q_EMIT searchHintsChanged();
        }
        m_search.clear();
        m_searchSongs.clear();
        m_searchAlbums.clear();
        m_searchArtists.clear();
        m_searchPlaylists.clear();
        m_searchCurators.clear();
        m_searchActivities.clear();
        return;
    }
    m_searchHistory.removeAll(trimmed);
    m_searchHistory.prepend(trimmed);
    while (m_searchHistory.size() > 8)
        m_searchHistory.removeLast();
    auto config = KSharedConfig::openConfig();
    KConfigGroup(config, "General").writeEntry("SearchHistory", m_searchHistory);
    config->sync();
    Q_EMIT searchHistoryChanged();
    m_searchTerm = trimmed;
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("term"), trimmed);
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("25"));
    if (m_searchLibrary) {
        query.addQueryItem(QStringLiteral("types"),
                           QStringLiteral("library-songs,library-albums,"
                                          "library-artists,library-playlists"));
        requestModel(QStringLiteral("search"), QStringLiteral("/v1/me/library/search?%1").arg(query.toString()), &m_search);
        m_searchCurators.clear();
        m_searchActivities.clear();
        return;
    }
    query.addQueryItem(QStringLiteral("types"), QStringLiteral("songs,albums,artists,playlists"));
    requestModel(QStringLiteral("search"), QStringLiteral("/v1/catalog/%1/search?%2").arg(m_api.storefront(), query.toString()), &m_search);

    // Fired as two separate requests, each with its own tag: whether Apple's
    // `types=` enum even accepts these values is unconfirmed, and folding them
    // into the query above would risk taking the whole search down with them
    // if it does not.
    QUrlQuery curatorQuery;
    curatorQuery.addQueryItem(QStringLiteral("term"), trimmed);
    curatorQuery.addQueryItem(QStringLiteral("limit"), QStringLiteral("10"));
    curatorQuery.addQueryItem(QStringLiteral("types"), QStringLiteral("curators"));
    requestModel(QStringLiteral("search-curators"), QStringLiteral("/v1/catalog/%1/search?%2").arg(m_api.storefront(), curatorQuery.toString()),
                 &m_searchCurators);

    QUrlQuery activityQuery;
    activityQuery.addQueryItem(QStringLiteral("term"), trimmed);
    activityQuery.addQueryItem(QStringLiteral("limit"), QStringLiteral("10"));
    activityQuery.addQueryItem(QStringLiteral("types"), QStringLiteral("activities"));
    requestModel(QStringLiteral("search-activities"), QStringLiteral("/v1/catalog/%1/search?%2").arg(m_api.storefront(), activityQuery.toString()),
                 &m_searchActivities);
}

void AppController::loadCharts(bool refresh, const QString &genreId)
{
    if (m_demo || !authenticated())
        return;
    // A genre change always refetches even without an explicit refresh; only
    // the plain "page just opened, already have data" case is skipped.
    if (!refresh && genreId.isEmpty() && m_chartSongs.rowCount() > 0)
        return;
    // offset must stay below 200 on this endpoint; a single page is plenty.
    QString path = QStringLiteral("/v1/catalog/%1/charts?types=songs,albums,"
                                  "playlists&chart=most-played&limit=20")
                       .arg(m_api.storefront());
    if (!genreId.isEmpty())
        path += QStringLiteral("&genre=%1").arg(genreId);
    requestModel(QStringLiteral("charts"), path, &m_chartSongs);
}

void AppController::loadGenres()
{
    if (m_demo || !authenticated())
        return;
    m_api.get(QStringLiteral("genres"), QStringLiteral("/v1/catalog/%1/genres").arg(m_api.storefront()));
}

void AppController::requestRatingsFor(MediaModel *model)
{
    if (m_demo || !authenticated() || !model)
        return;
    // Ratings are queried per resource type, so group the model's rows first.
    QHash<QString, QStringList> byType;
    QHash<QString, QSet<QString>> seen;
    for (int row = 0; row < model->rowCount(); ++row) {
        const auto *item = model->itemAt(row);
        if (!item || !ratableType(item->type))
            continue;
        QString resource = item->type;
        resource.remove(QStringLiteral("library-"));
        const QString id = item->catalogId.isEmpty() ? item->id : item->catalogId;
        // Library-only ids are not catalog resources and carry no catalog rating.
        if (id.isEmpty() || id.startsWith(QLatin1Char('i')))
            continue;
        auto &ids = byType[resource];
        // Deduplicated through a set: a linear contains() over the ids already
        // collected made this quadratic in the size of the model.
        if (!seen[resource].contains(id)) {
            seen[resource].insert(id);
            ids.push_back(id);
        }
    }
    for (auto it = byType.cbegin(); it != byType.cend(); ++it) {
        const QStringList &ids = it.value();
        for (int start = 0; start < ids.size(); start += kRatingsBatchSize) {
            const auto batch = ids.mid(start, kRatingsBatchSize);
            m_api.get(kRatingsTagPrefix + it.key() + QLatin1Char('-') + QString::number(start),
                      QStringLiteral("/v1/me/ratings/%1?ids=%2").arg(it.key(), batch.join(QLatin1Char(','))));
        }
    }
}

void AppController::applyRating(const QString &id, int rating)
{
    if (id.isEmpty())
        return;
    applyRatings({{id, rating}});
}

void AppController::applyRatings(const QHash<QString, int> &byId)
{
    if (byId.isEmpty())
        return;

    if (!m_detailRatingId.isEmpty()) {
        const auto found = byId.constFind(m_detailRatingId);
        if (found != byId.constEnd() && m_detailRating != found.value()) {
            m_detailRating = found.value();
            Q_EMIT detailRatingChanged();
        }
    }

    // One pass per model rather than one pass per rating: a reply carries up
    // to a hundred ids and the app keeps a couple of dozen models alive.
    auto models = allModels();
    models.append(m_player.queueModel());
    for (auto *model : models)
        model->setRatings(byId);
    for (auto *model : m_recommendationModels)
        model->setRatings(byId);

    // And one transaction rather than one per rating; every commit is an
    // fsync, which is what made a ratings reply stall the frame.
    QList<QPair<QString, int>> rows;
    rows.reserve(byId.size());
    for (auto it = byId.cbegin(); it != byId.cend(); ++it)
        rows.push_back({it.key(), it.value()});
    m_cache.setRatings(rows);
}

void AppController::handleRatings(const QString &, const QJsonDocument &document)
{
    QHash<QString, int> byId;
    for (const auto &value : document.object().value(QStringLiteral("data")).toArray()) {
        const auto rating = value.toObject();
        const QString id = rating.value(QStringLiteral("id")).toString();
        if (id.isEmpty())
            continue;
        byId.insert(id, rating.value(QStringLiteral("attributes")).toObject().value(QStringLiteral("value")).toInt());
    }
    applyRatings(byId);
}

void AppController::loadStations(bool refresh)
{
    if (m_demo || !authenticated())
        return;
    if (!refresh && m_stations.rowCount() > 0)
        return;
    // Stations have their own recents endpoint; they are not a `type` of
    // /v1/me/recent/played. The recent endpoints cap limit at 10.
    requestModel(QStringLiteral("stations"), QStringLiteral("/v1/me/recent/radio-stations?limit=10"), &m_stations);
}

void AppController::loadLiveStations(const QString &genreId)
{
    if (m_demo || !authenticated())
        return;
    const QString path = genreId.isEmpty()
        ? QStringLiteral("/v1/catalog/%1/stations?filter[featured]=apple-music-live-radio").arg(m_api.storefront())
        : QStringLiteral("/v1/catalog/%1/station-genres/%2/stations").arg(m_api.storefront(), genreId);
    requestModel(QStringLiteral("live-stations"), path, &m_liveStations);
}

void AppController::loadPersonalStation()
{
    if (m_demo || !authenticated())
        return;
    requestModel(QStringLiteral("personal-station"),
                 QStringLiteral("/v1/catalog/%1/stations?filter[identity]=personal").arg(m_api.storefront()),
                 &m_personalStation);
}

void AppController::loadStationGenres()
{
    if (m_demo || !authenticated())
        return;
    m_api.get(QStringLiteral("station-genres"), QStringLiteral("/v1/catalog/%1/station-genres").arg(m_api.storefront()));
}

void AppController::loadReplay(bool refresh)
{
    if (m_demo || !authenticated())
        return;
    if (!refresh && m_replayTopSongs.rowCount() > 0)
        return;
    requestModel(QStringLiteral("replay"),
                 QStringLiteral("/v1/me/music-summaries?filter[year]=latest&views=top-songs,top-albums,top-artists"),
                 &m_replayTopSongs);
}

void AppController::loadPlaylistFolder(const QString &id)
{
    if (m_demo || !authenticated())
        return;
    const QString folderId = id.isEmpty() ? QStringLiteral("p.playlistsroot") : id;
    requestModel(QStringLiteral("playlist-folder"), QStringLiteral("/v1/me/library/playlist-folders/%1?include=children").arg(folderId),
                 &m_playlistFolder);
}

void AppController::setRating(const QString &id, const QString &type, int value)
{
    if (m_demo || id.isEmpty())
        return;
    // Reflect the choice straight away; the request only confirms it.
    applyRating(id, value);
    // Ratings are keyed by the catalog resource type.
    QString resource = type;
    resource.remove(QStringLiteral("library-"));
    if (resource.isEmpty())
        resource = QStringLiteral("songs");
    const QString path = QStringLiteral("/v1/me/ratings/%1/%2").arg(resource, id);
    if (value == 0) {
        m_api.del(QStringLiteral("rating-clear"), path);
        return;
    }
    m_api.put(value > 0 ? QStringLiteral("rating-love") : QStringLiteral("rating-dislike"),
              path,
              {{QStringLiteral("type"), QStringLiteral("ratings")},
               {QStringLiteral("attributes"), QJsonObject{{QStringLiteral("value"), value > 0 ? 1 : -1}}}});
}

void AppController::rateDetail(int value)
{
    if (m_detailRatingId.isEmpty())
        return;
    setRating(m_detailRatingId, m_detailType, value);
}

void AppController::requestSearchHints(const QString &term)
{
    if (m_demo || !authenticated())
        return;
    const QString trimmed = term.trimmed();
    if (trimmed.isEmpty()) {
        if (!m_searchHints.isEmpty()) {
            m_searchHints.clear();
            Q_EMIT searchHintsChanged();
        }
        return;
    }
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("term"), trimmed);
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("8"));
    m_api.get(QStringLiteral("search-hints"), QStringLiteral("/v1/catalog/%1/search/hints?%2").arg(m_api.storefront(), query.toString()));
}

void AppController::lookup(const QString &type, const QString &term, const QString &name)
{
    if (m_demo || !authenticated() || term.trimmed().isEmpty())
        return;
    m_lookupType = type;
    m_lookupName = name;
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("term"), term.trimmed());
    query.addQueryItem(QStringLiteral("types"), type);
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("1"));
    m_api.get(QStringLiteral("lookup"), QStringLiteral("/v1/catalog/%1/search?%2").arg(m_api.storefront(), query.toString()));
}

void AppController::openArtistNamed(const QString &name)
{
    lookup(QStringLiteral("artists"), name, name);
}

void AppController::openAlbumNamed(const QString &album, const QString &artist)
{
    // Album titles repeat across artists, so the artist narrows the match.
    lookup(QStringLiteral("albums"), artist.isEmpty() ? album : album + QLatin1Char(' ') + artist, album);
}

void AppController::handleLookup(const QJsonDocument &document)
{
    const auto data
        = document.object().value(QStringLiteral("results")).toObject().value(m_lookupType).toObject().value(QStringLiteral("data")).toArray();
    if (data.isEmpty()) {
        setError(tr("Could not find “%1” in the Apple Music catalog.").arg(m_lookupName));
        return;
    }
    const MediaItem item = MediaItem::fromJson(data.first().toObject());
    openDetail(item.id, item.id, m_lookupType, item.title, item.subtitle, item.artworkUrl);
    Q_EMIT detailOpened();
}

void AppController::openDetail(
    const QString &id, const QString &catalogId, const QString &type, const QString &title, const QString &subtitle, const QString &artwork)
{
    m_detailTitle = title;
    m_detailSubtitle = subtitle;
    m_detailArtwork = artwork;
    m_detailType = type;
    m_detailId = id;
    m_detailCatalogId = catalogId.isEmpty() ? id : catalogId;
    // Apple exposes no way to ask whether a catalog resource is in the library,
    // so the mirrored library answers it. That also supplies the library id,
    // which is the id removing the item has to be sent with — the catalog id
    // will not do. A library-* page is in the library by definition.
    m_detailInLibrary = type.startsWith(QStringLiteral("library-"));
    if (!m_detailInLibrary) {
        const QString cachedId = m_cache.libraryIdFor(cacheKindFor(type), m_detailCatalogId);
        if (!cachedId.isEmpty()) {
            m_detailInLibrary = true;
            m_detailId = cachedId;
        }
    }
    m_detailTracks.clear();
    m_detailCuratorId.clear();
    m_detailCuratorName.clear();
    m_detailRecordLabelId.clear();
    m_detailRecordLabelName.clear();
    const QString ratingId = catalogId.isEmpty() ? id : catalogId;
    m_detailRatingId = ratableType(type) && !ratingId.startsWith(QLatin1Char('i')) ? ratingId : QString();
    m_detailRating = 0;
    Q_EMIT detailChanged();
    Q_EMIT detailRatingChanged();
    Q_EMIT detailLibraryChanged();
    if (!m_detailRatingId.isEmpty() && !m_demo && authenticated()) {
        QString resource = type;
        resource.remove(QStringLiteral("library-"));
        m_api.get(kRatingsTagPrefix + QStringLiteral("detail"), QStringLiteral("/v1/me/ratings/%1/%2").arg(resource, m_detailRatingId));
    }

    const bool artist = type.contains(QStringLiteral("artist"));
    if (artist) {
        for (auto *model : {&m_artistTopSongs, &m_artistAlbums, &m_artistSingles, &m_artistSimilar, &m_artistLatest})
            model->clear();
    }

    const QString path = collectionPath(id, catalogId, type);
    if (path.isEmpty())
        return;
    if (artist && path.contains(QStringLiteral("views=")))
        requestModel(QStringLiteral("artist-detail"), path, &m_detailTracks);
    else
        requestModel(QStringLiteral("detail"), path, &m_detailTracks);
}

QString AppController::collectionPath(const QString &id, const QString &catalogId, const QString &type)
{
    const QString resourceId = catalogId.isEmpty() ? id : catalogId;
    if (type == QStringLiteral("albums") || (type == QStringLiteral("library-albums") && !catalogId.isEmpty()))
        return QStringLiteral("/v1/catalog/%1/albums/%2?include=tracks").arg(m_api.storefront(), resourceId);
    if (type == QStringLiteral("playlists") || (type == QStringLiteral("library-playlists") && !catalogId.isEmpty()))
        return QStringLiteral("/v1/catalog/%1/playlists/%2?include=tracks").arg(m_api.storefront(), resourceId);
    if (type == QStringLiteral("library-albums"))
        return QStringLiteral("/v1/me/library/albums/%1?include=tracks,catalog").arg(id);
    if (type == QStringLiteral("library-playlists"))
        return QStringLiteral("/v1/me/library/playlists/%1?include=tracks,catalog").arg(id);
    if (type == QStringLiteral("artists"))
        return artistPath(resourceId);
    if (type == QStringLiteral("library-artists"))
        return catalogId.isEmpty() ? QStringLiteral("/v1/me/library/artists/%1?include=albums,catalog").arg(id) : artistPath(catalogId);
    if (type == QStringLiteral("curators"))
        return QStringLiteral("/v1/catalog/%1/curators/%2?include=playlists").arg(m_api.storefront(), resourceId);
    if (type == QStringLiteral("apple-curators"))
        return QStringLiteral("/v1/catalog/%1/apple-curators/%2?include=playlists").arg(m_api.storefront(), resourceId);
    if (type == QStringLiteral("activities"))
        return QStringLiteral("/v1/catalog/%1/activities/%2?include=playlists").arg(m_api.storefront(), resourceId);
    if (type == QStringLiteral("record-labels"))
        return QStringLiteral("/v1/catalog/%1/record-labels/%2?include=albums").arg(m_api.storefront(), resourceId);
    return {};
}

QString AppController::artistPath(const QString &catalogArtistId)
{
    // One request returns the whole artist page. Views are a catalog-only
    // feature, so a library artist without a catalog counterpart still falls
    // back to its plain album relationship.
    return QStringLiteral("/v1/catalog/%1/artists/%2?views=%3")
        .arg(m_api.storefront(),
             catalogArtistId,
             QStringLiteral("latest-release,top-songs,full-albums,singles,"
                            "similar-artists"));
}

void AppController::playCollection(const QString &id, const QString &catalogId, const QString &type)
{
    const QString path = collectionPath(id, catalogId, type);
    if (path.isEmpty())
        return;
    m_pendingPlayType = type;
    requestModel(QStringLiteral("play-collection"), path, &m_pendingPlay);
}

void AppController::playModel(MediaModel *model, int startIndex)
{
    if (!model)
        return;
    const auto ids = model->playbackIds();
    int songIndex = 0;
    for (int row = 0; row < startIndex; ++row) {
        const auto *item = model->itemAt(row);
        if (item && item->isPlayable())
            ++songIndex;
    }
    m_player.playSongs(ids, songIndex);
}

void AppController::playDetail(int startIndex)
{
    playModel(&m_detailTracks, startIndex);
}

QList<MediaModel *> AppController::allModels()
{
    return {&m_recent,
            &m_heavyRotation,
            &m_recentlyAdded,
            &m_stations,
            &m_songs,
            &m_albums,
            &m_artists,
            &m_playlists,
            &m_search,
            &m_searchSongs,
            &m_searchAlbums,
            &m_searchArtists,
            &m_searchPlaylists,
            &m_detailTracks};
}

void AppController::setFavorite(const QString &id, const QString &type, bool favorite)
{
    if (m_demo || id.isEmpty())
        return;
    QString resource = type;
    resource.remove(QStringLiteral("library-"));
    if (resource.isEmpty())
        resource = QStringLiteral("songs");
    const QString path = QStringLiteral("/v1/me/favorites?ids[%1]=%2").arg(resource, id);
    if (favorite)
        m_api.post(kFavoriteAddTagPrefix + id, path);
    else
        m_api.del(kFavoriteRemoveTagPrefix + id, path);
}

void AppController::setInLibrary(const QString &id, const QString &type, bool inLibrary)
{
    if (m_demo || id.isEmpty())
        return;
    QString resource = type;
    resource.remove(QStringLiteral("library-"));
    if (resource.isEmpty())
        resource = QStringLiteral("songs");
    if (inLibrary)
        m_api.post(kLibraryAddTagPrefix + id, QStringLiteral("/v1/me/library?ids[%1]=%2").arg(resource, id));
    else
        m_api.del(kLibraryRemoveTagPrefix + id, QStringLiteral("/v1/me/library/%1/%2").arg(resource, id));
}

// One place for the state update that a favorite or library write confirms,
// shared by both kinds since only the model method and message text differ.
void AppController::applyLibraryWrite(const QString &id, bool enabled, bool isFavorite)
{
    auto models = allModels();
    models.append(m_player.queueModel());
    for (auto *model : models) {
        if (isFavorite)
            model->setFavorite(id, enabled);
        else
            model->setInLibrary(id, enabled);
    }
    if (isFavorite)
        m_cache.setFavorite(id, enabled);
    else
        m_cache.setInLibrary(id, enabled);
    setMessage(isFavorite ? (enabled ? tr("Added to Favorites") : tr("Removed from Favorites"))
                          : (enabled ? tr("Added to Library") : tr("Removed from Library")));
}

void AppController::createPlaylist(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return;
    m_api.post(QStringLiteral("create-playlist"),
               QStringLiteral("/v1/me/library/playlists"),
               {{QStringLiteral("attributes"), QJsonObject{{QStringLiteral("name"), trimmed}}}});
}

void AppController::addToPlaylist(const QString &playlistId, const QString &songId)
{
    if (playlistId.isEmpty() || songId.isEmpty())
        return;
    m_api.post(
        QStringLiteral("add-playlist-track"),
        QStringLiteral("/v1/me/library/playlists/%1/tracks").arg(playlistId),
        {{QStringLiteral("data"), QJsonArray{QJsonObject{{QStringLiteral("id"), songId}, {QStringLiteral("type"), QStringLiteral("songs")}}}}});
}

void AppController::loadDemoData()
{
    const auto violet = demoItem("album-violet", "albums", "Violet Hours", "Mira Vale", "", "violet-hours.svg");
    const auto transit = demoItem("album-transit", "albums", "Night Transit", "North Arcade", "", "night-transit.svg");
    const auto tidal = demoItem("album-tidal", "albums", "Tidal Memory", "The Quiet Shapes", "", "tidal-memory.svg");
    const auto daylight = demoItem("album-daylight", "albums", "Soft Geometry", "Daylight Assembly", "", "soft-geometry.svg");

    m_recent.replaceItems({violet, transit, tidal, daylight});
    m_heavyRotation.replaceItems({tidal, violet, daylight, transit});
    m_recentlyAdded.replaceItems({daylight, violet, transit, tidal});
    m_albums.replaceItems({violet,
                           transit,
                           tidal,
                           daylight,
                           demoItem("album-cloudline", "albums", "Cloudline", "Juniper Coast", "", "cloudline.svg"),
                           demoItem("album-static", "albums", "Static Bloom", "Echo Season", "", "static-bloom.svg")});
    m_artists.replaceItems({demoItem("artist-mira", "artists", "Mira Vale", "Alternative", "", "violet-hours.svg"),
                            demoItem("artist-arcade", "artists", "North Arcade", "Electronic", "", "night-transit.svg"),
                            demoItem("artist-shapes", "artists", "The Quiet Shapes", "Indie Pop", "", "tidal-memory.svg"),
                            demoItem("artist-daylight", "artists", "Daylight Assembly", "Ambient", "", "soft-geometry.svg")});
    m_playlists.replaceItems({demoItem("playlist-focus", "playlists", "Quiet Focus", "Kadenza", "", "soft-geometry.svg"),
                              demoItem("playlist-evening", "playlists", "Late Evening", "Kadenza", "", "night-transit.svg")});

    QList<MediaItem> songs = {demoItem("demo-1", "songs", "Afterglow", "Mira Vale", "Violet Hours", "violet-hours.svg", 238000),
                              demoItem("demo-2", "songs", "Glass Gardens", "North Arcade", "Night Transit", "night-transit.svg", 213000),
                              demoItem("demo-3", "songs", "Blue Current", "The Quiet Shapes", "Tidal Memory", "tidal-memory.svg", 266000),
                              demoItem("demo-4", "songs", "Open Window", "Daylight Assembly", "Soft Geometry", "soft-geometry.svg", 194000)};
    m_songs.replaceItems(songs);
    m_search.replaceItems(
        {songs.at(0), violet, songs.at(1), transit, demoItem("artist-shapes", "artists", "The Quiet Shapes", "Indie Pop", "", "tidal-memory.svg")});
    m_searchSongs.replaceItems({songs.at(0), songs.at(1)});
    m_searchAlbums.replaceItems({violet, transit});
    m_searchArtists.replaceItems({demoItem("artist-shapes", "artists", "The Quiet Shapes", "Indie Pop", "", "tidal-memory.svg")});
    m_searchPlaylists.replaceItems({demoItem("playlist-focus", "playlists", "Quiet Focus", "Kadenza", "", "soft-geometry.svg")});
}

void AppController::handleSuccess(const QString &tag, const QJsonDocument &document)
{
    if (tag == QStringLiteral("artist-detail")) {
        m_pendingModels.remove(tag);
        if (m_loadingCount > 0)
            --m_loadingCount;
        Q_EMIT loadingChanged();
        handleArtistDetail(document);
        return;
    }
    if (tag == QStringLiteral("replay")) {
        m_pendingModels.remove(tag);
        if (m_loadingCount > 0)
            --m_loadingCount;
        Q_EMIT loadingChanged();
        handleReplay(document);
        return;
    }
    if (tag == QStringLiteral("playlist-folder")) {
        m_pendingModels.remove(tag);
        if (m_loadingCount > 0)
            --m_loadingCount;
        Q_EMIT loadingChanged();
        handleFolderContents(document);
        return;
    }
    if (tag == QStringLiteral("search-curators") || tag == QStringLiteral("search-activities")) {
        auto *model = m_pendingModels.take(tag);
        if (m_loadingCount > 0)
            --m_loadingCount;
        Q_EMIT loadingChanged();
        if (!model)
            return;
        const QString kind = tag == QStringLiteral("search-curators") ? QStringLiteral("curators") : QStringLiteral("activities");
        const auto sectionData = document.object().value(QStringLiteral("results")).toObject().value(kind).toObject().value(QStringLiteral("data")).toArray();
        model->replace(sectionData);
        model->setLoading(false);
        return;
    }
    if (tag == QStringLiteral("genres") || tag == QStringLiteral("station-genres")) {
        QVariantList list;
        for (const auto &value : document.object().value(QStringLiteral("data")).toArray()) {
            const auto resource = value.toObject();
            list.push_back(QVariantMap{
                {QStringLiteral("id"), resource.value(QStringLiteral("id")).toString()},
                {QStringLiteral("name"), resource.value(QStringLiteral("attributes")).toObject().value(QStringLiteral("name")).toString()},
            });
        }
        if (tag == QStringLiteral("genres")) {
            m_genres = list;
            Q_EMIT genresChanged();
        } else {
            m_stationGenres = list;
            Q_EMIT stationGenresChanged();
        }
        return;
    }
    if (tag.startsWith(kRatingsTagPrefix)) {
        handleRatings(tag, document);
        return;
    }
    if (tag.startsWith(kFavoriteAddTagPrefix) || tag.startsWith(kFavoriteRemoveTagPrefix)) {
        const bool enabled = tag.startsWith(kFavoriteAddTagPrefix);
        applyLibraryWrite(tag.mid((enabled ? kFavoriteAddTagPrefix : kFavoriteRemoveTagPrefix).size()), enabled, true);
        return;
    }
    if (tag.startsWith(kLibraryAddTagPrefix) || tag.startsWith(kLibraryRemoveTagPrefix)) {
        const bool enabled = tag.startsWith(kLibraryAddTagPrefix);
        applyLibraryWrite(tag.mid((enabled ? kLibraryAddTagPrefix : kLibraryRemoveTagPrefix).size()), enabled, false);
        return;
    }
    if (tag == QStringLiteral("lookup")) {
        handleLookup(document);
        return;
    }
    if (tag == QStringLiteral("rating-love") || tag == QStringLiteral("rating-dislike") || tag == QStringLiteral("rating-clear")) {
        setMessage(tag == QStringLiteral("rating-love")      ? tr("Loved")
                   : tag == QStringLiteral("rating-dislike") ? tr("Disliked")
                                                             : tr("Rating cleared"));
        return;
    }
    if (tag == QStringLiteral("charts")) {
        m_pendingModels.remove(tag);
        if (m_loadingCount > 0)
            --m_loadingCount;
        Q_EMIT loadingChanged();
        const auto results = document.object().value(QStringLiteral("results")).toObject();
        // Each type maps to an array of charts; Kadenza shows the first of each.
        const auto firstChart = [&results](const QString &name) {
            const auto charts = results.value(name).toArray();
            return charts.isEmpty() ? QJsonArray() : charts.first().toObject().value(QStringLiteral("data")).toArray();
        };
        m_chartSongs.replace(firstChart(QStringLiteral("songs")));
        requestRatingsFor(&m_chartSongs);
        m_chartAlbums.replace(firstChart(QStringLiteral("albums")));
        m_chartPlaylists.replace(firstChart(QStringLiteral("playlists")));
        requestRatingsFor(&m_chartAlbums);
        requestRatingsFor(&m_chartPlaylists);
        setError({});
        return;
    }
    if (tag == QStringLiteral("recommendations")) {
        handleRecommendations(document);
        return;
    }
    if (tag == QStringLiteral("search-hints")) {
        QStringList hints;
        const auto terms = document.object().value(QStringLiteral("results")).toObject().value(QStringLiteral("terms")).toArray();
        for (const auto &term : terms)
            hints.push_back(term.toString());
        if (hints != m_searchHints) {
            m_searchHints = hints;
            Q_EMIT searchHintsChanged();
        }
        return;
    }
    if (tag == QStringLiteral("storefront")) {
        const auto data = document.object().value(QStringLiteral("data")).toArray();
        if (data.isEmpty())
            return;
        const QString id = data.first().toObject().value(QStringLiteral("id")).toString();
        if (id.isEmpty() || id == m_api.storefront())
            return;
        m_api.setStorefront(id);
        // Anything already fetched came from the wrong region.
        refreshHome();
        return;
    }
    if (tag == QStringLiteral("create-playlist")) {
        setMessage(tr("Playlist created"));
        m_playlists.clear();
        loadLibrary(QStringLiteral("playlists"), true);
        return;
    }
    if (tag == QStringLiteral("add-playlist-track")) {
        setMessage(tr("Added to playlist"));
        return;
    }
    auto *model = m_pendingModels.take(tag);
    if (!model)
        return;
    QJsonArray data;
    QString next;
    const auto root = document.object();

    if (tag == QStringLiteral("search")) {
        const auto results = root.value(QStringLiteral("results")).toObject();
        const QString prefix = m_searchLibrary ? QStringLiteral("library-") : QString();
        const QList<QPair<QString, MediaModel *>> sections = {{prefix + QStringLiteral("artists"), &m_searchArtists},
                                                              {prefix + QStringLiteral("albums"), &m_searchAlbums},
                                                              {prefix + QStringLiteral("songs"), &m_searchSongs},
                                                              {prefix + QStringLiteral("playlists"), &m_searchPlaylists}};
        for (const auto &[kind, sectionModel] : sections) {
            const auto section = results.value(kind).toObject();
            const auto sectionData = section.value(QStringLiteral("data")).toArray();
            sectionModel->replace(sectionData);
            for (const auto &value : sectionData)
                data.append(value);
        }
        requestRatingsFor(&m_searchSongs);
        requestRatingsFor(&m_searchAlbums);
        requestRatingsFor(&m_searchPlaylists);
    } else if (tag == QStringLiteral("detail") && !root.value(QStringLiteral("data")).toArray().isEmpty()) {
        const auto resource = root.value(QStringLiteral("data")).toArray().first().toObject();
        const QString type = resource.value(QStringLiteral("type")).toString();
        const auto attributes = resource.value(QStringLiteral("attributes")).toObject();
        if (type != QStringLiteral("songs") && type != QStringLiteral("library-songs")) {
            if (m_detailTitle.isEmpty())
                m_detailTitle = attributes.value(QStringLiteral("name")).toString();
            Q_EMIT detailChanged();
        }
        // Only playlists carry a curator and only albums a record label; every
        // other type simply has neither relationship, so these stay empty. The
        // relationship reference (id) is present without an extra `include=` —
        // only its embedded attributes would need that — so the display name is
        // read from the plain attribute Apple already puts on the resource
        // itself rather than risking an unconfirmed relationship name on an
        // `include=` list that the existing album/playlist requests depend on.
        const auto relationships = resource.value(QStringLiteral("relationships")).toObject();
        const auto curatorRef = relationships.value(QStringLiteral("curator")).toObject().value(QStringLiteral("data")).toArray();
        if (!curatorRef.isEmpty()) {
            m_detailCuratorId = curatorRef.first().toObject().value(QStringLiteral("id")).toString();
            m_detailCuratorName = attributes.value(QStringLiteral("curatorName")).toString();
            Q_EMIT detailChanged();
        }
        auto labelRef = relationships.value(QStringLiteral("record-labels")).toObject().value(QStringLiteral("data")).toArray();
        if (labelRef.isEmpty())
            labelRef = relationships.value(QStringLiteral("recordLabels")).toObject().value(QStringLiteral("data")).toArray();
        if (!labelRef.isEmpty()) {
            m_detailRecordLabelId = labelRef.first().toObject().value(QStringLiteral("id")).toString();
            m_detailRecordLabelName = attributes.value(QStringLiteral("recordLabel")).toString();
            Q_EMIT detailChanged();
        }
        extractCollection(root, m_detailType, data, next);
    } else if (tag == QStringLiteral("play-collection")) {
        extractCollection(root, m_pendingPlayType, data, next);
    } else {
        data = root.value(QStringLiteral("data")).toArray();
        next = root.value(QStringLiteral("next")).toString();
    }

    const bool append = m_appendRequests.remove(tag);
    if (append)
        model->append(data, next);
    else
        model->replace(data, next);
    model->setLoading(false);
    if (tag == QStringLiteral("detail") || tag == QStringLiteral("recent-tracks") || tag == QStringLiteral("stations"))
        requestRatingsFor(model);
    if (m_loadingCount > 0)
        --m_loadingCount;
    Q_EMIT loadingChanged();
    setError({});
    if (tag == QStringLiteral("play-collection")) {
        if (m_pendingPlay.playableCount() > 0)
            playModel(&m_pendingPlay, 0);
        else
            setError(tr("Nothing in this selection can be played."));
        return;
    }
}

void AppController::handleFailure(const QString &tag, int status, const QString &message)
{
    if (tag == QStringLiteral("replay") && qEnvironmentVariableIsSet("KADENZA_TRACE_REPLAY"))
        qWarning().noquote() << "kadenza: replay request failed" << status << message;
    if (tag.startsWith(kRatingsTagPrefix)) {
        // Unknown rating state is not worth an error banner.
        return;
    }
    if (tag.startsWith(kFavoriteAddTagPrefix) || tag.startsWith(kFavoriteRemoveTagPrefix) || tag.startsWith(kLibraryAddTagPrefix)
        || tag.startsWith(kLibraryRemoveTagPrefix)) {
        setError(message.isEmpty() ? tr("Apple Music rejected that change.") : message);
        return;
    }
    if (tag == QStringLiteral("search-hints") || tag == QStringLiteral("storefront") || tag == QStringLiteral("recommendations")
        || tag == QStringLiteral("genres") || tag == QStringLiteral("station-genres")) {
        // All best-effort; failing them should not disturb the user.
        return;
    }
    if (tag == QStringLiteral("search-curators") || tag == QStringLiteral("search-activities")) {
        // Whether Apple's search `types=` enum even accepts these values was
        // never confirmed, so a rejection here just leaves the shelf empty
        // rather than surfacing "Apple Music request failed" over a working
        // search.
        auto *model = m_pendingModels.take(tag);
        if (model)
            model->setLoading(false);
        if (m_loadingCount > 0)
            --m_loadingCount;
        Q_EMIT loadingChanged();
        return;
    }
    if (tag.startsWith(kSyncTagPrefix)) {
        const QString cacheKind = tag.mid(kSyncTagPrefix.size());
        abandonSync(cacheKind);
        if (m_cache.count(cacheKind) == 0)
            setError(tr("Could not load your library (%1): %2").arg(status).arg(message));
        return;
    }
    if (tag == QStringLiteral("create-playlist") || tag == QStringLiteral("add-playlist-track")) {
        setError(tr("Apple Music request failed (%1): %2").arg(status).arg(message));
        return;
    }
    auto *model = m_pendingModels.take(tag);
    m_appendRequests.remove(tag);
    const QString text = tr("Apple Music request failed (%1): %2").arg(status).arg(message);
    if (model) {
        model->setLoading(false);
        model->setError(text);
    }
    if (m_loadingCount > 0)
        --m_loadingCount;
    Q_EMIT loadingChanged();
    setError(text);
}
