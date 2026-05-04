#pragma once

#include <QWidget>

class QLabel;
class QProgressBar;

class StatusBar : public QWidget {
    Q_OBJECT
public:
    explicit StatusBar(QWidget* parent = nullptr);

    void showStats(const QString& formatLabel,
                   int nodeCount,
                   qint64 fileBytes,
                   qint64 elapsedMs,
                   int truncatedCount = 0,
                   const QString& libraryCredit = {});
    void showError(const QString& message, int line = -1);
    void showLoading();
    void setSourceLine(int line);
    void showFilterNoHits(const QString& text);
    void clear();

private:
    QLabel*       m_formatLabel = nullptr;
    QLabel*       m_statsLabel  = nullptr;
    QLabel*       m_lineLabel   = nullptr;
    QLabel*       m_creditLabel = nullptr;
    QProgressBar* m_progress    = nullptr;
};
