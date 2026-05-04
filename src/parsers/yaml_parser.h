#pragma once

#include "core/iformat_parser.h"

class YamlParser : public IFormatParser {
public:
    ParseResult parse(std::string_view data) override;

    std::string format_name()    const override { return "YAML 1.2"; }
    std::string library_credit() const override { return "rapidyaml 0.7.2"; }

    static void registerSelf();
};
