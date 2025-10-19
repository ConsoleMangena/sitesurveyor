#include <QApplication>
#include <QGuiApplication>
#include <QByteArray>
#include <QFile>
#include <QFont>
#include <QIcon>
#include "mainwindow.h"
// License is now handled by a welcome page in MainWindow

int main(int argc, char *argv[])
{
    // Avoid buggy platform theme plugins from overriding palettes and causing crashes
    QGuiApplication::setDesktopSettingsAware(false);
    // Explicitly disable platform theme plugin (e.g., qt6ct) which is crashing on palette queries
    qputenv("QT_QPA_PLATFORMTHEME", QByteArray());

    QApplication app(argc, argv);
    
    app.setApplicationName("SiteSurveyor");
    app.setOrganizationName("Geomatics");
    app.setWindowIcon(QIcon(":/icons/compass.svg"));
    // Global programming font across the app (fallback chain)
    {
        QFont appFont;
        appFont.setFamilies(QStringList()
            << "JetBrains Mono"
            << "Fira Code"
            << "Cascadia Code"
            << "Source Code Pro"
            << "DejaVu Sans Mono"
            << "Monospace");
        appFont.setStyleHint(QFont::Monospace);
        app.setFont(appFont);
    }
    
    // Start with platform default style and palette (light by default).
    // Dark mode will be applied by MainWindow's toggleDarkMode() via stylesheet only,
    // so both modes share the same layout metrics.

    MainWindow window;
    window.showMaximized();
    
    return app.exec();
}
