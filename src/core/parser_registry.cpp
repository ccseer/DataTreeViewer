#include "parser_registry.h"

#include <algorithm>
#include <cctype>

#include "iformat_parser.h"
#include "parsers/jsonc_parser.h"
#include "parsers/toml_parser.h"
#include "parsers/ini_parser.h"
// #include "parsers/yaml_parser.h"  // Phase 7

ParserRegistry& ParserRegistry::instance()
{
    static ParserRegistry reg;
    return reg;
}

void ParserRegistry::registerParser(
    std::string                                      ext,
    std::function<std::unique_ptr<IFormatParser>()> factory)
{
    std::string lower;
    lower.reserve(ext.size());
    for (char c : ext)
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    m_factories[lower] = std::move(factory);
}

std::unique_ptr<IFormatParser> ParserRegistry::parserFor(
    const std::string& ext) const
{
    std::string lower;
    lower.reserve(ext.size());
    for (char c : ext)
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    auto it = m_factories.find(lower);
    if (it == m_factories.end())
        return nullptr;
    return it->second();
}

std::vector<std::string> ParserRegistry::supportedExtensions() const
{
    std::vector<std::string> exts;
    exts.reserve(m_factories.size());
    for (const auto& pair : m_factories)
        exts.push_back(pair.first);
    return exts;
}

void ParserRegistry::registerBuiltinParsers()
{
    // Force each parser TU to execute its registration.
    // These are idempotent if already registered via static init.
    // New parsers: add a call to YourParser::registerSelf() here.
    JsoncParser::registerSelf();
    TomlParser::registerSelf();
    IniParser::registerSelf();
    // YamlParser::registerSelf();  // Phase 7
}
