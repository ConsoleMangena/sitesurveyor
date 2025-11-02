#include <QApplication>
#include <QGuiApplication>
#include <QtGlobal>
#include <QByteArray>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QTextStream>
#include <QDir>
#include "mainwindow.h"
// License is now handled by a welcome page in MainWindow

static void loadDotEnv()
{
    // Load .env from CWD and application directory (first wins)
    QStringList candidates;
    candidates << QDir::current().filePath(".env")
               << QDir(QCoreApplication::applicationDirPath()).filePath(".env");
    for (const QString& path : candidates) {
        QFile f(path);
        if (!f.exists()) continue;
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine();
            QString s = line.trimmed();
            if (s.isEmpty() || s.startsWith('#')) continue;
            if (s.startsWith("export ")) s = s.mid(7).trimmed();
            int eq = s.indexOf('=');
            if (eq <= 0) continue;
            QString key = s.left(eq).trimmed();
            QString val = s.mid(eq+1).trimmed();
            const QChar dq(34);
            const QChar sq(39);
            if ((val.startsWith(dq) && val.endsWith(dq)) || (val.startsWith(sq) && val.endsWith(sq))) {
                val = val.mid(1, val.size()-2);
            }
            if (key.isEmpty()) continue;
            qputenv(key.toUtf8(), val.toUtf8());
        }
        f.close();
    }
}

// Early loader: reads only from current working directory so env flags are available before QApplication
static void loadDotEnvFromCWD()
{
    const QString path = QDir::current().filePath(".env");
    QFile f(path);
    if (!f.exists()) return;
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine();
        QString s = line.trimmed();
        if (s.isEmpty() || s.startsWith('#')) continue;
        if (s.startsWith("export ")) s = s.mid(7).trimmed();
        int eq = s.indexOf('=');
        if (eq <= 0) continue;
        QString key = s.left(eq).trimmed();
        QString val = s.mid(eq+1).trimmed();
        const QChar dq(34);
        const QChar sq(39);
        if ((val.startsWith(dq) && val.endsWith(dq)) || (val.startsWith(sq) && val.endsWith(sq))) {
            val = val.mid(1, val.size()-2);
        }
        if (key.isEmpty()) continue;
        qputenv(key.toUtf8(), val.toUtf8());
    }
    f.close();
}

// Optional Qt log filter to silence noisy qt6ct palette spam
static QtMessageHandler g_prevHandler = nullptr;
static void qtLogFilter(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    if (msg.contains("Qt6CTPlatformTheme", Qt::CaseInsensitive)
        || msg.contains("QPlatformTheme::", Qt::CaseInsensitive)
        || msg.contains("qt6ct", Qt::CaseInsensitive)) {
        return; // drop qt6ct platform theme spam
    }
    if (g_prevHandler) g_prevHandler(type, ctx, msg);
}

int main(int argc, char *argv[])
{
    // Load .env early from CWD so flags are available before QApplication
    loadDotEnvFromCWD();

    // Optional: silence Qt logs entirely
    if (qEnvironmentVariableIsSet("APP_SILENCE_QT_LOGS")) {
        qputenv("QT_LOGGING_RULES", QByteArray("*=false"));
    }
    // Optional: filter noisy qt6ct palette spam
    if (qEnvironmentVariableIsSet("APP_FILTER_QT6CT_LOGS")) {
        g_prevHandler = qInstallMessageHandler(qtLogFilter);
    }
    // Avoid buggy platform theme plugins from overriding palettes and causing crashes
    // (Enable this safety switch only when APP_FORCE_SAFE_THEME is set)
    if (qEnvironmentVariableIsSet("APP_FORCE_SAFE_THEME")) {
        QGuiApplication::setDesktopSettingsAware(false);
        // Explicitly disable platform theme plugin (e.g., qt6ct) which can be noisy/unstable
        qputenv("QT_QPA_PLATFORMTHEME", QByteArray());
    }

    QApplication app(argc, argv);
    
    // Load environment variables from .env (CWD + app dir). CWD values already applied above.
    loadDotEnv();
    
    app.setApplicationName("SiteSurveyor");
    app.setOrganizationName("Geomatics");
    app.setWindowIcon(QIcon(":/branding/logo.svg"));
    // Global programming font across the app (enable only when APP_USE_MONO_FONT is set)
    if (qEnvironmentVariableIsSet("APP_USE_MONO_FONT")) {
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
