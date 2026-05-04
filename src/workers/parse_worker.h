#pragma once

#include <QObject>
#include <QString>
#include <QElapsedTimer>

#include <memory>

class IFormatParser;
struct ParseResult;

class ParseWorker : public QObject {
    Q_OBJECT
public:
    explicit ParseWorker(QString path,
                         std::unique_ptr<IFormatParser> parser,
                         QObject* parent = nullptr);

public slots:
    void doParse();

signals:
    void parseCompleted(std::shared_ptr<const ParseResult> result,
                        qint64 elapsedMs);

private:
    static constexpr qint64 kMaxFileBytes = 64 * 1024 * 1024;

    QString                        m_path;
    std::unique_ptr<IFormatParser> m_parser;
};
