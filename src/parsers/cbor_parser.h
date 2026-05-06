#pragma once

#include "core/iformat_parser.h"

class CborParser : public IFormatParser {
public:
    ParseResult parse(std::string_view data) override;

    std::string format_name() const override { return "CBOR"; }
    std::string library_credit() const override;

    static void registerSelf();
};
