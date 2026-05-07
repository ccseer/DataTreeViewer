#include "jsonc_parser.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <queue>
#include <string_view>

#include "core/parser_registry.h"
#include "core/parser_helpers.h"
#include "parsers/nlohmann_convert.h"

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

void traceJsonc(const std::string &message)
{
    const std::string line = "[JsoncParser] " + message;
#ifdef _WIN32
    OutputDebugStringA((line + "\n").c_str());
#endif
    std::clog << line << std::endl;
}

struct CommentEntry {
    std::string key;
    std::string comment;
};

void trim(std::string &s)
{
    auto it1 = std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    });
    s.erase(s.begin(), it1);
    auto it2 = std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    });
    s.erase(it2.base(), s.end());
}

size_t findCommentStart(std::string_view line, size_t from = 0)
{
    bool inString = false;
    bool escaped = false;

    for(size_t i = from; i < line.size(); ++i) {
        char c = line[i];

        if(inString) {
            if(escaped) {
                escaped = false;
            } else if(c == '\\') {
                escaped = true;
            } else if(c == '"') {
                inString = false;
            }
            continue;
        }

        if(c == '"') {
            inString = true;
            continue;
        }

        if(c == '#')
            return i;
        if(c == '/' && i + 1 < line.size() && (line[i + 1] == '/' || line[i + 1] == '*'))
            return i;
    }

    return std::string_view::npos;
}

bool findJsonKey(std::string_view line, std::string &key, size_t &valueStart)
{
    bool escaped = false;
    bool inKey = false;
    size_t keyStart = 0;
    size_t keyEnd = 0;

    for(size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        if(inKey) {
            if(escaped) {
                escaped = false;
            } else if(c == '\\') {
                escaped = true;
            } else if(c == '"') {
                keyEnd = i;
                inKey = false;
                size_t j = i + 1;
                while(j < line.size() && std::isspace(static_cast<unsigned char>(line[j])))
                    ++j;
                if(j < line.size() && line[j] == ':') {
                    key = std::string(line.substr(keyStart, keyEnd - keyStart));
                    valueStart = j + 1;
                    return true;
                }
            }
            continue;
        }

        if(c == '"') {
            inKey = true;
            keyStart = i + 1;
        } else if(c == '/' && i + 1 < line.size() && (line[i + 1] == '/' || line[i + 1] == '*')) {
            return false;
        }
    }

    return false;
}

std::string commentTextFrom(std::string_view line, size_t commentStart)
{
    if(commentStart == std::string_view::npos)
        return {};

    size_t textStart = commentStart;
    if(line[commentStart] == '#') {
        textStart = commentStart + 1;
    } else if(commentStart + 1 < line.size() &&
              (line[commentStart + 1] == '/' || line[commentStart + 1] == '*')) {
        textStart = commentStart + 2;
    }

    std::string text(line.substr(textStart));
    size_t blockEnd = text.find("*/");
    if(blockEnd != std::string::npos)
        text.erase(blockEnd);
    trim(text);
    return text;
}

std::queue<CommentEntry> collectComments(std::string_view data)
{
    std::queue<CommentEntry> comments;
    std::string pendingComment;
    std::string blockComment;
    bool inBlockComment = false;

    size_t pos = 0;
    while(pos <= data.size()) {
        size_t nl = data.find('\n', pos);
        std::string_view line =
            nl == std::string_view::npos ? data.substr(pos) : data.substr(pos, nl - pos);
        if(!line.empty() && line.back() == '\r')
            line.remove_suffix(1);

        if(inBlockComment) {
            size_t end = line.find("*/");
            if(end == std::string_view::npos) {
                blockComment += "\n" + std::string(line);
            } else {
                blockComment += "\n" + std::string(line.substr(0, end));
                trim(blockComment);
                if(!blockComment.empty())
                    pendingComment = blockComment;
                blockComment.clear();
                inBlockComment = false;
            }

            if(nl == std::string_view::npos)
                break;
            pos = nl + 1;
            continue;
        }

        size_t first = line.find_first_not_of(" \t");
        if(first != std::string_view::npos) {
            std::string_view content = line.substr(first);
            if(content.rfind("//", 0) == 0 || content.rfind("#", 0) == 0 ||
               content.rfind("/*", 0) == 0) {
                if(content.rfind("/*", 0) == 0 && content.find("*/", 2) == std::string_view::npos) {
                    blockComment = std::string(content.substr(2));
                    inBlockComment = true;
                } else {
                    std::string text = commentTextFrom(content, 0);
                    if(!text.empty())
                        pendingComment = std::move(text);
                }
            } else {
                std::string key;
                size_t valueStart = 0;
                if(findJsonKey(line, key, valueStart)) {
                    size_t inlineComment = findCommentStart(line, valueStart);
                    std::string text = commentTextFrom(line, inlineComment);
                    if(!text.empty()) {
                        comments.push({std::move(key), std::move(text)});
                    } else if(!pendingComment.empty()) {
                        comments.push({std::move(key), pendingComment});
                        pendingComment.clear();
                    }
                } else {
                    size_t inlineComment = findCommentStart(line);
                    std::string text = commentTextFrom(line, inlineComment);
                    if(!text.empty()) {
                        comments.push({"", std::move(text)});
                    } else if(!pendingComment.empty()) {
                        comments.push({"", pendingComment});
                        pendingComment.clear();
                    }
                }
            }
        }

        if(nl == std::string_view::npos)
            break;
        pos = nl + 1;
    }

    return comments;
}

