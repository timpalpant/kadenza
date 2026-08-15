#pragma once

#include "mediaitem.h"

#include <QDateTime>
#include <QList>
#include <QString>

/**
 * Reads and writes the cached copy of the user's library.
 *
 * A sync is epoch-based: beginSync() reserves a new epoch, every upserted row
 * is stamped with it, and finishSync() removes rows left behind at an older
 * epoch. That deletes items dropped from the library without holding the whole
 * id set in memory, and an interrupted sync simply leaves the previous
 * contents in place.
 */
class LibraryCache
{
public:
    enum Sort {
        LibraryOrder,  ///< Alphabetical by title, as Apple orders library collections.
        DateAddedDesc, ///< Newest first, for Recently Added.
    };

    /// Library collections that are mirrored. These match the Apple Music
    /// library endpoint names.
    static bool isKnownKind(const QString &kind);

    [[nodiscard]] bool available() const;

    qint64 beginSync(const QString &kind);
    void upsert(const QString &kind, const QList<MediaItem> &items, qint64 epoch, int firstPosition);
    void finishSync(const QString &kind, qint64 epoch);

    [[nodiscard]] QList<MediaItem> items(const QString &kind, Sort sort, int limit, int offset = 0) const;
    [[nodiscard]] int count(const QString &kind) const;
    /// The library id of a cached row for `catalogId`, or an empty string when
    /// that catalog resource is not in the library. Apple offers no way to ask
    /// whether a catalog resource is in the library, so the mirrored library is
    /// what answers it — and it returns the library id, which is what removing
    /// the item requires.
    [[nodiscard]] QString libraryIdFor(const QString &kind, const QString &catalogId) const;
    [[nodiscard]] QDateTime lastSync(const QString &kind) const;
    [[nodiscard]] bool isStale(const QString &kind, int maximumAgeSeconds) const;

    /// Mirrors a favourite or library toggle so it survives a restart without
    /// waiting for the next sync.
    void setFavorite(const QString &id, bool favorite);
    void setInLibrary(const QString &id, bool inLibrary);
    /// Inserts a single cached row for a resource just added to the library,
    /// under a fresh epoch so a later real walk replaces it cleanly. The
    /// LibraryOrder view sorts by title, so the row lands in alphabetical
    /// place regardless of the stored position.
    void insertRow(const QString &kind, const MediaItem &item);
    /// Removes the row(s) for `kind` matching `id` by library or catalog id.
    void removeRow(const QString &kind, const QString &id);
    void setRating(const QString &id, int rating);
    /// Writes a whole ratings reply in one transaction. Apple returns up to a
    /// hundred ratings at a time, and a commit per row means a commit — and an
    /// fsync — per row.
    void setRatings(const QList<QPair<QString, int>> &ratings);

    void clear();

private:
    [[nodiscard]] QString metaValue(const QString &key) const;
    void setMetaValue(const QString &key, const QString &value);
};
