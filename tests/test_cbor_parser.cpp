#include <QTest>

#include <nlohmann/json.hpp>

#include "parsers/cbor_parser.h"

class TestCborParser : public QObject {
    Q_OBJECT

private:
    CborParser m_parser;

    static std::string toBytes(const std::vector<std::uint8_t> &bytes)
    {
        return std::string(reinterpret_cast<const char *>(bytes.data()), bytes.size());
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
        QCOMPARE(m_parser.format_name(), std::string("CBOR"));
    }

    void test_libraryCredit()
    {
        QVERIFY(!m_parser.library_credit().empty());
        QVERIFY(m_parser.library_credit().find("nlohmann/json") != std::string::npos);
    }

    void test_parseObject()
    {
        nlohmann::json j = {
            {"name", "alice"},
            {"count", 42},
            {"flag", true},
            {"none", nullptr},
        };

        auto result = m_parser.parse(toBytes(nlohmann::json::to_cbor(j)));
        QVERIFY2(result.ok, result.error.c_str());
        QVERIFY(!result.has_parse_error);
        QCOMPARE(result.root.type, ConfigNode::Type::Object);

        const auto *name = findByKey(result.root, "name");
        QVERIFY(name != nullptr);
        QCOMPARE(name->type, ConfigNode::Type::String);
        QCOMPARE(name->scalar, std::string("alice"));

        const auto *count = findByKey(result.root, "count");
        QVERIFY(count != nullptr);
        QCOMPARE(count->type, ConfigNode::Type::Integer);
        QCOMPARE(count->scalar, std::string("42"));

        const auto *flag = findByKey(result.root, "flag");
        QVERIFY(flag != nullptr);
        QCOMPARE(flag->type, ConfigNode::Type::Bool);
        QCOMPARE(flag->scalar, std::string("true"));

        const auto *none = findByKey(result.root, "none");
        QVERIFY(none != nullptr);
        QCOMPARE(none->type, ConfigNode::Type::Null);
        QCOMPARE(none->scalar, std::string("null"));
    }

    void test_binaryStringMapsToHex()
    {
        nlohmann::json j = nlohmann::json::binary({0x01, 0xab, 0xff});

        auto result = m_parser.parse(toBytes(nlohmann::json::to_cbor(j)));
        QVERIFY(result.ok);
        QVERIFY(!result.has_parse_error);
        QCOMPARE(result.root.type, ConfigNode::Type::String);
        QCOMPARE(result.root.scalar, std::string("0x01abff"));
    }

    void test_nestedContainers()
    {
        nlohmann::json j = {
            {"items", nlohmann::json::array({1, 2, 3})},
            {"obj", {{"inner", "x"}}},
        };

        auto result = m_parser.parse(toBytes(nlohmann::json::to_cbor(j)));
        QVERIFY(result.ok);
        QCOMPARE(result.root.type, ConfigNode::Type::Object);

        const auto *items = findByKey(result.root, "items");
        QVERIFY(items != nullptr);
        QCOMPARE(items->type, ConfigNode::Type::Array);
        QCOMPARE(items->children.size(), size_t(3));

        const auto *obj = findByKey(result.root, "obj");
        QVERIFY(obj != nullptr);
        QCOMPARE(obj->type, ConfigNode::Type::Object);
    }

    void test_parseInvalid()
    {
        const std::string invalid("\xff", 1);
        auto result = m_parser.parse(invalid);

        QVERIFY(result.ok);
        QVERIFY(result.has_parse_error);
        QCOMPARE(result.error, std::string("Invalid CBOR data"));
        QCOMPARE(result.err_line, -1);
        verifyParseErrorTree(result);
    }
};

QTEST_MAIN(TestCborParser)
#include "test_cbor_parser.moc"
