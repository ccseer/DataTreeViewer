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
    }

    void test_parseValid()
    {
        QFETCH(QString, path);
        auto result = m_parser.parse(readFixture(path.toUtf8().constData()));
        QVERIFY2(result.ok, result.error.c_str());
        QVERIFY(result.root.type == ConfigNode::Type::Object);
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
        QVERIFY(!result.ok);
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

    void test_brokenFile()
    {
        auto result = m_parser.parse(readFixture("broken.yaml"));
        QVERIFY(!result.ok);
    }
};

QTEST_MAIN(TestYamlParser)
#include "test_yaml_parser.moc"
