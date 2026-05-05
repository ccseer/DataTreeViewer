#include "toml_parser.h"

#include <toml++/toml.hpp>

#include "core/parser_registry.h"
#include "core/parser_helpers.h"

namespace {

ConfigNode walk(const toml::node& n)
{
    ConfigNode node;

    if (const auto* tbl = n.as_table()) {
        node.type = ConfigNode::Type::Object;
        node.children.reserve(tbl->size());
        if (tbl->is_inline())
            node.source_line = static_cast<int>(n.source().begin.line);
        for (auto&& [k, v] : *tbl) {
            ConfigNode child = walk(v);
            child.key = std::string(k);
            if (child.source_line < 0 && n.source().begin.line > 0)
                child.source_line = static_cast<int>(n.source().begin.line);
            node.children.push_back(std::move(child));
        }
    } else if (const auto* arr = n.as_array()) {
        node.type = ConfigNode::Type::Array;
        node.children.reserve(arr->size());
        for (auto&& elem : *arr) {
            ConfigNode child = walk(elem);
            if (child.source_line < 0 && n.source().begin.line > 0)
                child.source_line = static_cast<int>(n.source().begin.line);
            node.children.push_back(std::move(child));
        }
    } else if (const auto* str = n.as_string()) {
        node.type   = ConfigNode::Type::String;
        node.scalar = str->get();
    } else if (const auto* intVal = n.as_integer()) {
        node.type   = ConfigNode::Type::Integer;
        node.scalar = std::to_string(intVal->get());
    } else if (const auto* floatVal = n.as_floating_point()) {
        node.type   = ConfigNode::Type::Float;
        node.scalar = std::to_string(floatVal->get());
    } else if (const auto* boolVal = n.as_boolean()) {
        node.type   = ConfigNode::Type::Bool;
        node.scalar = boolVal->get() ? "true" : "false";
    } else if (const auto* dt = n.as_date_time()) {
        node.type = ConfigNode::Type::String;
        std::ostringstream oss;
        oss << *dt;
        node.scalar = oss.str();
    } else if (n.is_date()) {
        node.type = ConfigNode::Type::String;
        std::ostringstream oss;
        oss << *n.as_date();
        node.scalar = oss.str();
    } else if (n.is_time()) {
        node.type = ConfigNode::Type::String;
        std::ostringstream oss;
        oss << *n.as_time();
        node.scalar = oss.str();
    } else {
        node.type   = ConfigNode::Type::Null;
        node.scalar = "null";
    }

    if (n.source().begin.line > 0)
        node.source_line = static_cast<int>(n.source().begin.line);
    else if (n.source().end.line > 0)
        node.source_line = static_cast<int>(n.source().end.line);

    return node;
}

} // namespace

std::string TomlParser::library_credit() const
{
    return "toml++ "
           + std::to_string(TOML_LIB_MAJOR) + "."
           + std::to_string(TOML_LIB_MINOR) + "."
           + std::to_string(TOML_LIB_PATCH);
}

ParseResult TomlParser::parse(std::string_view data)
{
    ParseResult result;

#if TOML_EXCEPTIONS
    try {
        auto tbl = toml::parse(data);
        result.root = walk(tbl);

        auto commentMap = dtv::core::extractComments(data);
        dtv::core::applyComments(result.root, commentMap);

        result.ok   = true;
    } catch (const toml::parse_error& e) {
        result.root = dtv::core::createErrorNode(e.description(), static_cast<int>(e.source().begin.line));
        result.ok   = true;
        result.has_parse_error = true;
        result.error = e.description();
    } catch (const std::exception& e) {
        result.root = dtv::core::createErrorNode(e.what());
        result.ok    = true;
        result.has_parse_error = true;
        result.error = e.what();
    }
#else
    auto parseRes = toml::parse(data);
    if (parseRes) {
        result.root = walk(parseRes.table());
        result.ok   = true;
    } else {
        result.root = dtv::core::createErrorNode(std::string(parseRes.error().description()), static_cast<int>(parseRes.error().source().begin.line));
        result.ok   = true;
        result.has_parse_error = true;
        result.error = std::string(parseRes.error().description());
    }
#endif

    return result;
}

void TomlParser::registerSelf()
{
    auto& reg = ParserRegistry::instance();
    reg.registerParser("toml", [] { return std::make_unique<TomlParser>(); });
    reg.registerParser("tml",  [] { return std::make_unique<TomlParser>(); });
}

REGISTER_PARSER("toml", TomlParser)
REGISTER_PARSER("tml",  TomlParser)
