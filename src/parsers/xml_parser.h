#pragma once

#include "core/iformat_parser.h"

class XmlParser : public IFormatParser {
public:
    ParseResult parse(std::string_view data) override;

    std::string format_name() const override { return "XML"; }
    std::string library_credit() const override { return "pugixml v1.14"; }

    static void registerSelf();
};
