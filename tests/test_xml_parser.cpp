#include <QFile>
#include <QTest>

#include "parsers/xml_parser.h"

class TestXmlParser : public QObject {
    Q_OBJECT

private:
    XmlParser m_parser;

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

    void verifyParseErrorTree(const ParseResult &result)
    {
        QVERIFY(!result.root.children.empty());
        QCOMPARE(result.root.children.front().key, std::string("PARSE ERROR"));
        QVERIFY(!result.root.children.front().children.empty());
    }

private slots:
    void test_formatName()
    {
        QCOMPARE(m_parser.format_name(), std::string("XML"));
    }

    void test_libraryCredit()
    {
        QCOMPARE(m_parser.library_credit(), std::string("pugixml v1.14"));
    }

    void test_parseValid_data()
    {
        QTest::addColumn<QString>("path");
        QTest::newRow("sample") << "sample.xml";
    }

    void test_parseValid()
    {
        QFETCH(QString, path);
        auto result = m_parser.parse(readFixture(path.toUtf8().constData()));
        QVERIFY2(result.ok, result.error.c_str());
        QCOMPARE(result.root.type, ConfigNode::Type::Object);
        QCOMPARE(result.root.children.size(), size_t(1));
        QVERIFY(!result.has_parse_error);
    }

    void test_basicMapping()
    {
        auto result = m_parser.parse(readFixture("sample.xml"));
        QVERIFY(result.ok);

        const auto *config = findByKey(result.root, "config");
        QVERIFY(config != nullptr);
        QCOMPARE(config->type, ConfigNode::Type::Object);
        QCOMPARE(config->comment, std::string("app config"));
        QVERIFY(config->source_line > 0);

        const auto *version = findByKey(*config, "@version");
        QVERIFY(version != nullptr);
        QCOMPARE(version->type, ConfigNode::Type::String);
        QCOMPARE(version->scalar, std::string("2"));

        const auto *database = findByKey(*config, "database");
        QVERIFY(database != nullptr);
        QCOMPARE(database->type, ConfigNode::Type::Object);

        const auto *host = findByKey(*database, "@host");
        QVERIFY(host != nullptr);
        QCOMPARE(host->scalar, std::string("localhost"));

        const auto *port = findByKey(*database, "@port");
        QVERIFY(port != nullptr);
        QCOMPARE(port->scalar, std::string("5432"));

        const auto *name = findByKey(*database, "name");
        QVERIFY(name != nullptr);
        QCOMPARE(name->type, ConfigNode::Type::String);
        QCOMPARE(name->scalar, std::string("mydb"));

        const auto *debug = findByKey(*config, "debug");
        QVERIFY(debug != nullptr);
        QCOMPARE(debug->type, ConfigNode::Type::Null);
        QCOMPARE(debug->scalar, std::string(""));
    }

    void test_mixedContentAndCdata()
    {
        auto result = m_parser.parse(
            "<root attr=\"1\">hello<![CDATA[ world ]]><child/>tail</root>");
        QVERIFY(result.ok);

        const auto *root = findByKey(result.root, "root");
        QVERIFY(root != nullptr);
        QCOMPARE(root->type, ConfigNode::Type::Object);

        QCOMPARE(root->children.size(), size_t(5));
        QCOMPARE(root->children[0].key, std::string("@attr"));
        QCOMPARE(root->children[0].scalar, std::string("1"));
        QCOMPARE(root->children[1].key, std::string("#text"));
        QCOMPARE(root->children[1].scalar, std::string("hello"));
        QCOMPARE(root->children[2].key, std::string("#text"));
        QCOMPARE(root->children[2].scalar, std::string(" world "));
        QCOMPARE(root->children[3].key, std::string("child"));
        QCOMPARE(root->children[3].type, ConfigNode::Type::Null);
        QCOMPARE(root->children[4].key, std::string("#text"));
        QCOMPARE(root->children[4].scalar, std::string("tail"));
    }

    void test_repeatedSiblingsAndTrailingComment()
    {
        auto result = m_parser.parse(
            "<root>\n"
            "  <!-- before item -->\n"
            "  <item>1</item>\n"
            "  <item>2</item>\n"
            "  <!-- trailing -->\n"
            "</root>\n");
        QVERIFY(result.ok);

        const auto *root = findByKey(result.root, "root");
        QVERIFY(root != nullptr);
        QCOMPARE(root->children.size(), size_t(2));
        QCOMPARE(root->comment, std::string("trailing"));
        QCOMPARE(root->children[0].key, std::string("item"));
        QCOMPARE(root->children[0].comment, std::string("before item"));
        QCOMPARE(root->children[0].scalar, std::string("1"));
        QCOMPARE(root->children[1].key, std::string("item"));
        QCOMPARE(root->children[1].scalar, std::string("2"));
    }

    void test_parseInvalid()
    {
        auto result = m_parser.parse(readFixture("broken.xml"));
        QVERIFY(result.ok);
        QVERIFY(result.has_parse_error);
        QVERIFY(!result.error.empty());
        QVERIFY(result.err_line > 0);
        verifyParseErrorTree(result);
    }
};

QTEST_MAIN(TestXmlParser)
#include "test_xml_parser.moc"
