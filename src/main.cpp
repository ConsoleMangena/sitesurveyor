#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QMessageBox>
#include <QDir>
#include <QProcessEnvironment>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // Permanently disable Qt6CT and desktop theme integration (causes crash on Kali)
    QGuiApplication::setDesktopSettingsAware(false);
    qputenv("QT_QPA_PLATFORMTHEME", "");
    
    // Disable hardware acceleration which can cause crashes on some systems
    qputenv("QT_XCB_GL_INTEGRATION", "none");
    qputenv("LIBGL_ALWAYS_SOFTWARE", "1");

    // Ensure GDAL/PROJ data paths are available when bundled (Windows zip)
    {
        const QByteArray gdalEnv = qgetenv("GDAL_DATA");
        const QByteArray projEnv = qgetenv("PROJ_LIB");
        const QString appDir = QDir::cleanPath(QCoreApplication::applicationDirPath());
        // Common bundle layouts
        const QStringList gdalCandidates = {
            appDir + "/gdal-data",
            appDir + "/gdal/share/gdal"
        };
        const QStringList projCandidates = {
            appDir + "/proj-data",
            appDir + "/gdal/share/proj"
        };
        if (gdalEnv.isEmpty()) {
            for (const QString& p : gdalCandidates) { if (QDir(p).exists()) { qputenv("GDAL_DATA", p.toUtf8()); break; } }
        }
        if (projEnv.isEmpty()) {
            for (const QString& p : projCandidates) { if (QDir(p).exists()) { qputenv("PROJ_LIB", p.toUtf8()); break; } }
        }
    }

    QApplication app(argc, argv);
    app.setApplicationName("SiteSurveyor");
    app.setOrganizationName("Geomatics");
    app.setWindowIcon(QIcon(":/branding/logo.svg"));

    try {
        MainWindow window;
        window.showMaximized();
        return app.exec();
    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "Fatal Error", 
            QString("Application crashed: %1").arg(e.what()));
        return 1;
    } catch (...) {
        QMessageBox::critical(nullptr, "Fatal Error", 
            "Application crashed with unknown error");
        return 1;
    }
}