int estimateErrorLine(std::string_view data)
{
    int line = 1;
    bool inString = false;
    bool escaped = false;
    char previousSignificant = 0;

    for(char c : data) {
        if(c == '\n') {
            ++line;
            continue;
        }

        if(inString) {
            if(escaped) {
                escaped = false;
            } else if(c == '\\') {
                escaped = true;
            } else if(c == '"') {
                inString = false;
            }
            continue;
        }

        if(c == '"') {
            inString = true;
            previousSignificant = 'x';
            continue;
        }

        if(std::isspace(static_cast<unsigned char>(c)))
            continue;

        if((c == ',' && previousSignificant == ',') ||
           ((c == '}' || c == ']') && previousSignificant == ',')) {
            return line;
        }

        previousSignificant = c;
    }

    return line;
}

std::string stripComments(const std::string_view &data)
{
    std::string clean;
    clean.reserve(data.size());

    bool in_str = false;
    bool in_block = false;
    bool in_line = false;

    for(size_t i = 0; i < data.size(); ++i) {
        char c = data[i];
        char next = (i + 1 < data.size()) ? data[i + 1] : 0;

        if(in_block) {
            if(c == '*' && next == '/') {
                in_block = false;
                i++;
                clean.append("  ");
            } else {
                if(c == '\n')
                    clean.push_back('\n');
                else
                    clean.push_back(' ');
            }
            continue;
        }

        if(in_line) {
            if(c == '\n') {
                in_line = false;
                clean.push_back('\n');
            } else {
                clean.push_back(' ');
            }
            continue;
        }

        if(in_str) {
            if(c == '"') {
                bool escaped = false;
                size_t k = i;
                while(k > 0 && data[k - 1] == '\\') {
                    escaped = !escaped;
                    k--;
                }
                if(!escaped)
                    in_str = false;
            }
            clean.push_back(c);
            continue;
        }

        if(c == '"') {
            in_str = true;
            clean.push_back(c);
            continue;
        }

        if(c == '/' && next == '/') {
            in_line = true;
            i++;
            clean.append("  ");
            continue;
        }

        if(c == '/' && next == '*') {
            in_block = true;
            i++;
            clean.append("  ");
            continue;
        }

        if(c == '#') {
            in_line = true;
            clean.push_back(' ');
            continue;
        }

        clean.push_back(c);
    }

    return clean;
}

void annotateComments(ConfigNode &node, std::queue<CommentEntry> &comments)
{
    if(comments.empty())
        return;
    for(auto &child : node.children) {
        if(!comments.empty()) {
            if((!child.key.empty() && comments.front().key == child.key) ||
               (child.key.empty() && comments.front().key.empty())) {
                child.comment = comments.front().comment;
                comments.pop();
            }
        }
        annotateComments(child, comments);
    }
}

} // namespace

ParseResult JsoncParser::parse(std::string_view data)
{
    ParseResult result;
    std::queue<CommentEntry> comments;

    traceJsonc("parse enter, bytes=" + std::to_string(data.size()));

    try {
        comments = collectComments(data);
        std::string clean = stripComments(data);
        traceJsonc("comments stripped, clean bytes=" + std::to_string(clean.size()) +
                   ", pending comments=" + std::to_string(comments.size()));

        auto j = nlohmann::ordered_json::parse(clean, nullptr, false, true);
        if(j.is_discarded()) {
            result.err_line = estimateErrorLine(clean);
            result.root = dtv::core::createErrorNode("Invalid JSONC syntax", result.err_line);
            result.ok = true;
            result.has_parse_error = true;
            result.error = "Invalid JSONC syntax";
            traceJsonc("parse failed without exception, estimated line=" +
                       std::to_string(result.err_line));
            return result;
        }

        traceJsonc("nlohmann parse ok, walking tree");
        result.root = dtv::core::convertNlohmann(j);
        traceJsonc("walk complete, annotating comments");
        annotateComments(result.root, comments);
        result.ok = true;
        traceJsonc("parse leave ok");
    } catch(const std::exception &e) {
        traceJsonc(std::string("std::exception: ") + e.what());
        result.root = dtv::core::createErrorNode(e.what());
        result.ok = true;
        result.has_parse_error = true;
        result.error = e.what();
    } catch(...) {
        traceJsonc("unknown exception");
        result.root = dtv::core::createErrorNode("Unknown fatal error");
        result.ok = true;
        result.has_parse_error = true;
        result.error = "Unknown fatal error";
    }

    return result;
}

std::string JsoncParser::library_credit() const
{
    return "nlohmann/json " + std::to_string(NLOHMANN_JSON_VERSION_MAJOR) + "." +
           std::to_string(NLOHMANN_JSON_VERSION_MINOR) + "." +
           std::to_string(NLOHMANN_JSON_VERSION_PATCH);
}

void JsoncParser::registerSelf()
{
    auto &reg = ParserRegistry::instance();
    reg.registerParser("json", [] {
        return std::make_unique<JsoncParser>();
    });
    reg.registerParser("jsonc", [] {
        return std::make_unique<JsoncParser>();
    });
}

REGISTER_PARSER("json", JsoncParser)
REGISTER_PARSER("jsonc", JsoncParser)
