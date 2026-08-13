#pragma once

#include "librarycache.h"

#include <QByteArray>
#include <QObject>
#include <QString>

/**
 * Decodes library pages and writes them to the cache, off the GUI thread.
 *
 * A full album walk is dozens of pages, each a few hundred kilobytes of JSON
 * that has to be decoded, turned into MediaItems and written to SQLite. Doing
 * that between paint frames is what made switching to a large library page
 * stop responding, so the whole ingest step lives here and only the paging
 * decisions travel back to AppController.
 *
 * The worker owns nothing but its LibraryCache; the SQLite handle it uses is
 * the one Database hands out per thread.
 */
class LibrarySyncWorker : public QObject
{
    Q_OBJECT

public slots:
    /// Decodes one page and writes its rows under `epoch`, numbering them from
    /// `firstPosition`. Answers with pageIngested() so the caller can decide
    /// whether to ask Apple for another.
    void ingestPage(const QString &kind, qint64 epoch, int firstPosition, const QByteArray &payload);

    /// Drops rows left behind at an older epoch and stamps the sync time.
    void finishSync(const QString &kind, qint64 epoch);

    /// Releases this thread's SQLite handle. Called before the thread exits.
    void releaseDatabase();

signals:
    void pageIngested(const QString &kind, qint64 epoch, int itemCount, const QString &next);
    void pageFailed(const QString &kind, qint64 epoch, const QString &message);
    void syncFinished(const QString &kind, qint64 epoch);

private:
    LibraryCache m_cache;
};
