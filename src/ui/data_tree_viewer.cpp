#include "data_tree_viewer.h"

#include <QApplication>
#include <QFileInfo>
#include <QLayout>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QThread>

#include "breadcrumb_bar.h"
#include "core/iformat_parser.h"
#include "core/parser_helpers.h"
#include "core/parser_registry.h"
#include "search_bar.h"
#include "status_bar.h"
#include "style_assets.h"
#include "node_path.h"
#include "tree_renderer.h"
#include "workers/background_thread.h"
#include "workers/parse_worker.h"

#define qprintt qDebug() << "[DataTreeViewer]"

namespace {

QString visibleScalar(const ConfigNode &node)
{
    QString text = QString::fromStdString(node.scalar);
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.remove('\r');

    while(true) {
        int newline = text.indexOf('\n');
        if(newline < 0 || !text.left(newline).trimmed().isEmpty())
            break;
        text.remove(0, newline + 1);
    }

    while(true) {
        int newline = text.lastIndexOf('\n');
        if(newline < 0 || !text.mid(newline + 1).trimmed().isEmpty())
            break;
        text.truncate(newline);
    }

    return text;
}

QString statusValueForNode(const ConfigNode &node)
{
    QString value;
    if(node.is_container()) {
        const int childCount = static_cast<int>(node.children.size());
        if(node.type == ConfigNode::Type::Array)
            value = QString("[%1 items]").arg(childCount);
        else
            value = QString("{%1 keys}").arg(childCount);
    } else {
        value = visibleScalar(node);
        value.replace('\n', QStringLiteral("\\n"));
    }

    constexpr int kMaxStatusChars = 80;
    if(value.size() > kMaxStatusChars)
        value = value.left(kMaxStatusChars) + "...";

    return value;
}

} // namespace

DataTreeViewer::DataTreeViewer(QWidget *parent) : ViewerBase(parent)
{
    qprintt << this;
}

DataTreeViewer::~DataTreeViewer()
{
    qprintt << "~" << this;
    cancelPending();
}

void DataTreeViewer::init()
{
    m_breadcrumb = new BreadcrumbBar(nullptr); // reparented by setBreadcrumb
    m_renderer = new TreeRenderer(this);
    m_search = new SearchBar(this);
    m_status = new StatusBar(this);
    m_status->setBreadcrumb(m_breadcrumb);

    connect(m_renderer, &TreeRenderer::nodeActivated, this, &DataTreeViewer::onNodeActivated);
    connect(m_breadcrumb, &BreadcrumbBar::segmentClicked, this,
            &DataTreeViewer::onBreadcrumbClicked);
    connect(m_search, &SearchBar::filterChanged, this, &DataTreeViewer::onFilterChanged);
    connect(m_search, &SearchBar::nextMatch, m_renderer, &TreeRenderer::selectNextMatch);
    connect(m_search, &SearchBar::prevMatch, m_renderer, &TreeRenderer::selectPreviousMatch);

    qRegisterMetaType<std::shared_ptr<const ParseResult>>();
    ParserRegistry::registerBuiltinParsers();
}

void DataTreeViewer::loadImpl(QBoxLayout *lay_content, QHBoxLayout *lay_ctrlbar)
{
    init();

    // Layout: search → tree → status bar (breadcrumb inside)
    lay_content->addWidget(m_search);
    lay_content->addWidget(m_renderer);
    lay_content->addWidget(m_status);

    // Control bar: text view button (same as JsonTreeViewer)
    if(lay_ctrlbar) {
        lay_ctrlbar->addStretch();
        if(!m_btn_text_view) {
            m_btn_text_view = new QPushButton(this);
            m_btn_text_view->setObjectName("textViewBtn");
            m_btn_text_view->setFlat(true);
            m_btn_text_view->setToolTip("Text View");
            m_btn_text_view->setFocusPolicy(Qt::NoFocus);
            connect(m_btn_text_view, &QPushButton::clicked, this,
                    &DataTreeViewer::onTextViewBtnClicked);
        }
        lay_ctrlbar->addWidget(m_btn_text_view);
    }

    updateDPR(options()->dpr());
    updateTheme(options()->theme());

    QString path = options()->path();
    if(!path.isEmpty())
        doLoadFile(path);
}

