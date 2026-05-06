#include "tree_model.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QHeaderView>
#include <QPalette>
#include <QPixmap>

#include "core/config_node.h"
#include "style_assets.h"

namespace {

QString visibleScalar(const ConfigNode &node)
{
    QString text = QString::fromStdString(node.scalar);
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.remove('\r');

    while(true) {
        int newline = text.indexOf('\n');
        if(newline < 0 || !text.left(newline).trimmed().isEmpty())
            break;
        text.remove(0, newline + 1);
    }

    while(true) {
        int newline = text.lastIndexOf('\n');
        if(newline < 0 || !text.mid(newline + 1).trimmed().isEmpty())
            break;
        text.truncate(newline);
    }

    return text;
}

} // namespace

TreeModel::TreeModel(QObject *parent) : QAbstractItemModel(parent)
{
    refreshIcons();
}

void TreeModel::setDarkMode(bool on)
{
    if(m_isDarkMode == on)
        return;
    m_isDarkMode = on;
    refreshIcons();
    if(rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

void TreeModel::refreshIcons()
{
    using namespace dtv::ui;
    int sz = 18;

    QColor objColor = m_isDarkMode ? QColor(206, 147, 216) : QColor(156, 39, 176);
    QColor arrColor = m_isDarkMode ? QColor(128, 222, 234) : QColor(0, 188, 212);

    m_objIcon = createIcon(g_svg_object, objColor, sz);
    m_arrIcon = createIcon(g_svg_array, arrColor, sz);
    QPixmap empty(sz, sz);
    empty.fill(Qt::transparent);
    m_emptyIcon = QIcon(empty);
}

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

void TreeModel::clear()
{
    beginResetModel();
    m_root = nullptr;
    m_parents.clear();
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

    const ConfigNode *parentNode = parent.isValid() ? nodeFromIndex(parent) : m_root;
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
        return {};

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

    const ConfigNode *node = parent.isValid() ? nodeFromIndex(parent) : m_root;
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
        if(col == 0) {
            if(node->key.empty()) {
                auto it = m_parents.find(node);
                if(it != m_parents.end() && it->second->type == ConfigNode::Type::Array) {
                    for(size_t i = 0; i < it->second->children.size(); ++i) {
                        if(&it->second->children[i] == node)
                            return QString("[%1]").arg(i);
                    }
                }
            }
            return QString::fromStdString(node->key);
        }
        if(col == 1) {
            if(node->is_container())
                return containerHint(*node);
            return visibleScalar(*node);
        }
    }

    if(role == Qt::ToolTipRole) {
        if(!node->comment.empty())
            return QString::fromStdString(node->comment);
        if(col == 1 && !node->is_container())
            return visibleScalar(*node);
    }

    if(role == Qt::DecorationRole && col == 0) {
        if(node->type == ConfigNode::Type::Object)
            return m_objIcon;
        if(node->type == ConfigNode::Type::Array)
            return m_arrIcon;
        return m_emptyIcon;
    }

    if(role == Qt::ForegroundRole) {
        if(col == 0) {
            if(!node->key.empty() && node->key[0] == '@')
                return m_isDarkMode ? QColor("#CE93D8") : QColor("#6A1B9A");
            if(node->key == "#text")
                return m_isDarkMode ? QColor("#9E9E9E") : QColor("#757575");
        }
        if(col == 1) {
            if(node->is_container()) {
                QColor c = qApp->palette().color(QPalette::Text);
                c.setAlpha(140); // More dimmed for noise reduction
                return c;
            }
            switch(node->type) {
            case ConfigNode::Type::String:
                // Sage Green
                return m_isDarkMode ? QColor("#A5D6A7") : QColor("#2E7D32");
            case ConfigNode::Type::Integer:
            case ConfigNode::Type::Float:
                // Slate Blue
                return m_isDarkMode ? QColor("#90CAF9") : QColor("#1565C0");
            case ConfigNode::Type::Bool:
                // Warm Amber
                return m_isDarkMode ? QColor("#FFCC80") : QColor("#E65100");
            case ConfigNode::Type::Null:
                return m_isDarkMode ? QColor("#9E9E9E") : QColor("#757575");
            default:
                break;
            }
        }
    }

    if(role == Qt::FontRole && col == 0) {
        QFont f = qApp->font();
        if(node->is_container())
            f.setBold(true);
        return f;
    }

    if(role == Qt::SizeHintRole) {
        int lines = 1;
        if(col == 1 && !node->scalar.empty()) {
            const QString text = visibleScalar(*node);
            lines = text.count('\n') + 1;
            lines = std::min(lines, 10); // Cap at 10 lines for sanity
        }
        if(!node->comment.empty())
            ++lines;
        return QSize(0, 22 + lines * 18); // Base padding + line height
    }

    if(role == Qt::UserRole)
        return QVariant::fromValue(const_cast<ConfigNode *>(node));

    if(role == CommentRole && !node->comment.empty())
        return QString::fromStdString(node->comment);

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
