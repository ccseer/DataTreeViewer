#include "tree_renderer.h"

#include "core/config_node.h"

#include <QBrush>
#include <QFont>
#include <QHeaderView>

TreeRenderer::TreeRenderer(QWidget* parent)
    : QTreeWidget(parent)
{
    setColumnCount(2);
    setHeaderLabels({ "Key", "Value" });
    header()->setStretchLastSection(true);
    setAlternatingRowColors(false);
    setRootIsDecorated(true);
    setAnimated(true);
    setExpandsOnDoubleClick(false);

    setFont(QFont("Segoe UI", 10));

    connect(this, &QTreeWidget::itemClicked,
            this, [this](QTreeWidgetItem* item, int) {
                if (!item) return;
                auto* node = static_cast<const ConfigNode*>(
                    item->data(0, Qt::UserRole).value<void*>());
                if (node)
                    emit nodeActivated(node);
            });
}

void TreeRenderer::render(const ConfigNode& root)
{
    clear();
    m_matchItems.clear();
    m_currentMatch = -1;
    m_truncatedCount = 0;

    BuildState state;
    state.counted = countNodes(root);

    auto* rootItem = buildItem(root, 0, -1, state, nullptr);
    if (rootItem) {
        // Root item key is empty for top-level
        addTopLevelItem(rootItem);
        rootItem->setExpanded(true);
    }
}

void TreeRenderer::setAutoExpandDepth(int depth)
{
    m_autoExpandDepth = depth;
}

QTreeWidgetItem* TreeRenderer::buildItem(
    const ConfigNode& node, int depth, int arrayIndex,
    BuildState& state, QTreeWidgetItem* parent)
{
    if (state.inserted >= kMaxRenderNodes) {
        if (!state.truncated) {
            state.truncated = true;
            m_truncatedCount = state.counted - state.inserted;

            auto* sentinel = new QTreeWidgetItem();
            sentinel->setText(0, QString("... %1 more items (truncated)")
                                     .arg(m_truncatedCount));
            sentinel->setForeground(0, QBrush(QColor(128, 128, 128)));
            sentinel->setForeground(1, QBrush(QColor(128, 128, 128)));
            sentinel->setFlags(sentinel->flags() & ~Qt::ItemIsSelectable);
            if (parent)
                parent->addChild(sentinel);
        }
        return nullptr;
    }

    auto* item = new QTreeWidgetItem();
    state.inserted++;

    // Store ConfigNode pointer in UserRole
    item->setData(0, Qt::UserRole, QVariant::fromValue(
        static_cast<void*>(const_cast<ConfigNode*>(&node))));

    // Column 0: Key
    item->setText(0, displayKey(node, arrayIndex));

    // Column 1: Value / container hint
    if (node.is_container()) {
        item->setText(1, containerHint(node));
        item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    } else {
        item->setText(1, QString::fromStdString(node.scalar));
    }

    // Comment tooltip
    if (!node.comment.empty())
        item->setToolTip(0, QString::fromStdString(node.comment));

    // Style
    applyStyle(item, node);

    // Build children
    int childIdx = 0;
    for (const auto& child : node.children) {
        auto* childItem = buildItem(child, depth + 1,
            (node.type == ConfigNode::Type::Array) ? childIdx++ : -1,
            state, item);
        if (childItem)
            item->addChild(childItem);
    }

    // Auto-expand
    if (depth < m_autoExpandDepth)
        item->setExpanded(true);

    return item;
}

void TreeRenderer::applyStyle(QTreeWidgetItem* item, const ConfigNode& node)
{
    QBrush color;

    switch (node.type) {
    case ConfigNode::Type::String:
        color = QBrush(QColor("#6aab73"));
        break;
    case ConfigNode::Type::Integer:
    case ConfigNode::Type::Float:
        color = QBrush(QColor("#6897bb"));
        break;
    case ConfigNode::Type::Bool:
        color = QBrush(QColor("#cc7832"));
        break;
    case ConfigNode::Type::Null:
        color = QBrush(QColor("#808080"));
        break;
    case ConfigNode::Type::Object:
    case ConfigNode::Type::Array:
        color = QBrush(QColor(128, 128, 128));
        break;
    }

    item->setForeground(1, color);

    // Bold for container keys
    if (node.is_container()) {
        QFont f = item->font(0);
        f.setBold(true);
        item->setFont(0, f);
    }
}

QString TreeRenderer::containerHint(const ConfigNode& node) const
{
    int n = static_cast<int>(node.children.size());
    if (node.type == ConfigNode::Type::Array)
        return QString("[%1 items]").arg(n);
    return QString("{%1 keys}").arg(n);
}

QString TreeRenderer::displayKey(const ConfigNode& node, int index) const
{
    if (index >= 0)
        return QString("[%1]").arg(index);
    return QString::fromStdString(node.key);
}

int TreeRenderer::countNodes(const ConfigNode& root)
{
    int count = 0;
    std::vector<const ConfigNode*> stack = { &root };
    while (!stack.empty()) {
        const auto* n = stack.back();
        stack.pop_back();
        ++count;
        for (const auto& c : n->children)
            stack.push_back(&c);
    }
    return count;
}

// --- Filter / Search ---

void TreeRenderer::applyFilter(const QString& text)
{
    m_matchItems.clear();
    m_currentMatch = -1;

    if (text.isEmpty()) {
        // Show all items
        std::function<void(QTreeWidgetItem*)> showAll =
            [&](QTreeWidgetItem* item) {
                item->setHidden(false);
                for (int i = 0; i < item->childCount(); ++i)
                    showAll(item->child(i));
            };
        for (int i = 0; i < topLevelItemCount(); ++i)
            showAll(topLevelItem(i));
        return;
    }

    // Walk and hide non-matching items
    std::function<bool(QTreeWidgetItem*)> filterWalk =
        [&](QTreeWidgetItem* item) -> bool {
            bool anyVisible = false;
            bool isMatch = item->text(0).contains(text, Qt::CaseInsensitive) ||
                           item->text(1).contains(text, Qt::CaseInsensitive);

            // Check children
            for (int i = 0; i < item->childCount(); ++i) {
                if (filterWalk(item->child(i)))
                    anyVisible = true;
            }

            if (isMatch) {
                m_matchItems.append(item);
                anyVisible = true;
            }

            item->setHidden(!anyVisible);
            return anyVisible;
        };

    for (int i = 0; i < topLevelItemCount(); ++i)
        filterWalk(topLevelItem(i));

    // Select first match
    if (!m_matchItems.isEmpty()) {
        m_currentMatch = 0;
        scrollToItem(m_matchItems[0]);
        setCurrentItem(m_matchItems[0]);
    }
}

void TreeRenderer::selectNextMatch()
{
    if (m_matchItems.isEmpty()) return;
    m_currentMatch = (m_currentMatch + 1) % m_matchItems.size();
    scrollToItem(m_matchItems[m_currentMatch]);
    setCurrentItem(m_matchItems[m_currentMatch]);
}

void TreeRenderer::selectPreviousMatch()
{
    if (m_matchItems.isEmpty()) return;
    m_currentMatch = m_currentMatch <= 0
                         ? m_matchItems.size() - 1
                         : m_currentMatch - 1;
    scrollToItem(m_matchItems[m_currentMatch]);
    setCurrentItem(m_matchItems[m_currentMatch]);
}

int TreeRenderer::visibleMatchCount() const
{
    return m_matchItems.size();
}
