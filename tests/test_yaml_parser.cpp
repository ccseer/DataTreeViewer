#include <QFile>
#include <QTest>

#include "parsers/yaml_parser.h"

class TestYamlParser : public QObject {
    Q_OBJECT

private:
    YamlParser m_parser;

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
        QCOMPARE(m_parser.format_name(), std::string("YAML 1.2"));
    }

    void test_libraryCredit()
    {
        QVERIFY(!m_parser.library_credit().empty());
        QVERIFY(m_parser.library_credit().find("rapidyaml") != std::string::npos);
    }

    void test_empty()
    {
        auto result = m_parser.parse("");
        QVERIFY(result.ok);
    }

    void test_parseValid_data()
    {
        QTest::addColumn<QString>("path");
        QTest::newRow("sample") << "sample.yaml";
        QTest::newRow("edge") << "yaml_edge.yaml";
    }

    void test_parseValid()
    {
        QFETCH(QString, path);
        auto result = m_parser.parse(readFixture(path.toUtf8().constData()));
        QVERIFY2(result.ok, result.error.c_str());
        QVERIFY(result.root.type == ConfigNode::Type::Object);
        QVERIFY(!result.root.children.empty());
    }

    void test_wsjcppManifest()
    {
        auto result = m_parser.parse(
            "wsjcpp_version: \"v0.1.1\"\n"
            "cmake_minimum_required: \"3.0\"\n"
            "cmake_cxx_standard: \"11\"\n"
            "name: \"nlohmann/json\"\n"
            "version: \"v3.11.3\"\n"
            "description: \"JSON for Modern C++\"\n"
            "issues: \"https://github.com/nlohmann/json/issues\"\n"
            "keywords:\n"
            "  - \"c++\"\n"
            "  - \"json\"\n"
            "\n"
            "repositories:\n"
            "  - type: main\n"
            "    url: \"https://github.com/nlohmann/json\"\n"
            "\n"
            "authors:\n"
            "  - name: \"Niels Lohmann\"\n"
            "    email: \"mail@nlohmann.me\"\n"
            "\n"
            "distribution:\n"
            "  - source-file: \"single_include/nlohmann/json.hpp\"\n"
            "    target-file: \"json.hpp\"\n"
            "    type: \"source-code\"\n"
            "  - source-file: \"single_include/nlohmann/json_fwd.hpp\"\n"
            "    target-file: \"json_fwd.hpp\"\n"
            "    type: \"source-code\"\n");
        QVERIFY2(result.ok, result.error.c_str());
        QVERIFY(!result.has_parse_error);
        QCOMPARE(result.root.type, ConfigNode::Type::Object);
        QVERIFY(!result.root.children.empty());
    }

    void test_parseInvalid_data()
    {
        QTest::addColumn<QString>("input");
        QTest::newRow("bad_syntax") << "\"unclosed string";
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
        auto result = m_parser.parse(readFixture("sample.yaml"));
        QVERIFY(result.ok);

        auto findByKey = [](const ConfigNode &parent,
                            const std::string &key) -> const ConfigNode * {
            for(const auto &c : parent.children)
                if(c.key == key)
                    return &c;
            return nullptr;
        };

        const auto *owner = findByKey(result.root, "owner");
        QVERIFY(owner != nullptr);
        QCOMPARE(owner->type, ConfigNode::Type::Object);

        const auto *tags = findByKey(result.root, "tags");
        QVERIFY(tags != nullptr);
        QCOMPARE(tags->type, ConfigNode::Type::Array);
    }

    void test_irTypeRoundtrip()
    {
        auto result = m_parser.parse(readFixture("sample.yaml"));
        QVERIFY(result.ok);

        auto findByKey = [](const ConfigNode &parent,
                            const std::string &key) -> const ConfigNode * {
            for(const auto &c : parent.children)
                if(c.key == key)
                    return &c;
            return nullptr;
        };

        const auto *types = findByKey(result.root, "types");
        QVERIFY(types != nullptr);

        std::unordered_map<std::string, ConfigNode::Type> expected = {
            {"string_val", ConfigNode::Type::String}, {"int_val", ConfigNode::Type::Integer},
            {"neg_val", ConfigNode::Type::Integer},   {"float_val", ConfigNode::Type::Float},
            {"bool_true", ConfigNode::Type::Bool},    {"bool_false", ConfigNode::Type::Bool},
            {"null_val", ConfigNode::Type::Null},
        };

        for(const auto &child : types->children) {
            auto it = expected.find(child.key);
            if(it == expected.end())
                continue;
            QCOMPARE(child.type, it->second);
        }
    }

    void test_multilineStrings()
    {
        auto result = m_parser.parse(readFixture("sample.yaml"));
        QVERIFY(result.ok);

        auto findByKey = [](const ConfigNode &parent,
                            const std::string &key) -> const ConfigNode * {
            for(const auto &c : parent.children)
                if(c.key == key)
                    return &c;
            return nullptr;
        };

        const auto *desc = findByKey(result.root, "description");
        QVERIFY(desc != nullptr);
        QCOMPARE(desc->type, ConfigNode::Type::String);
        QVERIFY(desc->scalar.find("multi-line") != std::string::npos);

        const auto *summary = findByKey(result.root, "summary");
        QVERIFY(summary != nullptr);
        QCOMPARE(summary->type, ConfigNode::Type::String);
    }

    void test_commentExtraction()
    {
        auto result = m_parser.parse(
            "# leading a\n"
            "a: 1\n"
            "b: 2 # inline b\n"
            "# leading c\n"
            "c: 3\n");
        QVERIFY2(result.ok, result.error.c_str());

        auto findByKey = [](const ConfigNode &parent,
                            const std::string &key) -> const ConfigNode * {
            for(const auto &c : parent.children)
                if(c.key == key)
                    return &c;
            return nullptr;
        };

        const auto *aNode = findByKey(result.root, "a");
        QVERIFY(aNode != nullptr);
        QCOMPARE(QString::fromStdString(aNode->comment), QString("leading a"));

        const auto *bNode = findByKey(result.root, "b");
        QVERIFY(bNode != nullptr);
        QCOMPARE(QString::fromStdString(bNode->comment), QString("inline b"));

        const auto *cNode = findByKey(result.root, "c");
        QVERIFY(cNode != nullptr);
        QCOMPARE(QString::fromStdString(cNode->comment), QString("leading c"));

        auto fixture = m_parser.parse(readFixture("sample.yaml"));
        QVERIFY2(fixture.ok, fixture.error.c_str());
        const auto *commentedNode = findByKey(fixture.root, "commented_key");
        QVERIFY(commentedNode != nullptr);
        QCOMPARE(QString::fromStdString(commentedNode->comment), QString("inline value comment"));
    }

    void test_hashInsideQuotedScalarIsNotComment()
    {
        auto result = m_parser.parse(
            "single: 'value # not comment'\n"
            "double: \"value # not comment\"\n"
            "plain: value # real comment\n");
        QVERIFY2(result.ok, result.error.c_str());

        auto findByKey = [](const ConfigNode &parent,
                            const std::string &key) -> const ConfigNode * {
            for(const auto &c : parent.children)
                if(c.key == key)
                    return &c;
            return nullptr;
        };

        const auto *singleNode = findByKey(result.root, "single");
        QVERIFY(singleNode != nullptr);
        QVERIFY(singleNode->comment.empty());
        QCOMPARE(singleNode->scalar, std::string("value # not comment"));

        const auto *doubleNode = findByKey(result.root, "double");
        QVERIFY(doubleNode != nullptr);
        QVERIFY(doubleNode->comment.empty());
        QCOMPARE(doubleNode->scalar, std::string("value # not comment"));

        const auto *plainNode = findByKey(result.root, "plain");
        QVERIFY(plainNode != nullptr);
        QCOMPARE(QString::fromStdString(plainNode->comment), QString("real comment"));
    }

    void test_quotedScalarsStayStrings()
    {
        auto result = m_parser.parse(
            "quoted_int: \"42\"\n"
            "quoted_bool: 'true'\n"
            "plain_int: 42\n"
            "literal_bool: |\n"
            "  true\n");
        QVERIFY2(result.ok, result.error.c_str());

        auto findByKey = [](const ConfigNode &parent,
                            const std::string &key) -> const ConfigNode * {
            for(const auto &c : parent.children)
                if(c.key == key)
                    return &c;
            return nullptr;
        };

        const auto *quotedInt = findByKey(result.root, "quoted_int");
        QVERIFY(quotedInt != nullptr);
        QCOMPARE(quotedInt->type, ConfigNode::Type::String);

        const auto *quotedBool = findByKey(result.root, "quoted_bool");
        QVERIFY(quotedBool != nullptr);
        QCOMPARE(quotedBool->type, ConfigNode::Type::String);

        const auto *plainInt = findByKey(result.root, "plain_int");
        QVERIFY(plainInt != nullptr);
        QCOMPARE(plainInt->type, ConfigNode::Type::Integer);

        const auto *literalBool = findByKey(result.root, "literal_bool");
        QVERIFY(literalBool != nullptr);
        QCOMPARE(literalBool->type, ConfigNode::Type::String);
    }

    void test_brokenFile()
    {
        auto result = m_parser.parse(readFixture("broken.yaml"));
        QVERIFY(result.ok);
        QVERIFY(result.has_parse_error);
        QVERIFY(result.err_line > 0);
        verifyParseErrorTree(result);
    }

    void test_edgeFixture()
    {
        auto result = m_parser.parse(readFixture("yaml_edge.yaml"));
        QVERIFY(result.ok);

        auto findByKey = [](const ConfigNode &parent,
                            const std::string &key) -> const ConfigNode * {
            for(const auto &c : parent.children)
                if(c.key == key)
                    return &c;
            return nullptr;
        };

        const auto *meta = findByKey(result.root, "meta");
        QVERIFY(meta != nullptr);
        const auto *items = findByKey(result.root, "items");
        QVERIFY(items != nullptr);
        QCOMPARE(items->type, ConfigNode::Type::Array);
        QCOMPARE(items->children.size(), size_t(2));
        const auto *quotedHash = findByKey(result.root, "quoted_hash");
        QVERIFY(quotedHash != nullptr);
        QCOMPARE(quotedHash->scalar, std::string("value # still data"));
    }
};

QTEST_MAIN(TestYamlParser)
#include "test_yaml_parser.moc"
