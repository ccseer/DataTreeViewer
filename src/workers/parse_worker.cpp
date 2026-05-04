#include "parse_worker.h"

#include <QFile>
#include <QScopeGuard>
#include <QThread>

#include "core/iformat_parser.h"

ParseWorker::ParseWorker(QString path,
                         std::unique_ptr<IFormatParser> parser,
                         QObject* parent)
    : QObject(parent)
    , m_path(std::move(path))
    , m_parser(std::move(parser))
{
}

void ParseWorker::doParse()
{
    auto _cleanup = qScopeGuard([this] { deleteLater(); });

    QElapsedTimer timer;
    timer.start();

    auto* thread = QThread::currentThread();

    if (thread->isInterruptionRequested())
        return;

    // 1. Open + size check
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly)) {
        auto r = std::make_shared<ParseResult>();
        r->ok    = false;
        r->error = f.errorString().toStdString();
        emit parseCompleted(r, timer.elapsed());
        return;
    }
    if (f.size() > kMaxFileBytes) {
        auto r = std::make_shared<ParseResult>();
        r->ok    = false;
        r->error = "File too large (max 64 MB)";
        emit parseCompleted(r, timer.elapsed());
        return;
    }
    qint64 fileBytes = f.size();
    QByteArray raw = f.readAll();

    if (thread->isInterruptionRequested())
        return;

    // 2. Strip UTF-8 BOM if present
    if (raw.startsWith("\xEF\xBB\xBF"))
        raw.remove(0, 3);

    if (thread->isInterruptionRequested())
        return;

    // 3. Capture parser metadata before parse
    std::string fmtName   = m_parser->format_name();
    std::string fmtCredit = m_parser->library_credit();

    if (thread->isInterruptionRequested())
        return;

    // 4. Parse
    auto result = std::make_shared<ParseResult>(
        m_parser->parse(std::string_view(raw.constData(), size_t(raw.size()))));

    if (thread->isInterruptionRequested())
        return;

    // 5. Fill metadata
    result->format_name    = std::move(fmtName);
    result->library_credit = std::move(fmtCredit);
    result->file_bytes     = fileBytes;

    // 6. Count total nodes (non-recursive DFS)
    result->total_nodes = [](const ConfigNode& root) -> int {
        int count = 0;
        std::vector<const ConfigNode*> stack = { &root };
        while (!stack.empty()) {
            const auto* n = stack.back();
            stack.pop_back();
            ++count;
            for (const auto& c : n->children)
                stack.push_back(&c);
        }
        return count;
    }(result->root);

    if (thread->isInterruptionRequested())
        return;

    emit parseCompleted(result, timer.elapsed());
}
