#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class IFormatParser;

class ParserRegistry {
public:
    static ParserRegistry& instance();

    void registerParser(
        std::string                                      ext,
        std::function<std::unique_ptr<IFormatParser>()> factory);

    std::unique_ptr<IFormatParser> parserFor(const std::string& ext) const;

    std::vector<std::string> supportedExtensions() const;

    static void registerBuiltinParsers();

private:
    ParserRegistry() = default;
    std::unordered_map<
        std::string,
        std::function<std::unique_ptr<IFormatParser>()>> m_factories;
};

#define REGISTER_PARSER_IMPL_(a, b) a##b
#define REGISTER_PARSER_IMPL(a, b)  REGISTER_PARSER_IMPL_(a, b)

#define REGISTER_PARSER(ext, ParserClass)                           \
    namespace {                                                      \
    struct REGISTER_PARSER_IMPL(AutoReg_, __LINE__) {               \
        REGISTER_PARSER_IMPL(AutoReg_, __LINE__)() {                \
            ParserRegistry::instance().registerParser(               \
                ext, []{ return std::make_unique<ParserClass>(); }); \
        }                                                            \
    } REGISTER_PARSER_IMPL(autoreg_, __LINE__);                     \
    }
