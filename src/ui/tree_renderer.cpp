#include "tree_renderer.h"

#include <QFont>
#include <QHeaderView>
#include <QShowEvent>

#include "core/config_node.h"
#include "tree_filter_proxy.h"
#include "tree_model.h"

#define qprintt qDebug() << "[TreeRenderer]"

TreeRenderer::TreeRenderer(QWidget *parent) : QTreeView(parent)
{
    setUniformRowHeights(true);
    setAnimated(true);
    setExpandsOnDoubleClick(false);
    setIconSize(QSize(18, 18));
    setFont(QFont("Segoe UI", 10));

    m_model = new TreeModel(this);
    m_proxy = new TreeFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    QTreeView::setModel(m_proxy);

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
}

void TreeRenderer::clear()
{
    m_truncatedCount = 0;
    m_model->render(ConfigNode{});
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
    // Dark mode is handled via palette by the parent DataTreeViewer
    Q_UNUSED(on);
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
