#include "queueids.h"

#include <QTest>

class QueueIdsTest : public QObject
{
    Q_OBJECT

private slots:
    void keepsCatalogAndLibrarySongs();
    void dropsRadioStations();
    void filtersAWholeQueue();
};

void QueueIdsTest::keepsCatalogAndLibrarySongs()
{
    QVERIFY(QueueIds::isRestorable(QStringLiteral("1440857781"))); // catalog song
    QVERIFY(QueueIds::isRestorable(QStringLiteral("i.abc123")));   // library song
}

void QueueIdsTest::dropsRadioStations()
{
    // The exact id that produced "[mk-007] NOT_FOUND; One or more items could
    // not be resolved" on every launch after a station had been played.
    QVERIFY(!QueueIds::isRestorable(QStringLiteral("ra.978194965")));
    QVERIFY(!QueueIds::isRestorable(QString()));
}

void QueueIdsTest::filtersAWholeQueue()
{
    const QStringList mixed{QStringLiteral("1440857781"), QStringLiteral("ra.978194965"), QStringLiteral("i.abc123")};
    QCOMPARE(QueueIds::restorable(mixed), (QStringList{QStringLiteral("1440857781"), QStringLiteral("i.abc123")}));

    // A station-only queue filters to nothing, which is what lets the restore
    // path clear it rather than replay it.
    QVERIFY(QueueIds::restorable({QStringLiteral("ra.978194965")}).isEmpty());
}

QTEST_GUILESS_MAIN(QueueIdsTest)
#include "queueidstest.moc"
