#pragma once

#include <QWidget>

class QToolButton;

class BreadcrumbBar : public QWidget {
    Q_OBJECT
public:
    explicit BreadcrumbBar(QWidget* parent = nullptr);

    void setPath(const QStringList& segments);
    void clear();

signals:
    void segmentClicked(int pathIndex);

private:
    void rebuild(const QStringList& segments);

    QStringList m_segments;
};
