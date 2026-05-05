#include "ini_parser.h"

#define SI_SUPPORT_IOSTREAMS
#include <SimpleIni.h>

#include <cctype>
#include <deque>
#include <list>
#include <sstream>
#include <string>
#include <unordered_map>

#include "core/parser_registry.h"
#include "core/parser_helpers.h"

namespace {

struct NodeMeta {
    std::string comment;
    int line = -1;
};

struct KeyMeta {
    std::string comment;
    std::string scalarOverride;
    int line = -1;
    bool hasScalarOverride = false;
};

struct IniScan {
    bool hasError = false;
    std::string error;
    int errorLine = -1;
    std::unordered_map<std::string, NodeMeta> sections;
    std::unordered_map<std::string, std::deque<KeyMeta>> keys;
};

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

std::string metaKey(std::string_view section, std::string_view key)
{
    std::string out(section);
    out.push_back('\0');
    out.append(key);
    return out;
}

const char *safeText(const char *value)
{
    return value ? value : "";
}

void appendPendingComment(std::string &pending, std::string comment)
{
    if(comment.empty())
        return;
    if(!pending.empty())
        pending += "\n";
    pending += std::move(comment);
}

size_t findInlineComment(std::string_view value)
{
    char quote = 0;
    for(size_t i = 0; i < value.size(); ++i) {
        char c = value[i];
        if(quote != 0) {
            if(c == quote)
                quote = 0;
            continue;
        }

        if(c == '"' || c == '\'') {
            quote = c;
            continue;
        }

        if((c == ';' || c == '#') &&
           (i == 0 || std::isspace(static_cast<unsigned char>(value[i - 1])))) {
            return i;
        }
    }
    return std::string_view::npos;
}

IniScan scanIni(std::string_view data)
{
    IniScan scan;
    std::string currentSection;
    std::string pendingComment;

    int lineNumber = 1;
    size_t pos = 0;
    while(pos <= data.size()) {
        size_t nl = data.find('\n', pos);
        std::string_view line =
            nl == std::string_view::npos ? data.substr(pos) : data.substr(pos, nl - pos);
        if(!line.empty() && line.back() == '\r')
            line.remove_suffix(1);

        std::string trimmed = trimCopy(line);
        if(!trimmed.empty()) {
            if(trimmed[0] == ';' || trimmed[0] == '#') {
                appendPendingComment(pendingComment, trimCopy(std::string_view(trimmed).substr(1)));
            } else if(trimmed[0] == '[') {
                size_t close = trimmed.find(']');
                if(close == std::string::npos) {
                    scan.hasError = true;
                    scan.error = "Malformed INI section header";
                    scan.errorLine = lineNumber;
                    return scan;
                }

                currentSection = trimCopy(std::string_view(trimmed).substr(1, close - 1));
                NodeMeta &meta = scan.sections[currentSection];
                meta.line = lineNumber;
                if(!pendingComment.empty()) {
                    meta.comment = pendingComment;
                    pendingComment.clear();
                }
            } else {
                size_t sep = trimmed.find('=');
                if(sep == std::string::npos)
                    sep = trimmed.find(':');
                if(sep != std::string::npos) {
                    std::string key = trimCopy(std::string_view(trimmed).substr(0, sep));
                    std::string_view value = std::string_view(trimmed).substr(sep + 1);
                    size_t commentStart = findInlineComment(value);

                    KeyMeta meta;
                    meta.line = lineNumber;
                    if(commentStart != std::string_view::npos) {
                        meta.comment = trimCopy(value.substr(commentStart + 1));
                        meta.scalarOverride = trimCopy(value.substr(0, commentStart));
                        meta.hasScalarOverride = true;
                    } else if(!pendingComment.empty()) {
                        meta.comment = pendingComment;
                        pendingComment.clear();
                    }

                    scan.keys[metaKey(currentSection, key)].push_back(std::move(meta));
                }
            }
        }

        if(nl == std::string_view::npos)
            break;
        pos = nl + 1;
        ++lineNumber;
    }

    return scan;
}

} // namespace

ParseResult IniParser::parse(std::string_view data)
{
    ParseResult result;

    IniScan scan = scanIni(data);
    if(scan.hasError) {
        result.root = dtv::core::createErrorNode(scan.error, scan.errorLine);
        result.ok = true;
        result.has_parse_error = true;
        result.error = scan.error;
        result.err_line = scan.errorLine;
        return result;
    }

    CSimpleIniA ini(false, true, false);

    std::string buf(data);
    std::istringstream stream(buf);

    SI_Error rc = ini.LoadData(stream);
    if (rc < 0) {
        result.root = dtv::core::createErrorNode("Failed to parse INI data (SI_Error " + std::to_string(rc) + ")");
        result.ok   = true;
        result.has_parse_error = true;
        result.error = "Failed to parse INI data (SI_Error " + std::to_string(rc) + ")";
        return result;
    }

    result.root.type = ConfigNode::Type::Object;
    result.ok = true;

    CSimpleIniA::TNamesDepend sections;
    ini.GetAllSections(sections);
    sections.sort(CSimpleIniA::Entry::LoadOrder());

    for (const auto& section : sections) {
        ConfigNode sectionNode;
        sectionNode.type = ConfigNode::Type::Object;
        sectionNode.key  = safeText(section.pItem);
        auto sectionMeta = scan.sections.find(sectionNode.key);
        if(sectionMeta != scan.sections.end()) {
            sectionNode.comment = sectionMeta->second.comment;
            sectionNode.source_line = sectionMeta->second.line;
        }

        CSimpleIniA::TNamesDepend keys;
        ini.GetAllKeys(section.pItem, keys);
        keys.sort(CSimpleIniA::Entry::LoadOrder());

        for (const auto& key : keys) {
            std::list<CSimpleIniA::Entry> entries;
            ini.GetAllValues(section.pItem, key.pItem, entries);
            entries.sort(CSimpleIniA::Entry::LoadOrder());
            auto metaIt = scan.keys.find(metaKey(safeText(section.pItem), safeText(key.pItem)));

            for (const auto& entry : entries) {
                ConfigNode keyNode;
                keyNode.type   = ConfigNode::Type::String;
                keyNode.key    = safeText(key.pItem);
                keyNode.scalar = safeText(entry.pItem);
                if(metaIt != scan.keys.end() && !metaIt->second.empty()) {
                    keyNode.comment = metaIt->second.front().comment;
                    keyNode.source_line = metaIt->second.front().line;
                    if(metaIt->second.front().hasScalarOverride)
                        keyNode.scalar = metaIt->second.front().scalarOverride;
                    metaIt->second.pop_front();
                }

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
