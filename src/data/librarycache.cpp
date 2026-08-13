#include "librarycache.h"
#include "database.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

namespace {

const char *const SelectColumns = "id, catalog_id, artist_id, album_id, type, title, subtitle, album, "
                                  "artwork_url, url, date_added, duration_ms, track_number, "
                                  "explicit_content, streamable, favorite, in_library, rating";

MediaItem itemFromQuery(const QSqlQuery &query)
{
    MediaItem item;
    item.id = query.value(0).toString();
    item.catalogId = query.value(1).toString();
    item.artistId = query.value(2).toString();
    item.albumId = query.value(3).toString();
    item.type = query.value(4).toString();
    item.title = query.value(5).toString();
    item.subtitle = query.value(6).toString();
    item.album = query.value(7).toString();
    item.artworkUrl = query.value(8).toString();
    item.url = query.value(9).toString();
    item.dateAdded = query.value(10).toString();
    item.durationMs = query.value(11).toLongLong();
    item.trackNumber = query.value(12).toInt();
    item.explicitContent = query.value(13).toBool();
    item.streamable = query.value(14).toBool();
    item.favorite = query.value(15).toBool();
    item.inLibrary = query.value(16).toBool();
    item.rating = query.value(17).toInt();
    return item;
}

} // namespace

bool LibraryCache::isKnownKind(const QString &kind)
{
    return kind == QStringLiteral("albums") || kind == QStringLiteral("songs") || kind == QStringLiteral("artists")
           || kind == QStringLiteral("playlists") || kind == QStringLiteral("recently-added");
}

bool LibraryCache::available() const
{
    return Database::instance().isOpen();
}

QString LibraryCache::metaValue(const QString &key) const
{
    if (!available())
        return {};
    QSqlQuery query(Database::instance().db());
    query.prepare(QStringLiteral("SELECT value FROM meta WHERE key = ?"));
    query.addBindValue(key);
    if (query.exec() && query.next())
        return query.value(0).toString();
    return {};
}

