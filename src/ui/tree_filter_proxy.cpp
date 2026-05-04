#include "tree_filter_proxy.h"

#include <QAbstractItemView>

TreeFilterProxyModel::TreeFilterProxyModel(QObject *parent) : QSortFilterProxyModel(parent)
{
    setRecursiveFilteringEnabled(true);
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
    return static_cast<int>(m_matchCache.size());
}

void TreeFilterProxyModel::selectNextMatch(QAbstractItemView *view)
{
    if(m_cacheDirty) {
        m_matchCache.clear();
        collectMatches(QModelIndex());
        m_cacheDirty = false;
    }

    if(m_matchCache.isEmpty())
        return;
    m_currentMatch = (m_currentMatch + 1) % m_matchCache.size();

    QModelIndex proxyIdx = m_matchCache[m_currentMatch];
    view->scrollTo(proxyIdx, QAbstractItemView::PositionAtCenter);
    view->setCurrentIndex(proxyIdx);
}

void TreeFilterProxyModel::selectPreviousMatch(QAbstractItemView *view)
{
    if(m_cacheDirty) {
        m_matchCache.clear();
        collectMatches(QModelIndex());
        m_cacheDirty = false;
    }

    if(m_matchCache.isEmpty())
        return;
    m_currentMatch =
        m_currentMatch <= 0 ? static_cast<int>(m_matchCache.size()) - 1 : m_currentMatch - 1;

    QModelIndex proxyIdx = m_matchCache[m_currentMatch];
    view->scrollTo(proxyIdx, QAbstractItemView::PositionAtCenter);
    view->setCurrentIndex(proxyIdx);
}

void TreeFilterProxyModel::collectMatches(const QModelIndex &parent)
{
    int rows = rowCount(parent);
    for(int i = 0; i < rows; ++i) {
        QModelIndex idx = index(i, 0, parent);
        if(idx.isValid()) {
            m_matchCache.append(idx);
            collectMatches(idx);
        }
    }
}
