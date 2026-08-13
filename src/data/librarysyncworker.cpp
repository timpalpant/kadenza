#include "librarysyncworker.h"

#include "database.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

void LibrarySyncWorker::ingestPage(const QString &kind, qint64 epoch, int firstPosition, const QByteArray &payload)
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        Q_EMIT pageFailed(kind, epoch, parseError.errorString());
        return;
    }

    const auto root = document.object();
    const auto data = root.value(QStringLiteral("data")).toArray();
    QList<MediaItem> items;
    items.reserve(data.size());
    for (const auto &value : data)
        items.push_back(MediaItem::fromJson(value.toObject()));

    m_cache.upsert(kind, items, epoch, firstPosition);
    Q_EMIT pageIngested(kind, epoch, static_cast<int>(items.size()), root.value(QStringLiteral("next")).toString());
}

void LibrarySyncWorker::finishSync(const QString &kind, qint64 epoch)
{
    m_cache.finishSync(kind, epoch);
    Q_EMIT syncFinished(kind, epoch);
}

void LibrarySyncWorker::releaseDatabase()
{
    Database::releaseThreadConnection();
}
