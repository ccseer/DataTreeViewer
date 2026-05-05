#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "config_node.h"

namespace dtv {
namespace core {

struct CommentMap {
    // Comments on value/key lines are attached only to that exact line.
    std::unordered_map<int, std::string> inline_comments;

    // Standalone comments may describe the next nearby node.
    std::unordered_map<int, std::string> leading_comments;
};

CommentMap extractComments(std::string_view data);

void applyComments(ConfigNode &node, const CommentMap &map);

/** Create a standard error tree structure for parse failures. */
ConfigNode createErrorNode(const std::string &msg, int line = -1,
                           const std::string &title = "PARSE ERROR");

} // namespace core
} // namespace dtv