void DataTreeViewer::doLoadFile(const QString &path)
{
    qprintt << "load" << path;
    cancelPending();

    auto parser =
        ParserRegistry::instance().parserFor(QFileInfo(path).suffix().toLower().toStdString());
    if(!parser) {
        showError("Unsupported format", -1);
        emit sigCommand(VCT_StateChange, VCV_Error);
        return;
    }

    showLoadingState();

    m_generation++;
    auto gen = m_generation;

    auto *thread = new BackgroundThread;
    auto *worker = new ParseWorker(path, std::move(parser));

    QPointer<ParseWorker> workerPtr = worker;

    connect(this, &QObject::destroyed, this, [thread, workerPtr] {
        if(workerPtr)
            thread->requestInterruption();
    });
    connect(this, &DataTreeViewer::cancelRequested, this, [thread, workerPtr] {
        if(workerPtr)
            thread->requestInterruption();
    });

    connect(worker, &QObject::destroyed, thread, &QObject::deleteLater);
    connect(thread, &QThread::started, worker, &ParseWorker::doParse);
    connect(worker, &ParseWorker::parseCompleted, this,
            [this, gen](std::shared_ptr<const ParseResult> result, qint64 elapsedMs) {
                if(gen != m_generation)
                    return;
                onParseCompleted(result, elapsedMs);
            });

    worker->moveToThread(thread);
    thread->start();
}

void DataTreeViewer::cancel()
{
    cancelPending();
}

void DataTreeViewer::cancelPending()
{
    emit cancelRequested();
    m_generation++;
}

void DataTreeViewer::showLoadingState()
{
    m_status->showLoading();
}

void DataTreeViewer::onParseCompleted(std::shared_ptr<const ParseResult> result, qint64 elapsedMs)
{
    qprintt << "parseCompleted"
            << "ok" << result->ok
            << "parseError" << result->has_parse_error
            << "elapsed" << elapsedMs
            << "errorLine" << result->err_line
            << "errorBytes" << result->error.size()
            << "rootChildren" << result->root.children.size();

    if(!result->ok) {
        const std::string title = result->format_name.empty() ? "ERROR" : "PARSE ERROR";
        showError(result->error, result->err_line, title);
        emit sigCommand(VCT_StateChange, VCV_Error);
        return;
    }

    m_lastResult = std::move(result);
    m_renderer->setExpandAll(m_lastResult->file_bytes < 5 * 1024 * 1024);
    m_renderer->render(m_lastResult->root);
    if(m_lastResult->has_parse_error)
        m_renderer->expandAll();
    m_breadcrumb->clear();

    const QString formatName = QString::fromStdString(m_lastResult->format_name);
    m_status->setLoadInfo(m_lastResult->total_nodes, m_lastResult->file_bytes, elapsedMs, formatName,
                          QString::fromStdString(m_lastResult->library_credit),
                          m_lastResult->error_count,
                          QString::fromStdString(m_lastResult->warning));

    if(m_lastResult->has_parse_error)
        m_status->setValueText(QString("%1 Parse Error: %2")
                                   .arg(formatName, QString::fromStdString(m_lastResult->error)));

    emit sigCommand(VCT_StateChange, VCV_Loaded);
    qprintt << "loaded" << m_lastResult->total_nodes << "nodes" << elapsedMs << "ms";
}

void DataTreeViewer::onNodeActivated(const ConfigNode *node)
{
    QModelIndex current = m_renderer->currentIndex();
    NodePath nodePath = m_renderer->currentNodePath();

    if(node && (!m_lastResult || !m_lastResult->has_parse_error)) {
        const QString value = statusValueForNode(*node);
        if(nodePath.dotPath.isEmpty()) {
            m_status->setValueText(value);
        } else {
            const QString text = QString("%1  |  %2").arg(nodePath.dotPath, value);
            const QString tooltip =
                QString("Path: %1\nJSON Pointer: %2\nValue: %3")
                    .arg(nodePath.dotPath, nodePath.jsonPointer, value);
            m_status->setValueText(text, tooltip);
        }
    } else {
        m_status->setValueText({});
    }

    if(node && node->source_line >= 0)
        m_status->setSourceLine(node->source_line);
    else
        m_status->setSourceLine(-1);

    if(!current.isValid()) {
        m_breadcrumb->clear();
        return;
    }

    QStringList segments;
    for(QModelIndex cur = current; cur.isValid(); cur = cur.parent()) {
        QModelIndex keyIndex = cur.sibling(cur.row(), 0);
        QString key = keyIndex.data(Qt::DisplayRole).toString();
        if(!cur.parent().isValid() && key.isEmpty())
            key = "root";
        segments.prepend(key);
    }
    m_breadcrumb->setPath(segments);
}

void DataTreeViewer::onBreadcrumbClicked(int pathIndex)
{
    QModelIndex current = m_renderer->currentIndex();
    if(!current.isValid())
        return;

    QList<QModelIndex> path;
    for(QModelIndex cur = current; cur.isValid(); cur = cur.parent())
        path.prepend(cur);

    if(pathIndex < path.size()) {
        QModelIndex target = path[pathIndex];
        m_renderer->setCurrentIndex(target);
        m_renderer->scrollTo(target);
    }
}

