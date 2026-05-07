#include "node_path.h"

#include <algorithm>

#include "core/config_node.h"

namespace {

bool isPlainIdentifier(const QString &key)
{
    if(key.isEmpty())
        return false;

    const QChar first = key.front();
    if(!(first == '_' || first.isLetter()))
        return false;

    for(const QChar ch : key) {
        if(ch == '_' || ch.isLetterOrNumber())
            continue;
        return false;
    }

    return true;
}

QString quotedKey(QString key)
{
    key.replace('\\', QStringLiteral("\\\\"));
    key.replace('"', QStringLiteral("\\\""));
    return QStringLiteral("[\"%1\"]").arg(key);
}

QString pointerEscape(QString key)
{
    key.replace('~', QStringLiteral("~0"));
    key.replace('/', QStringLiteral("~1"));
    return key;
}

const ConfigNode *nodeFromIndex(const QModelIndex &index)
{
    if(!index.isValid())
        return nullptr;
    return static_cast<const ConfigNode *>(index.internalPointer());
}

} // namespace

QString dotPathFromSegments(const QVector<NodePathSegment> &segments)
{
    QString out;
    for(const NodePathSegment &segment : segments) {
        if(segment.isArrayIndex) {
            out += QStringLiteral("[%1]").arg(segment.arrayIndex);
            continue;
        }

        if(isPlainIdentifier(segment.key)) {
            if(!out.isEmpty())
                out += '.';
            out += segment.key;
        } else {
            out += quotedKey(segment.key);
        }
    }
    return out;
}

QString jsonPointerFromSegments(const QVector<NodePathSegment> &segments)
{
    QString out;
    for(const NodePathSegment &segment : segments) {
        out += '/';
        if(segment.isArrayIndex)
            out += QString::number(segment.arrayIndex);
        else
            out += pointerEscape(segment.key);
    }
    return out;
}

NodePath nodePathFromIndex(const QModelIndex &index)
{
    NodePath path;
    if(!index.isValid())
        return path;

    QVector<QModelIndex> chain;
    for(QModelIndex cur = index.sibling(index.row(), 0); cur.isValid();
        cur = cur.parent().sibling(cur.parent().row(), 0)) {
        chain.prepend(cur);
    }

    for(const QModelIndex &cur : chain) {
        const ConfigNode *node = nodeFromIndex(cur);
        if(!node)
            continue;

        QModelIndex parent = cur.parent();
        const ConfigNode *parentNode = nodeFromIndex(parent);
        if(parentNode && parentNode->type == ConfigNode::Type::Array) {
            path.segments.push_back(NodePathSegment{{}, cur.row(), true});
            continue;
        }

        if(!node->key.empty())
            path.segments.push_back(NodePathSegment{QString::fromStdString(node->key), -1, false});
    }

    path.dotPath = dotPathFromSegments(path.segments);
    path.jsonPointer = jsonPointerFromSegments(path.segments);
    return path;
}
