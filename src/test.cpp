#include <QApplication>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QString>

#include "ui/data_tree_viewer.h"
#include "core/parser_registry.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    qRegisterMetaType<std::shared_ptr<const ParseResult>>();
    ParserRegistry::registerBuiltinParsers();

    QString path;
    if (argc > 1) {
        path = QString::fromLocal8Bit(argv[1]);
    } else {
        path = QFileDialog::getOpenFileName(
            nullptr, "Open Data File", {},
            "Data Files (*.json *.jsonc *.yaml *.yml *.ini *.cfg *.conf *.toml *.tml);;"
            "All Files (*)");
    }
    if (path.isEmpty()) {
        return 0;
    }

    QElapsedTimer et;
    et.start();

    DataTreeViewer viewer;
    viewer.setWindowTitle(path);
    viewer.loadFile(path);
    qDebug() << "load" << et.restart() << "ms";
    viewer.resize(960, 700);
    viewer.show();
    qDebug() << "show" << et.restart() << "ms";

    return app.exec();
}
