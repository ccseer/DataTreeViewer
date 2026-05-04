#pragma once

#include "core/iformat_parser.h"

class IniParser : public IFormatParser {
public:
    ParseResult parse(std::string_view data) override;

    std::string format_name()    const override { return "INI"; }
    std::string library_credit() const override { return "SimpleIni 4.22"; }

    static void registerSelf();
};
