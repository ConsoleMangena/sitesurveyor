#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QSplashScreen>
#include <QPixmap>
#include <QPainter>
#include <QTimer>
#include <QSettings>
#include <QThread>
#include "app/mainwindow.h"
#include "app/startdialog.h"
#include "canvas/canvaswidget.h"
#include "auth/authmanager.h"
#include "app/logindialog.h"

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
    painter.drawText(pixmap.rect().adjusted(0, 172, 0, 0), Qt::AlignHCenter, "v1.0.8");
    
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
    QIcon appIcon(":/branding/logo.ico");
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
    for (int i = 0; i <= 100; i += 10) {
        updateSplash(i);
    }
    
    // Brief pause at 100%
    QThread::msleep(200);
    splash.close();

    // --- Appwrite Authentication (once on startup) ---
    AuthManager authManager;
    authManager.checkSession();
    
    // Wait for session check
    QEventLoop authLoop;
    QObject::connect(&authManager, &AuthManager::sessionVerified, &authLoop, &QEventLoop::quit);
    QObject::connect(&authManager, &AuthManager::sessionInvalid, &authLoop, &QEventLoop::quit);
    QTimer::singleShot(2000, &authLoop, &QEventLoop::quit); // Timeout
    authLoop.exec();
    
    if (!authManager.isAuthenticated()) {
        LoginDialog login(&authManager);
        if (login.exec() != QDialog::Accepted) {
            return 0; // User cancelled login
        }
    }
    // --- End Auth ---
    // (Update check removed by user request)


    try {
        // Main application loop - returns to start dialog when main window closes
        while (true) {
            QString projectToOpen;
            bool createFromTemplate = false;
            QString templateName;
            SurveyCategory projectCategory = SurveyCategory::Engineering;
    
    // Show Start Dialog to choose project or template
    StartDialog startDialog(&authManager);
    if (startDialog.shouldShowStartDialog() && startDialog.exec() == QDialog::Accepted) {
                projectCategory = startDialog.selectedCategory();
                switch (startDialog.startResult()) {
                    case StartDialog::OpenProject:
                    case StartDialog::OpenRecent:
                        projectToOpen = startDialog.selectedFilePath();
                        break;
                    case StartDialog::OpenTemplate:
                        createFromTemplate = true;
                        templateName = startDialog.selectedTemplate();
                        break;
                    case StartDialog::NewProject:
                    default:
                        // Just open blank
                        createFromTemplate = true;
                        templateName = "Blank";
                        break;
                }
            } else {
                // User closed dialog without choosing - exit app
                break;
            }
            
            // Create main window and set category
            MainWindow window(&authManager);
            window.setCategory(projectCategory);
            
            // Handle startup action
            if (!projectToOpen.isEmpty()) {
                // Load the selected project
                if (window.canvas() && window.canvas()->loadProject(projectToOpen)) {
                    window.setWindowTitle(QString("SiteSurveyor - %1").arg(QFileInfo(projectToOpen).fileName()));
                    window.addToRecentProjects(projectToOpen);
                }
            } else if (createFromTemplate) {
                // Create from template - set up layers based on category and template
                if (window.canvas()) {
                    window.canvas()->clearAll();
                    
                    // Create layers based on category
                    switch (projectCategory) {
                        case SurveyCategory::Cadastral:
                            window.canvas()->addLayer("Boundary", QColor(255, 255, 255));
                            window.canvas()->addLayer("Beacons", QColor(255, 165, 0));
                            window.canvas()->addLayer("Pegs", QColor(255, 0, 0));
                            window.canvas()->addLayer("Offset", QColor(0, 255, 255));
                            window.canvas()->addLayer("Servitudes", QColor(255, 255, 0));
                            window.canvas()->addLayer("Annotation", QColor(200, 200, 200));
                            break;
                            
                        case SurveyCategory::Mining:
                            window.canvas()->addLayer("Ore Body", QColor(255, 215, 0));
                            window.canvas()->addLayer("Waste", QColor(128, 128, 128));
                            window.canvas()->addLayer("Development", QColor(100, 149, 237));
                            window.canvas()->addLayer("Ventilation", QColor(0, 255, 255));
                            window.canvas()->addLayer("Services", QColor(255, 165, 0));
                            window.canvas()->addLayer("Safety", QColor(255, 0, 0));
                            break;
                            
                        case SurveyCategory::Topographic:
                            window.canvas()->addLayer("Contours", QColor(139, 69, 19));
                            window.canvas()->addLayer("Spot Levels", QColor(0, 255, 0));
                            window.canvas()->addLayer("Buildings", QColor(255, 255, 255));
                            window.canvas()->addLayer("Vegetation", QColor(34, 139, 34));
                            window.canvas()->addLayer("Water", QColor(0, 191, 255));
                            window.canvas()->addLayer("Roads", QColor(128, 128, 128));
                            break;
                            
                        case SurveyCategory::Geodetic:
                            window.canvas()->addLayer("Control Points", QColor(255, 0, 0));
                            window.canvas()->addLayer("Baselines", QColor(0, 255, 0));
                            window.canvas()->addLayer("Benchmarks", QColor(255, 255, 0));
                            window.canvas()->addLayer("Network", QColor(100, 149, 237));
                            break;
                            
                        case SurveyCategory::Engineering:
                        default:
                            window.canvas()->addLayer("Site Boundary", QColor(255, 255, 255));
                            window.canvas()->addLayer("Buildings", QColor(100, 149, 237));
                            window.canvas()->addLayer("Roads", QColor(128, 128, 128));
                            window.canvas()->addLayer("Services", QColor(255, 165, 0));
                            window.canvas()->addLayer("Setout Points", QColor(255, 0, 0));
                            window.canvas()->addLayer("Levels", QColor(0, 255, 0));
                            break;
                    }
                    
                    QString categoryStr = StartDialog::categoryToString(projectCategory);
                    window.setWindowTitle(QString("SiteSurveyor - New %1 (%2)").arg(templateName, categoryStr));
                }
            }
            
            window.showMaximized();
            
            // Run until window closes - then loop back to start dialog
            app.exec();
        }
        
        return 0;
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
