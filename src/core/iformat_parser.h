#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "config_node.h"

struct ParseResult {
    ConfigNode  root;
    bool        ok       = false;
    bool        has_parse_error = false;
    std::string error;
    int         err_line = -1;

    std::string format_name;
    std::string library_credit;
    long long    file_bytes  = 0;
    int         total_nodes  = 0;
    int         error_count  = 0;
    std::string warning;
};

class IFormatParser {
public:
    virtual ~IFormatParser() = default;

    virtual ParseResult parse(std::string_view data) = 0;

    virtual std::string format_name()    const = 0;
    virtual std::string library_credit() const = 0;
};
