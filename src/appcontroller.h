#pragma once

#include "apiclient.h"
#include "librarycache.h"
#include "mediamodel.h"
#include "playercontroller.h"

#include <QObject>
#include <QSet>
#include <QThread>
#include <QVariantList>
#include <qqmlintegration.h>

class QQmlEngine;
class QJSEngine;
class LibrarySyncWorker;

class AppController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(App)
    QML_SINGLETON
    Q_PROPERTY(PlayerController *player READ player CONSTANT)
    Q_PROPERTY(MediaModel *recentModel READ recentModel CONSTANT)
    Q_PROPERTY(MediaModel *heavyRotationModel READ heavyRotationModel CONSTANT)
    Q_PROPERTY(MediaModel *recentTracksModel READ recentTracksModel CONSTANT)
    Q_PROPERTY(QVariantList recommendations READ recommendations NOTIFY recommendationsChanged)
    Q_PROPERTY(MediaModel *recentlyAddedModel READ recentlyAddedModel CONSTANT)
    Q_PROPERTY(MediaModel *songsModel READ songsModel CONSTANT)
    Q_PROPERTY(MediaModel *albumsModel READ albumsModel CONSTANT)
    Q_PROPERTY(MediaModel *artistsModel READ artistsModel CONSTANT)
    Q_PROPERTY(MediaModel *playlistsModel READ playlistsModel CONSTANT)
    Q_PROPERTY(MediaModel *searchModel READ searchModel CONSTANT)
    Q_PROPERTY(MediaModel *searchSongsModel READ searchSongsModel CONSTANT)
    Q_PROPERTY(MediaModel *searchAlbumsModel READ searchAlbumsModel CONSTANT)
    Q_PROPERTY(MediaModel *searchArtistsModel READ searchArtistsModel CONSTANT)
    Q_PROPERTY(MediaModel *searchPlaylistsModel READ searchPlaylistsModel CONSTANT)
    Q_PROPERTY(MediaModel *detailTracksModel READ detailTracksModel CONSTANT)
    Q_PROPERTY(MediaModel *stationsModel READ stationsModel CONSTANT)
    Q_PROPERTY(MediaModel *liveStationsModel READ liveStationsModel CONSTANT)
    Q_PROPERTY(MediaModel *personalStationModel READ personalStationModel CONSTANT)
    Q_PROPERTY(QVariantList stationGenres READ stationGenres NOTIFY stationGenresChanged)
    Q_PROPERTY(MediaModel *chartSongsModel READ chartSongsModel CONSTANT)
    Q_PROPERTY(MediaModel *chartAlbumsModel READ chartAlbumsModel CONSTANT)
    Q_PROPERTY(MediaModel *chartPlaylistsModel READ chartPlaylistsModel CONSTANT)
    Q_PROPERTY(QVariantList genres READ genres NOTIFY genresChanged)
    Q_PROPERTY(MediaModel *artistTopSongsModel READ artistTopSongsModel CONSTANT)
    Q_PROPERTY(MediaModel *artistAlbumsModel READ artistAlbumsModel CONSTANT)
    Q_PROPERTY(MediaModel *artistSinglesModel READ artistSinglesModel CONSTANT)
    Q_PROPERTY(MediaModel *artistSimilarModel READ artistSimilarModel CONSTANT)
    Q_PROPERTY(MediaModel *artistLatestModel READ artistLatestModel CONSTANT)
    Q_PROPERTY(MediaModel *replayTopSongsModel READ replayTopSongsModel CONSTANT)
    Q_PROPERTY(MediaModel *replayTopAlbumsModel READ replayTopAlbumsModel CONSTANT)
    Q_PROPERTY(MediaModel *replayTopArtistsModel READ replayTopArtistsModel CONSTANT)
    Q_PROPERTY(MediaModel *playlistFolderModel READ playlistFolderModel CONSTANT)
    Q_PROPERTY(QString playlistFolderTitle READ playlistFolderTitle NOTIFY playlistFolderChanged)
    Q_PROPERTY(QString playlistFolderId READ playlistFolderId NOTIFY playlistFolderChanged)
    Q_PROPERTY(MediaModel *searchCuratorsModel READ searchCuratorsModel CONSTANT)
    Q_PROPERTY(MediaModel *searchActivitiesModel READ searchActivitiesModel CONSTANT)
    Q_PROPERTY(QString detailCuratorId READ detailCuratorId NOTIFY detailChanged)
    Q_PROPERTY(QString detailCuratorName READ detailCuratorName NOTIFY detailChanged)
    Q_PROPERTY(QString detailRecordLabelId READ detailRecordLabelId NOTIFY detailChanged)
    Q_PROPERTY(QString detailRecordLabelName READ detailRecordLabelName NOTIFY detailChanged)
    Q_PROPERTY(QString detailTitle READ detailTitle NOTIFY detailChanged)
    Q_PROPERTY(QString detailSubtitle READ detailSubtitle NOTIFY detailChanged)
    Q_PROPERTY(QString detailArtwork READ detailArtwork NOTIFY detailChanged)
    Q_PROPERTY(QString detailType READ detailType NOTIFY detailChanged)
    Q_PROPERTY(int detailRating READ detailRating NOTIFY detailRatingChanged)
    Q_PROPERTY(bool detailRatable READ detailRatable NOTIFY detailChanged)
    Q_PROPERTY(bool detailCollectable READ detailCollectable NOTIFY detailChanged)
    Q_PROPERTY(bool detailInLibrary READ detailInLibrary NOTIFY detailLibraryChanged)
    Q_PROPERTY(bool authenticated READ authenticated NOTIFY authenticatedChanged)
    Q_PROPERTY(int sidebarWidth READ sidebarWidth WRITE setSidebarWidth NOTIFY sidebarWidthChanged)
    Q_PROPERTY(bool sidebarCollapsed READ sidebarCollapsed WRITE setSidebarCollapsed NOTIFY sidebarCollapsedChanged)
    Q_PROPERTY(int artworkSize READ artworkSize WRITE setArtworkSize NOTIFY artworkSizeChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool syncing READ syncing NOTIFY syncingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(QStringList searchHistory READ searchHistory NOTIFY searchHistoryChanged)
    Q_PROPERTY(QStringList searchHints READ searchHints NOTIFY searchHintsChanged)
    Q_PROPERTY(bool searchLibrary READ searchLibrary WRITE setSearchLibrary NOTIFY searchLibraryChanged)
    Q_PROPERTY(QString lastPage READ lastPage WRITE setLastPage NOTIFY lastPageChanged)

public:
    // Deliberately not default-constructible: QML instantiates a singleton
    // itself when it can, which quietly produced a second AppController — and so
    // a second PlayerController and a second sidecar. The UI drove one instance
    // while MPRIS reported on the other. Without a default constructor the engine
    // has to go through create(), which hands back the one main() built.
    explicit AppController(QObject *parent);
    ~AppController() override;
    static AppController *create(QQmlEngine *, QJSEngine *);
    static void setInstance(AppController *instance);

    PlayerController *player();
    MediaModel *recentModel();
    MediaModel *heavyRotationModel();
    MediaModel *recentTracksModel();
    QVariantList recommendations() const;
    MediaModel *recentlyAddedModel();
    MediaModel *songsModel();
    MediaModel *albumsModel();
    MediaModel *artistsModel();
    MediaModel *playlistsModel();
    MediaModel *searchModel();
    MediaModel *searchSongsModel();
    MediaModel *searchAlbumsModel();
    MediaModel *searchArtistsModel();
    MediaModel *searchPlaylistsModel();
    MediaModel *detailTracksModel();
    MediaModel *stationsModel();
    MediaModel *liveStationsModel();
    MediaModel *personalStationModel();
    QVariantList stationGenres() const;
    MediaModel *chartSongsModel();
    MediaModel *chartAlbumsModel();
    MediaModel *chartPlaylistsModel();
    QVariantList genres() const;
    MediaModel *artistTopSongsModel();
    MediaModel *artistAlbumsModel();
    MediaModel *artistSinglesModel();
    MediaModel *artistSimilarModel();
    MediaModel *artistLatestModel();
    MediaModel *replayTopSongsModel();
    MediaModel *replayTopAlbumsModel();
    MediaModel *replayTopArtistsModel();
    MediaModel *playlistFolderModel();
    QString playlistFolderTitle() const;
    QString playlistFolderId() const;
    MediaModel *searchCuratorsModel();
    MediaModel *searchActivitiesModel();
    QString detailCuratorId() const;
    QString detailCuratorName() const;
    QString detailRecordLabelId() const;
    QString detailRecordLabelName() const;
    QString detailTitle() const;
    QString detailSubtitle() const;
    QString detailArtwork() const;
    QString detailType() const;
    int detailRating() const;
    bool detailRatable() const;
    bool detailCollectable() const;
    bool detailInLibrary() const;
    bool authenticated() const;
    int sidebarWidth() const;
    void setSidebarWidth(int width);
    bool sidebarCollapsed() const;
    void setSidebarCollapsed(bool collapsed);
    int artworkSize() const;
    void setArtworkSize(int gridUnits);
    bool loading() const;
    bool syncing() const;
    QString error() const;
    QString message() const;
    QStringList searchHistory() const;
    QStringList searchHints() const;
    bool searchLibrary() const;
    void setSearchLibrary(bool libraryOnly);
    QString lastPage() const;
    void setLastPage(const QString &page);

    Q_INVOKABLE void refreshHome();
    Q_INVOKABLE void loadCharts(bool refresh = false, const QString &genreId = {});
    Q_INVOKABLE void loadGenres();
    Q_INVOKABLE void loadStations(bool refresh = false);
    /// An empty genreId loads Apple's live radio stations; otherwise the live
    /// stations belonging to that station genre.
    Q_INVOKABLE void loadLiveStations(const QString &genreId = {});
    Q_INVOKABLE void loadPersonalStation();
    Q_INVOKABLE void loadStationGenres();
    Q_INVOKABLE void loadReplay(bool refresh = false);
    /// A folder id of "p.playlistsroot" (the default) opens the top level.
    Q_INVOKABLE void loadPlaylistFolder(const QString &id = QStringLiteral("p.playlistsroot"));
    /// value is 1 to love, -1 to dislike, 0 to clear an existing rating.
    Q_INVOKABLE void setRating(const QString &id, const QString &type, int value);
    /// Rates whatever the detail page is currently showing.
    Q_INVOKABLE void rateDetail(int value);
    /// Adds or removes whatever the detail page is showing.
    Q_INVOKABLE void toggleDetailInLibrary();
    /// Apple rates songs, albums, playlists and stations, but not artists.
    static bool ratableType(const QString &type);
    Q_INVOKABLE void loadLibrary(const QString &kind, bool refresh = false);
    Q_INVOKABLE void loadMore(const QString &kind);
    Q_INVOKABLE void search(const QString &term);
    Q_INVOKABLE void requestSearchHints(const QString &term);
    Q_INVOKABLE void openDetail(
        const QString &id, const QString &catalogId, const QString &type, const QString &title, const QString &subtitle, const QString &artwork);
    /// Opens an artist or album that is only known by name. Apple omits the
    /// artist and album relationships from most responses, so the ids the lists
    /// carry are frequently empty; looking the name up in the catalog keeps the
    /// links working everywhere rather than only where an id happened to arrive.
    Q_INVOKABLE void openArtistNamed(const QString &name);
    Q_INVOKABLE void openAlbumNamed(const QString &album, const QString &artist);
    Q_INVOKABLE void playModel(MediaModel *model, int startIndex);
    Q_INVOKABLE void playDetail(int startIndex = 0);
    Q_INVOKABLE void playCollection(const QString &id, const QString &catalogId, const QString &type);
    Q_INVOKABLE void setFavorite(const QString &id, const QString &type, bool favorite);
    Q_INVOKABLE void setInLibrary(const QString &id, const QString &type, bool inLibrary);
    Q_INVOKABLE void createPlaylist(const QString &name);
    Q_INVOKABLE void addToPlaylist(const QString &playlistId, const QString &songId);
    Q_INVOKABLE void clearMessage();
    Q_INVOKABLE void clearError();

signals:
    void authenticatedChanged();
    void sidebarWidthChanged();
    void sidebarCollapsedChanged();
    void artworkSizeChanged();
    void loadingChanged();
    void syncingChanged();
    void errorChanged();
    void detailChanged();
    /// A detail page was opened by the controller rather than by the UI, so the
    /// UI still has to navigate to it.
    void detailOpened();
    void detailRatingChanged();
    void detailLibraryChanged();
    void messageChanged();
    void searchHistoryChanged();
    void searchHintsChanged();
    void recommendationsChanged();
    void searchLibraryChanged();
    void lastPageChanged();
    void stationGenresChanged();
    void genresChanged();
    void playlistFolderChanged();

    /// Handed to LibrarySyncWorker across the thread boundary. Not part of the
    /// QML-facing surface; emitted only by the library sync.
    void ingestPageRequested(const QString &kind, qint64 epoch, int firstPosition, const QByteArray &payload);
    void finishSyncRequested(const QString &kind, qint64 epoch);

private:
    MediaModel *modelForKind(const QString &kind);
    QString collectionPath(const QString &id, const QString &catalogId, const QString &type);
    QString artistPath(const QString &catalogArtistId);
    // Library caching -------------------------------------------------------
    static QString cacheKindFor(const QString &uiKind);
    static LibraryCache::Sort sortFor(const QString &uiKind);
    static QString libraryPathFor(const QString &cacheKind);
    void fillFromCache(const QString &uiKind);
    void startLibrarySync(const QString &cacheKind);
    /// Hands a freshly arrived page to the worker thread.
    void handleSyncPayload(const QString &cacheKind, const QByteArray &payload);
    /// Decides whether to ask Apple for another page, once the worker has
    /// written the last one.
    void handleSyncPageIngested(const QString &cacheKind, qint64 epoch, int itemCount, const QString &next);
    void handleSyncFinished(const QString &cacheKind, qint64 epoch);
    void abandonSync(const QString &cacheKind);
    void handleArtistDetail(const QJsonDocument &document);
    void handleReplay(const QJsonDocument &document);
    /// Fills a Replay shelf from its unwrapped period-summary rows. Some rows
    /// arrive fully hydrated and some as bare id references depending on
    /// resource type (and possibly account); bare ones are resolved with a
    /// follow-up batched catalog lookup rather than assumed either way.
    void fillReplayShelf(const QJsonArray &items, const QString &catalogType, MediaModel *model);
    void handleFolderContents(const QJsonDocument &document);
    void handleRecommendations(const QJsonDocument &document);
    void handleRatings(const QString &tag, const QJsonDocument &document);
    void handleLookup(const QJsonDocument &document);
    void lookup(const QString &type, const QString &term, const QString &name);
    /// Fetches the user's love/dislike state for the songs in a model.
    void requestRatingsFor(MediaModel *model);
    void applyRating(const QString &id, int rating);
    /// Applies a whole ratings reply: one pass per model, one transaction.
    void applyRatings(const QHash<QString, int> &byId);
    /// Applies a confirmed favorite or library add/remove to every model and the cache.
    /// For a library add, `type` is the resource kind (albums, songs, ...) and
    /// `document` is the add response used to build the new row, so the change
    /// shows up in Albums / Recently Added immediately without a full re-walk.
    void applyLibraryWrite(const QString &id, bool enabled, bool isFavorite, const QString &type = {}, const QJsonDocument &document = {});
    /// For a confirmed library add/remove, mirrors it into the cache and the
    /// live library models (its own kind plus Recently Added) so the change
    /// appears immediately without re-walking the whole library.
    void applyLibraryChange(const QString &id, const QString &type, bool inLibrary, const QJsonDocument &addDocument);
    /// Finds the shelf row (from any live shelf model or the player queue)
    /// whose id or catalogId matches `id`, used to seed a new library row when
    /// the add response has no usable attributes.
    MediaItem findShelfRowFor(const QString &id);
    void refreshCachedModels(const QString &cacheKind);
    void requestModel(const QString &tag, const QString &path, MediaModel *model, bool append = false);
    void handleSuccess(const QString &tag, const QJsonDocument &document);
    void handleFailure(const QString &tag, int status, const QString &message);
    void loadDemoData();
    void setError(const QString &error);
    void setMessage(const QString &message);
    QList<MediaModel *> allModels();

    static AppController *s_instance;
    ApiClient m_api;
    PlayerController m_player;
    MediaModel m_recent;
    MediaModel m_heavyRotation;
    MediaModel m_recentTracks;
    // One MediaModel per recommendation shelf, owned here and rebuilt whenever
    // Apple returns a new set.
    QList<MediaModel *> m_recommendationModels;
    QVariantList m_recommendations;
    MediaModel m_recentlyAdded;
    MediaModel m_songs;
    MediaModel m_albums;
    MediaModel m_artists;
    MediaModel m_playlists;
    MediaModel m_search;
    MediaModel m_searchSongs;
    MediaModel m_searchAlbums;
    MediaModel m_searchArtists;
    MediaModel m_searchPlaylists;
    MediaModel m_detailTracks;
    MediaModel m_stations;
    MediaModel m_liveStations;
    MediaModel m_personalStation;
    QVariantList m_stationGenres;
    MediaModel m_chartSongs;
    MediaModel m_chartAlbums;
    MediaModel m_chartPlaylists;
    QVariantList m_genres;
    MediaModel m_artistTopSongs;
    MediaModel m_artistAlbums;
    MediaModel m_artistSingles;
    MediaModel m_artistSimilar;
    MediaModel m_artistLatest;
    MediaModel m_replayTopSongs;
    MediaModel m_replayTopAlbums;
    MediaModel m_replayTopArtists;
    MediaModel m_playlistFolder;
    QString m_playlistFolderTitle;
    QString m_playlistFolderId;
    MediaModel m_searchCurators;
    MediaModel m_searchActivities;
    QString m_detailCuratorId;
    QString m_detailCuratorName;
    QString m_detailRecordLabelId;
    QString m_detailRecordLabelName;
    // Holds the tracks of a collection the user asked to play straight from a
    // grid tile, so starting playback never disturbs the open detail page.
    MediaModel m_pendingPlay;
    QString m_pendingPlayType;
    QHash<QString, MediaModel *> m_pendingModels;
    QSet<QString> m_appendRequests;
    LibraryCache m_cache;
    // Progress of an in-flight full library walk, keyed by cache kind.
    struct LibrarySync {
        qint64 epoch = 0;
        int position = 0;
        bool bootstrapping = false;
    };
    QHash<QString, LibrarySync> m_syncs;
    // Page decoding and cache writes happen here, not on the GUI thread.
    QThread m_syncThread;
    LibrarySyncWorker *m_syncWorker = nullptr;
    QString m_detailTitle;
    QString m_detailSubtitle;
    QString m_detailArtwork;
    QString m_detailType;
    QString m_detailRatingId;
    QString m_detailId;
    QString m_detailCatalogId;
    bool m_detailInLibrary = false;
    // What an in-flight catalog lookup is trying to open, so the reply can be
    // turned into the right kind of detail page.
    QString m_lookupType;
    QString m_lookupName;
    int m_detailRating = 0;
    QString m_error;
    QString m_message;
    QStringList m_searchHistory;
    QStringList m_searchHints;
    QString m_searchTerm;
    bool m_searchLibrary = false;
    QString m_lastPage = QStringLiteral("home");
    int m_loadingCount = 0;
    bool m_demo = false;
    int m_sidebarWidth = 0;
    bool m_sidebarCollapsed = false;
    int m_artworkSize = 11;
};
