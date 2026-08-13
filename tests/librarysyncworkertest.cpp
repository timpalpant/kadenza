#include "database.h"
#include "librarycache.h"
#include "librarysyncworker.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

namespace {

const QLatin1String kAlbums("albums");

QJsonObject album(const QString &id, const QString &title)
{
    return {{QStringLiteral("id"), id},
            {QStringLiteral("type"), QStringLiteral("library-albums")},
            {QStringLiteral("attributes"), QJsonObject{{QStringLiteral("name"), title}, {QStringLiteral("artistName"), QStringLiteral("Artist")}}}};
}

QByteArray page(const QStringList &ids, const QString &next = {})
{
    QJsonArray data;
    for (const auto &id : ids)
        data.append(album(id, QStringLiteral("Album ") + id));
    QJsonObject root{{QStringLiteral("data"), data}};
    if (!next.isEmpty())
        root.insert(QStringLiteral("next"), next);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

} // namespace

class LibrarySyncWorkerTest : public QObject
{
    Q_OBJECT

signals:
    void ingestRequested(const QString &kind, qint64 epoch, int firstPosition, const QByteArray &payload);
    void finishRequested(const QString &kind, qint64 epoch);

private slots:
    void initTestCase();
    void writesPagesFromAWorkerThread();
    void reportsMalformedPages();
    void cleanupTestCase();

private:
    QTemporaryDir m_dir;
    QThread m_thread;
    LibrarySyncWorker *m_worker = nullptr;
    LibraryCache m_cache;
};

void LibrarySyncWorkerTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
    QVERIFY(Database::instance().open(m_dir.filePath(QStringLiteral("cache.sqlite"))));

    m_worker = new LibrarySyncWorker;
    m_worker->moveToThread(&m_thread);
    connect(this, &LibrarySyncWorkerTest::ingestRequested, m_worker, &LibrarySyncWorker::ingestPage);
    connect(this, &LibrarySyncWorkerTest::finishRequested, m_worker, &LibrarySyncWorker::finishSync);
    m_thread.start();
}

void LibrarySyncWorkerTest::cleanupTestCase()
{
    // The worker's SQLite handle belongs to its thread and has to be closed
    // there, exactly as AppController does it at shutdown.
    QMetaObject::invokeMethod(m_worker, &LibrarySyncWorker::releaseDatabase, Qt::BlockingQueuedConnection);
    m_thread.quit();
    QVERIFY(m_thread.wait());
    delete m_worker;
    Database::instance().close();
}

void LibrarySyncWorkerTest::writesPagesFromAWorkerThread()
{
    QSignalSpy ingested(m_worker, &LibrarySyncWorker::pageIngested);
    QSignalSpy finished(m_worker, &LibrarySyncWorker::syncFinished);

    const qint64 epoch = m_cache.beginSync(kAlbums);

    // Two pages, numbered continuously, written on the worker's own
    // connection while this thread holds one of its own.
    Q_EMIT ingestRequested(kAlbums, epoch, 0, page({QStringLiteral("1"), QStringLiteral("2")}, QStringLiteral("/next")));
    QVERIFY(ingested.wait());
    QCOMPARE(ingested.constLast().at(2).toInt(), 2);
    QCOMPARE(ingested.constLast().at(3).toString(), QStringLiteral("/next"));

    Q_EMIT ingestRequested(kAlbums, epoch, 2, page({QStringLiteral("3")}));
    QTRY_COMPARE(ingested.count(), 2);
    QCOMPARE(ingested.constLast().at(2).toInt(), 1);
    QVERIFY(ingested.constLast().at(3).toString().isEmpty());

    Q_EMIT finishRequested(kAlbums, epoch);
    QVERIFY(finished.wait());

    // Read back from the main thread's connection: the rows the worker wrote
    // are visible, in the order they were paged in.
    const auto items = m_cache.items(kAlbums, LibraryCache::LibraryOrder, 10);
    QCOMPARE(items.size(), 3);
    QCOMPARE(items.at(0).title, QStringLiteral("Album 1"));
    QCOMPARE(items.at(2).title, QStringLiteral("Album 3"));
    QVERIFY(!m_cache.isStale(kAlbums, 3600));
}

void LibrarySyncWorkerTest::reportsMalformedPages()
{
    QSignalSpy failed(m_worker, &LibrarySyncWorker::pageFailed);
    const qint64 epoch = m_cache.beginSync(kAlbums);

    Q_EMIT ingestRequested(kAlbums, epoch, 0, QByteArrayLiteral("{not json"));
    QVERIFY(failed.wait());
    QCOMPARE(failed.constLast().at(0).toString(), QString(kAlbums));

    // The epoch is reported back so a stale reply cannot end a newer sync.
    QCOMPARE(failed.constLast().at(1).toLongLong(), epoch);
}

QTEST_MAIN(LibrarySyncWorkerTest)
#include "librarysyncworkertest.moc"
