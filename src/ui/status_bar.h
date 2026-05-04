#pragma once

#include <QWidget>

class BreadcrumbBar;
class QLabel;
class QProgressBar;

class StatusBar : public QWidget {
    Q_OBJECT
public:
    explicit StatusBar(QWidget *parent = nullptr);

    void setBreadcrumb(BreadcrumbBar *bar);
    void setLoadInfo(int nodeCount, qint64 fileBytes, qint64 elapsedMs, const QString &formatName,
                     const QString &libraryCredit);
    void setSourceLine(int line);
    void showLoading();
    void showFilterNoHits(const QString &text);
    void updateTheme(bool dark, qreal dpr);
    void clear();

private:
    void repaintInfoIcon();

    BreadcrumbBar *m_breadcrumb = nullptr;
    QLabel *m_info = nullptr;
    QLabel *m_lineLabel = nullptr;
    QProgressBar *m_progress = nullptr;

    bool m_isDarkMode = false;
    qreal m_dpr = 1.0;
    bool m_hasLoadInfo = false;

    // Stored for theme repaint
    QString m_tooltipLines;
};
