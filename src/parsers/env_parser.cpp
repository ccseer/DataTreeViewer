#include "env_parser.h"

#include <cctype>
#include <memory>
#include <string>

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

void appendComment(std::string &target, std::string comment)
{
    if(comment.empty())
        return;
    if(!target.empty())
        target += "\n";
    target += std::move(comment);
}

std::string unescapeDoubleQuoted(std::string_view value)
{
    std::string out;
    out.reserve(value.size());

    for(size_t i = 0; i < value.size(); ++i) {
        char ch = value[i];
        if(ch == '\\' && i + 1 < value.size()) {
            char next = value[i + 1];
            switch(next) {
            case 'n':
                out.push_back('\n');
                ++i;
                continue;
            case 'r':
                out.push_back('\r');
                ++i;
                continue;
            case 't':
                out.push_back('\t');
                ++i;
                continue;
            case '\\':
                out.push_back('\\');
                ++i;
                continue;
            case '"':
                out.push_back('"');
                ++i;
                continue;
            default:
                out.push_back('\\');
                out.push_back(next);
                ++i;
                continue;
            }
        }

        out.push_back(ch);
    }

    return out;
}

size_t findInlineComment(std::string_view value)
{
    for(size_t i = 1; i < value.size(); ++i) {
        if(value[i] == '#' && std::isspace(static_cast<unsigned char>(value[i - 1])))
            return i;
    }
    return std::string_view::npos;
}

bool isValidKey(std::string_view key)
{
    if(key.empty())
        return false;

    for(char ch : key) {
        unsigned char uch = static_cast<unsigned char>(ch);
        if(std::isalnum(uch) || ch == '_')
            continue;
        return false;
    }

    return true;
}

} // namespace

ParseResult EnvParser::parse(std::string_view data)
{
    ParseResult result;

    if(data.find('\0') != std::string_view::npos) {
        result.root = dtv::core::createErrorNode("File is not valid UTF-8 text", -1);
        result.ok = true;
        result.has_parse_error = true;
        result.error = "File is not valid UTF-8 text";
        return result;
    }

    result.root.type = ConfigNode::Type::Object;
    result.ok = true;

    std::string pendingComment;
    int lineNumber = 1;
    size_t pos = 0;

    while(pos <= data.size()) {
        size_t nl = data.find('\n', pos);
        std::string_view line =
            nl == std::string_view::npos ? data.substr(pos) : data.substr(pos, nl - pos);
        if(!line.empty() && line.back() == '\r')
            line.remove_suffix(1);

        std::string trimmed = trimCopy(line);
        if(trimmed.empty()) {
            pendingComment.clear();
        } else if(trimmed[0] == '#') {
            appendComment(pendingComment, trimCopy(std::string_view(trimmed).substr(1)));
        } else {
            size_t eq = trimmed.find('=');
            if(eq != std::string::npos) {
                std::string key = trimCopy(std::string_view(trimmed).substr(0, eq));
                if(isValidKey(key)) {
                    ConfigNode node;
                    node.key = std::move(key);
                    node.source_line = lineNumber;
                    node.comment = pendingComment;
                    pendingComment.clear();

                    std::string value = trimCopy(std::string_view(trimmed).substr(eq + 1));
                    if(value.empty()) {
                        node.type = ConfigNode::Type::Null;
                        node.scalar.clear();
                    } else if(value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                        node.type = ConfigNode::Type::String;
                        node.scalar =
                            unescapeDoubleQuoted(std::string_view(value).substr(1, value.size() - 2));
                    } else if(value.size() >= 2 && value.front() == '\'' && value.back() == '\'') {
                        node.type = ConfigNode::Type::String;
                        node.scalar = value.substr(1, value.size() - 2);
                    } else {
                        size_t commentPos = findInlineComment(value);
                        if(commentPos != std::string::npos) {
                            std::string inlineComment =
                                trimCopy(std::string_view(value).substr(commentPos + 1));
                            appendComment(node.comment, std::move(inlineComment));
                            value = trimCopy(std::string_view(value).substr(0, commentPos));
                        }

                        if(value.empty()) {
                            node.type = ConfigNode::Type::Null;
                            node.scalar.clear();
                        } else {
                            node.type = ConfigNode::Type::String;
                            node.scalar = std::move(value);
                        }
                    }

                    result.root.children.push_back(std::move(node));
                }
            }
        }

        if(nl == std::string_view::npos)
            break;
        pos = nl + 1;
        ++lineNumber;
    }

    return result;
}

void EnvParser::registerSelf()
{
    auto &reg = ParserRegistry::instance();
    reg.registerParser("env", [] { return std::make_unique<EnvParser>(); });
}

REGISTER_PARSER("env", EnvParser)
