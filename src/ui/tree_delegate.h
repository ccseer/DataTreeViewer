#pragma once

#include <QStyledItemDelegate>

class TreeDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit TreeDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;
};
