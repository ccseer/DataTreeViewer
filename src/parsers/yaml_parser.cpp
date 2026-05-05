#include "yaml_parser.h"

#include <ryml.hpp>

#include <stdexcept>
#include <iostream>
#include <utility>

#include "core/parser_registry.h"
#include "core/parser_helpers.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

void traceYaml(const std::string &message)
{
    const std::string line = "[YamlParser] " + message;
#ifdef _WIN32
    OutputDebugStringA((line + "\n").c_str());
#endif
    std::clog << line << std::endl;
}

struct YamlParseException : public std::runtime_error {
    YamlParseException(std::string message, int errorLine)
        : std::runtime_error(std::move(message)),
          line(errorLine)
    {
    }

    int line = -1;
};

[[noreturn]] void yamlErrorCallback(const char* msg, size_t msgLen, ryml::Location location,
                                    void*)
{
    throw YamlParseException(std::string(msg, msgLen),
                             location.line > 0 ? static_cast<int>(location.line) : -1);
}

ryml::Callbacks yamlCallbacks()
{
    return ryml::Callbacks(nullptr, nullptr, nullptr, yamlErrorCallback);
}

ConfigNode::Type deduceScalarType(c4::csubstr val)
{
    if (val.empty())
        return ConfigNode::Type::String;

    if (val == "null" || val == "~" || val == "NULL" || val == "Null")
        return ConfigNode::Type::Null;

    if (val == "true" || val == "True" || val == "TRUE" ||
        val == "false" || val == "False" || val == "FALSE")
        return ConfigNode::Type::Bool;

    {
        const char* p = val.data();
        const char* end = p + val.size();
        if (p < end && (*p == '-' || *p == '+')) p++;
        if (p < end) {
            bool allDigits = true;
            for (; p < end; ++p) {
                if (*p < '0' || *p > '9') { allDigits = false; break; }
            }
            if (allDigits) return ConfigNode::Type::Integer;
        }
    }

    {
        bool hasDot = false;
        auto dotPos = val.find('.');
        if (dotPos != c4::csubstr::npos) {
            bool digitBefore = (dotPos > 0 && val[dotPos - 1] >= '0' && val[dotPos - 1] <= '9');
            bool digitAfter  = (dotPos + 1 < val.size() && val[dotPos + 1] >= '0' && val[dotPos + 1] <= '9');
            hasDot = digitBefore || digitAfter;
        }
        bool hasExp = false;
        auto epos = val.find('e');
        if (epos == c4::csubstr::npos) epos = val.find('E');
        if (epos != c4::csubstr::npos && epos > 0 && epos + 1 < val.size()) {
            char prev = val[epos - 1];
            char next = val[epos + 1];
            if ((prev >= '0' && prev <= '9') &&
                ((next >= '0' && next <= '9') || next == '+' || next == '-')) {
                hasExp = true;
            }
        }
        if (hasDot || hasExp) return ConfigNode::Type::Float;
    }

    return ConfigNode::Type::String;
}

int resolveLine(const ryml::Parser &parser, ryml::ConstNodeRef node)
{
    ryml::Location loc = parser.location(node);
    return loc ? static_cast<int>(loc.line) + 1 : -1;
}

void walk(ryml::ConstNodeRef node, const ryml::Parser &parser, ConfigNode& out)
{
    out.source_line = resolveLine(parser, node);

    if (node.is_map()) {
        out.type = ConfigNode::Type::Object;
        for (auto child : node.children()) {
            ConfigNode childNode;
            if (child.has_key()) {
                auto k = child.key();
                childNode.key = std::string(k.data(), k.size());
            }
            walk(child, parser, childNode);
            out.children.push_back(std::move(childNode));
        }
    } else if (node.is_seq()) {
        out.type = ConfigNode::Type::Array;
        for (auto child : node.children()) {
            ConfigNode childNode;
            walk(child, parser, childNode);
            out.children.push_back(std::move(childNode));
        }
    } else if (node.has_val()) {
        auto v = node.val();
        out.type = node.is_val_quoted() ? ConfigNode::Type::String : deduceScalarType(v);
        out.scalar = std::string(v.data(), v.size());
    } else {
        out.type = ConfigNode::Type::Null;
    }
}

} // namespace

ParseResult YamlParser::parse(std::string_view data)
{
    ParseResult result;

    try {
        traceYaml("parse enter, bytes=" + std::to_string(data.size()));
        c4::csubstr yaml(data.data(), data.size());
        ryml::Parser::handler_type eventHandler(yamlCallbacks());
        ryml::ParserOptions parserOptions;
        parserOptions.locations(true);
        ryml::Parser parser(&eventHandler, parserOptions);
        ryml::Tree tree(parser.callbacks());
        ryml::parse_in_arena(&parser, yaml, &tree);
        traceYaml("rapidyaml parse returned, empty=" + std::to_string(tree.empty()));

        if (tree.empty()) {
            result.ok = true;
            result.root.type = ConfigNode::Type::Null;
            return result;
        }
        traceYaml("root children=" + std::to_string(tree.rootref().num_children()));

        walk(tree.rootref(), parser, result.root);
        traceYaml("walk complete, children=" + std::to_string(result.root.children.size()));

        auto commentMap = dtv::core::extractComments(data);
        dtv::core::applyComments(result.root, commentMap);
        traceYaml("comments applied");

        result.ok = true;
        traceYaml("parse leave ok");
    } catch (const YamlParseException& e) {
        traceYaml("rapidyaml parse error: " + std::string(e.what()));
        result.root = dtv::core::createErrorNode(e.what(), e.line);
        result.ok   = true;
        result.has_parse_error = true;
        result.error = e.what();
        result.err_line = e.line;
    } catch (const std::exception& e) {
        traceYaml("std::exception: " + std::string(e.what()));
        result.root = dtv::core::createErrorNode(e.what());
        result.ok   = true;
        result.has_parse_error = true;
        result.error = e.what();
    }

    return result;
}

void YamlParser::registerSelf()
{
    auto& reg = ParserRegistry::instance();
    reg.registerParser("yaml", [] { return std::make_unique<YamlParser>(); });
    reg.registerParser("yml",  [] { return std::make_unique<YamlParser>(); });
}

REGISTER_PARSER("yaml", YamlParser)
REGISTER_PARSER("yml",  YamlParser)
