#include "cbor_parser.h"

#include <cstdint>
#include <memory>

#include <nlohmann/json.hpp>

#include "core/parser_helpers.h"
#include "core/parser_registry.h"
#include "parsers/nlohmann_convert.h"

ParseResult CborParser::parse(std::string_view data)
{
    ParseResult result;

    auto j = nlohmann::json::from_cbor(
        reinterpret_cast<const std::uint8_t *>(data.data()),
        reinterpret_cast<const std::uint8_t *>(data.data()) + data.size(),
        true,
        false);

    if(j.is_discarded()) {
        result.root = dtv::core::createErrorNode("Invalid CBOR data", -1);
        result.ok = true;
        result.has_parse_error = true;
        result.error = "Invalid CBOR data";
        return result;
    }

    result.root = dtv::core::convertNlohmann(j);
    result.ok = true;
    return result;
}

std::string CborParser::library_credit() const
{
    return "nlohmann/json " + std::to_string(NLOHMANN_JSON_VERSION_MAJOR) + "." +
           std::to_string(NLOHMANN_JSON_VERSION_MINOR) + "." +
           std::to_string(NLOHMANN_JSON_VERSION_PATCH);
}

void CborParser::registerSelf()
{
    auto &reg = ParserRegistry::instance();
    reg.registerParser("cbor", [] { return std::make_unique<CborParser>(); });
}

REGISTER_PARSER("cbor", CborParser)
