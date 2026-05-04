#pragma once

#include "core/iformat_parser.h"

class TomlParser : public IFormatParser {
public:
    ParseResult parse(std::string_view data) override;

    std::string format_name()    const override { return "TOML 1.0"; }
    std::string library_credit() const override;

    static void registerSelf();
};
