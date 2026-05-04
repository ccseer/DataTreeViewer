#pragma once

#include <QTreeWidget>

struct ConfigNode;

class TreeRenderer : public QTreeWidget {
    Q_OBJECT
public:
    explicit TreeRenderer(QWidget* parent = nullptr);

    void render(const ConfigNode& root);
    void setAutoExpandDepth(int depth);

    void applyFilter(const QString& text);
    void selectNextMatch();
    void selectPreviousMatch();
    int  visibleMatchCount() const;

    int truncatedCount() const { return m_truncatedCount; }

signals:
    void nodeActivated(const ConfigNode* node);

private:
    struct BuildState {
        int inserted = 0;
        int counted  = 0;
        bool truncated = false;
    };

    QTreeWidgetItem* buildItem(const ConfigNode& node, int depth,
                               int arrayIndex, BuildState& state,
                               QTreeWidgetItem* parent);

    void       applyStyle(QTreeWidgetItem* item, const ConfigNode& node);
    QString    containerHint(const ConfigNode& node) const;
    QString    displayKey(const ConfigNode& node, int index) const;

    static int countNodes(const ConfigNode& root);

    static constexpr int kMaxRenderNodes = 50000;

    int m_autoExpandDepth = 2;
    int m_truncatedCount  = 0;

    QList<QTreeWidgetItem*> m_matchItems;
    int                     m_currentMatch = -1;
};
