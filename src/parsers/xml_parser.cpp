#include "xml_parser.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <vector>

#include <pugixml.hpp>

#include "core/parser_helpers.h"
#include "core/parser_registry.h"

namespace {

std::string trimCopy(std::string_view value)
{
    size_t first = 0;
    while(first < value.size() && std::isspace(static_cast<unsigned char>(value[first])))
        ++first;

    size_t last = value.size();
    while(last > first && std::isspace(static_cast<unsigned char>(value[last - 1])))
        --last;

    return std::string(value.substr(first, last - first));
}

bool hasNonWhitespace(std::string_view text)
{
    for(char ch : text) {
        if(!std::isspace(static_cast<unsigned char>(ch)))
            return true;
    }
    return false;
}

std::string joinComment(std::string existing, std::string_view next)
{
    std::string trimmed = trimCopy(next);
    if(trimmed.empty())
        return existing;
    if(existing.empty())
        return trimmed;
    existing += "\n";
    existing += trimmed;
    return existing;
}

std::vector<size_t> buildLineOffsets(std::string_view data)
{
    std::vector<size_t> offsets;
    for(size_t i = 0; i < data.size(); ++i) {
        if(data[i] == '\n')
            offsets.push_back(i);
    }
    return offsets;
}

int offsetToLine(const std::vector<size_t> &offsets, size_t offset)
{
    return static_cast<int>(
               std::upper_bound(offsets.begin(), offsets.end(), offset) - offsets.begin()) +
           1;
}

int nodeLine(const std::vector<size_t> &offsets, const pugi::xml_node &node)
{
    const ptrdiff_t offset = node.offset_debug();
    if(offset < 0)
        return -1;
    return offsetToLine(offsets, static_cast<size_t>(offset));
}

ConfigNode convertElement(const pugi::xml_node &node, const std::vector<size_t> &offsets);

void appendContainerChildren(ConfigNode &out, const pugi::xml_node &node,
                             const std::vector<size_t> &offsets)
{
    for(const auto &attr : node.attributes()) {
        ConfigNode attrNode;
        attrNode.key = "@" + std::string(attr.name());
        attrNode.type = ConfigNode::Type::String;
        attrNode.scalar = attr.value();
        attrNode.source_line = out.source_line;
        out.children.push_back(std::move(attrNode));
    }

    std::string pendingComment;
    for(const auto &child : node.children()) {
        switch(child.type()) {
        case pugi::node_comment:
            pendingComment = joinComment(std::move(pendingComment), child.value());
            break;
        case pugi::node_element: {
            ConfigNode childNode = convertElement(child, offsets);
            if(!pendingComment.empty()) {
                childNode.comment = std::move(pendingComment);
                pendingComment.clear();
            }
            out.children.push_back(std::move(childNode));
            break;
        }
        case pugi::node_pcdata:
        case pugi::node_cdata:
            if(hasNonWhitespace(child.value())) {
                ConfigNode textNode;
                textNode.key = "#text";
                textNode.type = ConfigNode::Type::String;
                textNode.scalar = child.value();
                textNode.source_line = out.source_line;
                out.children.push_back(std::move(textNode));
            }
            break;
        default:
            break;
        }
    }

    if(!pendingComment.empty() && out.comment.empty())
        out.comment = std::move(pendingComment);
}

ConfigNode convertElement(const pugi::xml_node &node, const std::vector<size_t> &offsets)
{
    ConfigNode out;
    out.key = node.name();
    out.source_line = nodeLine(offsets, node);

    const bool hasAttributes = node.first_attribute();

    bool hasElementChildren = false;
    bool hasText = false;
    std::string textValue;
    for(const auto &child : node.children()) {
        if(child.type() == pugi::node_element)
            hasElementChildren = true;
        if(child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata) {
            if(hasNonWhitespace(child.value()))
                hasText = true;
            textValue += child.value();
        }
    }

    if(!hasAttributes && !hasElementChildren) {
        if(hasText) {
            out.type = ConfigNode::Type::String;
            out.scalar = textValue;
        } else {
            out.type = ConfigNode::Type::Null;
            out.scalar.clear();
        }
        return out;
    }

    out.type = ConfigNode::Type::Object;
    appendContainerChildren(out, node, offsets);
    return out;
}

} // namespace

ParseResult XmlParser::parse(std::string_view data)
{
    ParseResult result;

    pugi::xml_document doc;
    const unsigned int flags = pugi::parse_default | pugi::parse_comments;
    const auto parseResult = doc.load_buffer(data.data(), data.size(), flags);
    if(!parseResult) {
        const int line = offsetToLine(buildLineOffsets(data), parseResult.offset);
        result.root = dtv::core::createErrorNode(parseResult.description(), line);
        result.ok = true;
        result.has_parse_error = true;
        result.error = parseResult.description();
        result.err_line = line;
        return result;
    }

    result.root.type = ConfigNode::Type::Object;
    result.ok = true;

    const std::vector<size_t> offsets = buildLineOffsets(data);
    std::string pendingTopComment;

    for(const auto &child : doc.children()) {
        if(child.type() == pugi::node_comment) {
            pendingTopComment = joinComment(std::move(pendingTopComment), child.value());
            continue;
        }
        if(child.type() != pugi::node_element)
            continue;

        ConfigNode childNode = convertElement(child, offsets);
        if(!pendingTopComment.empty()) {
            childNode.comment = std::move(pendingTopComment);
            pendingTopComment.clear();
        }
        result.root.children.push_back(std::move(childNode));
    }

    return result;
}

void XmlParser::registerSelf()
{
    auto &reg = ParserRegistry::instance();
    reg.registerParser("xml", [] { return std::make_unique<XmlParser>(); });
    reg.registerParser("svg", [] { return std::make_unique<XmlParser>(); });
    reg.registerParser("xhtml", [] { return std::make_unique<XmlParser>(); });
}

REGISTER_PARSER("xml", XmlParser)
REGISTER_PARSER("svg", XmlParser)
REGISTER_PARSER("xhtml", XmlParser)
