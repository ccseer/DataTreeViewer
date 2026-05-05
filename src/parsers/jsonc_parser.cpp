#include "jsonc_parser.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <queue>
#include <regex>
#include <string_view>

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

void trim(std::string &s) {
    auto it1 = std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); });
    s.erase(s.begin(), it1);
    auto it2 = std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); });
    s.erase(it2.base(), s.end());
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

std::string stripComments(const std::string_view &data, std::queue<CommentEntry> &outComments)
{
    std::string clean;
    clean.reserve(data.size());

    std::string pending_comment;
    bool in_str = false;
    bool in_block = false;
    bool in_line = false;

    std::string current_comment;

    for (size_t i = 0; i < data.size(); ++i) {
        char c = data[i];
        char next = (i + 1 < data.size()) ? data[i+1] : 0;

        if (in_block) {
            if (c == '*' && next == '/') {
                in_block = false;
                i++;
                clean.append("  ");
                trim(current_comment);
                if (!current_comment.empty()) pending_comment = current_comment;
                current_comment.clear();
            } else {
                if (c == '\n') clean.push_back('\n');
                else clean.push_back(' ');
                current_comment.push_back(c);
            }
            continue;
        }

        if (in_line) {
            if (c == '\n') {
                in_line = false;
                clean.push_back('\n');
                trim(current_comment);
                if (!current_comment.empty()) pending_comment = current_comment;
                current_comment.clear();
            } else {
                clean.push_back(' ');
                current_comment.push_back(c);
            }
            continue;
        }

        if (in_str) {
            if (c == '"') {
                bool escaped = false;
                size_t k = i;
                while (k > 0 && data[k-1] == '\\') {
                    escaped = !escaped;
                    k--;
                }
                if (!escaped) in_str = false;
            }
            clean.push_back(c);
            continue;
        }

        if (c == '"') {
            in_str = true;
            clean.push_back(c);

            if (!pending_comment.empty()) {
                size_t line_start = clean.rfind('\n');
                if (line_start == std::string::npos) line_start = 0;
                else line_start++;

                if (line_start < clean.size()) {
                    std::string current_line = clean.substr(line_start);
                    std::regex key_re(R"re("([^"]+)"\s*:)re");
                    std::smatch m;
                    if (std::regex_search(current_line, m, key_re)) {
                        outComments.push({m[1].str(), pending_comment});
                        pending_comment.clear();
                    }
                }
            }
            continue;
        }

        if (c == '/' && next == '/') {
            in_line = true;
            i++;
            clean.append("  ");
            continue;
        }

        if (c == '/' && next == '*') {
            in_block = true;
            i++;
            clean.append("  ");
            continue;
        }

        if (c == '#') {
            in_line = true;
            clean.push_back(' ');
            continue;
        }

        clean.push_back(c);
    }

    return clean;
}

ConfigNode walkJ(const nlohmann::json &j)
{
    ConfigNode node;
    try {
        if (j.is_object()) {
            node.type = ConfigNode::Type::Object;
            for (auto it = j.begin(); it != j.end(); ++it) {
                ConfigNode child = walkJ(it.value());
                child.key = it.key();
                node.children.push_back(std::move(child));
            }
        } else if (j.is_array()) {
            node.type = ConfigNode::Type::Array;
            for (const auto &elem : j) {
                node.children.push_back(walkJ(elem));
            }
        } else if (j.is_string()) {
            node.type = ConfigNode::Type::String;
            node.scalar = j.get<std::string>();
        } else if (j.is_number_integer() || j.is_number_unsigned()) {
            node.type = ConfigNode::Type::Integer;
            node.scalar = j.dump();
        } else if (j.is_number_float()) {
            node.type = ConfigNode::Type::Float;
            node.scalar = j.dump();
        } else if (j.is_boolean()) {
            node.type = ConfigNode::Type::Bool;
            node.scalar = j.get<bool>() ? "true" : "false";
        } else if (j.is_null()) {
            node.type = ConfigNode::Type::Null;
            node.scalar = "null";
        } else {
            node.type = ConfigNode::Type::Null;
        }
    } catch (...) {
        node.type = ConfigNode::Type::Null;
    }
    return node;
}

void annotateComments(ConfigNode &node, std::queue<CommentEntry> &comments)
{
    if (comments.empty()) return;
    for(auto &child : node.children) {
        if(!child.key.empty() && !comments.empty()) {
            if(comments.front().key == child.key) {
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
        std::string clean = stripComments(data, comments);
        traceJsonc("comments stripped, clean bytes=" + std::to_string(clean.size()) +
                   ", pending comments=" + std::to_string(comments.size()));

        auto j = nlohmann::json::parse(clean, nullptr, false, true);
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
        result.root = walkJ(j);
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
    reg.registerParser("json", [] { return std::make_unique<JsoncParser>(); });
    reg.registerParser("jsonc", [] { return std::make_unique<JsoncParser>(); });
    reg.registerParser("json5", [] { return std::make_unique<JsoncParser>(); });
}

REGISTER_PARSER("json", JsoncParser)
REGISTER_PARSER("jsonc", JsoncParser)
REGISTER_PARSER("json5", JsoncParser)
