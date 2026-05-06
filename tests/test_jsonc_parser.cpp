#include <QFile>
#include <QTest>

#include "parsers/jsonc_parser.h"

class TestJsoncParser : public QObject {
    Q_OBJECT

private:
    JsoncParser m_parser;

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

private slots:
    void test_formatName()
    {
        QCOMPARE(m_parser.format_name(), std::string("JSONC"));
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
        QVERIFY(result.has_parse_error);
        verifyParseErrorTree(result);
    }

    void test_parseValid_data()
    {
        QTest::addColumn<QString>("path");
        QTest::newRow("sample") << "sample.jsonc";
        QTest::newRow("edge") << "jsonc_edge.jsonc";
    }

    void test_parseValid()
    {
        QFETCH(QString, path);
        auto result = m_parser.parse(readFixture(path.toUtf8().constData()));
        QVERIFY2(result.ok, result.error.c_str());
        QCOMPARE(result.root.type, ConfigNode::Type::Object);
        QVERIFY(!result.root.children.empty());
    }

    void test_parseInvalid_data()
    {
        QTest::addColumn<QString>("input");
        QTest::newRow("bare_string") << "hello";
        QTest::newRow("unclosed") << "{";
        QTest::newRow("trailing") << "{,}";
        QTest::newRow("bad_number") << "[1,2,]";
    }

    void test_parseInvalid()
    {
        QFETCH(QString, input);
        auto result = m_parser.parse(input.toStdString());
        QVERIFY(result.ok);
        QVERIFY(result.has_parse_error);
        QVERIFY(!result.error.empty());
        verifyParseErrorTree(result);
    }

