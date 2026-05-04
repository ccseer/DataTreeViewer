#include <QApplication>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QString>

#include "ui/data_tree_viewer.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QString path;
    if(argc > 1) {
        path = QString::fromLocal8Bit(argv[1]);
    } else {
        path = QFileDialog::getOpenFileName(
            nullptr, "Open Data File", {},
            "Data Files (*.json *.jsonc *.yaml *.yml *.ini *.cfg *.conf *.toml *.tml);;"
            "All Files (*)");
    }
    if(path.isEmpty()) {
        return 0;
    }

    QElapsedTimer et;
    et.start();

    DataTreeViewer viewer;

    ViewOptionsPrivate d;
    d.dpr = 1;
    d.path = path;
    d.theme = 1;
    d.viewer_type = viewer.name();

    ViewOptions opts;
    opts.d_ptr = &d;

    viewer.setWindowTitle(d.path);
    viewer.load(nullptr, &opts);
    qDebug() << "load" << et.restart() << "ms";
    viewer.resize(viewer.getContentSize());
    viewer.show();
    qDebug() << "show" << et.restart() << "ms";

    return app.exec();
}