void DataTreeViewer::onFilterChanged(const QString &text)
{
    m_renderer->applyFilter(text);
    if(!text.isEmpty() && m_renderer->matchCount() == 0)
        m_status->showFilterNoHits(text);
    else
        m_status->restoreInfo();
}

void DataTreeViewer::onTextViewBtnClicked()
{
    emit sigCommand(VCT_LoadViewerWithNewType, QString("Text"));
}

void DataTreeViewer::showError(const std::string &message, int errLine, const std::string &title)
{
    auto errorResult = std::make_shared<ParseResult>();
    errorResult->ok = false;
    errorResult->error = message;
    errorResult->err_line = errLine;
    errorResult->root = dtv::core::createErrorNode(message, errLine, title);
    m_lastResult = std::move(errorResult);

    m_renderer->setExpandAll(true);
    m_renderer->render(m_lastResult->root);
    m_renderer->expandAll();
    m_breadcrumb->clear();
    m_status->restoreInfo();

    QString text = QString::fromStdString(message);
    if(errLine >= 0)
        text = QString("Error line %1: %2").arg(errLine).arg(text);
    m_status->setValueText(text);

    qprintt << "ERROR" << text;
}

void DataTreeViewer::updateDPR(qreal r)
{
    m_dpr = r;
    reapplyStyles();
}

void DataTreeViewer::updateTheme(int theme)
{
    m_isDarkMode = (theme == 1);
    reapplyStyles();
}

void DataTreeViewer::reapplyStyles()
{
    using namespace dtv::ui;
    qreal r = m_dpr;
    auto font = qApp->font();
    font.setPixelSize(qRound(12 * r));
    setFont(font);
    if(layout())
        layout()->setSpacing(qRound(6 * r));

    QPalette p = palette();
    p.setColor(QPalette::Window, QColor(m_isDarkMode ? Colors::DarkBG : Colors::LightBG));
    p.setColor(QPalette::Base, QColor(m_isDarkMode ? Colors::DarkBG : Colors::LightBG));
    p.setColor(QPalette::WindowText, QColor(m_isDarkMode ? Colors::DarkText : Colors::LightText));
    p.setColor(QPalette::Text, QColor(m_isDarkMode ? Colors::DarkText : Colors::LightText));
    p.setColor(QPalette::PlaceholderText,
               QColor(m_isDarkMode ? Colors::DarkTextDim : Colors::LightTextDim));
    setPalette(p);
    if(m_renderer) {
        m_renderer->setPalette(p);
        m_renderer->setDarkMode(m_isDarkMode);
        m_renderer->updateDPR(r);
    }

    if(m_search) {
        m_search->setFont(font);
        m_search->setFixedHeight(qRound(30 * r));
        m_search->updateDPR(r);
        m_search->updateTheme(m_isDarkMode);
        const QString borderColor = m_isDarkMode ? Colors::DarkBorder : Colors::LightBorder;
        const QString inputColor = m_isDarkMode ? Colors::DarkInput : Colors::LightInput;
        const QString textColor = m_isDarkMode ? Colors::DarkText : Colors::LightText;
        m_search->setStyleSheet(QString(g_qss_top_bar)
                                    .arg("transparent")
                                    .arg(borderColor)
                                    .arg(inputColor)
                                    .arg(textColor)
                                    .arg(Colors::Accent)
                                    .arg(qRound(4 * r))
                                    .arg(qRound(4 * r))
                                    .arg(qRound(2 * r)));
    }

    if(!m_status)
        return;

    m_status->updateTheme(m_isDarkMode, r);

    // Apply QSS to bottom bar
    QString surfaceColor = m_isDarkMode ? Colors::DarkSurface : Colors::LightSurface;
    QString borderColor = m_isDarkMode ? Colors::DarkBorder : Colors::LightBorder;
    m_status->setStyleSheet(QString(g_qss_bottom_bar).arg(surfaceColor, borderColor));

    if(m_btn_text_view) {
        constexpr int ctrlbar_btn_sz = 30;
        constexpr int ctrlbar_btn_icon_sz = 24;
        QColor iconColor(m_isDarkMode ? Colors::DarkText : Colors::LightText);
        m_btn_text_view->setFixedSize(qRound(ctrlbar_btn_sz * r), qRound(ctrlbar_btn_sz * r));
        m_btn_text_view->setIconSize(
            QSize(qRound(ctrlbar_btn_icon_sz * r), qRound(ctrlbar_btn_icon_sz * r)));
        m_btn_text_view->setIcon(
            dtv::ui::createMultiStateIcon(g_svg_article, iconColor, ctrlbar_btn_icon_sz));
    }
}
