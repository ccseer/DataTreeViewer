#pragma once

#include <QAbstractItemModel>
#include <unordered_map>

struct ConfigNode;

class TreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit TreeModel(QObject *parent = nullptr);

    void render(const ConfigNode &root);

    const ConfigNode *nodeFromIndex(const QModelIndex &index) const;

    // QAbstractItemModel overrides
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation, int role) const override;

private:
    const ConfigNode *m_root = nullptr;
    std::unordered_map<const ConfigNode *, const ConfigNode *> m_parents;

    static QString containerHint(const ConfigNode &node);
};
