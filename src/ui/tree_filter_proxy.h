#pragma once

#include <QSortFilterProxyModel>

class QAbstractItemView;

class TreeFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit TreeFilterProxyModel(QObject *parent = nullptr);

    void setSearchText(const QString &text);
    int matchCount() const;
    void selectNextMatch(QAbstractItemView *view);
    void selectPreviousMatch(QAbstractItemView *view);

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;

private:
    bool hasAcceptedChild(int row, const QModelIndex &parent) const;
    bool indexMatches(const QModelIndex &index) const;
    void collectMatches(const QModelIndex &parent) const;
    void rebuildMatchCache() const;

    QString m_searchText;
    mutable int m_currentMatch = -1;

    mutable QList<QPersistentModelIndex> m_matchCache;
    mutable bool m_cacheDirty = true;
};
