#include <QFile>
#include <QTest>

#include "parsers/env_parser.h"

class TestEnvParser : public QObject {
    Q_OBJECT

private:
    EnvParser m_parser;

    std::string readFixture(const char *name)
    {
        QString path = QString(FIXTURES_DIR) + "/" + name;
        QFile f(path);
        f.open(QIODevice::ReadOnly | QIODevice::Text);
        return f.readAll().toStdString();
    }

    void verifyParseErrorTree(const ParseResult &result)
    {
        QVERIFY(!result.root.children.empty());
        QCOMPARE(result.root.children.front().key, std::string("PARSE ERROR"));
        QVERIFY(!result.root.children.front().children.empty());
    }

    const ConfigNode *findByKey(const ConfigNode &parent, const std::string &key)
    {
        for(const auto &child : parent.children) {
            if(child.key == key)
                return &child;
        }
        return nullptr;
    }

private slots:
    void test_formatName()
    {
        QCOMPARE(m_parser.format_name(), std::string("Env"));
    }

    void test_libraryCredit()
    {
        QCOMPARE(m_parser.library_credit(), std::string(""));
    }

    void test_empty()
    {
        auto result = m_parser.parse("");
        QVERIFY(result.ok);
        QCOMPARE(result.root.type, ConfigNode::Type::Object);
        QCOMPARE(result.root.children.size(), size_t(0));
        QVERIFY(!result.has_parse_error);
    }

    void test_parseValid_data()
    {
        QTest::addColumn<QString>("path");
        QTest::newRow("sample") << "sample.env";
    }

    void test_parseValid()
    {
        QFETCH(QString, path);
        auto result = m_parser.parse(readFixture(path.toUtf8().constData()));
        QVERIFY2(result.ok, result.error.c_str());
        QCOMPARE(result.root.type, ConfigNode::Type::Object);
        QVERIFY(!result.root.children.empty());
    }

    void test_basicParsing()
    {
        auto result = m_parser.parse(readFixture("sample.env"));
        QVERIFY(result.ok);
        QVERIFY(!result.has_parse_error);
        QCOMPARE(result.root.type, ConfigNode::Type::Object);
        QCOMPARE(result.root.children.size(), size_t(12));

        const auto *appEnv = findByKey(result.root, "APP_ENV");
        QVERIFY(appEnv != nullptr);
        QCOMPARE(appEnv->type, ConfigNode::Type::String);
        QCOMPARE(appEnv->scalar, std::string("production"));
        QCOMPARE(appEnv->source_line, 2);

        const auto *appPort = findByKey(result.root, "APP_PORT");
        QVERIFY(appPort != nullptr);
        QCOMPARE(appPort->scalar, std::string("3000"));
        QCOMPARE(appPort->source_line, 3);

        const auto *secret = findByKey(result.root, "SECRET");
        QVERIFY(secret != nullptr);
        QCOMPARE(secret->type, ConfigNode::Type::Null);
        QCOMPARE(secret->scalar, std::string(""));

        const auto *spaced = findByKey(result.root, "SPACED");
        QVERIFY(spaced != nullptr);
        QCOMPARE(spaced->scalar, std::string("value with spaces"));
        QCOMPARE(spaced->source_line, 8);

        const auto *digitKey = findByKey(result.root, "9KEY");
        QVERIFY(digitKey != nullptr);
        QCOMPARE(digitKey->scalar, std::string("accepted"));
    }

    void test_commentAssociation()
    {
        auto result = m_parser.parse(readFixture("sample.env"));
        QVERIFY(result.ok);

        const auto *appEnv = findByKey(result.root, "APP_ENV");
        QVERIFY(appEnv != nullptr);
        QCOMPARE(appEnv->comment, std::string("app settings"));

        const auto *appPort = findByKey(result.root, "APP_PORT");
        QVERIFY(appPort != nullptr);
        QCOMPARE(appPort->comment, std::string("HTTP port"));

        const auto *dbUrl = findByKey(result.root, "DB_URL");
        QVERIFY(dbUrl != nullptr);
        QCOMPARE(dbUrl->comment, std::string("database"));

        const auto *orphan = findByKey(result.root, "ORPHAN");
        QVERIFY(orphan != nullptr);
        QVERIFY(orphan->comment.empty());
    }

    void test_inlineAndQuotedValues()
    {
        auto result = m_parser.parse(
            "DOUBLE=\"line\\nnext\\tend\\\\\\\"\"\n");

        QVERIFY(result.ok);

        const auto *doubleQuoted = findByKey(result.root, "DOUBLE");
        QVERIFY(doubleQuoted != nullptr);
        QCOMPARE(doubleQuoted->type, ConfigNode::Type::String);
        QCOMPARE(doubleQuoted->scalar, std::string("line\nnext\tend\\\""));

        result = m_parser.parse(readFixture("sample.env"));

        QVERIFY(result.ok);

        const auto *singleQuoted = findByKey(result.root, "SINGLE");
        QVERIFY(singleQuoted != nullptr);
        QCOMPARE(singleQuoted->scalar, std::string("raw # value"));

        const auto *unquoted = findByKey(result.root, "UNQUOTED");
        QVERIFY(unquoted != nullptr);
        QCOMPARE(unquoted->scalar, std::string("value"));
        QCOMPARE(unquoted->comment, std::string("comment here"));

        const auto *hashy = findByKey(result.root, "HASHY");
        QVERIFY(hashy != nullptr);
        QCOMPARE(hashy->scalar, std::string("value#not comment"));
        QVERIFY(hashy->comment.empty());

        const auto *unknown = findByKey(result.root, "UNKNOWN");
        QVERIFY(unknown != nullptr);
        QCOMPARE(unknown->scalar, std::string("keep \\x"));
    }

    void test_invalidLinesAreSkipped()
    {
        auto result = m_parser.parse(
            "NO_EQUALS\n"
            "=value\n"
            " GOOD = ok \n"
            "EXTRA=a=b=c\n"
            "9KEY=accepted\n");

        QVERIFY(result.ok);
        QCOMPARE(result.root.children.size(), size_t(3));

        QVERIFY(findByKey(result.root, "GOOD") != nullptr);

        const auto *extra = findByKey(result.root, "EXTRA");
        QVERIFY(extra != nullptr);
        QCOMPARE(extra->scalar, std::string("a=b=c"));

        const auto *digitKey = findByKey(result.root, "9KEY");
        QVERIFY(digitKey != nullptr);
        QCOMPARE(digitKey->scalar, std::string("accepted"));
    }

    void test_nulByteFails()
    {
        std::string input("A=1\0B=2", 7);
        auto result = m_parser.parse(input);

        QVERIFY(result.ok);
        QVERIFY(result.has_parse_error);
        QCOMPARE(result.error, std::string("File is not valid UTF-8 text"));
        verifyParseErrorTree(result);
    }
};

QTEST_MAIN(TestEnvParser)
#include "test_env_parser.moc"
