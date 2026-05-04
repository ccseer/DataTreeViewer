#include <QApplication>
#include <QFileDialog>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>

#include "ui/data_tree_viewer.h"
#include "core/parser_registry.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    qRegisterMetaType<std::shared_ptr<const ParseResult>>();
    ParserRegistry::registerBuiltinParsers();

    QMainWindow window;
    window.setWindowTitle("DataTreeViewer - Demo");
    window.resize(960, 700);

    auto* viewer = new DataTreeViewer(&window);
    window.setCentralWidget(viewer);

    auto* fileMenu = window.menuBar()->addMenu("&File");
    QAction* openAct = fileMenu->addAction("&Open...");
    openAct->setShortcut(QKeySequence::Open);
    QObject::connect(openAct, &QAction::triggered, [&window, viewer] {
        QString path = QFileDialog::getOpenFileName(
            &window, "Open Data File", {},
            "Data Files (*.json *.jsonc *.json5 *.yaml *.yml *.ini *.cfg *.conf *.toml *.tml);;All Files (*)");
        if (!path.isEmpty())
            viewer->loadFile(path);
    });

    if (argc > 1)
        viewer->loadFile(QString::fromLocal8Bit(argv[1]));

    window.show();
    return app.exec();
}
