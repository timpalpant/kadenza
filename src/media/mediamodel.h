#pragma once

#include "mediaitem.h"

#include <QAbstractListModel>
#include <QJsonArray>
#include <qqmlintegration.h>

class MediaModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int playableCount READ playableCount NOTIFY countChanged)
    Q_PROPERTY(bool loading READ loading WRITE setLoading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error WRITE setError NOTIFY errorChanged)
    Q_PROPERTY(QString nextPath READ nextPath WRITE setNextPath NOTIFY nextPathChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        CatalogIdRole,
        TypeRole,
        TitleRole,
        SubtitleRole,
        AlbumRole,
        ArtworkRole,
        DurationRole,
        TrackNumberRole,
        ExplicitRole,
        PlayableRole,
        DateAddedRole,
        ArtistIdRole,
        AlbumIdRole,
        FavoriteRole,
        RatingRole,
        InLibraryRole,
    };
    Q_ENUM(Role)

    explicit MediaModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    int playableCount() const;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool loading() const;
    void setLoading(bool loading);
    QString error() const;
    void setError(const QString &error);
    QString nextPath() const;
    void setNextPath(const QString &path);

    void replace(const QJsonArray &data, const QString &next = {});
    void append(const QJsonArray &data, const QString &next = {});
    void replaceItems(QList<MediaItem> items);
    void appendItems(const QList<MediaItem> &items);
    void clear();
    [[nodiscard]] const MediaItem *itemAt(int row) const;
    void setFavorite(const QString &id, bool favorite);
    void setInLibrary(const QString &id, bool inLibrary);
    void setRating(const QString &id, int rating);
    /// Applies a whole ratings reply in a single pass over the rows.
    void setRatings(const QHash<QString, int> &byId);

    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE QStringList playbackIds() const;
    Q_INVOKABLE int indexOfId(const QString &id) const;

signals:
    void countChanged();
    void loadingChanged();
    void errorChanged();
    void nextPathChanged();

private:
    QList<MediaItem> m_items;
    bool m_loading = false;
    QString m_error;
    QString m_nextPath;
};
