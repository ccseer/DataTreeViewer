#include "jsonc_parser.h"

#include <nlohmann/json.hpp>

#include <queue>
#include <regex>
#include <string_view>

#include "core/parser_registry.h"

namespace {

// ---- Comment extraction ----

struct CommentEntry {
    std::string key;
    std::string comment;
};

std::string stripComments(const std::string_view &data, std::queue<CommentEntry> &outComments)
{
    std::string clean;
    clean.reserve(data.size());

    std::string pending_comment;

    std::string_view remaining = data;

    while(!remaining.empty()) {
        size_t nl_pos = remaining.find('\n');
        std::string_view line =
            (nl_pos != std::string_view::npos) ? remaining.substr(0, nl_pos + 1) : remaining;

        bool has_comment = false;
        size_t comment_start = std::string_view::npos;

        // Find trailing // comment (skip strings)
        bool in_str = false;
        for(size_t i = 0; i < line.size(); ++i) {
            if(line[i] == '"' && (i == 0 || line[i - 1] != '\\'))
                in_str = !in_str;
            if(!in_str && line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/') {
                comment_start = i;
                has_comment = true;
                break;
            }
        }

        if(has_comment) {
            clean.append(line.data(), comment_start);

            // Extract and trim comment text
            std::string comment_text(line.substr(comment_start + 2));
            while(!comment_text.empty() &&
                  (comment_text.back() == '\n' || comment_text.back() == '\r' ||
                   comment_text.back() == ' ' || comment_text.back() == '\t'))
                comment_text.pop_back();
            size_t lead = 0;
            while(lead < comment_text.size() &&
                  (comment_text[lead] == ' ' || comment_text[lead] == '\t'))
                ++lead;
            if(lead > 0)
                comment_text.erase(0, lead);

            // Find the last key on this line
            std::string before_comment{line.substr(0, comment_start)};
            std::regex key_re(R"re("([^"]+)"\s*:)re");
            std::smatch m;
            std::string last_key;
            auto it = before_comment.cbegin();
            while(std::regex_search(it, before_comment.cend(), m, key_re)) {
                last_key = m[1].str();
                it = m.suffix().first;
            }

            if(!last_key.empty()) {
                // Trailing comment on a key-value line
                outComments.push({last_key, comment_text});
            } else {
                // Standalone comment — save as pending for the NEXT key
                pending_comment = comment_text;
            }

            // Replace comment with spaces
            for(size_t i = comment_start; i < line.size(); ++i) {
                if(line[i] == '\n' || line[i] == '\r')
                    clean.push_back(line[i]);
                else
                    clean.push_back(' ');
            }
        } else {
            // No // comment on this line — check if there's a key to consume pending comment
            std::string line_copy{line.begin(), line.end()};
            std::regex key_re(R"re("([^"]+)"\s*:)re");
            std::smatch m;
            std::string last_key;
            auto it = line_copy.cbegin();
            while(std::regex_search(it, line_copy.cend(), m, key_re)) {
                last_key = m[1].str();
                it = m.suffix().first;
            }

            if(!last_key.empty() && !pending_comment.empty()) {
                outComments.push({last_key, pending_comment});
                pending_comment.clear();
            }

            clean.append(line.data(), line.size());
        }

        if(nl_pos == std::string_view::npos)
            break;
        remaining = remaining.substr(nl_pos + 1);
    }

    // Strip block comments /* ... */ (replace with spaces)
    std::string result;
    result.reserve(clean.size());
    bool in_str2 = false;
    for(size_t i = 0; i < clean.size();) {
        if(!in_str2 && clean[i] == '/' && i + 1 < clean.size() && clean[i + 1] == '*') {
            size_t j = i + 2;
            while(j < clean.size() &&
                  !(clean[j] == '*' && j + 1 < clean.size() && clean[j + 1] == '/'))
                ++j;
            if(j < clean.size())
                j += 2;
            while(i < j) {
                result.push_back(clean[i] == '\n' ? '\n' : ' ');
                ++i;
            }
            continue;
        }
        if(clean[i] == '"') {
            bool escaped = (i > 0 && clean[i - 1] == '\\');
            if(!escaped)
                in_str2 = !in_str2;
        }
        result.push_back(clean[i]);
        ++i;
    }

    return result;
}

ConfigNode walkJ(const nlohmann::json &j)
{
    ConfigNode node;

    switch(j.type()) {
    case nlohmann::json::value_t::object:
        node.type = ConfigNode::Type::Object;
        node.children.reserve(j.size());
        for(auto it = j.begin(); it != j.end(); ++it) {
            ConfigNode child = walkJ(it.value());
            child.key = it.key();
            node.children.push_back(std::move(child));
        }
        break;

    case nlohmann::json::value_t::array:
        node.type = ConfigNode::Type::Array;
        node.children.reserve(j.size());
        for(const auto &elem : j)
            node.children.push_back(walkJ(elem));
        break;

    case nlohmann::json::value_t::string:
        node.type = ConfigNode::Type::String;
        node.scalar = j.get<std::string>();
        break;

    case nlohmann::json::value_t::number_integer:
    case nlohmann::json::value_t::number_unsigned:
        node.type = ConfigNode::Type::Integer;
        node.scalar = j.dump();
        break;

    case nlohmann::json::value_t::number_float:
        node.type = ConfigNode::Type::Float;
        node.scalar = j.dump();
        break;

    case nlohmann::json::value_t::boolean:
        node.type = ConfigNode::Type::Bool;
        node.scalar = j.get<bool>() ? "true" : "false";
        break;

    case nlohmann::json::value_t::null:
        node.type = ConfigNode::Type::Null;
        node.scalar = "null";
        break;

    case nlohmann::json::value_t::discarded:
    case nlohmann::json::value_t::binary:
        node.type = ConfigNode::Type::Null;
        break;
    }

    return node;
}

// ---- Annotate tree with comments ----

void annotateComments(ConfigNode &root, std::queue<CommentEntry> &comments)
{
    std::vector<ConfigNode *> stack;
    stack.push_back(&root);

    while(!stack.empty()) {
        ConfigNode *node = stack.back();
        stack.pop_back();

        for(auto &child : node->children) {
            if(!child.key.empty() && !comments.empty()) {
                if(comments.front().key == child.key) {
                    child.comment = comments.front().comment;
                    comments.pop();
                }
            }
            stack.push_back(&child);
        }
    }
}

} // namespace

std::string JsoncParser::library_credit() const
{
    return "nlohmann/json " + std::to_string(NLOHMANN_JSON_VERSION_MAJOR) + "." +
           std::to_string(NLOHMANN_JSON_VERSION_MINOR) + "." +
           std::to_string(NLOHMANN_JSON_VERSION_PATCH);
}

ParseResult JsoncParser::parse(std::string_view data)
{
    ParseResult result;
    std::queue<CommentEntry> comments;

    std::string clean = stripComments(data, comments);

    try {
        auto j = nlohmann::json::parse(clean, nullptr, true, true);
        result.root = walkJ(j);
        annotateComments(result.root, comments);
        result.ok = true;
    } catch(const nlohmann::json::parse_error &e) {
        result.ok = false;
        result.error = e.what();
        result.err_line = static_cast<int>(e.byte);
    }

    return result;
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
    reg.registerParser("json5", [] {
        return std::make_unique<JsoncParser>();
    });
}

REGISTER_PARSER("json", JsoncParser)
REGISTER_PARSER("jsonc", JsoncParser)
REGISTER_PARSER("json5", JsoncParser)
