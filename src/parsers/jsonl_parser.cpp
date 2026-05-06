#include "jsonl_parser.h"

#include <cctype>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "core/parser_helpers.h"
#include "core/parser_registry.h"
#include "parsers/nlohmann_convert.h"

namespace {

constexpr int kMaxLines = 10000;

std::string trimCopy(std::string_view value)
{
    size_t first = 0;
    while(first < value.size() && std::isspace(static_cast<unsigned char>(value[first])))
        ++first;

    size_t last = value.size();
    while(last > first && std::isspace(static_cast<unsigned char>(value[last - 1])))
        --last;

    return std::string(value.substr(first, last - first));
}

} // namespace

ParseResult JsonlParser::parse(std::string_view data)
{
    ParseResult result;
    result.root.type = ConfigNode::Type::Array;
    result.ok = true;

    int lineNumber = 0;
    int parsedCount = 0;
    int nonEmptyCount = 0;
    size_t pos = 0;

    while(pos <= data.size()) {
        size_t nl = data.find('\n', pos);
        std::string_view line =
            nl == std::string_view::npos ? data.substr(pos) : data.substr(pos, nl - pos);
        if(!line.empty() && line.back() == '\r')
            line.remove_suffix(1);

        ++lineNumber;
        std::string trimmed = trimCopy(line);
        if(!trimmed.empty() && trimmed[0] != '#')
            ++nonEmptyCount;

        if(trimmed.empty() || trimmed[0] == '#') {
            if(nl == std::string_view::npos)
                break;
            pos = nl + 1;
            continue;
        }

        if(parsedCount >= kMaxLines) {
            if(nonEmptyCount > kMaxLines) {
                result.warning =
                    "Showing first 10,000 of " + std::to_string(nonEmptyCount) + " lines";
            }
            if(nl == std::string_view::npos)
                break;
            pos = nl + 1;
            continue;
        }

        nlohmann::json j = nlohmann::json::parse(trimmed, nullptr, false, true);

        ConfigNode child;
        if(j.is_discarded()) {
            child = dtv::core::createErrorNode(
                "Line " + std::to_string(lineNumber) + ": invalid JSON", lineNumber);
            result.has_parse_error = true;
            result.error_count += 1;
            if(result.err_line < 0)
                result.err_line = lineNumber;
            if(result.error.empty())
                result.error = "Line " + std::to_string(lineNumber) + ": invalid JSON";
        } else {
            child = dtv::core::convertNlohmann(j);
        }

        child.key = "[" + std::to_string(parsedCount) + "]";
        child.source_line = lineNumber;
        result.root.children.push_back(std::move(child));
        parsedCount += 1;

        if(nl == std::string_view::npos)
            break;
        pos = nl + 1;
    }

    if(result.warning.empty() && nonEmptyCount > kMaxLines) {
        result.warning = "Showing first 10,000 of " + std::to_string(nonEmptyCount) + " lines";
    }

    return result;
}

std::string JsonlParser::library_credit() const
{
    return "nlohmann/json " + std::to_string(NLOHMANN_JSON_VERSION_MAJOR) + "." +
           std::to_string(NLOHMANN_JSON_VERSION_MINOR) + "." +
           std::to_string(NLOHMANN_JSON_VERSION_PATCH);
}

void JsonlParser::registerSelf()
{
    auto &reg = ParserRegistry::instance();
    reg.registerParser("jsonl", [] { return std::make_unique<JsonlParser>(); });
    reg.registerParser("ndjson", [] { return std::make_unique<JsonlParser>(); });
}

REGISTER_PARSER("jsonl", JsonlParser)
REGISTER_PARSER("ndjson", JsonlParser)
