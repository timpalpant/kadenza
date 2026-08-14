#pragma once

#include "mediamodel.h"

#include <QSortFilterProxyModel>
#include <qqmlintegration.h>

// QSortFilterProxyModel is not exposed to QML, and the QML-native
// QtQml.Models.SortFilterProxyModel replacement only ships with Qt 6.10
// (kadenza requires 6.8), so this thin subclass carries the standard
// filterAcceptsRow behavior into QML. It matches rows whose title, subtitle or
// album contains the filter string, case-insensitively.
class MediaFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString filterString READ filterString WRITE setFilterString NOTIFY filterStringChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    QML_ELEMENT

public:
    explicit MediaFilterProxyModel(QObject *parent = nullptr);

    QString filterString() const;
    void setFilterString(const QString &filter);
    int count() const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

signals:
    void filterStringChanged();
    void countChanged();

private:
    QString m_filterString;
};