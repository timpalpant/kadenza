#pragma once

#include <QList>
#include <QRectF>

namespace SpatialNavigation {

enum class Direction { Left, Right, Up, Down };

/**
 * Picks the control a D-pad press should move to, the way TV platforms do:
 * the nearest candidate lying in that direction, with distance along the
 * direction of travel weighed far more heavily than sideways drift.
 *
 * A candidate qualifies only if it starts past the middle of the current
 * rect. That keeps focus out of a control's own children (a row's buttons sit
 * inside the row) and away from neighbours that merely overlap it by a few
 * pixels, such as the sidebar's footer beside the player bar.
 *
 * With `requireOverlap`, the candidate must also share some of the current
 * rect's span across the direction of travel: Right at the end of a shelf
 * stops there instead of dropping to whatever lies right and far below.
 *
 * Scoring follows Android's FocusFinder (13·major² + minor²). With equal
 * weights, a short shelf below the current one lost to a full shelf two rows
 * further down that happened to line up, and Down skipped a row.
 *
 * Returns the index of the best candidate, or -1 when nothing lies that way.
 */
[[nodiscard]] int nearest(const QRectF &from, const QList<QRectF> &candidates, Direction direction, bool requireOverlap = false);

} // namespace SpatialNavigation
