#include "mediafilterproxymodel.h"

MediaFilterProxyModel::MediaFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    // invalidateFilter() (filter_changed()) only re-evaluates and emits
    // rowsRemoved/rowsInserted for parents whose internal mapping already
    // exists, so relay those structural changes to the count property.
    connect(this, &QAbstractItemModel::modelReset, this, &MediaFilterProxyModel::countChanged);
    connect(this, &QAbstractItemModel::rowsInserted, this, &MediaFilterProxyModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &MediaFilterProxyModel::countChanged);
}

QString MediaFilterProxyModel::filterString() const
{
    return m_filterString;
}

void MediaFilterProxyModel::setFilterString(const QString &filter)
{
    if (m_filterString == filter)
        return;
    if (!sourceModel())
        return;

    // The proxy builds its mapping lazily on first access, and re-filtering
    // (invalidateFilter()/endFilterChange()) is a no-op for unmapped parents.
    // beginFilterChange() (Qt 6.10+) builds the mapping with the old filter;
    // on older Qt rowCount() does the same. Re-filtering then diffs the old
    // mapping against the new filter and emits rowsRemoved/rowsInserted
    // synchronously, updating bound views and the count property.
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
#else
    rowCount(QModelIndex());
#endif

    m_filterString = filter;
    Q_EMIT filterStringChanged();

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    invalidateFilter();
#endif
}

int MediaFilterProxyModel::count() const
{
    return rowCount();
}

bool MediaFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_filterString.isEmpty())
        return true;
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!index.isValid())
        return false;
    const QString needle = m_filterString.trimmed().toCaseFolded();
    // A filter of only whitespace matches everything.
    if (needle.isEmpty())
        return true;
    for (int role : {MediaModel::TitleRole, MediaModel::SubtitleRole, MediaModel::AlbumRole}) {
        if (index.data(role).toString().toCaseFolded().contains(needle))
            return true;
    }
    return false;
}