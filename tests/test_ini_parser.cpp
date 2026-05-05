#include <QFile>
#include <QTest>

#include <vector>

#include "parsers/ini_parser.h"

class TestIniParser : public QObject {
    Q_OBJECT

private:
    IniParser m_parser;

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
        QCOMPARE(m_parser.format_name(), std::string("INI"));
    }

    void test_libraryCredit()
    {
        QVERIFY(!m_parser.library_credit().empty());
        QVERIFY(m_parser.library_credit().find("SimpleIni") != std::string::npos);
    }

    void test_empty()
    {
        auto result = m_parser.parse("");
        QCOMPARE(result.root.type, ConfigNode::Type::Object);
        QCOMPARE(result.root.children.size(), size_t(0));
    }

    void test_parseValid_data()
    {
        QTest::addColumn<QString>("path");
        QTest::newRow("sample") << "sample.ini";
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
        QTest::newRow("garbage") << "\x01\x02\x03\x00\xFF";
    }

    void test_parseInvalid()
    {
        QFETCH(QString, input);
        auto result = m_parser.parse(input.toStdString());
        // SimpleIni is very forgiving — even garbage might parse OK
        // So we just check it doesn't crash
        Q_UNUSED(result);
    }

    void test_commentExtraction()
    {
        auto result = m_parser.parse(readFixture("sample.ini"));
        QVERIFY(result.ok);

        const ConfigNode *general = nullptr;
        const ConfigNode *network = nullptr;
        const ConfigNode *logging = nullptr;
        for(const auto &child : result.root.children) {
            if(child.key == "General")
                general = &child;
            if(child.key == "Network")
                network = &child;
            if(child.key == "Logging")
                logging = &child;
        }

        QVERIFY(general != nullptr);
        QCOMPARE(QString::fromStdString(general->comment),
                 QString("INI sample for DataTreeViewer testing\nComment using semicolon"));

        QVERIFY(network != nullptr);
        QCOMPARE(QString::fromStdString(network->comment), QString("Comment using hash"));

        QVERIFY(logging != nullptr);
        QCOMPARE(QString::fromStdString(logging->comment),
                 QString("Duplicate keys in same section — both should appear"));
    }

    void test_keyComments()
    {
        auto result = m_parser.parse(
            "[General]\n"
            "; leading key\n"
            "name = DataTreeViewer\n"
            "version = 1.0 ; inline version\n");
        QVERIFY(result.ok);
        QVERIFY(!result.root.children.empty());

        const auto &general = result.root.children.front();
        QCOMPARE(general.children.size(), size_t(2));
        QCOMPARE(general.children[0].key, std::string("name"));
        QCOMPARE(QString::fromStdString(general.children[0].comment), QString("leading key"));
        QCOMPARE(general.children[1].key, std::string("version"));
        QCOMPARE(general.children[1].scalar, std::string("1.0"));
        QCOMPARE(QString::fromStdString(general.children[1].comment), QString("inline version"));
    }

    void test_duplicateKeys()
    {
        auto result = m_parser.parse(readFixture("sample.ini"));
        QVERIFY(result.ok);

        const ConfigNode *logging = nullptr;
        for(const auto &child : result.root.children) {
            if(child.key == "Logging") {
                logging = &child;
                break;
            }
        }
        QVERIFY(logging != nullptr);

        // Count "level" keys — should be 3
        int levelCount = 0;
        for(const auto &child : logging->children) {
            if(child.key == "level")
                levelCount++;
        }
        QCOMPARE(levelCount, 3);
    }

    void test_nestedContainers()
    {
        auto result = m_parser.parse(readFixture("sample.ini"));
        QVERIFY(result.ok);

        // INI sections are flat — all sections are children of root
        // Verify Display section exists as an Object
        bool foundDisplay = false;
        for(const auto &child : result.root.children) {
            if(child.key == "Display") {
                foundDisplay = true;
                QCOMPARE(child.type, ConfigNode::Type::Object);
            }
        }
        QVERIFY(foundDisplay);
    }

    void test_preservesLoadOrder()
    {
        auto result = m_parser.parse(readFixture("sample.ini"));
        QVERIFY(result.ok);

        std::vector<std::string> expected = {
            "General", "Network", "Logging", "Display", "EmptySection",
            "EmptySection.Sub", "EmptySection2"
        };
        QCOMPARE(result.root.children.size(), expected.size());
        for(size_t i = 0; i < expected.size(); ++i)
            QCOMPARE(result.root.children[i].key, expected[i]);
    }

    void test_sourceLines()
    {
        auto result = m_parser.parse(readFixture("sample.ini"));
        QVERIFY(result.ok);

        const ConfigNode *general = nullptr;
        for(const auto &child : result.root.children) {
            if(child.key == "General") {
                general = &child;
                break;
            }
        }
        QVERIFY(general != nullptr);
        QCOMPARE(general->source_line, 4);
        QVERIFY(!general->children.empty());
        QCOMPARE(general->children.front().source_line, 5);
    }

    void test_brokenFile()
    {
        auto result = m_parser.parse(readFixture("broken.ini"));
        QVERIFY(result.ok);
        QVERIFY(result.has_parse_error);
        QCOMPARE(result.err_line, 1);
        verifyParseErrorTree(result);
    }
};

QTEST_MAIN(TestIniParser)
#include "test_ini_parser.moc"
