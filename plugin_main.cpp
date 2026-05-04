#include "ui/data_tree_viewer.h"
#include "core/parser_registry.h"

extern "C" __declspec(dllexport)
QWidget* createWidget(const QString& filePath, QWidget* parent)
{
    ParserRegistry::registerBuiltinParsers();
    qRegisterMetaType<std::shared_ptr<const ParseResult>>();

    auto* viewer = new DataTreeViewer(parent);
    viewer->loadFile(filePath);
    return viewer;
}

extern "C" __declspec(dllexport)
void destroyWidget(QWidget* widget)
{
    delete widget;
}

extern "C" __declspec(dllexport)
const char* supportedExtensions()
{
    static const std::string exts = [] {
        std::string s;
        for (const auto& e : ParserRegistry::instance().supportedExtensions()) {
            if (!s.empty()) s += ";";
            s += "." + e;
        }
        return s;
    }();
    return exts.c_str();
}

extern "C" __declspec(dllexport)
int pluginVersion()
{
    return 1;
}
