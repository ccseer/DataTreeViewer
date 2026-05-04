#pragma once

#include "core/iformat_parser.h"

class JsoncParser : public IFormatParser {
public:
    ParseResult parse(std::string_view data) override;

    std::string format_name()    const override { return "JSONC"; }
    std::string library_credit() const override;

    static void registerSelf();
};
