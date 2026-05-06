#pragma once

#include "core/iformat_parser.h"

class EnvParser : public IFormatParser {
public:
    ParseResult parse(std::string_view data) override;

    std::string format_name() const override { return "Env"; }
    std::string library_credit() const override { return ""; }

    static void registerSelf();
};
