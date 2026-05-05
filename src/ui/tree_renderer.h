#pragma once

#include <QTreeView>

struct ConfigNode;

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

    void applyFilter(const QString &text);
    void selectNextMatch();
    void selectPreviousMatch();
    int matchCount() const;
    int truncatedCount() const
    {
        return m_truncatedCount;
    }

    const ConfigNode *currentNode() const;

signals:
    void nodeActivated(const ConfigNode *node);

protected:
    void showEvent(QShowEvent *event) override;
    void drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const override;

private:
    TreeModel *m_model = nullptr;
    TreeFilterProxyModel *m_proxy = nullptr;
    int m_truncatedCount = 0;
    bool m_expandAll = false;
};
