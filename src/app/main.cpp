#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QSplashScreen>
#include <QPixmap>
#include <QPainter>
#include <QTimer>
#include <QSettings>
#include <QThread>
#include "app/mainwindow.h"

QPixmap createSplashPixmap(int progress = 0)
{
    QPixmap pixmap(400, 280);
    pixmap.fill(Qt::white);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    // Subtle border
    painter.setPen(QPen(QColor(220, 220, 220), 1));
    painter.drawRect(0, 0, 399, 279);
    
    // Load and draw logo
    QPixmap logo(":/branding/logo-256.png");
    if (!logo.isNull()) {
        QPixmap scaledLogo = logo.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawPixmap((400 - scaledLogo.width()) / 2, 50, scaledLogo);
    }
    
    // App name - elegant style
    QFont nameFont("Segoe UI", 22, QFont::Light);
    painter.setFont(nameFont);
    painter.setPen(QColor(50, 50, 50));
    painter.drawText(pixmap.rect().adjusted(0, 140, 0, 0), Qt::AlignHCenter, "SiteSurveyor");
    
    // Version - small
    QFont versionFont("Segoe UI", 9);
    painter.setFont(versionFont);
    painter.setPen(QColor(150, 150, 150));
    painter.drawText(pixmap.rect().adjusted(0, 172, 0, 0), Qt::AlignHCenter, "v1.07");
    
    // Progress bar background
    int barX = 80, barY = 220, barWidth = 240, barHeight = 6;
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(230, 230, 230));
    painter.drawRoundedRect(barX, barY, barWidth, barHeight, 3, 3);
    
    // Progress bar fill
    if (progress > 0) {
        int fillWidth = (barWidth * progress) / 100;
        QLinearGradient progressGradient(barX, 0, barX + fillWidth, 0);
        progressGradient.setColorAt(0, QColor(0, 120, 212));
        progressGradient.setColorAt(1, QColor(0, 150, 255));
        painter.setBrush(progressGradient);
        painter.drawRoundedRect(barX, barY, fillWidth, barHeight, 3, 3);
    }
    
    // Loading text
    QFont loadFont("Segoe UI", 8);
    painter.setFont(loadFont);
    painter.setPen(QColor(130, 130, 130));
    painter.drawText(QRect(0, barY + 14, 400, 20), Qt::AlignHCenter, "Loading...");
    
    return pixmap;
}

int main(int argc, char *argv[])
{
    // Permanently disable Qt6CT and desktop theme integration (causes crash on Kali)
    QGuiApplication::setDesktopSettingsAware(false);
    qputenv("QT_QPA_PLATFORMTHEME", "");
    
    // Disable hardware acceleration which can cause crashes on some systems
    qputenv("QT_XCB_GL_INTEGRATION", "none");
    qputenv("LIBGL_ALWAYS_SOFTWARE", "1");

    QApplication app(argc, argv);

    app.setApplicationName("SiteSurveyor");
    app.setOrganizationName("Geomatics");
    QIcon appIcon;
    appIcon.addFile(":/branding/logo-128.png");
    appIcon.addFile(":/branding/logo-256.png");
    app.setWindowIcon(appIcon);
    
    // Load and apply saved theme (default: light)
    QSettings settings;
    QString theme = settings.value("appearance/theme", "light").toString();
    QString themePath = (theme == "dark") 
        ? ":/styles/dark-theme.qss" 
        : ":/styles/light-theme.qss";
    QFile styleFile(themePath);
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = QLatin1String(styleFile.readAll());
        app.setStyleSheet(styleSheet);
        styleFile.close();
    }
    
    // Show splash screen with animated progress
    QSplashScreen splash(createSplashPixmap(0), Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    splash.show();
    app.processEvents();
    
    // Animate progress bar
    auto updateSplash = [&splash, &app](int progress) {
        splash.setPixmap(createSplashPixmap(progress));
        app.processEvents();
        QThread::msleep(50);
    };
    
    // Loading animation
    for (int i = 0; i <= 30; i += 5) {
        updateSplash(i);
    }

    try {
        // Initialize main window
        for (int i = 30; i <= 60; i += 5) {
            updateSplash(i);
        }
        
        MainWindow window;
        
        // Complete loading
        for (int i = 60; i <= 100; i += 5) {
            updateSplash(i);
        }
        
        // Brief pause at 100%
        QThread::msleep(300);
        
        splash.close();
        window.showMaximized();
        return app.exec();
    } catch (const std::exception& e) {
        splash.close();
        QMessageBox::critical(nullptr, "Fatal Error", 
            QString("Application crashed: %1").arg(e.what()));
        return 1;
    } catch (...) {
        splash.close();
        QMessageBox::critical(nullptr, "Fatal Error", 
            "Application crashed with unknown error");
        return 1;
    }
}
