#include "parser_helpers.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace dtv {
namespace core {

namespace {

void trim(std::string &s)
{
    auto first =
        std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); });
    s.erase(s.begin(), first);

    auto last =
        std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); });
    s.erase(last.base(), s.end());
}

void storeComment(CommentMap &map, int line, std::string text, bool leading)
{
    trim(text);
    if(text.empty())
        return;

    auto &target = leading ? map.leading_comments : map.inline_comments;
    target[line] = std::move(text);
}

} // namespace

CommentMap extractComments(std::string_view data)
{
    CommentMap map;
    std::string_view remaining = data;
    int current_line = 1;

    bool in_block = false;
    bool block_is_leading = false;
    std::string current_block;

    while(!remaining.empty()) {
        size_t nl_pos = remaining.find('\n');
        std::string_view line =
            (nl_pos != std::string_view::npos) ? remaining.substr(0, nl_pos) : remaining;

        if(in_block) {
            size_t end_pos = line.find("*/");
            if(end_pos != std::string_view::npos) {
                current_block += "\n" + std::string(line.substr(0, end_pos));
                storeComment(map, current_line, current_block, block_is_leading);
                current_block.clear();
                in_block = false;
                line = line.substr(end_pos + 2);
            } else {
                current_block += "\n" + std::string(line);
            }
        }

        if(!in_block && !line.empty()) {
            size_t start = line.find_first_not_of(" \t\r");
            if(start != std::string_view::npos) {
                std::string_view content = line.substr(start);

                size_t comment_start = std::string_view::npos;
                if(content.size() >= 2 && content[0] == '/' && content[1] == '*') {
                    in_block = true;
                    block_is_leading = true;
                    size_t end_pos = content.find("*/", 2);
                    if(end_pos != std::string_view::npos) {
                        std::string block = std::string(content.substr(2, end_pos - 2));
                        storeComment(map, current_line, block, true);
                        in_block = false;
                    } else {
                        current_block = std::string(content.substr(2));
                    }
                } else if(content.size() >= 1 && content[0] == '#') {
                    comment_start = start + 1;
                } else if(content.size() >= 2 && content[0] == '/' && content[1] == '/') {
                    comment_start = start + 2;
                } else {
                    // Look for inline comment
                    char quote = 0;
                    for(size_t i = 0; i < line.size(); ++i) {
                        if(quote != 0) {
                            if(line[i] == quote) {
                                bool escaped = false;
                                if(quote == '"') {
                                    size_t k = i;
                                    while(k > 0 && line[k - 1] == '\\') {
                                        escaped = !escaped;
                                        --k;
                                    }
                                }
                                if(!escaped)
                                    quote = 0;
                            }
                            continue;
                        }

                        if(line[i] == '"' || line[i] == '\'') {
                            quote = line[i];
                            continue;
                        }

                        {
                            if(line[i] == '#' ||
                               (line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/')) {
                                comment_start = i + ((line[i] == '#') ? 1 : 2);
                                break;
                            } else if(line[i] == '/' && i + 1 < line.size() &&
                                      line[i + 1] == '*') {
                                in_block = true;
                                block_is_leading = false;
                                size_t end_pos = line.find("*/", i + 2);
                                if(end_pos != std::string_view::npos) {
                                    std::string block =
                                        std::string(line.substr(i + 2, end_pos - (i + 2)));
                                    storeComment(map, current_line, block, false);
                                    in_block = false;
                                } else {
                                    current_block = std::string(line.substr(i + 2));
                                }
                                break;
                            }
                        }
                    }
                }

                if(comment_start != std::string_view::npos) {
                    std::string comment_text(line.substr(comment_start));
                    const bool leading = comment_start == start + 1 || comment_start == start + 2;
                    storeComment(map, current_line, comment_text, leading);
                }
            }
        }

        if(nl_pos == std::string_view::npos)
            break;
        remaining = remaining.substr(nl_pos + 1);
        current_line++;
    }

    return map;
}

void applyComments(ConfigNode &node, const CommentMap &map)
{
    if(node.source_line > 0) {
        auto sameLine = map.inline_comments.find(node.source_line);
        if(sameLine != map.inline_comments.end()) {
            node.comment = sameLine->second;
        } else {
            // A standalone comment may be separated from the node by a few lines
            // in small preview files, but inline comments must not bleed downward.
            for(int i = 0; i < 5; ++i) {
                int line = node.source_line - i;
                if(line <= 0)
                    break;
                auto it = map.leading_comments.find(line);
                if(it == map.leading_comments.end())
                    continue;
                node.comment = it->second;
                break;
            }
        }
    }

    for(auto &child : node.children) {
        applyComments(child, map);
    }
}

ConfigNode createErrorNode(const std::string &msg, int line, const std::string &title)
{
    ConfigNode root;
    root.type = ConfigNode::Type::Object;

    ConfigNode errorNode;
    errorNode.key = title;
    errorNode.type = ConfigNode::Type::Object;

    ConfigNode msgNode;
    msgNode.key = "Message";
    msgNode.type = ConfigNode::Type::String;
    msgNode.scalar = msg;
    errorNode.children.push_back(std::move(msgNode));

    if(line != -1) {
        ConfigNode lineNode;
        lineNode.key = "Line/Byte";
        lineNode.type = ConfigNode::Type::Integer;
        lineNode.scalar = std::to_string(line);
        errorNode.children.push_back(std::move(lineNode));
    }

    ConfigNode hintNode;
    hintNode.key = "Hint";
    hintNode.type = ConfigNode::Type::String;
    hintNode.scalar = "The file contains syntax errors. Please check the structure and indentation.";
    errorNode.children.push_back(std::move(hintNode));

    root.children.push_back(std::move(errorNode));

    return root;
}

} // namespace core
} // namespace dtv
