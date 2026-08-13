#pragma once

#include <QQmlNetworkAccessManagerFactory>

#include <atomic>

/**
 * Gives QML's image loading a disk cache.
 *
 * Artwork comes from Apple's CDN and never changes once published, but Qt's
 * pixmap cache only lives in memory and is bounded: leaving a library page and
 * coming back re-fetched and re-decoded every tile, which on a library of
 * several thousand albums is most of what made a page expensive to open.
 *
 * Installed on the QML engine, so it covers artwork and nothing else; the API
 * client keeps its own manager, and authenticated JSON has no business in a
 * shared HTTP cache.
 */
class ArtworkCachingFactory : public QQmlNetworkAccessManagerFactory
{
public:
    /// Defaults to a subdirectory of the application cache location.
    ArtworkCachingFactory();
    /// Overridable so tests can point it somewhere disposable.
    explicit ArtworkCachingFactory(QString cacheDirectory, qint64 maximumSize);

    QNetworkAccessManager *create(QObject *parent) override;

    [[nodiscard]] QString cacheDirectory() const { return m_cacheDirectory; }

private:
    QString m_cacheDirectory;
    qint64 m_maximumSize;
    /// Numbers the per-manager cache directories. create() runs on whichever
    /// thread wants the manager, so this is touched off the GUI thread.
    std::atomic<int> m_nextManager{0};
};
