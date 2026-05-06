#include <QFile>
#include <QTest>

#include "parsers/jsonl_parser.h"

class TestJsonlParser : public QObject {
    Q_OBJECT

private:
    JsonlParser m_parser;

    std::string readFixture(const char *name)
    {
        QString path = QString(FIXTURES_DIR) + "/" + name;
        QFile f(path);
        f.open(QIODevice::ReadOnly | QIODevice::Text);
        return f.readAll().toStdString();
    }

    const ConfigNode *findByKey(const ConfigNode &parent, const std::string &key)
    {
        for(const auto &child : parent.children) {
            if(child.key == key)
                return &child;
        }
        return nullptr;
    }

    void verifyParseErrorTree(const ConfigNode &node)
    {
        QVERIFY(!node.children.empty());
        QCOMPARE(node.children.front().key, std::string("PARSE ERROR"));
        QVERIFY(!node.children.front().children.empty());
    }

private slots:
    void test_formatName()
    {
        QCOMPARE(m_parser.format_name(), std::string("JSON Lines"));
    }

    void test_libraryCredit()
    {
        QVERIFY(!m_parser.library_credit().empty());
        QVERIFY(m_parser.library_credit().find("nlohmann/json") != std::string::npos);
    }

    void test_empty()
    {
        auto result = m_parser.parse("");
        QVERIFY(result.ok);
        QCOMPARE(result.root.type, ConfigNode::Type::Array);
        QCOMPARE(result.root.children.size(), size_t(0));
        QVERIFY(!result.has_parse_error);
        QCOMPARE(result.error_count, 0);
        QVERIFY(result.warning.empty());
    }

    void test_parseValid_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<int>("expectedCount");
        QTest::newRow("sample") << "sample.jsonl" << 4;
        QTest::newRow("edge") << "jsonl_edge.jsonl" << 7;
    }

    void test_parseValid()
    {
        QFETCH(QString, path);
        QFETCH(int, expectedCount);
        auto result = m_parser.parse(readFixture(path.toUtf8().constData()));
        QVERIFY2(result.ok, result.error.c_str());
        QCOMPARE(result.root.type, ConfigNode::Type::Array);
        QCOMPARE(result.root.children.size(), size_t(expectedCount));
        QVERIFY(!result.has_parse_error);
        QCOMPARE(result.error_count, 0);
    }

    void test_perLineErrors()
    {
        auto result = m_parser.parse(
            "{\"event\":\"login\"}\n"
            "not valid json\n"
            "\"hello\"\n");

        QVERIFY(result.ok);
        QCOMPARE(result.root.type, ConfigNode::Type::Array);
        QCOMPARE(result.root.children.size(), size_t(3));
        QVERIFY(result.has_parse_error);
        QCOMPARE(result.error_count, 1);
        QCOMPARE(result.err_line, 2);
        QCOMPARE(result.error, std::string("Line 2: invalid JSON"));

        const auto *okNode = findByKey(result.root, "[0]");
        QVERIFY(okNode != nullptr);
        QCOMPARE(okNode->type, ConfigNode::Type::Object);
        QCOMPARE(okNode->source_line, 1);

        const auto *errorNode = findByKey(result.root, "[1]");
        QVERIFY(errorNode != nullptr);
        QCOMPARE(errorNode->type, ConfigNode::Type::Object);
        QCOMPARE(errorNode->source_line, 2);
        verifyParseErrorTree(*errorNode);

        const auto *stringNode = findByKey(result.root, "[2]");
        QVERIFY(stringNode != nullptr);
        QCOMPARE(stringNode->type, ConfigNode::Type::String);
        QCOMPARE(stringNode->scalar, std::string("hello"));
        QCOMPARE(stringNode->source_line, 3);
    }

    void test_skipsBlankAndCommentLines()
    {
        auto result = m_parser.parse(
            "\n"
            "# tooling comment\n"
            "  {\"a\":1}\r\n"
            "   \n"
            "# another comment\n"
            "[1,2]\n");

        QVERIFY(result.ok);
        QCOMPARE(result.root.children.size(), size_t(2));

        const auto *first = findByKey(result.root, "[0]");
        QVERIFY(first != nullptr);
        QCOMPARE(first->source_line, 3);

        const auto *second = findByKey(result.root, "[1]");
        QVERIFY(second != nullptr);
        QCOMPARE(second->source_line, 6);
        QCOMPARE(second->type, ConfigNode::Type::Array);
    }

    void test_mixedTypesFixture()
    {
        auto result = m_parser.parse(readFixture("jsonl_edge.jsonl"));
        QVERIFY(result.ok);
        QVERIFY(!result.has_parse_error);
        QCOMPARE(result.root.children.size(), size_t(7));
        QCOMPARE(findByKey(result.root, "[0]")->type, ConfigNode::Type::Object);
        QCOMPARE(findByKey(result.root, "[1]")->type, ConfigNode::Type::Array);
        QCOMPARE(findByKey(result.root, "[2]")->type, ConfigNode::Type::Integer);
        QCOMPARE(findByKey(result.root, "[3]")->type, ConfigNode::Type::Bool);
        QCOMPARE(findByKey(result.root, "[4]")->type, ConfigNode::Type::Null);
        QCOMPARE(findByKey(result.root, "[5]")->type, ConfigNode::Type::String);
        QCOMPARE(findByKey(result.root, "[6]")->type, ConfigNode::Type::Object);
    }

    void test_largeInputWarning()
    {
        std::string input;
        for(int i = 0; i < 10005; ++i)
            input += "{\"n\":" + std::to_string(i) + "}\n";

        auto result = m_parser.parse(input);
        QVERIFY(result.ok);
        QCOMPARE(result.root.children.size(), size_t(10000));
        QVERIFY(!result.warning.empty());
        QVERIFY(result.warning.find("Showing first 10,000 of 10005 lines") != std::string::npos);
    }
};

QTEST_MAIN(TestJsonlParser)
#include "test_jsonl_parser.moc"
