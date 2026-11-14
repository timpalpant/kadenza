#include "spatialnavigation.h"

#include <limits>

namespace SpatialNavigation {

namespace {

bool liesBeyond(const QRectF &from, const QRectF &candidate, Direction direction)
{
    switch (direction) {
    case Direction::Left:
        return candidate.right() <= from.center().x() && candidate.left() < from.left();
    case Direction::Right:
        return candidate.left() >= from.center().x() && candidate.right() > from.right();
    case Direction::Up:
        return candidate.bottom() <= from.center().y() && candidate.top() < from.top();
    case Direction::Down:
        return candidate.top() >= from.center().y() && candidate.bottom() > from.bottom();
    }
    return false;
}

bool overlapsAcross(const QRectF &from, const QRectF &candidate, Direction direction)
{
    if (direction == Direction::Left || direction == Direction::Right)
        return candidate.top() < from.bottom() && candidate.bottom() > from.top();
    return candidate.left() < from.right() && candidate.right() > from.left();
}

// Gap between the leading edge of `from` and the near edge of the candidate.
// Zero when they touch or overlap, as adjacent grid cells do.
qreal majorDistance(const QRectF &from, const QRectF &candidate, Direction direction)
{
    switch (direction) {
    case Direction::Left:
        return qMax<qreal>(0, from.left() - candidate.right());
    case Direction::Right:
        return qMax<qreal>(0, candidate.left() - from.right());
    case Direction::Up:
        return qMax<qreal>(0, from.top() - candidate.bottom());
    case Direction::Down:
        return qMax<qreal>(0, candidate.top() - from.bottom());
    }
    return 0;
}

qreal minorDistance(const QRectF &from, const QRectF &candidate, Direction direction)
{
    if (direction == Direction::Left || direction == Direction::Right)
        return qAbs(candidate.center().y() - from.center().y());
    return qAbs(candidate.center().x() - from.center().x());
}

} // namespace

int nearest(const QRectF &from, const QList<QRectF> &candidates, Direction direction, bool requireOverlap)
{
    int best = -1;
    qreal bestScore = std::numeric_limits<qreal>::max();
    for (int i = 0; i < candidates.size(); ++i) {
        const QRectF &candidate = candidates.at(i);
        if (candidate.isEmpty() || !liesBeyond(from, candidate, direction))
            continue;
        if (requireOverlap && !overlapsAcross(from, candidate, direction))
            continue;
        const qreal major = majorDistance(from, candidate, direction);
        const qreal minor = minorDistance(from, candidate, direction);
        const qreal score = 13 * major * major + minor * minor;
        if (score < bestScore) {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

} // namespace SpatialNavigation
