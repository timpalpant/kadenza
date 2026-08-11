#include "database.h"
#include "librarycache.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

MediaItem makeItem(const QString &id, const QString &title,
                   const QString &dateAdded, const QString &catalogId = {}) {
  MediaItem item;
  item.id = id;
  item.catalogId = catalogId;
  item.type = QStringLiteral("library-albums");
  item.title = title;
  item.subtitle = QStringLiteral("Artist");
  item.dateAdded = dateAdded;
  item.streamable = true;
  return item;
}

QStringList titlesOf(const QList<MediaItem> &items) {
  QStringList titles;
  for (const auto &item : items)
    titles.push_back(item.title);
  return titles;
}

const QLatin1String kAlbums("albums");

} // namespace

class LibraryCacheTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void storesAndReadsInLibraryOrder();
  void sortsRecentlyAddedNewestFirst();
  void undatedItemsSortLast();
  void pagesWithLimitAndOffset();
  void finishedSyncPrunesRemovedItems();
  void interruptedSyncKeepsPreviousContents();
  void updatesFavoriteByCatalogId();
  void storesAndUpdatesRatings();
  void reportsStalenessBeforeFirstSync();
  void cleanupTestCase();

private:
  QTemporaryDir m_dir;
  LibraryCache m_cache;
};

void LibraryCacheTest::initTestCase() {
  QVERIFY(m_dir.isValid());
  QVERIFY(Database::instance().open(m_dir.filePath("test.sqlite")));
}

void LibraryCacheTest::init() { m_cache.clear(); }

void LibraryCacheTest::cleanupTestCase() { Database::instance().close(); }

void LibraryCacheTest::storesAndReadsInLibraryOrder() {
  const qint64 epoch = m_cache.beginSync(kAlbums);
  m_cache.upsert(kAlbums,
                 {makeItem("1", "First", "2026-01-01T00:00:00Z"),
                  makeItem("2", "Second", "2026-02-01T00:00:00Z")},
                 epoch, 0);
  m_cache.finishSync(kAlbums, epoch);

  QCOMPARE(m_cache.count(kAlbums), 2);
  const auto items = m_cache.items(kAlbums, LibraryCache::LibraryOrder, 10);
  QCOMPARE(titlesOf(items), QStringList({"First", "Second"}));
  QCOMPARE(items.first().type, QStringLiteral("library-albums"));
  QVERIFY(items.first().streamable);
}

void LibraryCacheTest::sortsRecentlyAddedNewestFirst() {
  const qint64 epoch = m_cache.beginSync(kAlbums);
  // Deliberately inserted oldest-first, in the order Apple paginates.
  m_cache.upsert(kAlbums,
                 {makeItem("1", "Oldest", "2024-01-01T00:00:00Z"),
                  makeItem("2", "Newest", "2026-06-01T00:00:00Z"),
                  makeItem("3", "Middle", "2025-03-01T00:00:00Z")},
                 epoch, 0);
  m_cache.finishSync(kAlbums, epoch);

  const auto items = m_cache.items(kAlbums, LibraryCache::DateAddedDesc, 10);
  QCOMPARE(titlesOf(items), QStringList({"Newest", "Middle", "Oldest"}));
}

void LibraryCacheTest::undatedItemsSortLast() {
  const qint64 epoch = m_cache.beginSync(kAlbums);
  m_cache.upsert(kAlbums,
                 {makeItem("1", "Undated", QString()),
                  makeItem("2", "Dated", "2025-01-01T00:00:00Z")},
                 epoch, 0);
  m_cache.finishSync(kAlbums, epoch);

  const auto items = m_cache.items(kAlbums, LibraryCache::DateAddedDesc, 10);
  QCOMPARE(titlesOf(items), QStringList({"Dated", "Undated"}));
}

void LibraryCacheTest::pagesWithLimitAndOffset() {
  const qint64 epoch = m_cache.beginSync(kAlbums);
  QList<MediaItem> items;
  for (int i = 0; i < 10; ++i) {
    items.push_back(makeItem(QString::number(i),
                             QStringLiteral("Album %1").arg(i),
                             QStringLiteral("2026-01-01T00:00:00Z")));
  }
  m_cache.upsert(kAlbums, items, epoch, 0);
  m_cache.finishSync(kAlbums, epoch);

  const auto first = m_cache.items(kAlbums, LibraryCache::LibraryOrder, 4, 0);
  const auto second = m_cache.items(kAlbums, LibraryCache::LibraryOrder, 4, 4);
  QCOMPARE(titlesOf(first),
           QStringList({"Album 0", "Album 1", "Album 2", "Album 3"}));
  QCOMPARE(titlesOf(second),
           QStringList({"Album 4", "Album 5", "Album 6", "Album 7"}));
}

