#include "ui/MainWindow.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QStyleFactory>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName("Versionary");
    app.setApplicationName("Versionary");
    app.setApplicationVersion(VERSIONARY_VERSION);
    // Window/taskbar icon. On Windows the executable also carries the icon as
    // a resource (see Versionary.rc.in); setting it here keeps the window icon
    // correct on every platform that ships the icon resource.
    app.setWindowIcon(QIcon(":/icons/Versionary.png"));
    // Pinned look: platform themes (qt6ct, Breeze, …) must not override
    // style or fonts, otherwise colors and metrics diverge per system.
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setFont(QFont("Sans Serif", 9));
    MainWindow window;
    window.show();
    return app.exec();
}
