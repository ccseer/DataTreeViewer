#include "tree_filter_proxy.h"
#include "core/config_node.h"
#include <QAbstractItemView>

TreeFilterProxyModel::TreeFilterProxyModel(QObject *parent) : QSortFilterProxyModel(parent)
{
    setRecursiveFilteringEnabled(true);
    setDynamicSortFilter(true);
}

void TreeFilterProxyModel::setSearchText(const QString &text)
{
    m_searchText = text;
    m_cacheDirty = true;
    m_currentMatch = -1;
    invalidateFilter();
}

bool TreeFilterProxyModel::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    if(m_searchText.isEmpty())
        return true;

    QAbstractItemModel *src = sourceModel();
    if(!src)
        return false;

    QModelIndex keyIdx = src->index(row, 0, parent);
    QModelIndex valIdx = src->index(row, 1, parent);

    if(src->data(keyIdx, Qt::DisplayRole).toString().contains(m_searchText, Qt::CaseInsensitive))
        return true;
    if(src->data(valIdx, Qt::DisplayRole).toString().contains(m_searchText, Qt::CaseInsensitive))
        return true;

    return hasAcceptedChild(row, parent);
}

bool TreeFilterProxyModel::hasAcceptedChild(int row, const QModelIndex &parent) const
{
    QAbstractItemModel *src = sourceModel();
    if(!src)
        return false;

    QModelIndex idx = src->index(row, 0, parent);
    int childCount = src->rowCount(idx);
    for(int i = 0; i < childCount; ++i) {
        if(filterAcceptsRow(i, idx))
            return true;
    }
    return false;
}

int TreeFilterProxyModel::matchCount() const
{
    rebuildMatchCache();
    return static_cast<int>(m_matchCache.size());
}

void TreeFilterProxyModel::selectNextMatch(QAbstractItemView *view)
{
    rebuildMatchCache();

    if(m_matchCache.isEmpty())
        return;
    m_currentMatch = (m_currentMatch + 1) % m_matchCache.size();

    QModelIndex proxyIdx = m_matchCache[m_currentMatch];
    view->scrollTo(proxyIdx, QAbstractItemView::PositionAtCenter);
    view->setCurrentIndex(proxyIdx);
}

void TreeFilterProxyModel::selectPreviousMatch(QAbstractItemView *view)
{
    rebuildMatchCache();

    if(m_matchCache.isEmpty())
        return;
    m_currentMatch =
        m_currentMatch <= 0 ? static_cast<int>(m_matchCache.size()) - 1 : m_currentMatch - 1;

    QModelIndex proxyIdx = m_matchCache[m_currentMatch];
    view->scrollTo(proxyIdx, QAbstractItemView::PositionAtCenter);
    view->setCurrentIndex(proxyIdx);
}

bool TreeFilterProxyModel::indexMatches(const QModelIndex &index) const
{
    if(m_searchText.isEmpty() || !index.isValid())
        return false;

    const QModelIndex keyIdx = index.sibling(index.row(), 0);
    const QModelIndex valIdx = index.sibling(index.row(), 1);
    return keyIdx.data(Qt::DisplayRole).toString().contains(m_searchText, Qt::CaseInsensitive) ||
           valIdx.data(Qt::DisplayRole).toString().contains(m_searchText, Qt::CaseInsensitive);
}

void TreeFilterProxyModel::collectMatches(const QModelIndex &parent) const
{
    int rows = rowCount(parent);
    for(int i = 0; i < rows; ++i) {
        QModelIndex idx = index(i, 0, parent);
        if(idx.isValid()) {
            if(indexMatches(idx))
                m_matchCache.append(idx);
            collectMatches(idx);
        }
    }
}

void TreeFilterProxyModel::rebuildMatchCache() const
{
    if(!m_cacheDirty)
        return;

    m_matchCache.clear();
    collectMatches(QModelIndex());
    m_cacheDirty = false;
}

bool TreeFilterProxyModel::lessThan(const QModelIndex &source_left,
                                   const QModelIndex &source_right) const
{
    const ConfigNode *leftNode =
        static_cast<const ConfigNode *>(source_left.internalPointer());
    const ConfigNode *rightNode =
        static_cast<const ConfigNode *>(source_right.internalPointer());

    if(!leftNode || !rightNode)
        return QSortFilterProxyModel::lessThan(source_left, source_right);

    // If we have a parent, check if it's an array. We don't sort array elements.
    QModelIndex parent = source_left.parent();
    if(parent.isValid()) {
        const ConfigNode *parentNode =
            static_cast<const ConfigNode *>(parent.internalPointer());
        if(parentNode && parentNode->type == ConfigNode::Type::Array) {
            // Arrays always stay in original order. We must compensate for DescendingOrder
            // because QSortFilterProxyModel will invert the result.
            if(sortOrder() == Qt::DescendingOrder)
                return source_left.row() > source_right.row();
            return source_left.row() < source_right.row();
        }
    }

    // Default: alphabetical sort by key for Objects
    QString leftKey = sourceModel()->data(source_left, Qt::DisplayRole).toString();
    QString rightKey = sourceModel()->data(source_right, Qt::DisplayRole).toString();

    return QString::compare(leftKey, rightKey, Qt::CaseInsensitive) < 0;
}