void LibraryCache::setMetaValue(const QString &key, const QString &value)
{
    if (!available())
        return;
    QSqlQuery query(Database::instance().db());
    query.prepare(QStringLiteral("INSERT INTO meta (key, value) VALUES (?, ?) "
                                 "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    query.addBindValue(key);
    query.addBindValue(value);
    query.exec();
}

qint64 LibraryCache::beginSync(const QString &kind)
{
    const qint64 epoch = metaValue(QStringLiteral("epoch:") + kind).toLongLong() + 1;
    setMetaValue(QStringLiteral("epoch:") + kind, QString::number(epoch));
    return epoch;
}

void LibraryCache::upsert(const QString &kind, const QList<MediaItem> &items, qint64 epoch, int firstPosition)
{
    if (!available() || items.isEmpty())
        return;

    QSqlDatabase database = Database::instance().db();
    database.transaction();

    QSqlQuery query(database);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO library_items ("
                                 "kind, id, catalog_id, artist_id, album_id, type, title, subtitle, "
                                 "album, artwork_url, url, date_added, duration_ms, track_number, "
                                 "explicit_content, streamable, favorite, in_library, rating, position, "
                                 "sync_epoch) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                                 "?, ?, ?, ?, ?)"));

    int position = firstPosition;
    for (const auto &item : items) {
        if (item.id.isEmpty())
            continue;
        query.addBindValue(kind);
        query.addBindValue(item.id);
        query.addBindValue(item.catalogId);
        query.addBindValue(item.artistId);
        query.addBindValue(item.albumId);
        query.addBindValue(item.type);
        query.addBindValue(item.title);
        query.addBindValue(item.subtitle);
        query.addBindValue(item.album);
        query.addBindValue(item.artworkUrl);
        query.addBindValue(item.url);
        query.addBindValue(item.dateAdded);
        query.addBindValue(item.durationMs);
        query.addBindValue(item.trackNumber);
        query.addBindValue(item.explicitContent);
        query.addBindValue(item.streamable);
        query.addBindValue(item.favorite);
        query.addBindValue(item.inLibrary);
        query.addBindValue(item.rating);
        query.addBindValue(position++);
        query.addBindValue(epoch);
        if (!query.exec())
            qWarning() << "kadenza: cache upsert failed" << query.lastError().text();
    }

    database.commit();
}

void LibraryCache::finishSync(const QString &kind, qint64 epoch)
{
    if (!available())
        return;
    QSqlQuery query(Database::instance().db());
    query.prepare(QStringLiteral("DELETE FROM library_items WHERE kind = ? AND sync_epoch < ?"));
    query.addBindValue(kind);
    query.addBindValue(epoch);
    query.exec();
    setMetaValue(QStringLiteral("synced_at:") + kind, QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
}

QList<MediaItem> LibraryCache::items(const QString &kind, Sort sort, int limit, int offset) const
{
    QList<MediaItem> result;
    if (!available() || limit <= 0)
        return result;

    // Apple returns ISO-8601 UTC timestamps, so a lexicographic sort is also a
    // chronological one. Items without a date sort last rather than first.
    const QString order = sort == DateAddedDesc ? QStringLiteral("ORDER BY (date_added = '') ASC, date_added DESC, "
                                                                 "title COLLATE NOCASE ASC")
                                                : QStringLiteral("ORDER BY position ASC");

    QSqlQuery query(Database::instance().db());
    query.prepare(QStringLiteral("SELECT %1 FROM library_items WHERE kind = ? %2 "
                                 "LIMIT ? OFFSET ?")
                      .arg(QLatin1String(SelectColumns), order));
    query.addBindValue(kind);
    query.addBindValue(limit);
    query.addBindValue(offset);
    if (!query.exec()) {
        qWarning() << "kadenza: cache read failed" << query.lastError().text();
        return result;
    }
    while (query.next())
        result.push_back(itemFromQuery(query));
    return result;
}

int LibraryCache::count(const QString &kind) const
{
    if (!available())
        return 0;
    QSqlQuery query(Database::instance().db());
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM library_items WHERE kind = ?"));
    query.addBindValue(kind);
    if (query.exec() && query.next())
        return query.value(0).toInt();
    return 0;
}

QDateTime LibraryCache::lastSync(const QString &kind) const
{
    const QString value = metaValue(QStringLiteral("synced_at:") + kind);
    if (value.isEmpty())
        return {};
    return QDateTime::fromString(value, Qt::ISODate);
}

bool LibraryCache::isStale(const QString &kind, int maximumAgeSeconds) const
{
    const QDateTime synced = lastSync(kind);
    if (!synced.isValid())
        return true;
    return synced.secsTo(QDateTime::currentDateTimeUtc()) > maximumAgeSeconds;
}

void LibraryCache::setFavorite(const QString &id, bool favorite)
{
    if (!available() || id.isEmpty())
        return;
    QSqlQuery query(Database::instance().db());
    query.prepare(QStringLiteral("UPDATE library_items SET favorite = ? "
                                 "WHERE id = ? OR catalog_id = ?"));
    query.addBindValue(favorite);
    query.addBindValue(id);
    query.addBindValue(id);
    query.exec();
}

QString LibraryCache::libraryIdFor(const QString &kind, const QString &catalogId) const
{
    if (!available() || kind.isEmpty() || catalogId.isEmpty())
        return {};
    QSqlQuery query(Database::instance().db());
    query.prepare(QStringLiteral("SELECT id FROM library_items WHERE kind = ? AND in_library = 1 "
                                 "AND (catalog_id = ? OR id = ?) LIMIT 1"));
    query.addBindValue(kind);
    query.addBindValue(catalogId);
    query.addBindValue(catalogId);
    if (!query.exec() || !query.next())
        return {};
    return query.value(0).toString();
}

void LibraryCache::setInLibrary(const QString &id, bool inLibrary)
{
    if (!available() || id.isEmpty())
        return;
    QSqlQuery query(Database::instance().db());
    query.prepare(QStringLiteral("UPDATE library_items SET in_library = ? "
                                 "WHERE id = ? OR catalog_id = ?"));
    query.addBindValue(inLibrary);
    query.addBindValue(id);
    query.addBindValue(id);
    query.exec();
}

void LibraryCache::setRating(const QString &id, int rating)
{
    setRatings({{id, rating}});
}

void LibraryCache::setRatings(const QList<QPair<QString, int>> &ratings)
{
    if (!available() || ratings.isEmpty())
        return;

    QSqlDatabase database = Database::instance().db();
    database.transaction();

    QSqlQuery query(database);
    query.prepare(QStringLiteral("UPDATE library_items SET rating = ? "
                                 "WHERE id = ? OR catalog_id = ?"));
    for (const auto &[id, rating] : ratings) {
        if (id.isEmpty())
            continue;
        query.addBindValue(rating);
        query.addBindValue(id);
        query.addBindValue(id);
        if (!query.exec())
            qWarning() << "kadenza: cache rating update failed" << query.lastError().text();
    }

    database.commit();
}

void LibraryCache::clear()
{
    Database::instance().wipe();
}