    void test_nestedContainers()
    {
        const auto *data = R"({
            "obj": {"a": 1},
            "arr": [1, 2, 3],
            "nested": {"inner": [{"x": true}]}
        })";
        auto result = m_parser.parse(data);
        QVERIFY(result.ok);
        QCOMPARE(result.root.type, ConfigNode::Type::Object);
        QCOMPARE(result.root.children.size(), size_t(3));

        auto findByKey = [](const ConfigNode &parent,
                            const std::string &key) -> const ConfigNode * {
            for(const auto &c : parent.children)
                if(c.key == key)
                    return &c;
            return nullptr;
        };

        const auto *objNode = findByKey(result.root, "obj");
        QVERIFY(objNode != nullptr);
        QCOMPARE(objNode->type, ConfigNode::Type::Object);

        const auto *arrNode = findByKey(result.root, "arr");
        QVERIFY(arrNode != nullptr);
        QCOMPARE(arrNode->type, ConfigNode::Type::Array);
        QCOMPARE(arrNode->children.size(), size_t(3));

        const auto *nestedNode = findByKey(result.root, "nested");
        QVERIFY(nestedNode != nullptr);
        QCOMPARE(nestedNode->type, ConfigNode::Type::Object);
    }

    void test_irTypeRoundtrip()
    {
        const auto *data = R"({
            "str": "hello",
            "int": 42,
            "neg": -7,
            "float": 3.14,
            "true_val": true,
            "false_val": false,
            "null_val": null
        })";
        auto result = m_parser.parse(data);
        QVERIFY(result.ok);

        std::unordered_map<std::string, std::pair<ConfigNode::Type, std::string>> expected = {
            {"str", {ConfigNode::Type::String, "hello"}},
            {"int", {ConfigNode::Type::Integer, "42"}},
            {"neg", {ConfigNode::Type::Integer, "-7"}},
            {"float", {ConfigNode::Type::Float, "3.14"}},
            {"true_val", {ConfigNode::Type::Bool, "true"}},
            {"false_val", {ConfigNode::Type::Bool, "false"}},
            {"null_val", {ConfigNode::Type::Null, "null"}},
        };

        for(const auto &child : result.root.children) {
            auto it = expected.find(child.key);
            QVERIFY2(it != expected.end(), (std::string("Unexpected key: ") + child.key).c_str());
            QCOMPARE(child.type, it->second.first);
            QCOMPARE(child.scalar, it->second.second);
        }
    }

    void test_commentExtraction()
    {
        auto result = m_parser.parse(readFixture("sample.jsonc"));
        QVERIFY(result.ok);

        // The standalone comment "This is a sample JSONC file with comments"
        // (line 2) is assigned to the first key "name"
        auto findKey = [](const ConfigNode &parent, const std::string &key) -> const ConfigNode * {
            for(const auto &c : parent.children)
                if(c.key == key)
                    return &c;
            return nullptr;
        };

        const auto *nameNode = findKey(result.root, "name");
        QVERIFY(nameNode != nullptr);
        QCOMPARE(QString::fromStdString(nameNode->comment),
                 QString("This is a sample JSONC file with comments"));
    }

    void test_trailingComment()
    {
        const auto *data = R"({
            "key": "value", // trailing comment
            "next": 1
        })";
        auto result = m_parser.parse(data);
        QVERIFY(result.ok);
        QCOMPARE(result.root.children.size(), size_t(2));
        QCOMPARE(QString::fromStdString(result.root.children[0].comment),
                 QString("trailing comment"));
        QVERIFY(result.root.children[1].comment.empty());
    }

    void test_standaloneComment()
    {
        const auto *data = R"({
            // first comment
            "a": 1,
            "b": 2,
            // above c
            "c": 3
        })";
        auto result = m_parser.parse(data);
        QVERIFY(result.ok);
        QCOMPARE(result.root.children.size(), size_t(3));

        auto f = [](const ConfigNode &p, const std::string &k) -> const ConfigNode * {
            for(const auto &c : p.children)
                if(c.key == k)
                    return &c;
            return nullptr;
        };

        const auto *aNode = f(result.root, "a");
        QVERIFY(aNode != nullptr);
        QCOMPARE(QString::fromStdString(aNode->comment), QString("first comment"));

        const auto *bNode = f(result.root, "b");
        QVERIFY(bNode != nullptr);
        QVERIFY(bNode->comment.empty());

        const auto *cNode = f(result.root, "c");
        QVERIFY(cNode != nullptr);
        QCOMPARE(QString::fromStdString(cNode->comment), QString("above c"));
    }

    void test_blockComment()
    {
        const auto *data = R"({
            /* Multi-line
               comment block */
            "unicode": "Hello"
        })";
        auto result = m_parser.parse(data);
        QVERIFY(result.ok);

        const ConfigNode *unicodeNode = nullptr;
        for(const auto &child : result.root.children) {
            if(child.key == "unicode") {
                unicodeNode = &child;
                break;
            }
        }

        QVERIFY(unicodeNode != nullptr);
        QCOMPARE(QString::fromStdString(unicodeNode->comment),
                 QString("Multi-line\n               comment block"));
    }

    void test_brokenFile()
    {
        auto result = m_parser.parse(readFixture("broken.jsonc"));
        QVERIFY(result.ok);
        QVERIFY(result.has_parse_error);
        QVERIFY(!result.error.empty());
        verifyParseErrorTree(result);
    }

    void test_edgeFixture()
    {
        auto result = m_parser.parse(readFixture("jsonc_edge.jsonc"));
        QVERIFY(result.ok);
        QCOMPARE(result.root.type, ConfigNode::Type::Object);

        const auto *arrayNode = [&]() -> const ConfigNode * {
            for(const auto &child : result.root.children)
                if(child.key == "array")
                    return &child;
            return nullptr;
        }();
        QVERIFY(arrayNode != nullptr);
        QCOMPARE(arrayNode->type, ConfigNode::Type::Array);
        QCOMPARE(arrayNode->children.size(), size_t(2));

        const auto *urlNode = [&]() -> const ConfigNode * {
            for(const auto &child : result.root.children)
                if(child.key == "url")
                    return &child;
            return nullptr;
        }();
        QVERIFY(urlNode != nullptr);
        QCOMPARE(urlNode->scalar, std::string("https://example.com/#fragment"));
    }
};

QTEST_MAIN(TestJsoncParser)
#include "test_jsonc_parser.moc"
