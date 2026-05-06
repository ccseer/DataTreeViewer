#pragma once

#include "core/iformat_parser.h"

class JsonlParser : public IFormatParser {
public:
    ParseResult parse(std::string_view data) override;

    std::string format_name() const override { return "JSON Lines"; }
    std::string library_credit() const override;

    static void registerSelf();
};
