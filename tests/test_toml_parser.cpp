#include <QFile>
#include <QTest>

#include <functional>
#include <unordered_map>
#include <vector>

#include "parsers/toml_parser.h"

class TestTomlParser : public QObject {
    Q_OBJECT

private:
    TomlParser m_parser;

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
        for(const auto &c : parent.children) {
            if(c.key == key)
                return &c;
        }
        return nullptr;
    }

private slots:
    void test_formatName()
    {
        QCOMPARE(m_parser.format_name(), std::string("TOML 1.0"));
    }

    void test_libraryCredit()
    {
        QVERIFY(!m_parser.library_credit().empty());
        QVERIFY(m_parser.library_credit().find("toml++") != std::string::npos);
    }

    void test_empty()
    {
        auto result = m_parser.parse("");
        // Empty string is valid TOML (empty document)
        QVERIFY(result.ok);
    }

    void test_parseValid_data()
    {
        QTest::addColumn<QString>("path");
        QTest::newRow("sample") << "sample.toml";
        QTest::newRow("edge") << "toml_edge.toml";
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
        QTest::newRow("bare_string") << "just some text";
        QTest::newRow("bad_syntax") << "[section\nkey =";
    }

    void test_parseInvalid()
    {
        QFETCH(QString, input);
        auto result = m_parser.parse(input.toStdString());
        QVERIFY(result.ok);
        QVERIFY(result.has_parse_error);
        verifyParseErrorTree(result);
    }

    void test_nestedContainers()
    {
        auto result = m_parser.parse(readFixture("sample.toml"));
        QVERIFY(result.ok);

        const auto *owner = findByKey(result.root, "owner");
        QVERIFY(owner != nullptr);
        QCOMPARE(owner->type, ConfigNode::Type::Object);

        const auto *products = findByKey(result.root, "products");
        QVERIFY(products != nullptr);
        QCOMPARE(products->type, ConfigNode::Type::Array);
    }

    void test_irTypeRoundtrip()
    {
        auto result = m_parser.parse(readFixture("sample.toml"));
        QVERIFY(result.ok);

        const auto *types = findByKey(result.root, "types");
        QVERIFY(types != nullptr);

        std::unordered_map<std::string, ConfigNode::Type> expected = {
            {"string_val", ConfigNode::Type::String}, {"int_val", ConfigNode::Type::Integer},
            {"neg_val", ConfigNode::Type::Integer},   {"float_val", ConfigNode::Type::Float},
            {"bool_true", ConfigNode::Type::Bool},    {"bool_false", ConfigNode::Type::Bool},
            {"date_val", ConfigNode::Type::String},   {"time_val", ConfigNode::Type::String},
            {"array_val", ConfigNode::Type::Array},
        };

        for(const auto &child : types->children) {
            auto it = expected.find(child.key);
            QVERIFY2(it != expected.end(), (std::string("Unexpected key: ") + child.key).c_str());
            QCOMPARE(child.type, it->second);
        }
    }

    void test_commentExtraction()
    {
        auto result = m_parser.parse(readFixture("sample.toml"));
        QVERIFY(result.ok);

        const auto *title = findByKey(result.root, "title");
        QVERIFY(title != nullptr);
        QCOMPARE(title->comment, std::string("TOML 1.0 sample for DataTreeViewer testing"));

        const auto *emptySection = findByKey(result.root, "empty_section");
        QVERIFY(emptySection != nullptr);
        QCOMPARE(emptySection->comment, std::string("Standalone section tests"));
    }

    void test_inlineComment()
    {
        auto result = m_parser.parse(
            "title = \"TOML\" # inline title\n"
            "hash = \"value # not comment\"\n");
        QVERIFY(result.ok);

        const auto *title = findByKey(result.root, "title");
        QVERIFY(title != nullptr);
        QCOMPARE(title->comment, std::string("inline title"));

        const auto *hash = findByKey(result.root, "hash");
        QVERIFY(hash != nullptr);
        QCOMPARE(hash->scalar, std::string("value # not comment"));
        QVERIFY(hash->comment.empty());
    }

    void test_preservesLoadOrder()
    {
        auto result = m_parser.parse(readFixture("sample.toml"));
        QVERIFY(result.ok);

        const std::vector<std::string> expected = {
            "title", "owner", "database", "servers", "products", "empty_section", "types",
        };
        QVERIFY(result.root.children.size() >= expected.size());
        for(size_t i = 0; i < expected.size(); ++i)
            QCOMPARE(result.root.children[i].key, expected[i]);
    }

    void test_sourceLine()
    {
        auto result = m_parser.parse(readFixture("sample.toml"));
        QVERIFY(result.ok);

        const auto *title = findByKey(result.root, "title");
        QVERIFY(title != nullptr);
        QCOMPARE(title->source_line, 3);

        const auto *owner = findByKey(result.root, "owner");
        QVERIFY(owner != nullptr);
        QCOMPARE(owner->source_line, 5);

        // Walk tree, at least some nodes should have source_line > 0
        std::function<bool(const ConfigNode &)> hasLines = [&](const ConfigNode &node) -> bool {
            if(node.source_line > 0)
                return true;
            for(const auto &c : node.children)
                if(hasLines(c))
                    return true;
            return false;
        };
        QVERIFY(hasLines(result.root));
    }

    void test_brokenFile()
    {
        auto result = m_parser.parse(readFixture("broken.toml"));
        QVERIFY(result.ok);
        QVERIFY(result.has_parse_error);
        QVERIFY(result.err_line > 0);
        verifyParseErrorTree(result);
    }

    void test_edgeFixture()
    {
        auto result = m_parser.parse(readFixture("toml_edge.toml"));
        QVERIFY(result.ok);

        const auto *products = findByKey(result.root, "products");
        QVERIFY(products != nullptr);
        QCOMPARE(products->type, ConfigNode::Type::Array);
        QCOMPARE(products->children.size(), size_t(2));

        const auto *server = findByKey(result.root, "server");
        QVERIFY(server != nullptr);
        const auto *ports = findByKey(*server, "ports");
        QVERIFY(ports != nullptr);
        QCOMPARE(ports->type, ConfigNode::Type::Array);
        QCOMPARE(ports->children.size(), size_t(3));
    }
};

QTEST_MAIN(TestTomlParser)
#include "test_toml_parser.moc"
