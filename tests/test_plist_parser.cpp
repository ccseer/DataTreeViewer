#include <QFile>
#include <QTest>

#include "parsers/plist_parser.h"

class TestPlistParser : public QObject {
    Q_OBJECT

private:
    PlistParser m_parser;

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
        QCOMPARE(m_parser.format_name(), std::string("Plist"));
    }

    void test_libraryCredit()
    {
        QCOMPARE(m_parser.library_credit(), std::string("pugixml v1.14"));
    }

    void test_parseValid_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<int>("expectedRootType");
        QTest::newRow("sample") << "sample.plist" << int(ConfigNode::Type::Object);
        QTest::newRow("array") << "plist_array.plist" << int(ConfigNode::Type::Array);
    }

    void test_parseValid()
    {
        QFETCH(QString, path);
        QFETCH(int, expectedRootType);
        auto result = m_parser.parse(readFixture(path.toUtf8().constData()));
        QVERIFY2(result.ok, result.error.c_str());
        QCOMPARE(result.root.type, static_cast<ConfigNode::Type>(expectedRootType));
        QVERIFY(!result.has_parse_error);
    }

    void test_basicMapping()
    {
        auto result = m_parser.parse(readFixture("sample.plist"));
        QVERIFY(result.ok);
        QVERIFY(!result.has_parse_error);
        QCOMPARE(result.root.comment, std::string("plist version=\"1.0\""));

        const auto *name = findByKey(result.root, "Name");
        QVERIFY(name != nullptr);
        QCOMPARE(name->type, ConfigNode::Type::String);
        QCOMPARE(name->scalar, std::string("DataTreeViewer"));

        const auto *version = findByKey(result.root, "Version");
        QVERIFY(version != nullptr);
        QCOMPARE(version->type, ConfigNode::Type::Integer);
        QCOMPARE(version->scalar, std::string("1"));

        const auto *enabled = findByKey(result.root, "Enabled");
        QVERIFY(enabled != nullptr);
        QCOMPARE(enabled->type, ConfigNode::Type::Bool);
        QCOMPARE(enabled->scalar, std::string("true"));

        const auto *ratio = findByKey(result.root, "Ratio");
        QVERIFY(ratio != nullptr);
        QCOMPARE(ratio->type, ConfigNode::Type::Float);
        QCOMPARE(ratio->scalar, std::string("1.5"));

        const auto *created = findByKey(result.root, "Created");
        QVERIFY(created != nullptr);
        QCOMPARE(created->type, ConfigNode::Type::String);
        QCOMPARE(created->scalar, std::string("2026-05-06T12:00:00Z"));

        const auto *payload = findByKey(result.root, "Payload");
        QVERIFY(payload != nullptr);
        QCOMPARE(payload->type, ConfigNode::Type::String);
        QCOMPARE(payload->scalar, std::string("QUJDRA=="));
    }

    void test_arrayAndNestedDict()
    {
        auto result = m_parser.parse(readFixture("sample.plist"));
        QVERIFY(result.ok);

        const auto *items = findByKey(result.root, "Items");
        QVERIFY(items != nullptr);
        QCOMPARE(items->type, ConfigNode::Type::Array);
        QCOMPARE(items->children.size(), size_t(3));
        QVERIFY(items->children[0].key.empty());
        QCOMPARE(items->children[0].scalar, std::string("one"));
        QCOMPARE(items->children[1].type, ConfigNode::Type::Bool);
        QCOMPARE(items->children[1].scalar, std::string("false"));
        QCOMPARE(items->children[2].type, ConfigNode::Type::Object);

        const auto *inner = findByKey(items->children[2], "Inner");
        QVERIFY(inner != nullptr);
        QCOMPARE(inner->scalar, std::string("x"));
    }

    void test_binaryPlistRejected()
    {
        const std::string input = "bplist00xxxx";
        auto result = m_parser.parse(input);

        QVERIFY(result.ok);
        QVERIFY(result.has_parse_error);
        QCOMPARE(result.error, std::string("Binary plist is not supported"));
        verifyParseErrorTree(result.root);
    }

    void test_unknownTypeAndSchemaViolations()
    {
        auto result = m_parser.parse(
            "<?xml version=\"1.0\"?>\n"
            "<plist version=\"1.0\">\n"
            "<dict>\n"
            "  <key>Unknown</key>\n"
            "  <uid>7</uid>\n"
            "  <key>Missing</key>\n"
            "  <string>ok</string>\n"
            "  <integer>9</integer>\n"
            "</dict>\n"
            "</plist>\n");

        QVERIFY(result.ok);
        QVERIFY(result.has_parse_error);
        QCOMPARE(result.error_count, 2);

        const auto *unknown = findByKey(result.root, "Unknown");
        QVERIFY(unknown != nullptr);
        QCOMPARE(unknown->type, ConfigNode::Type::String);
        QCOMPARE(unknown->scalar, std::string("uid (unsupported)"));

        const auto *missing = findByKey(result.root, "Missing");
        QVERIFY(missing != nullptr);
        QCOMPARE(missing->scalar, std::string("ok"));

        const auto *unpaired = findByKey(result.root, "<unpaired>");
        QVERIFY(unpaired != nullptr);
        verifyParseErrorTree(*unpaired);
    }

    void test_parseInvalid()
    {
        auto result = m_parser.parse(readFixture("broken.plist"));
        QVERIFY(result.ok);
        QVERIFY(result.has_parse_error);
        QVERIFY(!result.error.empty());
        verifyParseErrorTree(result.root);
    }

    void test_topLevelArrayFixture()
    {
        auto result = m_parser.parse(readFixture("plist_array.plist"));
        QVERIFY(result.ok);
        QVERIFY(!result.has_parse_error);
        QCOMPARE(result.root.type, ConfigNode::Type::Array);
        QCOMPARE(result.root.children.size(), size_t(4));
        QCOMPARE(result.root.children[0].scalar, std::string("alpha"));
        QCOMPARE(result.root.children[1].scalar, std::string("7"));
        QCOMPARE(result.root.children[2].scalar, std::string("true"));
        const auto *name = findByKey(result.root.children[3], "Name");
        QVERIFY(name != nullptr);
        QCOMPARE(name->scalar, std::string("nested"));
    }

    void test_unknownFixture()
    {
        auto result = m_parser.parse(readFixture("plist_unknown.plist"));
        QVERIFY(result.ok);
        QVERIFY(result.has_parse_error);
        QCOMPARE(result.error_count, 2);
        const auto *known = findByKey(result.root, "Known");
        QVERIFY(known != nullptr);
        QCOMPARE(known->scalar, std::string("ok"));
    }
};

QTEST_MAIN(TestPlistParser)
#include "test_plist_parser.moc"