void LibraryCacheTest::finishedSyncPrunesRemovedItems() {
  const qint64 first = m_cache.beginSync(kAlbums);
  m_cache.upsert(kAlbums,
                 {makeItem("1", "Kept", "2026-01-01T00:00:00Z"),
                  makeItem("2", "Removed", "2026-01-02T00:00:00Z")},
                 first, 0);
  m_cache.finishSync(kAlbums, first);
  QCOMPARE(m_cache.count(kAlbums), 2);

  // A later sync no longer reports "Removed", so it should disappear.
  const qint64 second = m_cache.beginSync(kAlbums);
  m_cache.upsert(kAlbums, {makeItem("1", "Kept", "2026-01-01T00:00:00Z")},
                 second, 0);
  m_cache.finishSync(kAlbums, second);

  QCOMPARE(m_cache.count(kAlbums), 1);
  QCOMPARE(titlesOf(m_cache.items(kAlbums, LibraryCache::LibraryOrder, 10)),
           QStringList({"Kept"}));
}

void LibraryCacheTest::interruptedSyncKeepsPreviousContents() {
  const qint64 first = m_cache.beginSync(kAlbums);
  m_cache.upsert(kAlbums,
                 {makeItem("1", "One", "2026-01-01T00:00:00Z"),
                  makeItem("2", "Two", "2026-01-02T00:00:00Z"),
                  makeItem("3", "Three", "2026-01-03T00:00:00Z")},
                 first, 0);
  m_cache.finishSync(kAlbums, first);

  // A sync that dies after one page must not prune the rest of the library.
  const qint64 second = m_cache.beginSync(kAlbums);
  m_cache.upsert(kAlbums, {makeItem("1", "One", "2026-01-01T00:00:00Z")},
                 second, 0);
  // No finishSync() — the walk was abandoned.

  QCOMPARE(m_cache.count(kAlbums), 3);
}

void LibraryCacheTest::updatesFavoriteByCatalogId() {
  const qint64 epoch = m_cache.beginSync(kAlbums);
  m_cache.upsert(
      kAlbums, {makeItem("l.1", "Album", "2026-01-01T00:00:00Z", "cat-1")},
      epoch, 0);
  m_cache.finishSync(kAlbums, epoch);

  QVERIFY(!m_cache.items(kAlbums, LibraryCache::LibraryOrder, 1).first().favorite);
  // The UI knows catalog ids; the cache row is keyed by the library id.
  m_cache.setFavorite(QStringLiteral("cat-1"), true);
  QVERIFY(m_cache.items(kAlbums, LibraryCache::LibraryOrder, 1).first().favorite);
}

void LibraryCacheTest::storesAndUpdatesRatings() {
  const qint64 epoch = m_cache.beginSync(kAlbums);
  auto loved = makeItem("1", "Loved", "2026-01-01T00:00:00Z", "cat-1");
  loved.rating = 1;
  m_cache.upsert(kAlbums, {loved, makeItem("2", "Unrated", "2026-01-02T00:00:00Z")},
                 epoch, 0);
  m_cache.finishSync(kAlbums, epoch);

  auto items = m_cache.items(kAlbums, LibraryCache::LibraryOrder, 10);
  QCOMPARE(items.at(0).rating, 1);
  QCOMPARE(items.at(1).rating, 0);

  // The UI knows catalog ids; the row is keyed by the library id.
  m_cache.setRating(QStringLiteral("cat-1"), -1);
  items = m_cache.items(kAlbums, LibraryCache::LibraryOrder, 10);
  QCOMPARE(items.at(0).rating, -1);
}

void LibraryCacheTest::reportsStalenessBeforeFirstSync() {
  QVERIFY(m_cache.isStale(kAlbums, 3600));
  const qint64 epoch = m_cache.beginSync(kAlbums);
  m_cache.upsert(kAlbums, {makeItem("1", "One", "2026-01-01T00:00:00Z")}, epoch,
                 0);
  m_cache.finishSync(kAlbums, epoch);
  QVERIFY(!m_cache.isStale(kAlbums, 3600));
}

QTEST_GUILESS_MAIN(LibraryCacheTest)
#include "librarycachetest.moc"
