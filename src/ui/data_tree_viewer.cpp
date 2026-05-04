#include "data_tree_viewer.h"

#include <QFileInfo>
#include <QLabel>
#include <QPointer>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "breadcrumb_bar.h"
#include "core/iformat_parser.h"
#include "core/parser_registry.h"
#include "search_bar.h"
#include "status_bar.h"
#include "tree_renderer.h"
#include "workers/background_thread.h"
#include "workers/parse_worker.h"

DataTreeViewer::DataTreeViewer(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_breadcrumb = new BreadcrumbBar(this);
    layout->addWidget(m_breadcrumb);

    m_renderer = new TreeRenderer(this);
    layout->addWidget(m_renderer);

    m_search = new SearchBar(this);
    layout->addWidget(m_search);

    m_status = new StatusBar(this);
    layout->addWidget(m_status);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(
        "QLabel { color: #cc0000; font-size: 14px; "
        "background: rgba(255,255,255,0.92); padding: 24px; }");
    m_errorLabel->hide();

    connect(m_renderer, &TreeRenderer::nodeActivated,
            this,       &DataTreeViewer::onNodeActivated);

    connect(m_breadcrumb, &BreadcrumbBar::segmentClicked,
            this,         &DataTreeViewer::onBreadcrumbClicked);

    connect(m_search, &SearchBar::filterChanged,
            this,     &DataTreeViewer::onFilterChanged);
    connect(m_search, &SearchBar::nextMatch,
            m_renderer, &TreeRenderer::selectNextMatch);
    connect(m_search, &SearchBar::prevMatch,
            m_renderer, &TreeRenderer::selectPreviousMatch);
}

DataTreeViewer::~DataTreeViewer()
{
    cancelPending();
}

void DataTreeViewer::loadFile(const QString& path)
{
    if (!isVisible()) {
        m_pendingLoad = path;
        return;
    }

    cancelPending();

    auto parser = ParserRegistry::instance().parserFor(
        QFileInfo(path).suffix().toLower().toStdString());
    if (!parser) {
        m_status->showError("Unsupported format");
        return;
    }

    showLoadingState();

    m_generation++;
    auto gen = m_generation;

    auto* thread = new BackgroundThread;       // no parent — self-destructs via chain
    auto* worker = new ParseWorker(path, std::move(parser)); // no parent

    QPointer<ParseWorker> workerPtr = worker;  // guards against dangling access

    // Widget destruction — interrupt the worker so chain cleanup proceeds
    connect(this, &QObject::destroyed, this, [thread, workerPtr] {
        if (workerPtr)
            thread->requestInterruption();
    });

    // Explicit cancel — same pattern
    connect(this, &DataTreeViewer::cancelRequested, this, [thread, workerPtr] {
        if (workerPtr)
            thread->requestInterruption();
    });

    // Chain cleanup: worker deletes itself → destroyed → thread deleteLater → ~BackgroundThread quit/wait
    connect(worker, &QObject::destroyed, thread, &QObject::deleteLater);

    connect(thread, &QThread::started, worker, &ParseWorker::doParse);
    connect(worker, &ParseWorker::parseCompleted,
            this,   [this, gen](std::shared_ptr<const ParseResult> result,
                                qint64 elapsedMs) {
                if (gen != m_generation) return;
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
    // Old thread+worker self-destruct via chain:
    //   QScopeGuard → worker->deleteLater()
    //   → destroyed → thread->deleteLater()
    //   → ~BackgroundThread { quit(); wait(); }
}

void DataTreeViewer::showLoadingState()
{
    m_status->showLoading();
}

void DataTreeViewer::onParseCompleted(
    std::shared_ptr<const ParseResult> result, qint64 elapsedMs)
{
    m_errorLabel->hide();

    if (!result->ok) {
        showError(result->error, result->err_line);
        return;
    }

    m_lastResult = std::move(result);

    m_renderer->render(m_lastResult->root);
    m_breadcrumb->clear();

    m_status->showStats(
        QString::fromStdString(m_lastResult->format_name),
        m_lastResult->total_nodes,
        m_lastResult->file_bytes,
        elapsedMs,
        m_renderer->truncatedCount(),
        QString::fromStdString(m_lastResult->library_credit));
}

void DataTreeViewer::onNodeActivated(const ConfigNode* node)
{
    if (node && node->source_line >= 0)
        m_status->setSourceLine(node->source_line);
    else
        m_status->setSourceLine(-1);

    QTreeWidgetItem* item = m_renderer->currentItem();
    if (!item) {
        m_breadcrumb->clear();
        return;
    }

    QStringList segments;
    QTreeWidgetItem* cur = item;
    while (cur) {
        QString key = cur->text(0);
        if (cur->parent() == nullptr && key.isEmpty())
            key = "root";
        segments.prepend(key);
        cur = cur->parent();
    }
    m_breadcrumb->setPath(segments);
}

void DataTreeViewer::onBreadcrumbClicked(int pathIndex)
{
    QTreeWidgetItem* item = m_renderer->currentItem();
    if (!item) return;

    QTreeWidgetItem* cur = item;
    QList<QTreeWidgetItem*> path;
    while (cur) {
        path.prepend(cur);
        cur = cur->parent();
    }

    if (pathIndex < path.size()) {
        QTreeWidgetItem* target = path[pathIndex];
        m_renderer->setCurrentItem(target);
        m_renderer->scrollToItem(target);
    }
}

void DataTreeViewer::onFilterChanged(const QString& text)
{
    m_renderer->applyFilter(text);

    if (!text.isEmpty() && m_renderer->visibleMatchCount() == 0) {
        m_status->showFilterNoHits(text);
    } else if (text.isEmpty()) {
        m_status->showFilterNoHits(QString());
    }
}

void DataTreeViewer::showError(const std::string& message, int errLine)
{
    m_renderer->clear();
    m_breadcrumb->clear();

    QString text = QString::fromStdString(message);
    if (errLine >= 0)
        text = QString("Parse error — line %1:\n%2").arg(errLine).arg(text);

    m_errorLabel->setText(text);
    m_errorLabel->resize(size());
    m_errorLabel->show();
    m_errorLabel->raise();
}

void DataTreeViewer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_errorLabel->setGeometry(0, 0, event->size().width(), event->size().height());
}

void DataTreeViewer::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (!m_pendingLoad.isEmpty()) {
        QString p = m_pendingLoad;
        m_pendingLoad.clear();
        loadFile(p);
    }
}
