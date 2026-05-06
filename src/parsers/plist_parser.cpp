#include "plist_parser.h"

#include <memory>
#include <string>

#include <pugixml.hpp>

#include "core/parser_helpers.h"
#include "core/parser_registry.h"

namespace {

int offsetToLine(std::string_view data, ptrdiff_t offset)
{
    if(offset < 0)
        return -1;

    int line = 1;
    const size_t end = static_cast<size_t>(offset) < data.size() ? static_cast<size_t>(offset)
                                                                  : data.size();
    for(size_t i = 0; i < end; ++i) {
        if(data[i] == '\n')
            ++line;
    }
    return line;
}

pugi::xml_node nextElementSibling(pugi::xml_node node)
{
    for(pugi::xml_node current = node.next_sibling(); current; current = current.next_sibling()) {
        if(current.type() == pugi::node_element)
            return current;
    }
    return {};
}

ConfigNode convertValue(pugi::xml_node node, ParseResult &result);
ConfigNode convertValue(pugi::xml_node node, std::string_view data, ParseResult &result);

ConfigNode makeSchemaError(const std::string &message, int line, std::string key, ParseResult &result)
{
    ConfigNode node = dtv::core::createErrorNode(message, line);
    node.key = std::move(key);
    result.has_parse_error = true;
    result.error_count += 1;
    if(result.error.empty())
        result.error = message;
    if(result.err_line < 0)
        result.err_line = line;
    return node;
}

ConfigNode convertDict(pugi::xml_node node, std::string_view data, ParseResult &result)
{
    ConfigNode out;
    out.type = ConfigNode::Type::Object;

    for(pugi::xml_node child = node.first_child(); child; child = child.next_sibling()) {
        if(child.type() != pugi::node_element)
            continue;

        const std::string tag = child.name();
        const int line = offsetToLine(data, child.offset_debug());

        if(tag == "key") {
            const std::string keyText = child.child_value();
            pugi::xml_node valueNode = nextElementSibling(child);
            if(!valueNode) {
                out.children.push_back(
                    makeSchemaError("Missing value for key \"" + keyText + "\"", line, keyText, result));
                continue;
            }

            ConfigNode value = convertValue(valueNode, data, result);
            value.key = keyText;
            out.children.push_back(std::move(value));
            child = valueNode;
            continue;
        }

        out.children.push_back(
            makeSchemaError("Value element without preceding <key>", line, "<unpaired>", result));
    }

    return out;
}

ConfigNode convertArray(pugi::xml_node node, std::string_view data, ParseResult &result)
{
    ConfigNode out;
    out.type = ConfigNode::Type::Array;

    for(const auto &child : node.children()) {
        if(child.type() != pugi::node_element)
            continue;
        out.children.push_back(convertValue(child, data, result));
    }

    return out;
}

ConfigNode convertValue(pugi::xml_node node, std::string_view data, ParseResult &result)
{
    ConfigNode out;
    const std::string tag = node.name();

    if(tag == "string") {
        out.type = ConfigNode::Type::String;
        out.scalar = node.child_value();
    } else if(tag == "integer") {
        out.type = ConfigNode::Type::Integer;
        out.scalar = node.child_value();
    } else if(tag == "real") {
        out.type = ConfigNode::Type::Float;
        out.scalar = node.child_value();
    } else if(tag == "true") {
        out.type = ConfigNode::Type::Bool;
        out.scalar = "true";
    } else if(tag == "false") {
        out.type = ConfigNode::Type::Bool;
        out.scalar = "false";
    } else if(tag == "date" || tag == "data") {
        out.type = ConfigNode::Type::String;
        out.scalar = node.child_value();
    } else if(tag == "dict") {
        out = convertDict(node, data, result);
    } else if(tag == "array") {
        out = convertArray(node, data, result);
    } else {
        out.type = ConfigNode::Type::String;
        out.scalar = tag + " (unsupported)";
        result.has_parse_error = true;
        result.error_count += 1;
        if(result.error.empty())
            result.error = "Unsupported plist element: " + tag;
    }

    return out;
}

} // namespace

ParseResult PlistParser::parse(std::string_view data)
{
    ParseResult result;

    if(data.size() >= 8 && data.substr(0, 8) == "bplist00") {
        result.root = dtv::core::createErrorNode(
            "Binary plist is not supported. Convert with: plutil -convert xml1 file.plist", -1);
        result.ok = true;
        result.has_parse_error = true;
        result.error = "Binary plist is not supported";
        return result;
    }

    pugi::xml_document doc;
    const auto parseResult = doc.load_buffer(data.data(), data.size(), pugi::parse_default);
    if(!parseResult) {
        const int line = offsetToLine(data, parseResult.offset);
        result.root = dtv::core::createErrorNode(parseResult.description(), line);
        result.ok = true;
        result.has_parse_error = true;
        result.error = parseResult.description();
        result.err_line = line;
        return result;
    }

    pugi::xml_node plistNode = doc.child("plist");
    if(!plistNode) {
        result.root = dtv::core::createErrorNode("Empty plist", -1);
        result.ok = true;
        result.has_parse_error = true;
        result.error = "Empty plist";
        return result;
    }

    pugi::xml_node rootValue;
    for(const auto &child : plistNode.children()) {
        if(child.type() == pugi::node_element) {
            rootValue = child;
            break;
        }
    }
    if(!rootValue) {
        result.root = dtv::core::createErrorNode("Empty plist", -1);
        result.ok = true;
        result.has_parse_error = true;
        result.error = "Empty plist";
        return result;
    }

    result.root = convertValue(rootValue, data, result);
    result.root.comment = std::string("plist version=\"") + plistNode.attribute("version").value() + "\"";
    result.ok = true;
    return result;
}

void PlistParser::registerSelf()
{
    auto &reg = ParserRegistry::instance();
    reg.registerParser("plist", [] { return std::make_unique<PlistParser>(); });
}

REGISTER_PARSER("plist", PlistParser)
