#include "status_bar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>

StatusBar::StatusBar(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);

    m_formatLabel = new QLabel(this);
    m_formatLabel->setStyleSheet("font-weight: bold;");
    layout->addWidget(m_formatLabel);

    m_statsLabel = new QLabel(this);
    layout->addWidget(m_statsLabel);

    layout->addStretch();

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);
    m_progress->setMaximumWidth(120);
    m_progress->setMaximumHeight(16);
    m_progress->hide();
    layout->addWidget(m_progress);

    m_lineLabel = new QLabel(this);
    layout->addWidget(m_lineLabel);

    m_creditLabel = new QLabel(this);
    m_creditLabel->setStyleSheet("color: gray;");
    layout->addWidget(m_creditLabel);

    clear();
}

void StatusBar::showStats(const QString& formatLabel,
                          int nodeCount,
                          qint64 fileBytes,
                          qint64 elapsedMs,
                          int truncatedCount,
                          const QString& libraryCredit)
{
    m_progress->hide();

    m_formatLabel->setText(formatLabel);
    m_formatLabel->show();

    auto fileSizeStr = [](qint64 bytes) -> QString {
        if (bytes < 1024)
            return QString::number(bytes) + " B";
        if (bytes < 1024 * 1024)
            return QString::number(bytes / 1024.0, 'f', 1) + " KB";
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 2) + " MB";
    };

    QString stats = QString("%1 nodes · %2 · %3 ms")
                        .arg(nodeCount)
                        .arg(fileSizeStr(fileBytes))
                        .arg(elapsedMs);

    if (truncatedCount > 0) {
        stats += QString(" · showing %1 of %2 nodes (truncated)")
                     .arg(50000)
                     .arg(50000 + truncatedCount);
    }

    m_statsLabel->setText(stats);
    m_statsLabel->show();

    m_creditLabel->setText(libraryCredit);
    m_creditLabel->show();

    m_lineLabel->clear();
}

void StatusBar::showError(const QString& message, int line)
{
    m_progress->hide();
    m_formatLabel->hide();
    m_lineLabel->hide();
    m_creditLabel->hide();

    QString text = message;
    if (line >= 0)
        text = QString("Line %1: %2").arg(line).arg(message);

    m_statsLabel->setText(text);
    m_statsLabel->setStyleSheet("color: red;");
    m_statsLabel->show();
}

void StatusBar::showLoading()
{
    m_formatLabel->hide();
    m_statsLabel->setText("Loading…");
    m_statsLabel->setStyleSheet("");
    m_statsLabel->show();
    m_lineLabel->clear();
    m_creditLabel->clear();
    m_progress->show();
}

void StatusBar::setSourceLine(int line)
{
    if (line >= 0)
        m_lineLabel->setText(QString("Ln %1").arg(line));
    else
        m_lineLabel->clear();
}

void StatusBar::showFilterNoHits(const QString& text)
{
    m_statsLabel->setText(QString("No matches for \"%1\"").arg(text));
    m_statsLabel->setStyleSheet("color: #808080;");
}

void StatusBar::clear()
{
    m_formatLabel->clear();
    m_statsLabel->clear();
    m_statsLabel->setStyleSheet("");
    m_lineLabel->clear();
    m_creditLabel->clear();
    m_progress->hide();
}
