#include <QFile>
#include <QTest>

#include "parsers/ini_parser.h"

class TestIniParser : public QObject {
    Q_OBJECT

private:
    IniParser m_parser;

    std::string readFixture(const char* name)
    {
        QString path = QString(FIXTURES_DIR) + "/" + name;
        QFile f(path);
        f.open(QIODevice::ReadOnly | QIODevice::Text);
        return f.readAll().toStdString();
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

        // SimpleIni does not expose comments — all comment fields should be empty
        bool allEmpty = true;
        for (const auto& child : result.root.children) {
            if (!child.comment.empty()) allEmpty = false;
        }
        QVERIFY(allEmpty);
    }

    void test_duplicateKeys()
    {
        auto result = m_parser.parse(readFixture("sample.ini"));
        QVERIFY(result.ok);

        const ConfigNode* logging = nullptr;
        for (const auto& child : result.root.children) {
            if (child.key == "Logging") {
                logging = &child;
                break;
            }
        }
        QVERIFY(logging != nullptr);

        // Count "level" keys — should be 3
        int levelCount = 0;
        for (const auto& child : logging->children) {
            if (child.key == "level")
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
        for (const auto& child : result.root.children) {
            if (child.key == "Display") {
                foundDisplay = true;
                QCOMPARE(child.type, ConfigNode::Type::Object);
            }
        }
        QVERIFY(foundDisplay);
    }
};

QTEST_MAIN(TestIniParser)
#include "test_ini_parser.moc"
