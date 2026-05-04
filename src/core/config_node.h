#pragma once

#include <string>
#include <vector>

struct ConfigNode {
    enum class Type {
        Object,
        Array,
        String,
        Integer,
        Float,
        Bool,
        Null
    };

    std::string key;
    std::string scalar;
    std::string comment;
    int         source_line = -1;

    Type                    type = Type::Null;
    std::vector<ConfigNode> children;

    bool is_leaf()      const { return children.empty(); }
    bool is_container() const { return !children.empty(); }
};
