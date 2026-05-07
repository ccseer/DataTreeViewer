#pragma once

#include <QModelIndex>
#include <QString>
#include <QVector>

struct NodePathSegment {
    QString key;
    int arrayIndex = -1;
    bool isArrayIndex = false;
};

struct NodePath {
    QVector<NodePathSegment> segments;
    QString dotPath;
    QString jsonPointer;
};

QString dotPathFromSegments(const QVector<NodePathSegment> &segments);
QString jsonPointerFromSegments(const QVector<NodePathSegment> &segments);
NodePath nodePathFromIndex(const QModelIndex &index);
