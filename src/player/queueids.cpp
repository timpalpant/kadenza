#include "queueids.h"

namespace QueueIds {

bool isRestorable(const QString &id)
{
    // Catalog songs are numeric and library songs are prefixed "i."; a station
    // is the one identifier the player is already known to hand back in place
    // of a track list, so it is the one that gets dropped.
    return !id.isEmpty() && !id.startsWith(QLatin1String("ra."));
}

QStringList restorable(const QStringList &ids)
{
    QStringList out;
    out.reserve(ids.size());
    for (const auto &id : ids) {
        if (isRestorable(id))
            out.push_back(id);
    }
    return out;
}

} // namespace QueueIds
