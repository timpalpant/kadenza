#include "mediamodel.h"

#include <algorithm>

MediaModel::MediaModel(QObject *parent)
    : QAbstractListModel(parent)
{}

int MediaModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

int MediaModel::playableCount() const
{
    return static_cast<int>(std::count_if(m_items.cbegin(), m_items.cend(), [](const MediaItem &item) { return item.isPlayable(); }));
}

QVariant MediaModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }
    const auto &item = m_items.at(index.row());
    switch (role) {
    case IdRole:
        return item.id;
    case CatalogIdRole:
        return item.catalogId;
    case TypeRole:
        return item.type;
    case TitleRole:
        return item.title;
    case SubtitleRole:
        return item.subtitle;
    case AlbumRole:
        return item.album;
    case ArtworkRole:
        return item.artworkUrl;
    case DurationRole:
        return item.durationMs;
    case TrackNumberRole:
        return item.trackNumber;
    case ExplicitRole:
        return item.explicitContent;
    case PlayableRole:
        return item.isPlayable();
    case DateAddedRole:
        return item.dateAdded;
    case ArtistIdRole:
        return item.artistId;
    case AlbumIdRole:
        return item.albumId;
    case FavoriteRole:
        return item.favorite;
    case RatingRole:
        return item.rating;
    case InLibraryRole:
        return item.inLibrary;
    default:
        return {};
    }
}

QHash<int, QByteArray> MediaModel::roleNames() const
{
    return {{IdRole, "mediaId"},
            {CatalogIdRole, "catalogId"},
            {TypeRole, "mediaType"},
            {TitleRole, "title"},
            {SubtitleRole, "subtitle"},
            {AlbumRole, "album"},
            {ArtworkRole, "artwork"},
            {DurationRole, "durationMs"},
            {TrackNumberRole, "trackNumber"},
            {ExplicitRole, "explicitContent"},
            {PlayableRole, "playable"},
            {DateAddedRole, "dateAdded"},
            {ArtistIdRole, "artistId"},
            {AlbumIdRole, "albumId"},
            {FavoriteRole, "favorite"},
            {RatingRole, "rating"},
            {InLibraryRole, "inLibrary"}};
}

bool MediaModel::loading() const
{
    return m_loading;
}
void MediaModel::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    Q_EMIT loadingChanged();
}
QString MediaModel::error() const
{
    return m_error;
}
void MediaModel::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    Q_EMIT errorChanged();
}
QString MediaModel::nextPath() const
{
    return m_nextPath;
}
void MediaModel::setNextPath(const QString &path)
{
    if (m_nextPath == path)
        return;
    m_nextPath = path;
    Q_EMIT nextPathChanged();
}

void MediaModel::replace(const QJsonArray &data, const QString &next)
{
    QList<MediaItem> items;
    items.reserve(data.size());
    for (const auto &value : data)
        items.push_back(MediaItem::fromJson(value.toObject()));
    replaceItems(std::move(items));
    setNextPath(next);
}

void MediaModel::append(const QJsonArray &data, const QString &next)
{
    if (data.isEmpty()) {
        setNextPath(next);
        return;
    }
    const int first = m_items.size();
    beginInsertRows({}, first, first + data.size() - 1);
    for (const auto &value : data)
        m_items.push_back(MediaItem::fromJson(value.toObject()));
    endInsertRows();
    Q_EMIT countChanged();
    setNextPath(next);
}

void MediaModel::replaceItems(QList<MediaItem> items)
{
    beginResetModel();
    m_items = std::move(items);
    endResetModel();
    setLoading(false);
    setError({});
    Q_EMIT countChanged();
}

void MediaModel::appendItems(const QList<MediaItem> &items)
{
    if (items.isEmpty())
        return;
    const int first = m_items.size();
    beginInsertRows({}, first, first + items.size() - 1);
    m_items.append(items);
    endInsertRows();
    Q_EMIT countChanged();
}

void MediaModel::clear()
{
    replaceItems({});
    setNextPath({});
}

const MediaItem *MediaModel::itemAt(int row) const
{
    return row >= 0 && row < m_items.size() ? &m_items.at(row) : nullptr;
}

void MediaModel::setFavorite(const QString &id, bool favorite)
{
    for (int row = 0; row < m_items.size(); ++row) {
        auto &item = m_items[row];
        if (item.id != id && item.catalogId != id)
            continue;
        if (item.favorite == favorite)
            continue;
        item.favorite = favorite;
        const auto modelIndex = index(row, 0);
        Q_EMIT dataChanged(modelIndex, modelIndex, {FavoriteRole});
    }
}

void MediaModel::setInLibrary(const QString &id, bool inLibrary)
{
    for (int row = 0; row < m_items.size(); ++row) {
        auto &item = m_items[row];
        if (item.id != id && item.catalogId != id)
            continue;
        if (item.inLibrary == inLibrary)
            continue;
        item.inLibrary = inLibrary;
        const auto modelIndex = index(row, 0);
        Q_EMIT dataChanged(modelIndex, modelIndex, {InLibraryRole});
    }
}

void MediaModel::setRating(const QString &id, int rating)
{
    setRatings({{id, rating}});
}

void MediaModel::setRatings(const QHash<QString, int> &byId)
{
    if (byId.isEmpty())
        return;
    // One pass over the rows for the whole reply. Applying a hundred ratings
    // one at a time meant a hundred passes over every model in the app.
    for (int row = 0; row < m_items.size(); ++row) {
        auto &item = m_items[row];
        auto found = item.id.isEmpty() ? byId.constEnd() : byId.constFind(item.id);
        if (found == byId.constEnd() && !item.catalogId.isEmpty())
            found = byId.constFind(item.catalogId);
        if (found == byId.constEnd() || item.rating == found.value())
            continue;
        item.rating = found.value();
        const auto modelIndex = index(row, 0);
        Q_EMIT dataChanged(modelIndex, modelIndex, {RatingRole});
    }
}

QVariantMap MediaModel::get(int row) const
{
    QVariantMap result;
    const auto index = createIndex(row, 0);
    const auto roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
        result.insert(QString::fromUtf8(it.value()), data(index, it.key()));
    }
    return result;
}

QStringList MediaModel::playbackIds() const
{
    QStringList ids;
    for (const auto &item : m_items) {
        if (item.isPlayable())
            ids.push_back(item.playbackId());
    }
    return ids;
}

int MediaModel::indexOfId(const QString &id) const
{
    for (int row = 0; row < m_items.size(); ++row) {
        if (m_items.at(row).id == id || m_items.at(row).catalogId == id)
            return row;
    }
    return -1;
}
