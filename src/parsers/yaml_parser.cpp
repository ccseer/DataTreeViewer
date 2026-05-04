#include "yaml_parser.h"

#include <ryml.hpp>

#include "core/parser_registry.h"

namespace {

ConfigNode::Type deduceScalarType(c4::csubstr val)
{
    if (val.empty())
        return ConfigNode::Type::String;

    // Null
    if (val == "null" || val == "~" || val == "NULL" || val == "Null")
        return ConfigNode::Type::Null;

    // Bool — YAML 1.2: only true/false (yes/no/on/off removed in 1.2)
    if (val == "true" || val == "True" || val == "TRUE" ||
        val == "false" || val == "False" || val == "FALSE")
        return ConfigNode::Type::Bool;

    // Integer: optional sign then all digits
    {
        const char* p = val.data();
        const char* end = p + val.size();
        if (*p == '-' || *p == '+') p++;
        if (p < end) {
            bool allDigits = true;
            for (; p < end; ++p) {
                if (*p < '0' || *p > '9') { allDigits = false; break; }
            }
            if (allDigits) return ConfigNode::Type::Integer;
        }
    }

    // Float: contains '.' with digit context, or scientific notation (e/E preceded by digit)
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

void walk(ryml::ConstNodeRef node, ConfigNode& out)
{
    if (node.is_map()) {
        out.type = ConfigNode::Type::Object;
        for (auto child : node.children()) {
            ConfigNode childNode;
            if (child.has_key()) {
                auto k = child.key();
                childNode.key = std::string(k.data(), k.size());
            }
            walk(child, childNode);
            out.children.push_back(std::move(childNode));
        }
    } else if (node.is_seq()) {
        out.type = ConfigNode::Type::Array;
        for (auto child : node.children()) {
            ConfigNode childNode;
            walk(child, childNode);
            out.children.push_back(std::move(childNode));
        }
    } else if (node.has_val()) {
        auto v = node.val();
        out.type = deduceScalarType(v);
        out.scalar = std::string(v.data(), v.size());
    } else {
        out.type = ConfigNode::Type::Null;
    }
    // Note: rapidyaml v0.7.2 does not expose comments
}

} // namespace

ParseResult YamlParser::parse(std::string_view data)
{
    ParseResult result;

    try {
        c4::csubstr yaml(data.data(), data.size());
        ryml::Tree tree = ryml::parse_in_arena(yaml);

        if (tree.empty()) {
            result.ok = true;
            result.root.type = ConfigNode::Type::Null;
            return result;
        }

        walk(tree.rootref(), result.root);
        result.ok = true;
    } catch (const std::exception& e) {
        result.ok       = false;
        result.error    = e.what();
        result.err_line = -1;  // ryml exception message includes line info in text
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
