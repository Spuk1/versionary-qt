#include "ui/MainWindow.h"

#include <QApplication>
#include <QFont>
#include <QStyleFactory>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName("Versionary");
    app.setApplicationName("Versionary");
    // Pinned look: platform themes (qt6ct, Breeze, …) must not override
    // style or fonts, otherwise colors and metrics diverge per system.
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setFont(QFont("Sans Serif", 9));
    MainWindow window;
    window.show();
    return app.exec();
}
