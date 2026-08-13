#include "artworkcache.h"

#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QStandardPaths>

namespace {
// Artwork is requested at 512px and lands at roughly 150 KiB apiece, so a
// library of a few thousand albums plus its artists runs close to a gigabyte.
// Sized to hold one rather than to a round number: at 256 MiB a large library
// never fit, so every insert evicted an entry that was about to be wanted
// again and the grid spent its time re-fetching artwork it had already had.
constexpr qint64 kDefaultMaximumSize = 1024LL * 1024 * 1024;
} // namespace

ArtworkCachingFactory::ArtworkCachingFactory()
    : ArtworkCachingFactory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/artwork"), kDefaultMaximumSize)
{}

ArtworkCachingFactory::ArtworkCachingFactory(QString cacheDirectory, qint64 maximumSize)
    : m_cacheDirectory(std::move(cacheDirectory))
    , m_maximumSize(maximumSize)
{}

QNetworkAccessManager *ArtworkCachingFactory::create(QObject *parent)
{
    auto *manager = new QNetworkAccessManager(parent);
    auto *cache = new QNetworkDiskCache(manager);
    // A directory per manager. Qt calls this for every thread that loads
    // images — the QML engine's own manager and QQuickPixmapReader's at least —
    // and a QNetworkDiskCache assumes it is the only writer in its directory.
    // Pointing them all at one directory let them overwrite each other's index
    // and hand back truncated entries, which the decoders then reported as
    // "Unable to read image data" on pictures that had downloaded perfectly.
    // The reader's manager does effectively all the work, so the extra
    // directories stay near-empty.
    cache->setCacheDirectory(m_cacheDirectory + QLatin1Char('/') + QString::number(m_nextManager++));
    cache->setMaximumCacheSize(m_maximumSize);
    manager->setCache(cache);
    return manager;
}
