#include "status_bar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>

#include "breadcrumb_bar.h"
#include "style_assets.h"

#define qprintt qDebug() << "[StatusBar]"

namespace {

QString fileSizeStr(qint64 bytes)
{
    if(bytes < 1024)
        return QString::number(bytes) + " B";
    if(bytes < 1024 * 1024)
        return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    return QString::number(bytes / (1024.0 * 1024.0), 'f', 2) + " MB";
}

} // namespace

StatusBar::StatusBar(QWidget *parent) : QWidget(parent)
{
    setObjectName("btmBar");

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 2, 12, 2);
    layout->setSpacing(4);

    // Breadcrumb placeholder — setBreadcrumb will add the actual widget
    m_lineLabel = new QLabel(this);
    m_lineLabel->setStyleSheet("color: gray;");
    layout->addWidget(m_lineLabel, 0);

    m_valueLabel = new QLabel(this);
    m_valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_valueLabel, 1);

    m_info = new QLabel(this);
    m_info->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_info->setCursor(Qt::ArrowCursor);
    m_info->setToolTip("DataTreeViewer");
    layout->addWidget(m_info, 0);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);
    m_progress->setMaximumWidth(80);
    m_progress->setMaximumHeight(12);
    m_progress->hide();
    layout->addWidget(m_progress);

    clear();
}

void StatusBar::setBreadcrumb(BreadcrumbBar *bar)
{
    m_breadcrumb = bar;
    m_breadcrumb->setParent(this);
    auto *lay = qobject_cast<QHBoxLayout *>(layout());
    if(!lay)
        return;
    if(m_lineLabel)
        m_lineLabel->hide();
    lay->insertWidget(0, m_breadcrumb, 0);
}

void StatusBar::setLoadInfo(int nodeCount, qint64 fileBytes, qint64 elapsedMs,
                            const QString &formatName, const QString &libraryCredit,
                            int errorCount, const QString &warning)
{
    QStringList lines;
    lines << QString("Format: %1").arg(formatName);
    lines << QString("Nodes: %1").arg(nodeCount);
    lines << QString("File size: %1").arg(fileSizeStr(fileBytes));
    lines << QString("Load time: %1 ms").arg(elapsedMs);
    if(errorCount > 0)
        lines << QString("Parse errors: %1").arg(errorCount);
    if(!warning.isEmpty())
        lines << QString("Notice: %1").arg(warning);
    if(!libraryCredit.isEmpty())
        lines << QString("Library: %1").arg(libraryCredit);

    m_tooltipLines = lines.join("\n");
    m_hasLoadInfo = true;

    m_info->setToolTip(m_tooltipLines);
    repaintInfoIcon();
    m_progress->hide();
}

void StatusBar::setSourceLine(int line)
{
    if(line >= 0)
        m_lineLabel->setText(QString("Ln %1").arg(line));
    else
        m_lineLabel->clear();
}

void StatusBar::setValueText(const QString &text)
{
    if(text.isEmpty()) {
        m_valueLabel->clear();
        m_valueLabel->setToolTip({});
        return;
    }

    m_valueLabel->setText(text);
    m_valueLabel->setToolTip(text);
}

void StatusBar::showLoading()
{
    m_hasLoadInfo = false;
    m_info->setPixmap(QPixmap());
    m_info->setText("Loading...");
    m_info->setToolTip("DataTreeViewer");
    setValueText({});
    m_progress->show();
}

void StatusBar::showFilterNoHits(const QString &text)
{
    m_info->setPixmap(QPixmap());
    m_info->setText(QString("No matches for \"%1\"").arg(text));
    m_info->setToolTip(QString("No matches for \"%1\"").arg(text));
}

void StatusBar::restoreInfo()
{
    m_info->setText({});
    if(m_hasLoadInfo)
        repaintInfoIcon();
    else {
        m_info->setPixmap(QPixmap());
        m_info->setToolTip("DataTreeViewer");
    }
    m_progress->hide();
}

void StatusBar::updateTheme(bool dark, qreal dpr)
{
    m_isDarkMode = dark;
    m_dpr = dpr;
    setFixedHeight(qRound(26 * m_dpr));
    const int infoBox = qRound(24 * m_dpr);
    m_info->setFixedSize(infoBox, infoBox);
    if(auto *lay = qobject_cast<QHBoxLayout *>(layout()))
        lay->setContentsMargins(qRound(12 * m_dpr), 0, qRound(12 * m_dpr), 0);
    if(m_hasLoadInfo)
        repaintInfoIcon();
}

void StatusBar::clear()
{
    m_hasLoadInfo = false;
    m_info->setPixmap(QPixmap());
    m_info->clear();
    m_info->setToolTip("DataTreeViewer");
    m_lineLabel->clear();
    setValueText({});
    m_progress->hide();
}

void StatusBar::repaintInfoIcon()
{
    if(!m_hasLoadInfo)
        return;

    using namespace dtv::ui;

    QColor iconColor(Colors::Accent);

    int iconSize = qRound(20 * m_dpr);
    QIcon icon = dtv::ui::createMultiStateIcon(g_svg_info, iconColor, iconSize);
    m_info->setPixmap(icon.pixmap(iconSize, iconSize));
    m_info->setToolTip(m_tooltipLines);
}
