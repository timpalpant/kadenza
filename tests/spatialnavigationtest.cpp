#include "spatialnavigation.h"

#include <QTest>

using SpatialNavigation::Direction;
using SpatialNavigation::nearest;

class SpatialNavigationTest : public QObject
{
    Q_OBJECT

private slots:
    void movesWithinAGridRow();
    void movesToTheTileDirectlyBelow();
    void doesNotSkipAShortShelf();
    void stopsAtTheEdge();
    void ignoresAControlsOwnChildren();
    void ignoresASlightOverlap();
    void staysInTheRowWhenAsked();
};

namespace {
// A 4-column grid of 100×130 tiles starting at the origin.
QRectF tile(int row, int column)
{
    return QRectF(column * 100, row * 130, 100, 130);
}
} // namespace

void SpatialNavigationTest::movesWithinAGridRow()
{
    const QList<QRectF> grid{tile(0, 0), tile(0, 1), tile(0, 2), tile(1, 0), tile(1, 1), tile(1, 2)};
    QCOMPARE(nearest(tile(0, 1), grid, Direction::Right), 2);
    QCOMPARE(nearest(tile(0, 1), grid, Direction::Left), 0);
}

void SpatialNavigationTest::movesToTheTileDirectlyBelow()
{
    const QList<QRectF> grid{tile(0, 0), tile(0, 1), tile(0, 2), tile(1, 0), tile(1, 1), tile(1, 2)};
    QCOMPARE(nearest(tile(0, 2), grid, Direction::Down), 5);
    QCOMPARE(nearest(tile(1, 0), grid, Direction::Up), 0);
}

void SpatialNavigationTest::doesNotSkipAShortShelf()
{
    // Listen Now: a shelf with one tile, a heading, then a full shelf whose
    // third column lines up with the current tile.
    const QRectF current = tile(0, 2);
    const QRectF shortShelf(0, 170, 100, 130);
    const QRectF fullShelfAligned(200, 340, 100, 130);
    QCOMPARE(nearest(current, {fullShelfAligned, shortShelf}, Direction::Down), 1);
}

void SpatialNavigationTest::stopsAtTheEdge()
{
    const QList<QRectF> row{tile(0, 0), tile(0, 1)};
    QCOMPARE(nearest(tile(0, 1), row, Direction::Right), -1);
    QCOMPARE(nearest(tile(0, 0), row, Direction::Up), -1);
}

void SpatialNavigationTest::ignoresAControlsOwnChildren()
{
    // A track row with a menu button at its right end, and the next row below.
    const QRectF trackRow(0, 0, 800, 50);
    const QRectF menuButton(760, 10, 30, 30);
    const QRectF nextRow(0, 50, 800, 50);
    QCOMPARE(nearest(trackRow, {menuButton}, Direction::Right), -1);
    QCOMPARE(nearest(trackRow, {menuButton, nextRow}, Direction::Down), 1);
}

void SpatialNavigationTest::ignoresASlightOverlap()
{
    // The sidebar's footer button reaches a few pixels below the player
    // bar's buttons; it is beside them, not below them.
    const QRectF shuffle(329, 755, 34, 34);
    const QRectF sidebarFooter(0, 766, 180, 34);
    QCOMPARE(nearest(shuffle, {sidebarFooter}, Direction::Down), -1);
}

void SpatialNavigationTest::staysInTheRowWhenAsked()
{
    // The last tile of a one-tile shelf, with the player bar right and below.
    const QRectF lastTile(60, 380, 230, 290);
    const QRectF shuffle(329, 755, 34, 34);
    QCOMPARE(nearest(lastTile, {shuffle}, Direction::Right), 0);
    QCOMPARE(nearest(lastTile, {shuffle}, Direction::Right, true), -1);
    // A sidebar entry level with the tile still counts.
    const QRectF sidebarEntry(0, 498, 50, 36);
    QCOMPARE(nearest(lastTile, {sidebarEntry}, Direction::Left, true), 0);
}

QTEST_GUILESS_MAIN(SpatialNavigationTest)
#include "spatialnavigationtest.moc"
