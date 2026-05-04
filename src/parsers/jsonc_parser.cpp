#include "jsonc_parser.h"

#include <nlohmann/json.hpp>

#include "core/parser_registry.h"

namespace {

ConfigNode walk(const nlohmann::json& j)
{
    ConfigNode node;

    switch (j.type()) {
    case nlohmann::json::value_t::object:
        node.type = ConfigNode::Type::Object;
        node.children.reserve(j.size());
        for (auto it = j.begin(); it != j.end(); ++it) {
            ConfigNode child = walk(it.value());
            child.key = it.key();
            node.children.push_back(std::move(child));
        }
        break;

    case nlohmann::json::value_t::array:
        node.type = ConfigNode::Type::Array;
        node.children.reserve(j.size());
        for (const auto& elem : j)
            node.children.push_back(walk(elem));
        break;

    case nlohmann::json::value_t::string:
        node.type   = ConfigNode::Type::String;
        node.scalar = j.get<std::string>();
        break;

    case nlohmann::json::value_t::number_integer:
    case nlohmann::json::value_t::number_unsigned:
        node.type   = ConfigNode::Type::Integer;
        node.scalar = j.dump();
        break;

    case nlohmann::json::value_t::number_float:
        node.type   = ConfigNode::Type::Float;
        node.scalar = j.dump();
        break;

    case nlohmann::json::value_t::boolean:
        node.type   = ConfigNode::Type::Bool;
        node.scalar = j.get<bool>() ? "true" : "false";
        break;

    case nlohmann::json::value_t::null:
        node.type   = ConfigNode::Type::Null;
        node.scalar = "null";
        break;

    case nlohmann::json::value_t::discarded:
    case nlohmann::json::value_t::binary:
        node.type   = ConfigNode::Type::Null;
        break;
    }

    return node;
}

} // namespace

std::string JsoncParser::library_credit() const
{
    return "nlohmann/json "
           + std::to_string(NLOHMANN_JSON_VERSION_MAJOR) + "."
           + std::to_string(NLOHMANN_JSON_VERSION_MINOR) + "."
           + std::to_string(NLOHMANN_JSON_VERSION_PATCH);
}

ParseResult JsoncParser::parse(std::string_view data)
{
    ParseResult result;

    try {
        auto j = nlohmann::json::parse(data, nullptr, true, true);
        result.root = walk(j);
        result.ok   = true;
    } catch (const nlohmann::json::parse_error& e) {
        result.ok       = false;
        result.error    = e.what();
        result.err_line = static_cast<int>(e.byte);
    }

    return result;
}

void JsoncParser::registerSelf()
{
    auto& reg = ParserRegistry::instance();
    reg.registerParser("json",  [] { return std::make_unique<JsoncParser>(); });
    reg.registerParser("jsonc", [] { return std::make_unique<JsoncParser>(); });
    reg.registerParser("json5", [] { return std::make_unique<JsoncParser>(); });
}

REGISTER_PARSER("json",  JsoncParser)
REGISTER_PARSER("jsonc", JsoncParser)
REGISTER_PARSER("json5", JsoncParser)
