#include "parse_worker.h"

#include <QDebug>
#include <QFile>
#include <QScopeGuard>
#include <QThread>

#include <exception>
#include <string_view>

#include "core/iformat_parser.h"

#define qprintt qDebug() << "[ParseWorker]"

ParseWorker::ParseWorker(QString path, std::unique_ptr<IFormatParser> parser, QObject *parent)
    : QObject(parent), m_path(std::move(path)), m_parser(std::move(parser))
{}

void ParseWorker::doParse()
{
    qprintt << "start" << m_path;
    auto _cleanup = qScopeGuard([this] {
        deleteLater();
    });

    QElapsedTimer timer;
    timer.start();

    auto *thread = QThread::currentThread();

    if(thread->isInterruptionRequested()) {
        qprintt << "interrupted before start";
        return;
    }

    // 1. Open + size check
    QFile f(m_path);
    if(!f.open(QIODevice::ReadOnly)) {
        qprintt << "open failed:" << f.errorString();
        auto r = std::make_shared<ParseResult>();
        r->ok = false;
        r->error = f.errorString().toStdString();
        emit parseCompleted(r, timer.elapsed());
        return;
    }
    if(f.size() > kMaxFileBytes) {
        qprintt << "file too large:" << f.size() << "bytes";
        auto r = std::make_shared<ParseResult>();
        r->ok = false;
        r->error = "File too large (max 64 MB)";
        emit parseCompleted(r, timer.elapsed());
        return;
    }
    qint64 fileBytes = f.size();
    QByteArray raw = f.readAll();
    qprintt << "read" << fileBytes << "bytes";

    if(thread->isInterruptionRequested()) {
        qprintt << "interrupted after read";
        return;
    }

    // 2. Strip UTF-8 BOM if present
    if(raw.startsWith("\xEF\xBB\xBF")) {
        raw.remove(0, 3);
        qprintt << "BOM stripped";
    }

    if(thread->isInterruptionRequested()) {
        qprintt << "interrupted after BOM";
        return;
    }

    // 3. Capture parser metadata before parse
    std::string fmtName = m_parser->format_name();
    std::string fmtCredit = m_parser->library_credit();

    if(thread->isInterruptionRequested()) {
        qprintt << "interrupted before parse";
        return;
    }

    // 4. Parse
    qprintt << "parsing with" << fmtName.c_str() << "payload bytes" << raw.size();
    auto result = std::make_shared<ParseResult>();
    try {
        qprintt << "calling parser->parse";
        *result = m_parser->parse(std::string_view(raw.constData(), size_t(raw.size())));
        qprintt << "parser->parse returned"
                << "ok" << result->ok
                << "parseError" << result->has_parse_error
                << "error bytes" << result->error.size()
                << "children" << result->root.children.size();
    } catch(const std::exception &e) {
        qprintt << "parser threw std::exception:" << e.what();
        result->ok = false;
        result->error = e.what();
    } catch(...) {
        qprintt << "parser threw unknown exception";
        result->ok = false;
        result->error = "Parser threw an unknown exception";
    }

    if(!result->ok) {
        qprintt << "parse failed:"
                << "line" << result->err_line
                << "message" << result->error.c_str();
    } else if(result->has_parse_error) {
        qprintt << "parse produced error tree:"
                << "line" << result->err_line
                << "message" << result->error.c_str();
    }

    if(thread->isInterruptionRequested()) {
        qprintt << "interrupted after parse";
        return;
    }

    // 5. Fill metadata
    result->format_name = std::move(fmtName);
    result->library_credit = std::move(fmtCredit);
    result->file_bytes = fileBytes;

    // 6. Count total nodes (non-recursive DFS)
    result->total_nodes = [](const ConfigNode &root) -> int {
        int count = 0;
        std::vector<const ConfigNode *> stack = {&root};
        while(!stack.empty()) {
            const auto *n = stack.back();
            stack.pop_back();
            ++count;
            for(const auto &c : n->children)
                stack.push_back(&c);
        }
        return count;
    }(result->root);

    if(thread->isInterruptionRequested()) {
        qprintt << "interrupted after count";
        return;
    }

    qprintt << "done" << result->total_nodes << "nodes" << timer.elapsed() << "ms";
    emit parseCompleted(result, timer.elapsed());
}
