#pragma once

#include <QTreeView>

struct ConfigNode;
#include "node_path.h"

class TreeModel;
class TreeFilterProxyModel;

class TreeRenderer : public QTreeView {
    Q_OBJECT
public:
    explicit TreeRenderer(QWidget *parent = nullptr);

    void render(const ConfigNode &root);
    void clear();
    void setExpandAll(bool on);
    void setDarkMode(bool on);
    void updateDPR(qreal r);

    void applyFilter(const QString &text);
    void selectNextMatch();
    void selectPreviousMatch();
    int matchCount() const;
    int truncatedCount() const
    {
        return m_truncatedCount;
    }

    const ConfigNode *currentNode() const;
    NodePath currentNodePath() const;

signals:
    void nodeActivated(const ConfigNode *node);

protected:
    void showEvent(QShowEvent *event) override;
    void drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void copyKey();
    void copyValue();
    void copyKeyValuePair();
    void copyDotPath();
    void copyJsonPointer();

private:
    TreeModel *m_model = nullptr;
    TreeFilterProxyModel *m_proxy = nullptr;
    int m_truncatedCount = 0;
    bool m_expandAll = false;
};
