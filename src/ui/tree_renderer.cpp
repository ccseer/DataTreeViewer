#include "tree_renderer.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFont>
#include <QHeaderView>
#include <QMenu>
#include <QPainter>
#include <QShowEvent>

#include "core/config_node.h"
#include "tree_delegate.h"
#include "tree_filter_proxy.h"
#include "tree_model.h"

#define qprintt qDebug() << "[TreeRenderer]"

TreeRenderer::TreeRenderer(QWidget *parent) : QTreeView(parent)
{
    setUniformRowHeights(false);
    setWordWrap(true);
    setAnimated(true);
    setExpandsOnDoubleClick(false);
    setIconSize(QSize(18, 18));
    setFont(QFont("Segoe UI", 10));

    m_model = new TreeModel(this);
    m_proxy = new TreeFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    QTreeView::setModel(m_proxy);
    setItemDelegate(new TreeDelegate(this));

    setRootIsDecorated(true);
    header()->setStretchLastSection(true);
    header()->setMinimumSectionSize(100);

    connect(selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current, const QModelIndex &) {
                if(!current.isValid())
                    return;
                QModelIndex src = m_proxy->mapToSource(current);
                const ConfigNode *node = m_model->nodeFromIndex(src);
                if(node)
                    emit nodeActivated(node);
            });
}

void TreeRenderer::showEvent(QShowEvent *event)
{
    QTreeView::showEvent(event);
    int w = width();
    if(w > 0)
        header()->resizeSection(0, w / 3);
    if(indentation() <= 0)
        setIndentation(22);
}

void TreeRenderer::drawBranches(QPainter *painter, const QRect &rect,
                                const QModelIndex &index) const
{
    QTreeView::drawBranches(painter, rect, index);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);

    QColor lineColor = palette().color(QPalette::Text);
    lineColor.setAlpha(40); // Very subtle
    painter->setPen(QPen(lineColor, 1.0, Qt::DotLine));

    int indent = indentation();
    if(indent > 0) {
        int depth = 0;
        QModelIndex temp = index.parent();
        while(temp.isValid()) {
            depth++;
            temp = temp.parent();
        }

        for(int i = 0; i < depth; ++i) {
            int x = rect.left() + (i * indent) + indent / 2;
            painter->drawLine(x, rect.top(), x, rect.bottom());
        }
    }

    painter->restore();
}

void TreeRenderer::clear()
{
    m_truncatedCount = 0;
    m_model->clear();
}

void TreeRenderer::render(const ConfigNode &root)
{
    m_truncatedCount = 0;
    m_model->render(root);

    if(m_expandAll)
        expandAll();
    else
        expand(m_proxy->index(0, 0));

    header()->resizeSection(0, width() / 3);
}

void TreeRenderer::setExpandAll(bool on)
{
    m_expandAll = on;
}

void TreeRenderer::setDarkMode(bool on)
{
    m_model->setDarkMode(on);
}

void TreeRenderer::updateDPR(qreal r)
{
    setIndentation(qRound(22 * r));
    setIconSize(QSize(qRound(18 * r), qRound(18 * r)));
    scheduleDelayedItemsLayout();
}

void TreeRenderer::applyFilter(const QString &text)
{
    m_proxy->setSearchText(text);
}

void TreeRenderer::selectNextMatch()
{
    m_proxy->selectNextMatch(this);
}

void TreeRenderer::selectPreviousMatch()
{
    m_proxy->selectPreviousMatch(this);
}

int TreeRenderer::matchCount() const
{
    return m_proxy->matchCount();
}

const ConfigNode *TreeRenderer::currentNode() const
{
    QModelIndex current = currentIndex();
    if(!current.isValid())
        return nullptr;
    QModelIndex src = m_proxy->mapToSource(current);
    return m_model->nodeFromIndex(src);
}

void TreeRenderer::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    const ConfigNode *node = currentNode();

    if(node) {
        menu.addAction("Copy Key", this, &TreeRenderer::copyKey);
        menu.addAction("Copy Value", this, &TreeRenderer::copyValue);
        menu.addAction("Copy Key: Value", this, &TreeRenderer::copyKeyValuePair);
        menu.addSeparator();
    }

    QMenu *sortMenu = menu.addMenu("Sort");
    QAction *origAction = sortMenu->addAction("Original Order", this, [this] { m_proxy->sort(-1); });
    QAction *ascAction =
        sortMenu->addAction("A-Z (Ascending)", this, [this] { m_proxy->sort(0, Qt::AscendingOrder); });
    QAction *descAction =
        sortMenu->addAction("Z-A (Descending)", this, [this] { m_proxy->sort(0, Qt::DescendingOrder); });

    // Mark current sort state
    if(m_proxy->sortColumn() == -1)
        origAction->setCheckable(true), origAction->setChecked(true);
    else if(m_proxy->sortOrder() == Qt::AscendingOrder)
        ascAction->setCheckable(true), ascAction->setChecked(true);
    else
        descAction->setCheckable(true), descAction->setChecked(true);

    menu.addSeparator();
    menu.addAction("Expand All", this, &TreeRenderer::expandAll);
    menu.addAction("Collapse All", this, &TreeRenderer::collapseAll);

    menu.exec(event->globalPos());
}

void TreeRenderer::copyKey()
{
    QModelIndex idx = currentIndex();
    if(idx.isValid()) {
        QString text = idx.sibling(idx.row(), 0).data(Qt::DisplayRole).toString();
        QApplication::clipboard()->setText(text);
    }
}

void TreeRenderer::copyValue()
{
    QModelIndex idx = currentIndex();
    if(idx.isValid()) {
        QString text = idx.sibling(idx.row(), 1).data(Qt::DisplayRole).toString();
        QApplication::clipboard()->setText(text);
    }
}

void TreeRenderer::copyKeyValuePair()
{
    QModelIndex idx = currentIndex();
    if(idx.isValid()) {
        QString key = idx.sibling(idx.row(), 0).data(Qt::DisplayRole).toString();
        QString val = idx.sibling(idx.row(), 1).data(Qt::DisplayRole).toString();
        QApplication::clipboard()->setText(QString("%1: %2").arg(key, val));
    }
}
