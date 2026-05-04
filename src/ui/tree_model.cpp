#include "tree_model.h"

#include <unordered_map>

#include <QColor>
#include <QFont>

#include "core/config_node.h"

TreeModel::TreeModel(QObject *parent) : QAbstractItemModel(parent)
{}

void TreeModel::render(const ConfigNode &root)
{
    beginResetModel();
    m_root = &root;
    m_parents.clear();

    std::vector<const ConfigNode *> stack = {m_root};
    while(!stack.empty()) {
        const ConfigNode *node = stack.back();
        stack.pop_back();
        for(const auto &child : node->children) {
            m_parents[&child] = node;
            stack.push_back(&child);
        }
    }

    endResetModel();
}

const ConfigNode *TreeModel::nodeFromIndex(const QModelIndex &index) const
{
    if(!index.isValid())
        return nullptr;
    return static_cast<const ConfigNode *>(index.internalPointer());
}

QModelIndex TreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if(!m_root || column < 0 || column > 1)
        return {};

    if(!parent.isValid()) {
        if(row == 0)
            return createIndex(0, column, const_cast<ConfigNode *>(m_root));
        return {};
    }

    const ConfigNode *parentNode = nodeFromIndex(parent);
    if(!parentNode || row < 0 || row >= static_cast<int>(parentNode->children.size()))
        return {};

    return createIndex(row, column, const_cast<ConfigNode *>(&parentNode->children[row]));
}

QModelIndex TreeModel::parent(const QModelIndex &index) const
{
    if(!index.isValid())
        return {};

    const ConfigNode *node = nodeFromIndex(index);
    if(!node || node == m_root)
        return {};

    auto it = m_parents.find(node);
    if(it == m_parents.end())
        return {};

    const ConfigNode *parentNode = it->second;

    if(parentNode == m_root)
        return createIndex(0, 0, const_cast<ConfigNode *>(m_root));

    // Find row of parentNode in ITS parent
    auto grandIt = m_parents.find(parentNode);
    if(grandIt == m_parents.end())
        return {};

    const ConfigNode *grand = grandIt->second;
    for(size_t i = 0; i < grand->children.size(); ++i) {
        if(&grand->children[i] == parentNode)
            return createIndex(static_cast<int>(i), 0, const_cast<ConfigNode *>(parentNode));
    }
    return {};
}

int TreeModel::rowCount(const QModelIndex &parent) const
{
    if(!m_root)
        return 0;
    if(!parent.isValid())
        return 1; // root node

    const ConfigNode *node = nodeFromIndex(parent);
    if(!node)
        return 0;
    return static_cast<int>(node->children.size());
}

int TreeModel::columnCount(const QModelIndex &) const
{
    return 2;
}

QVariant TreeModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return {};

    const ConfigNode *node = nodeFromIndex(index);
    if(!node)
        return {};

    int col = index.column();

    if(role == Qt::DisplayRole) {
        if(col == 0)
            return QString::fromStdString(node->key);
        if(col == 1) {
            if(node->is_container())
                return containerHint(*node);
            return QString::fromStdString(node->scalar);
        }
    }

    if(role == Qt::ToolTipRole && !node->comment.empty())
        return QString::fromStdString(node->comment);

    if(role == Qt::UserRole)
        return QVariant::fromValue(const_cast<ConfigNode *>(node));

    if(!node->comment.empty()) {
        if(role == Qt::UserRole + 1)
            return QString::fromStdString(node->comment);
        if(role == Qt::ForegroundRole && col == 0)
            return QColor("#2196F3");
        if(role == Qt::FontRole && col == 0) {
            QFont f;
            f.setItalic(true);
            return f;
        }
    }

    if(node->is_container() && role == Qt::FontRole && col == 0) {
        QFont f;
        f.setBold(true);
        return f;
    }

    return {};
}

QVariant TreeModel::headerData(int section, Qt::Orientation, int role) const
{
    if(role != Qt::DisplayRole)
        return {};
    return section == 0 ? "Key" : "Value";
}

QString TreeModel::containerHint(const ConfigNode &node)
{
    int n = static_cast<int>(node.children.size());
    if(node.type == ConfigNode::Type::Array)
        return QString("[%1 items]").arg(n);
    return QString("{%1 keys}").arg(n);
}
