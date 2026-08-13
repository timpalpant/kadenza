#pragma once

#include <QString>
#include <QStringList>

namespace QueueIds {

/**
 * Filters a remembered queue down to ids that can be played back as tracks.
 *
 * A radio station is an endless stream rather than a list of songs, which is
 * why playStation() deliberately drops the saved queue. MusicKit then reports
 * its own queue back to us, and that queue carries the station's `ra.` id — so
 * the cleared queue was immediately overwritten with it and persisted. On the
 * next launch the id was replayed into setQueue{songs:[…]}, and Apple answered
 * "[mk-007] NOT_FOUND; One or more items could not be resolved".
 */
[[nodiscard]] bool isRestorable(const QString &id);
[[nodiscard]] QStringList restorable(const QStringList &ids);

} // namespace QueueIds
