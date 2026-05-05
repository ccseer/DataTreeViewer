#pragma once

#include <QAbstractItemModel>
#include <QIcon>
#include <unordered_map>

struct ConfigNode;

class TreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Roles {
        CommentRole = Qt::UserRole + 1
    };

    explicit TreeModel(QObject *parent = nullptr);

    void render(const ConfigNode &root);
    void clear();

    const ConfigNode *nodeFromIndex(const QModelIndex &index) const;

    // QAbstractItemModel overrides
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation, int role) const override;

    void setDarkMode(bool on);

private:
    const ConfigNode *m_root = nullptr;
    std::unordered_map<const ConfigNode *, const ConfigNode *> m_parents;
    bool m_isDarkMode = false;

    QIcon m_objIcon;
    QIcon m_arrIcon;
    QIcon m_emptyIcon;

    void refreshIcons();
    static QString containerHint(const ConfigNode &node);
};
