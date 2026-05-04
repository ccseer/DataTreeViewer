#include "ini_parser.h"

#define SI_SUPPORT_IOSTREAMS
#include <SimpleIni.h>

#include <sstream>

#include "core/parser_registry.h"

ParseResult IniParser::parse(std::string_view data)
{
    ParseResult result;

    CSimpleIniA ini(false, true, false);

    std::string buf(data);
    std::istringstream stream(buf);

    SI_Error rc = ini.LoadData(stream);
    if (rc < 0) {
        result.ok    = false;
        result.error = "Failed to parse INI data (SI_Error " + std::to_string(rc) + ")";
        return result;
    }

    result.root.type = ConfigNode::Type::Object;
    result.ok = true;

    CSimpleIniA::TNamesDepend sections;
    ini.GetAllSections(sections);

    for (const auto& section : sections) {
        ConfigNode sectionNode;
        sectionNode.type = ConfigNode::Type::Object;
        sectionNode.key  = section.pItem;

        CSimpleIniA::TNamesDepend keys;
        ini.GetAllKeys(section.pItem, keys);

        for (const auto& key : keys) {
            std::list<CSimpleIniA::Entry> entries;
            ini.GetAllValues(section.pItem, key.pItem, entries);

            for (const auto& entry : entries) {
                ConfigNode keyNode;
                keyNode.type   = ConfigNode::Type::String;
                keyNode.key    = key.pItem;
                keyNode.scalar = entry.pItem;

                sectionNode.children.push_back(std::move(keyNode));
            }
        }

        result.root.children.push_back(std::move(sectionNode));
    }

    return result;
}

void IniParser::registerSelf()
{
    auto& reg = ParserRegistry::instance();
    reg.registerParser("ini",  [] { return std::make_unique<IniParser>(); });
    reg.registerParser("cfg",  [] { return std::make_unique<IniParser>(); });
    reg.registerParser("conf", [] { return std::make_unique<IniParser>(); });
}

REGISTER_PARSER("ini",  IniParser)
REGISTER_PARSER("cfg",  IniParser)
REGISTER_PARSER("conf", IniParser)
