#pragma once

#include <nlohmann/json.hpp>

#include <string>

#include "core/config_node.h"

namespace dtv {
namespace core {

inline std::string binaryToHex(const nlohmann::json::binary_t &bytes)
{
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(2 + bytes.size() * 2);
    out += "0x";
    for(uint8_t b : bytes) {
        out += kHex[b >> 4];
        out += kHex[b & 0x0f];
    }
    return out;
}

template <typename Json> inline ConfigNode convertNlohmann(const Json &j)
{
    ConfigNode node;
    switch(j.type()) {
    case nlohmann::json::value_t::object:
        node.type = ConfigNode::Type::Object;
        node.children.reserve(j.size());
        for(auto it = j.begin(); it != j.end(); ++it) {
            ConfigNode child = convertNlohmann(it.value());
            child.key = it.key();
            node.children.push_back(std::move(child));
        }
        break;
    case nlohmann::json::value_t::array:
        node.type = ConfigNode::Type::Array;
        node.children.reserve(j.size());
        for(const auto &elem : j)
            node.children.push_back(convertNlohmann(elem));
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
    case nlohmann::json::value_t::binary:
        node.type = ConfigNode::Type::String;
        node.scalar = binaryToHex(j.get_binary());
        break;
    case nlohmann::json::value_t::null:
    default:
        node.type = ConfigNode::Type::Null;
        node.scalar = "null";
        break;
    }

    return node;
}

} // namespace core
} // namespace dtv
