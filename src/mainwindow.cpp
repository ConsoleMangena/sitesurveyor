#include "mainwindow.h"
#include "pointmanager.h"
#include "canvaswidget.h"
#include "commandprocessor.h"
#include "settingsdialog.h"
#include "appsettings.h"
#include "joinpolardialog.h"
#include "polarinputdialog.h"
#include "iconmanager.h"
#include "layermanager.h"
#include "layerpanel.h"
#include "propertiespanel.h"
#include "welcomewidget.h"
#include "surveycalculator.h"
#include "traversedialog.h"
#include "projectplanpanel.h"
#include "intersectresectiondialog.h"
#include "levelingdialog.h"
#include "lsnetworkdialog.h"
#include "transformdialog.h"
#include "masspolardialog.h"
#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTableWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QHeaderView>
#include <QToolButton>
#include <QComboBox>
#include <QPixmap>
#include <QApplication>
#include <QUndoStack>
#include <QFrame>
#include <QStackedWidget>
#include <QTabWidget>
#include <QFile>
#include <QTextStream>
#include <QEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QDateTime>
#include <QSettings>
#include <QWidgetAction>
#include <limits>
#include <QtMath>
#include <QSet>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QToolTip>
#include <QPointer>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("SiteSurveyor");
    // Smooth, responsive dock animations and behavior
    setDockOptions(dockOptions() | QMainWindow::AllowTabbedDocks | QMainWindow::AllowNestedDocks | QMainWindow::AnimatedDocks);
    // Keep left/right/bottom docking areas consistent
    setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    // Ensure the menubar appears inside the window (useful across desktops)
    if (menuBar()) {
        menuBar()->setNativeMenuBar(false);
    }
    // Modern tooltips styling
    if (qApp) {
        const QString tipCss = "QToolTip { background-color: #2b2b2b; color: #f0f0f0; border: 1px solid #555; padding: 4px; }";
        qApp->setStyleSheet(qApp->styleSheet() + " " + tipCss);
    }

    // Initialize components
    m_pointManager = new PointManager(this);
    m_canvas = new CanvasWidget(this);
    m_canvas->setGaussMode(AppSettings::gaussMode());
    m_layerManager = new LayerManager(this);
    m_canvas->setLayerManager(m_layerManager);
    m_commandProcessor = new CommandProcessor(m_pointManager, m_canvas, this);
    m_settingsDialog = nullptr;
    m_joinDialog = nullptr;
    m_polarDialog = nullptr;
    m_traverseDialog = nullptr;
    m_layerPanel = nullptr;
    m_projectPlanPanel = nullptr;
    m_projectPlanDock = nullptr;
    m_layerCombo = nullptr;
    m_preferencesAction = nullptr;
    m_leftPanelButton = nullptr;
    m_rightPanelButton = nullptr;
    m_centerStack = nullptr;
    m_welcomeWidget = nullptr;
    // Initialize panel toggle actions to avoid using garbage pointers before setupMenus()
    m_toggleLeftPanelAction = nullptr;
    m_toggleRightPanelAction = nullptr;
    m_toggleCommandPanelAction = nullptr;
    m_toggleProjectPlanAction = nullptr;
    m_undoStack = new QUndoStack(this);
    m_canvas->setUndoStack(m_undoStack);
    
    // Initialize More dock pointers to prevent crashes
    m_moreDock = nullptr;
    m_moreButton = nullptr;
    m_morePinAction = nullptr;
    m_morePinned = false;
    
    toggleDarkMode(false);
    setupUI();
    setupMenus();
    setupToolbar();
    // Apply Engineering Surveying preset (layers, units) if needed
    applyEngineeringPresetIfNeeded();
    setupConnections();
    // Determine initial UI state based on license
    updateLicenseStateUI();
    // Always show Start page (Welcome) on app launch
    updatePanelToggleStates();
    
    updateStatus();

    // Autosave/recovery
    tryRecoverAutosave();
    setupAutosave();
}

void MainWindow::exportGeoJSON()
{
    const QString fileName = QFileDialog::getSaveFileName(this, "Export GeoJSON",
                                                          QString(),
                                                          "GeoJSON (*.geojson *.json);;All Files (*)");
    if (fileName.isEmpty()) return;

    // Build FeatureCollection
    QJsonObject root;
    root["type"] = QStringLiteral("FeatureCollection");

    // Optional CRS (non-standard for RFC 7946; include as property for reference)
    QJsonObject props;
    props["crs"] = AppSettings::crs();
    props["units"] = AppSettings::measurementUnits();
    root["properties"] = props;

    QJsonArray features;

    // Points
    const QVector<Point> pts = m_pointManager->getAllPoints();
    for (const auto& p : pts) {
        QJsonObject f;
        f["type"] = QStringLiteral("Feature");
        QJsonObject geom;
        geom["type"] = QStringLiteral("Point");
        QJsonArray coords; coords.append(p.x); coords.append(p.y); // GeoJSON is [x, y]
        geom["coordinates"] = coords;
        f["geometry"] = geom;
        QJsonObject fp;
        fp["name"] = p.name;
        fp["z"] = p.z;
        fp["layer"] = m_canvas ? m_canvas->pointLayer(p.name) : QStringLiteral("0");
        f["properties"] = fp;
        features.append(f);
    }

    // Lines
    if (m_canvas) {
        int lc = m_canvas->lineCount();
        for (int i = 0; i < lc; ++i) {
            QPointF a, b;
            if (!m_canvas->lineEndpoints(i, a, b)) continue;
            QJsonObject f;
            f["type"] = QStringLiteral("Feature");
            QJsonObject geom;
            geom["type"] = QStringLiteral("LineString");
            QJsonArray coords;
            {
                QJsonArray a0; a0.append(a.x()); a0.append(a.y()); coords.append(a0);
                QJsonArray a1; a1.append(b.x()); a1.append(b.y()); coords.append(a1);
            }
            geom["coordinates"] = coords;
            f["geometry"] = geom;
            QJsonObject fp; fp["layer"] = m_canvas->lineLayer(i);
            f["properties"] = fp;
            features.append(f);
        }
    }

    root["features"] = features;

    QJsonDocument doc(root);
    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Export GeoJSON", QString("Failed to write %1").arg(fileName));
        return;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    file.commit();
    if (m_commandOutput) m_commandOutput->append(QString("Exported GeoJSON to %1").arg(QFileInfo(fileName).fileName()));
    showToast(QStringLiteral("Exported GeoJSON"));
}

void MainWindow::importDXF()
{
    const QString fileName = QFileDialog::getOpenFileName(this, "Import DXF",
                                                          QString(),
                                                          "DXF (*.dxf);;All Files (*)");
    if (fileName.isEmpty()) return;
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Import DXF", QString("Failed to open %1").arg(fileName));
        return;
    }
    QTextStream in(&f);
    QStringList lines;
    while (!in.atEnd()) lines.append(in.readLine());
    f.close();

    auto aciToColor = [](int aci)->QColor{
        switch (aci) {
        case 1: return Qt::red; case 2: return Qt::yellow; case 3: return Qt::green;
        case 4: return Qt::cyan; case 5: return Qt::blue; case 6: return Qt::magenta;
        case 7: default: return QColor(255,255,255);
        }
    };

    QString section;
    bool inLayerTable = false;
    QVector<QPointF> curPoly; bool polyClosed = false; QString entityLayer = QStringLiteral("0");

    int i = 0;
    auto readPair = [&](int& code, QString& val)->bool {
        if (i+1 >= lines.size()) return false;
        bool ok = false; code = lines[i++].trimmed().toInt(&ok);
        val = (i < lines.size()) ? lines[i++].trimmed() : QString();
        if (!ok) code = -999; return true;
    };

    while (i+1 < lines.size()) {
        int code; QString val; if (!readPair(code, val)) break;
        if (code == 0) {
            if (val == QLatin1String("SECTION")) {
                int c2; QString sname; if (readPair(c2, sname) && c2 == 2) { section = sname; inLayerTable = false; }
                continue;
            }
            if (val == QLatin1String("ENDSEC")) { section.clear(); inLayerTable = false; continue; }
            if (section == QLatin1String("TABLES") && val == QLatin1String("TABLE")) {
                int c2; QString tname; if (readPair(c2, tname) && c2 == 2) { inLayerTable = (tname == QLatin1String("LAYER")); }
                continue;
            }
            if (section == QLatin1String("TABLES") && val == QLatin1String("ENDTAB")) { inLayerTable = false; continue; }
            if (section == QLatin1String("TABLES") && inLayerTable && val == QLatin1String("LAYER")) {
                QString lname = QStringLiteral("0"); int aci = 7;
                while (i+1 < lines.size()) {
                    int pcode; QString pval; int pos = i; if (!readPair(pcode, pval)) break;
                    if (pcode == 0) { i = pos; break; }
                    if (pcode == 2) lname = pval; else if (pcode == 62) aci = pval.toInt();
                }
                if (m_layerManager) {
                    if (!m_layerManager->hasLayer(lname)) m_layerManager->addLayer(lname, aciToColor(aci));
                    else m_layerManager->setLayerColor(lname, aciToColor(aci));
                }
                continue;
            }
            if (section == QLatin1String("ENTITIES")) {
                if (val == QLatin1String("LINE")) {
                    double x1=0,y1=0,x2=0,y2=0; entityLayer = QStringLiteral("0");
                    while (i+1 < lines.size()) {
                        int pcode; QString pval; int pos = i; if (!readPair(pcode, pval)) break;
                        if (pcode == 0) { i = pos; break; }
                        if (pcode == 8) entityLayer = pval;
                        else if (pcode == 10) x1 = pval.toDouble();
                        else if (pcode == 20) y1 = pval.toDouble();
                        else if (pcode == 11) x2 = pval.toDouble();
                        else if (pcode == 21) y2 = pval.toDouble();
                    }
                    if (m_canvas) {
                        m_canvas->addLine(QPointF(x1,y1), QPointF(x2,y2));
                        int idx = m_canvas->lineCount()-1; if (idx>=0) m_canvas->setLineLayer(idx, entityLayer);
                    }
                    continue;
                }
                if (val == QLatin1String("POINT")) {
                    double x=0,y=0,z=0; entityLayer = QStringLiteral("0");
                    while (i+1 < lines.size()) {
                        int pcode; QString pval; int pos = i; if (!readPair(pcode, pval)) break;
                        if (pcode == 0) { i = pos; break; }
                        if (pcode == 8) entityLayer = pval;
                        else if (pcode == 10) x = pval.toDouble();
                        else if (pcode == 20) y = pval.toDouble();
                        else if (pcode == 30) z = pval.toDouble();
                    }
                    QString name = QString("P%1").arg(m_pointManager ? (m_pointManager->getPointCount()+1) : 1, 3, 10, QChar('0'));
                    if (m_pointManager) m_pointManager->addPoint(name, x, y, z);
                    if (m_canvas) { Point p(name, x, y, z); m_canvas->addPoint(p); m_canvas->setPointLayer(name, entityLayer); }
                    continue;
                }
                if (val == QLatin1String("TEXT")) {
                    QString t; double x=0,y=0,h=2.0, ang=0.0; entityLayer = QStringLiteral("0");
                    while (i+1 < lines.size()) {
                        int pcode; QString pval; int pos = i; if (!readPair(pcode, pval)) break;
                        if (pcode == 0) { i = pos; break; }
                        if (pcode == 8) entityLayer = pval;
                        else if (pcode == 10) x = pval.toDouble();
                        else if (pcode == 20) y = pval.toDouble();
                        else if (pcode == 40) h = pval.toDouble();
                        else if (pcode == 50) ang = pval.toDouble();
                        else if (pcode == 1) t = pval;
                    }
                    if (!t.isEmpty() && m_canvas) m_canvas->addText(t, QPointF(x,y), h, ang, entityLayer);
                    continue;
                }
                if (val == QLatin1String("POLYLINE")) {
                    curPoly.clear(); polyClosed = false; entityLayer = QStringLiteral("0");
                    while (i+1 < lines.size()) {
                        int pcode; QString pval; int pos = i; if (!readPair(pcode, pval)) break;
                        if (pcode == 0) { i = pos; break; }
                        if (pcode == 8) entityLayer = pval;
                        else if (pcode == 70) { int flg = pval.toInt(); polyClosed = (flg & 1); }
                    }
                    // read VERTEX blocks until SEQEND
                    while (i+1 < lines.size()) {
                        int code2; QString val2; int pos2 = i; if (!readPair(code2, val2)) break;
                        if (code2 == 0 && val2 == QLatin1String("VERTEX")) {
                            double vx=0, vy=0;
                            while (i+1 < lines.size()) {
                                int c; QString v; int posv = i; if (!readPair(c, v)) break;
                                if (c == 0) { i = posv; break; }
                                if (c == 10) vx = v.toDouble(); else if (c == 20) vy = v.toDouble();
                            }
                            curPoly.append(QPointF(vx, vy));
                        } else if (code2 == 0 && val2 == QLatin1String("SEQEND")) {
                            break;
                        } else {
                            i = pos2; break;
                        }
                    }
                    if (!curPoly.isEmpty() && m_canvas) m_canvas->addPolylineEntity(curPoly, polyClosed, entityLayer);
                    continue;
                }
            }
        }
    }

    updatePointsTable();
    updateStatus();
    if (m_commandOutput) m_commandOutput->append(QString("Imported DXF %1").arg(QFileInfo(fileName).fileName()));
}

QString MainWindow::autosavePath() const
{
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(baseDir);
    return baseDir + "/autosave.json";
}

void MainWindow::autosaveNow()
{
    if (!AppSettings::autosaveEnabled()) return;
    // Serialize to JSON (points + lines + minimal settings)
    QJsonObject root; root["type"] = QStringLiteral("SiteSurveyorProject"); root["version"] = 1;
    QJsonObject settings;
    settings["gaussMode"] = AppSettings::gaussMode();
    settings["use3D"] = AppSettings::use3D();
    settings["units"] = AppSettings::measurementUnits();
    settings["angleFormat"] = AppSettings::angleFormat();
    settings["crs"] = AppSettings::crs();
    root["settings"] = settings;
    // Points
    QJsonArray points;
    const QVector<Point> pts = m_pointManager->getAllPoints();
    for (const auto& p : pts) {
        QJsonObject o; o["name"] = p.name; o["x"] = p.x; o["y"] = p.y; o["z"] = p.z; o["layer"] = m_canvas ? m_canvas->pointLayer(p.name) : QStringLiteral("0"); points.append(o);
    }
    root["points"] = points;
    // Lines
    QJsonArray lines;
    if (m_canvas) {
        int lc = m_canvas->lineCount();
        for (int i = 0; i < lc; ++i) {
            QPointF a,b; if (!m_canvas->lineEndpoints(i, a, b)) continue;
            QJsonObject o; o["ax"] = a.x(); o["ay"] = a.y(); o["bx"] = b.x(); o["by"] = b.y(); o["layer"] = m_canvas->lineLayer(i); lines.append(o);
        }
    }
    root["lines"] = lines;

    QJsonDocument doc(root);
    QSaveFile file(autosavePath());
    if (file.open(QIODevice::WriteOnly)) { file.write(doc.toJson(QJsonDocument::Compact)); file.commit(); }
}

void MainWindow::setupAutosave()
{
    if (m_autosaveTimer) { m_autosaveTimer->stop(); m_autosaveTimer->deleteLater(); m_autosaveTimer = nullptr; }
    if (!AppSettings::autosaveEnabled()) return;
    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setInterval(AppSettings::autosaveIntervalMinutes() * 60 * 1000);
    connect(m_autosaveTimer, &QTimer::timeout, this, &MainWindow::autosaveNow);
    m_autosaveTimer->start();
}

void MainWindow::tryRecoverAutosave()
{
    QFile f(autosavePath());
    if (!f.exists()) return;
    // Ask user to recover; if declined, keep file for next run
    auto ret = QMessageBox::question(this, "Recover Autosave",
                                     "An autosave from a previous session was found. Recover it now?",
                                     QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
    if (!f.open(QIODevice::ReadOnly)) return;
    const QByteArray data = f.readAll(); f.close();
    QJsonParseError err; QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;
    QJsonObject root = doc.object();
    if (root.value("type").toString() != QLatin1String("SiteSurveyorProject")) return;
    // Clear current
    m_pointManager->clearAllPoints();
    m_canvas->clearAll();
    // Reapply settings
    QJsonObject settings = root.value("settings").toObject();
    AppSettings::setGaussMode(settings.value("gaussMode").toBool(AppSettings::gaussMode()));
    AppSettings::setUse3D(settings.value("use3D").toBool(AppSettings::use3D()));
    if (settings.contains("units")) AppSettings::setMeasurementUnits(settings.value("units").toString());
    if (settings.contains("angleFormat")) AppSettings::setAngleFormat(settings.value("angleFormat").toString());
    if (settings.contains("crs")) AppSettings::setCrs(settings.value("crs").toString());
    // Points
    QJsonArray points = root.value("points").toArray();
    for (const auto& v : points) {
        QJsonObject o = v.toObject();
        QString name = o.value("name").toString();
        double x = o.value("x").toDouble(); double y = o.value("y").toDouble(); double z = o.value("z").toDouble();
        QString layer = o.value("layer").toString();
        if (name.isEmpty()) continue;
        Point p(name, x, y, z);
        m_pointManager->addPoint(p);
        m_canvas->addPoint(p);
        if (!layer.isEmpty()) m_canvas->setPointLayer(name, layer);
    }
    // Lines
    QJsonArray lines = root.value("lines").toArray();
    for (const auto& v : lines) {
        QJsonObject o = v.toObject();
        QPointF a(o.value("ax").toDouble(), o.value("ay").toDouble());
        QPointF b(o.value("bx").toDouble(), o.value("by").toDouble());
        QString layer = o.value("layer").toString();
        int before = m_canvas->lineCount();
        m_canvas->addLine(a, b);
        int idx = m_canvas->lineCount() - 1;
        if (idx >= 0 && !layer.isEmpty()) m_canvas->setLineLayer(idx, layer);
    }
    updatePointsTable();
    updateStatus();
    if (m_commandOutput) m_commandOutput->append("Recovered project from autosave.");
}

void MainWindow::applyEngineeringPresetIfNeeded()
{
    // Only apply when the selected discipline is Engineering Surveying
    const QString disc = AppSettings::discipline();
    if (disc.compare(QStringLiteral("Engineering Surveying"), Qt::CaseInsensitive) != 0) return;
    // One-time preference defaults
    if (!AppSettings::engineeringPresetApplied()) {
        AppSettings::setMeasurementUnits(QStringLiteral("metric"));
        AppSettings::setAngleFormat(QStringLiteral("dms"));
        if (AppSettings::crs().isEmpty()) {
            AppSettings::setCrs(QStringLiteral("EPSG:4326"));
        }
        AppSettings::setEngineeringPresetApplied(true);
    }

    // Always ensure common engineering layers exist (idempotent)
    if (m_layerManager) {
        auto ensureLayer = [&](const QString& name, const QColor& color){
            if (!m_layerManager->hasLayer(name)) {
                m_layerManager->addLayer(name, color);
            }
        };
        ensureLayer(QStringLiteral("Control"), QColor(0, 114, 189));      // blue
        ensureLayer(QStringLiteral("Survey"), QColor(46, 204, 113));       // green
        ensureLayer(QStringLiteral("Design"), QColor(26, 188, 156));       // teal
        ensureLayer(QStringLiteral("Setout"), QColor(241, 196, 15));       // yellow
        ensureLayer(QStringLiteral("Reference"), QColor(142, 68, 173));    // purple
        ensureLayer(QStringLiteral("Text"), QColor(236, 240, 241));        // light

        // Prefer Survey layer as current if it exists
        if (m_layerManager->hasLayer(QStringLiteral("Survey"))) {
            m_layerManager->setCurrentLayer(QStringLiteral("Survey"));
        }
    }

    // Refresh UI bits that depend on layers
    refreshLayerCombo();
    updateLayerStatusText();
    if (m_canvas) m_canvas->update();
}

void MainWindow::resetLayout()
{
    if (!m_defaultLayoutState.isEmpty()) {
        restoreState(m_defaultLayoutState);
        // Nudge default sizes again for good measure (keep Layers smaller)
        int rpw = AppSettings::rightPanelWidth();
        if (rpw < 200) rpw = 200;
        if (m_layersDock) resizeDocks({m_layersDock}, {rpw}, Qt::Horizontal);
        if (m_pointsDock) resizeDocks({m_pointsDock}, {360}, Qt::Horizontal);
        if (m_commandDock) resizeDocks({m_commandDock}, {200}, Qt::Vertical);
    }
}

void MainWindow::showSelectedProperties()
{
    if (!m_canvas) return;
    int li = m_canvas->selectedLineIndex();
    if (li >= 0) {
        QPointF a, b;
        if (m_canvas->lineEndpoints(li, a, b)) {
            const QString layer = m_canvas->lineLayer(li);
            const double len = SurveyCalculator::distance(a, b);
            QMessageBox::information(this, "Properties",
                QString("Line #%1\nLayer: %2\nStart: (%3, %4)\nEnd: (%5, %6)\nLength: %7")
                    .arg(li)
                    .arg(layer)
                    .arg(a.x(), 0, 'f', 3)
                    .arg(a.y(), 0, 'f', 3)
                    .arg(b.x(), 0, 'f', 3)
                    .arg(b.y(), 0, 'f', 3)
                    .arg(len, 0, 'f', 3));
            return;
        }
    }
    QMessageBox::information(this, "Properties", "No detailed selection. Select a single line to view properties.");
}

void MainWindow::toggleDarkMode(bool on)
{
    m_darkMode = on;
    if (on) {
        // Apply dark stylesheet, then enforce white text globally
        qApp->setPalette(QApplication::style()->standardPalette());
        QFile qss(":/styles/freecad-dark.qss");
        QString style;
        if (qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
            style = QString::fromUtf8(qss.readAll());
        }
        // Use stylesheet colors only; avoid forcing global colors to keep contrast consistent
        qApp->setStyleSheet(style);
        // White icons in dark mode
        IconManager::setMonochrome(true);
        IconManager::setMonochromeColor(Qt::white);
        // Dark theme for command output panel
        if (m_commandOutput) m_commandOutput->setStyleSheet("QTextEdit { color: #ffffff; background-color: #202020; }");
    } else {
        // Light mode
        qApp->setPalette(QApplication::style()->standardPalette());
        qApp->setStyleSheet("");
        IconManager::setMonochrome(true);
        IconManager::setMonochromeColor(Qt::black);
        if (m_commandOutput) m_commandOutput->setStyleSheet("QTextEdit { color: #000000; background-color: #f0f0f0; }");
    }
    // Rebuild toolbars so icons pick up monochrome state, but only if they already exist
    QList<QToolBar*> bars = findChildren<QToolBar*>();
    bool hadBars = false;
    for (QToolBar* b : bars) {
        if (!b) continue;
        const QString obj = b->objectName();
        if (obj == QLatin1String("TopBarRow1") || obj == QLatin1String("TopBarRow2")) {
            hadBars = true;
            removeToolBar(b);
            b->deleteLater();
        }
    }
    if (hadBars) setupToolbar();
    // Persist preference
    AppSettings::setDarkMode(on);
}

MainWindow::~MainWindow()
{
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateToggleButtonPositions();
    updateMoreDock();
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    // Detect user-initiated close on right docks so we can close both in sync
    if (obj == m_layersDock) {
        if (event->type() == QEvent::Close) {
            m_rightDockClosingByUser = true;
        } else if (event->type() == QEvent::Resize) {
            if (m_layersDock) {
                int w = m_layersDock->width();
                if (w > 0) AppSettings::setRightPanelWidth(w);
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::setupUI()
{
    // Central widget: stacked between Welcome and Canvas
    m_centerStack = new QStackedWidget(this);
    // Ensure canvas exists already
    if (m_canvas) m_centerStack->addWidget(m_canvas);
    // Welcome page with license entry
    m_welcomeWidget = new WelcomeWidget(this);
    connect(m_welcomeWidget, &WelcomeWidget::activated, this, &MainWindow::onLicenseActivated);
    connect(m_welcomeWidget, &WelcomeWidget::disciplineChanged, this, &MainWindow::onLicenseActivated);
    // Start tab actions
    connect(m_welcomeWidget, &WelcomeWidget::newProjectRequested, this, &MainWindow::newProject);
    connect(m_welcomeWidget, &WelcomeWidget::openProjectRequested, this, &MainWindow::openProject);
    connect(m_welcomeWidget, &WelcomeWidget::openPathRequested, this, &MainWindow::importCoordinatesFrom);
    m_centerStack->addWidget(m_welcomeWidget);
    setCentralWidget(m_centerStack);
    
    // Create dock widgets and arrange panels
    setupCommandDock();    // Command line at bottom
    setupLayersDock();     // Layers/Properties merged on right (tabbed inside one dock)
    setupPointsDock();     // Coordinates on left (wider)
    setupProjectPlanDock(); // Project planning panel (left, tabbed with coordinates)
    
    // Initial dock arrangement
    
    // Set initial widths: left (Coordinates) wide, right (Layers/Properties) reasonable default
    if (m_layersDock) {
        int rpw = AppSettings::rightPanelWidth();
        // Enforce a sensible minimum so the sidebar content is visible
        if (rpw < 200) rpw = 200;
        resizeDocks({m_layersDock}, {rpw}, Qt::Horizontal);
    }
    if (m_pointsDock) resizeDocks({m_pointsDock}, {360}, Qt::Horizontal);
    
    // Bottom: Command line (compact height)
    if (m_commandDock) {
        resizeDocks({m_commandDock}, {120}, Qt::Vertical);
    }
    
    // Save default layout state for reset
    m_defaultLayoutState = saveState();
    
    // Toggle states will be synced after menus/toolbars are created
    
    // Status bar with coordinate display and mode toggles
    m_statusBar = statusBar();
    m_statusBar->setStyleSheet("QStatusBar { border-top: 1px solid #999; }");
    
    // Left side: Coordinates
    m_coordLabel = new QLabel(AppSettings::gaussMode() ? "Y: 0.000  X: 0.000" : "X: 0.000  Y: 0.000");
    m_coordLabel->setStyleSheet("font-family: Consolas, monospace; font-weight: bold;");
    m_coordLabel->setMinimumWidth(200);
    m_statusBar->addWidget(m_coordLabel);
    
    // Separator
    QFrame* sep1 = new QFrame();
    sep1->setFrameStyle(QFrame::VLine | QFrame::Sunken);
    m_statusBar->addWidget(sep1);
    
    // Drawing aids toggles (SNAP, GRID, ORTHO, OSNAP)
    m_snapButton = new QToolButton(this);
    m_snapButton->setText("SNAP");
    m_snapButton->setCheckable(true);
    m_snapButton->setAutoRaise(false);
    m_snapButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_snapButton->setMinimumWidth(45);
    m_snapAction = new QAction("SNAP", this);
    m_snapAction->setCheckable(true);
    m_snapAction->setShortcut(QKeySequence(Qt::Key_F9));
    m_snapButton->setDefaultAction(m_snapAction);
    connect(m_snapAction, &QAction::toggled, this, [this](bool on){
        m_canvas->setSnapMode(on);
        m_snapButton->setStyleSheet(on ? "QToolButton { background-color: #4a90d9; color: white; }" : "");
    });
    addAction(m_snapAction);
    m_statusBar->addWidget(m_snapButton);
    
    // GRID toggle (F7)
    QToolButton* gridButton = new QToolButton(this);
    gridButton->setText("GRID");
    gridButton->setCheckable(true);
    gridButton->setChecked(true);
    gridButton->setAutoRaise(false);
    gridButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    gridButton->setMinimumWidth(45);
    QAction* gridAction = new QAction("GRID", this);
    gridAction->setCheckable(true);
    gridAction->setChecked(true);
    gridAction->setShortcut(QKeySequence(Qt::Key_F7));
    gridButton->setDefaultAction(gridAction);
    // Track for license locking
    m_gridButton = gridButton;
    m_gridAction = gridAction;
    connect(gridAction, &QAction::toggled, this, [this, gridButton](bool on){
        m_canvas->setShowGrid(on);
        gridButton->setStyleSheet(on ? "QToolButton { background-color: #4a90d9; color: white; }" : "");
    });
    addAction(gridAction);
    m_statusBar->addWidget(gridButton);
    
    // ORTHO toggle (F8)
    m_orthoButton = new QToolButton(this);
    m_orthoButton->setText("ORTHO");
    m_orthoButton->setCheckable(true);
    m_orthoButton->setAutoRaise(false);
    m_orthoButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_orthoButton->setMinimumWidth(50);
    m_orthoAction = new QAction("ORTHO", this);
    m_orthoAction->setCheckable(true);
    m_orthoAction->setShortcut(QKeySequence(Qt::Key_F8));
    m_orthoButton->setDefaultAction(m_orthoAction);
    connect(m_orthoAction, &QAction::toggled, this, [this](bool on){
        m_canvas->setOrthoMode(on);
        m_orthoButton->setStyleSheet(on ? "QToolButton { background-color: #4a90d9; color: white; }" : "");
    });
    addAction(m_orthoAction);
    m_statusBar->addWidget(m_orthoButton);
    
    // OSNAP toggle (F3)  
    m_osnapButton = new QToolButton(this);
    m_osnapButton->setText("OSNAP");
    m_osnapButton->setCheckable(true);
    m_osnapButton->setChecked(true);
    m_osnapButton->setAutoRaise(false);
    m_osnapButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_osnapButton->setMinimumWidth(50);
    m_osnapAction = new QAction("OSNAP", this);
    m_osnapAction->setCheckable(true);
    m_osnapAction->setChecked(true);
    m_osnapAction->setShortcut(QKeySequence(Qt::Key_F3));
    m_osnapButton->setDefaultAction(m_osnapAction);
    connect(m_osnapAction, &QAction::toggled, this, [this](bool on){
        m_canvas->setOsnapMode(on);
        m_osnapButton->setStyleSheet(on ? "QToolButton { background-color: #4a90d9; color: white; }" : "");
    });
    addAction(m_osnapAction);
    m_statusBar->addWidget(m_osnapButton);
    
    // Separator
    QFrame* sep2 = new QFrame();
    sep2->setFrameStyle(QFrame::VLine | QFrame::Sunken);
    m_statusBar->addWidget(sep2);

    // POLAR toggle (F10)
    m_polarButton = new QToolButton(this);
    m_polarButton->setText("POLAR");
    m_polarButton->setCheckable(true);
    m_polarButton->setAutoRaise(false);
    m_polarButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_polarButton->setMinimumWidth(55);
    m_polarAction = new QAction("POLAR", this);
    m_polarAction->setCheckable(true);
    m_polarAction->setShortcut(QKeySequence(Qt::Key_F10));
    m_polarButton->setDefaultAction(m_polarAction);
    connect(m_polarAction, &QAction::toggled, this, [this](bool on){
        m_canvas->setPolarMode(on);
        m_polarButton->setStyleSheet(on ? "QToolButton { background-color: #4a90d9; color: white; }" : "");
    });
    addAction(m_polarAction);
    m_statusBar->addWidget(m_polarButton);

    // OTRACK toggle (F11)
    m_otrackButton = new QToolButton(this);
    m_otrackButton->setText("OTRACK");
    m_otrackButton->setCheckable(true);
    m_otrackButton->setAutoRaise(false);
    m_otrackButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_otrackButton->setMinimumWidth(65);
    m_otrackAction = new QAction("OTRACK", this);
    m_otrackAction->setCheckable(true);
    m_otrackAction->setShortcut(QKeySequence(Qt::Key_F11));
    m_otrackButton->setDefaultAction(m_otrackAction);
    connect(m_otrackAction, &QAction::toggled, this, [this](bool on){
        m_canvas->setOtrackMode(on);
        m_otrackButton->setStyleSheet(on ? "QToolButton { background-color: #4a90d9; color: white; }" : "");
    });
    addAction(m_otrackAction);
    m_statusBar->addWidget(m_otrackButton);

    // DYN (Dynamic Input) toggle (F12)
    m_dynButton = new QToolButton(this);
    m_dynButton->setText("DYN");
    m_dynButton->setCheckable(true);
    m_dynButton->setChecked(true);
    m_dynButton->setAutoRaise(false);
    m_dynButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_dynButton->setMinimumWidth(45);
    m_dynAction = new QAction("DYN", this);
    m_dynAction->setCheckable(true);
    m_dynAction->setChecked(true);
    m_dynAction->setShortcut(QKeySequence(Qt::Key_F12));
    m_dynButton->setDefaultAction(m_dynAction);
    connect(m_dynAction, &QAction::toggled, this, [this](bool on){
        m_canvas->setDynInputEnabled(on);
        m_dynButton->setStyleSheet(on ? "QToolButton { background-color: #4a90d9; color: white; }" : "");
    });
    addAction(m_dynAction);
    m_statusBar->addWidget(m_dynButton);
    
    // Status information
    m_layerStatusLabel = new QLabel("Layer: 0");
    m_statusBar->addWidget(m_layerStatusLabel);
    
    m_statusBar->addWidget(new QLabel(" | "));
    m_pointCountLabel = new QLabel("Points: 0");
    m_statusBar->addWidget(m_pointCountLabel);
    
    m_statusBar->addWidget(new QLabel(" | "));
    m_zoomLabel = new QLabel("Zoom: 100%");
    m_statusBar->addWidget(m_zoomLabel);
    
    m_statusBar->addWidget(new QLabel(" | "));
    m_measureLabel = new QLabel("");
    m_statusBar->addWidget(m_measureLabel);

    m_statusBar->addWidget(new QLabel(" | "));
    m_selectionLabel = new QLabel("Selected: 0 pts, 0 lines");
    m_statusBar->addWidget(m_selectionLabel);
    
    updateLayerStatusText();
}

void MainWindow::onLicenseActivated()
{
    // User entered/saved license key from the welcome page
    applyEngineeringPresetIfNeeded();
    updateLicenseStateUI();
}

void MainWindow::onSelectionChanged(int points, int lines)
{
    if (m_selectionLabel) {
        m_selectionLabel->setText(QString("Selected: %1 pts, %2 lines").arg(points).arg(lines));
    }
}

void MainWindow::updateLicenseStateUI()
{
    // Gate UI by license state (set when admin issues access token) with offline grace TTL
    const bool hasLocal = AppSettings::hasLicenseFor(AppSettings::discipline());
    QSettings s;
    const int graceDays = s.value("license/offlineGraceDays", 5).toInt();
    const QString ts = s.value("license/lastVerifiedUtc").toString();
    const QDateTime last = QDateTime::fromString(ts, Qt::ISODate);
    bool withinGrace = last.isValid() && last.secsTo(QDateTime::currentDateTimeUtc()) <= graceDays * 86400;
    const bool licensed = hasLocal && withinGrace;

    // Switch central page
    if (m_centerStack && m_canvas && m_welcomeWidget) {
        int canvasIdx = m_centerStack->indexOf(m_canvas);
        int welcomeIdx = m_centerStack->indexOf(m_welcomeWidget);
        if (licensed && canvasIdx >= 0) m_centerStack->setCurrentIndex(canvasIdx);
        else if (welcomeIdx >= 0) m_centerStack->setCurrentIndex(welcomeIdx);
    }

    // Show/hide docks
    if (m_pointsDock) m_pointsDock->setVisible(licensed);
    if (m_layersDock) m_layersDock->setVisible(licensed);
    if (m_commandDock) m_commandDock->setVisible(licensed);

    // Show/hide all toolbars
    const auto toolbars = findChildren<QToolBar*>();
    for (QToolBar* tb : toolbars) {
        if (tb) tb->setVisible(licensed);
    }

    // Disable/enable all actions globally based on license
    const auto allActions = findChildren<QAction*>();
    for (QAction* act : allActions) {
        if (!act) continue;
        act->setEnabled(licensed);
    }
    // Menus themselves (QMenu has an associated menuAction)
    const auto menus = findChildren<QMenu*>();
    for (QMenu* m : menus) {
        if (!m) continue;
        if (m->menuAction()) m->menuAction()->setEnabled(licensed);
    }

    // When unlicensed, re-enable only License entry (and optionally Exit/About)
    if (!licensed) {
        if (m_leftPanelButton) m_leftPanelButton->setVisible(false);
        if (m_rightPanelButton) m_rightPanelButton->setVisible(false);

        if (m_settingsMenu && m_settingsMenu->menuAction()) m_settingsMenu->menuAction()->setEnabled(true);
        // Keep Exit and About accessible if present
        if (m_exitAction) m_exitAction->setEnabled(true);
        if (m_aboutAction) m_aboutAction->setEnabled(true);

        // Ensure Preferences is disabled while unlicensed
        if (m_preferencesAction) m_preferencesAction->setEnabled(false);
    } else {
        // Licensed: restore panel action states and handles
        if (m_toggleLeftPanelAction) m_toggleLeftPanelAction->setEnabled(true);
        if (m_toggleRightPanelAction) m_toggleRightPanelAction->setEnabled(true);
        if (m_toggleCommandPanelAction) m_toggleCommandPanelAction->setEnabled(true);
        updatePanelToggleStates();
    }
}

void MainWindow::setupPointsDock()
{
    m_pointsDock = new QDockWidget("Coordinates", this);
    m_pointsDock->setObjectName("PointsDock");
    m_pointsDock->setAllowedAreas(Qt::LeftDockWidgetArea);
    m_pointsDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    m_pointsDock->setMinimumWidth(300);
    
    m_pointsTable = new QTableWidget(0, 5);
    {
        QStringList headers = AppSettings::gaussMode()
            ? (QStringList() << "Name" << "Y" << "X" << "Z" << "Layer")
            : (QStringList() << "Name" << "X" << "Y" << "Z" << "Layer");
        m_pointsTable->setHorizontalHeaderLabels(headers);
    }
    m_pointsTable->horizontalHeader()->setStretchLastSection(true);
    m_pointsTable->setAlternatingRowColors(true);
    m_pointsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pointsTable->setColumnHidden(3, !AppSettings::use3D());
    
    m_pointsDock->setWidget(m_pointsTable);
    addDockWidget(Qt::LeftDockWidgetArea, m_pointsDock);
    connect(m_pointsDock, &QDockWidget::visibilityChanged, this, [this](bool){ updatePanelToggleStates(); });

    // Coordinates table interactions
    connect(m_pointsTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::onPointsTableSelectionChanged);
    connect(m_pointsTable, &QTableWidget::itemChanged, this, &MainWindow::onPointsCellChanged);
}

void MainWindow::setupCommandDock()
{
    // Command line at bottom
    QDockWidget* commandDock = new QDockWidget("Command Line", this);
    commandDock->setObjectName("CommandDock");
    commandDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    commandDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    
    QWidget* commandWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(commandWidget);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);
    
    // Command history (compact)
    m_commandOutput = new QTextEdit();
    m_commandOutput->setReadOnly(true);
    m_commandOutput->setMaximumHeight(60);
    m_commandOutput->setFont(QFont("Consolas", 9));
    m_commandOutput->setStyleSheet("QTextEdit { background-color: #f0f0f0; }");
    
    // Command input line
    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel* cmdLabel = new QLabel("Command:");
    cmdLabel->setStyleSheet("font-weight: bold;");
    
    m_commandInput = new QLineEdit();
    m_commandInput->setPlaceholderText("Type a command or press F1 for help");
    m_commandInput->setFont(QFont("Consolas", 10));
    
    inputLayout->addWidget(cmdLabel);
    inputLayout->addWidget(m_commandInput);
    
    layout->addWidget(m_commandOutput);
    layout->addLayout(inputLayout);
    
    commandDock->setWidget(commandWidget);
    addDockWidget(Qt::BottomDockWidgetArea, commandDock);
    m_commandDock = commandDock;
    connect(m_commandDock, &QDockWidget::visibilityChanged, this, [this](bool){ updatePanelToggleStates(); });
    
    // Connect command input (press Enter to execute)
    connect(m_commandInput, &QLineEdit::returnPressed, this, &MainWindow::executeCommand);
}

void MainWindow::executeCommand()
{
    if (!m_commandProcessor || !m_commandInput) return;
    const QString cmd = m_commandInput->text().trimmed();
    if (cmd.isEmpty()) return;

    if (m_commandOutput) m_commandOutput->append(QString("> %1").arg(cmd));
    const QString result = m_commandProcessor->processCommand(cmd);
    if (m_commandOutput && !result.trimmed().isEmpty()) m_commandOutput->append(result);

    // Refresh UI to reflect changes made by commands
    updatePointsTable();
    updateStatus();

    m_commandInput->clear();
}

void MainWindow::setupLayersDock()
{
    // Single right dock with tabs for Layers and Properties
    m_layersDock = new QDockWidget("Layers", this);
    m_layersDock->setObjectName("LayersDock");
    m_layersDock->setAllowedAreas(Qt::RightDockWidgetArea);
    m_layersDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    m_layersDock->setMinimumWidth(180);

    // Build tab widget
    QTabWidget* tabs = new QTabWidget(this);
    m_rightTabs = tabs;
    tabs->setTabPosition(QTabWidget::North);
    tabs->setMinimumWidth(180);
    tabs->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    // Layers tab
    m_layerPanel = new LayerPanel(this);
    m_layerPanel->setMinimumWidth(180);
    m_layerPanel->setLayerManager(m_layerManager);
    connect(m_layerManager, &LayerManager::layersChanged, m_layerPanel, &LayerPanel::reload);
    connect(m_layerPanel, &LayerPanel::requestSetCurrent, this, [this](const QString& name){ if (m_layerManager) m_layerManager->setCurrentLayer(name); });
    tabs->addTab(m_layerPanel, QIcon(), QStringLiteral("Layers"));

    // Properties tab
    m_propertiesPanel = new PropertiesPanel(this);
    m_propertiesPanel->setMinimumWidth(180);
    m_propertiesPanel->setLayerManager(m_layerManager);
    m_propertiesPanel->setCanvas(m_canvas);
    tabs->addTab(m_propertiesPanel, QIcon(), QStringLiteral("Properties"));

    m_layersDock->setWidget(tabs);
    addDockWidget(Qt::RightDockWidgetArea, m_layersDock);
    connect(m_layersDock, &QDockWidget::visibilityChanged, this, &MainWindow::onRightDockVisibilityChanged);
    m_layersDock->installEventFilter(this);

    // No separate properties dock when merged
    m_propertiesDock = nullptr;
}


void MainWindow::setupMenus()
{
    // File Menu
    QMenu* fileMenu = menuBar()->addMenu("&File");
    m_fileMenu = fileMenu;
    
    QAction* newProject = fileMenu->addAction("&New Project");
    newProject->setIcon(IconManager::iconUnique("file-plus", "file_new", "N"));
    newProject->setShortcut(QKeySequence::New);
    connect(newProject, &QAction::triggered, this, &MainWindow::newProject);
    m_newProjectAction = newProject;
    
    QAction* openProject = fileMenu->addAction("&Open Project...");
    openProject->setIcon(IconManager::iconUnique("folder-open", "file_open", "O"));
    openProject->setShortcut(QKeySequence::Open);
    connect(openProject, &QAction::triggered, this, &MainWindow::openProject);
    m_openProjectAction = openProject;
    
    QAction* saveProject = fileMenu->addAction("&Save Project");
    saveProject->setIcon(IconManager::iconUnique("device-floppy", "file_save", "S"));
    saveProject->setShortcut(QKeySequence::Save);
    connect(saveProject, &QAction::triggered, this, &MainWindow::saveProject);
    m_saveProjectAction = saveProject;
    
    fileMenu->addSeparator();
    
    QAction* importPoints = fileMenu->addAction("&Import Coordinates...");
    importPoints->setIcon(IconManager::iconUnique("file-import", "import_points", "IM"));
    connect(importPoints, &QAction::triggered, this, &MainWindow::importCoordinates);
    m_importPointsAction = importPoints;
    QAction* exportPoints = fileMenu->addAction("&Export Coordinates (CSV)...");
    exportPoints->setIcon(IconManager::iconUnique("file-export", "export_points", "EX"));
    connect(exportPoints, &QAction::triggered, this, &MainWindow::exportCoordinates);
    m_exportPointsAction = exportPoints;

    QAction* exportGeoJson = fileMenu->addAction("Export &GeoJSON...");
    exportGeoJson->setIcon(IconManager::iconUnique("file-export", "export_geojson", "GJ"));
    connect(exportGeoJson, &QAction::triggered, this, &MainWindow::exportGeoJSON);
    m_exportGeoJsonAction = exportGeoJson;
    QAction* exportDxf = fileMenu->addAction("Export D&XF (R12)...");
    exportDxf->setIcon(IconManager::iconUnique("file-export", "export_dxf", "DX"));
    connect(exportDxf, &QAction::triggered, this, [this](){
        const QString fileName = QFileDialog::getSaveFileName(this, "Export DXF (R12)", QString(), "DXF (*.dxf);;All Files (*)");
        if (fileName.isEmpty()) return;
        QFile f(fileName);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { QMessageBox::warning(this, "Export DXF", QString("Failed to write %1").arg(fileName)); return; }
        QTextStream out(&f);
        // Minimal ASCII DXF (R12)
        // HEADER
        out << "0\nSECTION\n2\nHEADER\n9\n$ACADVER\n1\nAC1009\n0\nENDSEC\n";
        // TABLES (LTYPE, LAYER)
        out << "0\nSECTION\n2\nTABLES\n";
        // LTYPE table
        out << "0\nTABLE\n2\nLTYPE\n70\n1\n";
        out << "0\nLTYPE\n2\nCONTINUOUS\n70\n64\n3\nSolid line\n72\n65\n73\n0\n40\n0.0\n";
        out << "0\nENDTAB\n";
        // LAYER table
        int layerCount = 0;
        QVector<Layer> layers = m_layerManager ? m_layerManager->layers() : QVector<Layer>{};
        bool hasZero = false;
        for (const auto& L : layers) if (L.name == QLatin1String("0")) { hasZero = true; break; }
        if (!hasZero) {
            Layer L; L.name = QStringLiteral("0"); L.color = QColor(255,255,255); L.visible = true; L.locked = false; layers.prepend(L);
        }
        layerCount = layers.size();
        out << "0\nTABLE\n2\nLAYER\n70\n" << layerCount << "\n";
        auto toACI = [](const QColor& c){
            if (!c.isValid()) return 7; // white
            // very small mapping to ACI 1..7
            if (c == Qt::red) return 1;
            if (c == Qt::yellow) return 2;
            if (c == Qt::green) return 3;
            if (c == Qt::cyan) return 4;
            if (c == Qt::blue) return 5;
            if (c == Qt::magenta) return 6;
            // grayscale heuristic
            int g = qGray(c.rgb());
            if (g > 200) return 7;
            if (g > 128) return 8;
            return 9;
        };
        for (const auto& L : layers) {
            out << "0\nLAYER\n2\n" << L.name << "\n70\n0\n62\n" << toACI(L.color) << "\n6\nCONTINUOUS\n";
        }
        out << "0\nENDTAB\n0\nENDSEC\n";
        // ENTITIES
        out << "0\nSECTION\n2\nENTITIES\n";
        // Polylines (R12 POLYLINE/VERTEX format)
        if (m_canvas) {
            int pln = m_canvas->polylineCount();
            for (int i=0;i<pln;++i) {
                QVector<QPointF> pts; bool closed=false; QString layer;
                if (!m_canvas->polylineData(i, pts, closed, layer)) continue;
                if (pts.size() < 2) continue;
                out << "0\nPOLYLINE\n8\n" << layer << "\n66\n1\n70\n" << (closed?1:0) << "\n";
                for (const auto& p : pts) {
                    out << "0\nVERTEX\n8\n" << layer << "\n10\n" << p.x() << "\n20\n" << p.y() << "\n30\n0\n";
                }
                out << "0\nSEQEND\n";
            }
        }
        // Lines (skip those that belong to polylines)
        QSet<int> used;
        if (m_canvas) m_canvas->linesUsedByPolylines(used);
        if (m_canvas) {
            for (int i=0;i<m_canvas->lineCount();++i) {
                if (used.contains(i)) continue;
                QPointF a,b; if (!m_canvas->lineEndpoints(i,a,b)) continue; QString layer = m_canvas->lineLayer(i);
                out << "0\nLINE\n8\n" << layer << "\n10\n" << a.x() << "\n20\n" << a.y() << "\n30\n0\n11\n" << b.x() << "\n21\n" << b.y() << "\n31\n0\n";
            }
        }
        // Points
        for (const auto& p : m_pointManager->getAllPoints()) {
            QString layer = m_canvas ? m_canvas->pointLayer(p.name) : QStringLiteral("0");
            out << "0\nPOINT\n8\n" << layer << "\n10\n" << p.x << "\n20\n" << p.y << "\n30\n" << p.z << "\n";
        }
        // Texts
        if (m_canvas) {
            const int tn = m_canvas->textCount();
            for (int i=0;i<tn;++i) {
                QString t; QPointF pos; double h=0.0, ang=0.0; QString layer;
                if (!m_canvas->textData(i, t, pos, h, ang, layer)) continue;
                if (t.isEmpty() || h <= 0) continue;
                out << "0\nTEXT\n8\n" << layer
                    << "\n10\n" << pos.x() << "\n20\n" << pos.y() << "\n30\n0\n"
                    << "40\n" << h << "\n1\n" << t << "\n50\n" << ang << "\n";
            }
        }
        // Dimensions (exported as LINE + TEXT at midpoint)
        if (m_canvas) {
            const int dn = m_canvas->dimensionCount();
            for (int i=0;i<dn;++i) {
                QPointF a,b; double th=0.0; QString layer;
                if (!m_canvas->dimensionData(i, a, b, th, layer)) continue;
                out << "0\nLINE\n8\n" << layer << "\n10\n" << a.x() << "\n20\n" << a.y() << "\n30\n0\n11\n" << b.x() << "\n21\n" << b.y() << "\n31\n0\n";
                QPointF mid((a.x()+b.x())*0.5, (a.y()+b.y())*0.5);
                double len = SurveyCalculator::distance(a, b);
                double ang = qRadiansToDegrees(qAtan2(b.y()-a.y(), b.x()-a.x()));
                out << "0\nTEXT\n8\n" << layer
                    << "\n10\n" << mid.x() << "\n20\n" << mid.y() << "\n30\n0\n"
                    << "40\n" << (th>0?th:2.0) << "\n1\n" << QString::number(len, 'f', 3) << "\n50\n" << ang << "\n";
            }
        }
        out << "0\nENDSEC\n0\nEOF\n";
        f.close(); if (m_commandOutput) m_commandOutput->append(QString("Exported DXF to %1").arg(QFileInfo(fileName).fileName()));
    });
    QAction* importDxf = fileMenu->addAction("&Import DXF...");
    importDxf->setIcon(IconManager::icon("file-import"));
    connect(importDxf, &QAction::triggered, this, &MainWindow::importDXF);
    
    fileMenu->addSeparator();
    
    QAction* exitAction = fileMenu->addAction("E&xit");
    m_exitAction = exitAction;
    exitAction->setIcon(IconManager::icon("logout-2"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    
    // Edit Menu
    QMenu* editMenu = menuBar()->addMenu("&Edit");
    m_editMenu = editMenu;
    // Undo/Redo actions
    m_undoAction = editMenu->addAction("Undo");
    m_undoAction->setIcon(IconManager::icon("arrow-back-up"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    connect(m_undoAction, &QAction::triggered, this, [this](){ if (m_undoStack) m_undoStack->undo(); });
    m_redoAction = editMenu->addAction("Redo");
    m_redoAction->setIcon(IconManager::icon("arrow-forward-up"));
    m_redoAction->setShortcut(QKeySequence::Redo);
    connect(m_redoAction, &QAction::triggered, this, [this](){ if (m_undoStack) m_undoStack->redo(); });
    editMenu->addSeparator();
    // Delete Selected
    m_deleteSelectedAction = editMenu->addAction("Delete Selected");
    m_deleteSelectedAction->setIcon(IconManager::icon("trash"));
    m_deleteSelectedAction->setShortcut(QKeySequence::Delete);
    connect(m_deleteSelectedAction, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->deleteSelected(); });
    
    QAction* addPointAction = editMenu->addAction("&Add Coordinate...");
    addPointAction->setIcon(IconManager::icon("add-point"));
    addPointAction->setShortcut(QKeySequence("Ctrl+N"));
    connect(addPointAction, &QAction::triggered, this, &MainWindow::showAddPointDialog);
    
    QAction* deletePointAction = editMenu->addAction("&Delete Coordinate");
    deletePointAction->setIcon(IconManager::icon("trash"));
    // Avoid conflict with global Delete (geometry). Use Shift+Delete for coordinates.
    deletePointAction->setShortcut(QKeySequence("Shift+Delete"));
    connect(deletePointAction, &QAction::triggered, this, &MainWindow::deleteSelectedCoordinates);
    editMenu->addSeparator();
    // Selection operations
    QAction* selectAllAct = editMenu->addAction("Select All");
    selectAllAct->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAct, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->selectAllVisible(); });
    QAction* invertSelAct = editMenu->addAction("Invert Selection");
    invertSelAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+I")));
    connect(invertSelAct, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->invertSelectionVisible(); });
    QAction* selectByLayerAct = editMenu->addAction("Select by Current Layer");
    connect(selectByLayerAct, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->selectByCurrentLayer(); });
    QAction* hideSelLayersAct = editMenu->addAction("Hide Selected Layers");
    hideSelLayersAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+H")));
    connect(hideSelLayersAct, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->hideSelectedLayers(); });
    QAction* isolateSelLayersAct = editMenu->addAction("Isolate Selection Layers");
    isolateSelLayersAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+I")));
    connect(isolateSelLayersAct, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->isolateSelectionLayers(); });
    QAction* lockSelLayersAct = editMenu->addAction("Lock Selected Layers");
    lockSelLayersAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+L")));
    connect(lockSelLayersAct, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->lockSelectedLayers(); });
    
    editMenu->addSeparator();
    
    QAction* clearAllAction = editMenu->addAction("Clear &All");
    clearAllAction->setIcon(IconManager::icon("eraser"));
    connect(clearAllAction, &QAction::triggered, this, &MainWindow::clearAll);

    // Also expose Preferences under Edit for easy discovery
    editMenu->addSeparator();
    QAction* editPreferencesAction = editMenu->addAction("&Preferences...");
    editPreferencesAction->setIcon(IconManager::icon("settings"));
    if (editPreferencesAction) {
        editPreferencesAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+,")));
        connect(editPreferencesAction, &QAction::triggered, this, &MainWindow::showSettings);
    }
    
    // View Menu
    QMenu* viewMenu = menuBar()->addMenu("&View");
    m_viewMenu = viewMenu;
    
    // Show Start Page (Welcome) on demand
    QAction* showStart = viewMenu->addAction("Start Page");
    showStart->setIcon(IconManager::icon("home"));
    showStart->setShortcut(QKeySequence("Ctrl+Alt+S"));
    connect(showStart, &QAction::triggered, this, [this](){
        if (m_centerStack && m_welcomeWidget) m_centerStack->setCurrentWidget(m_welcomeWidget);
    });
    m_showStartPageAction = showStart;
    
    QAction* zoomIn = viewMenu->addAction("Zoom &In");
    zoomIn->setIcon(IconManager::icon("zoom-in"));
    zoomIn->setShortcut(QKeySequence::ZoomIn);
    connect(zoomIn, &QAction::triggered, m_canvas, &CanvasWidget::zoomInAnimated);
    
    QAction* zoomOut = viewMenu->addAction("Zoom &Out");
    zoomOut->setIcon(IconManager::icon("zoom-out"));
    zoomOut->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOut, &QAction::triggered, m_canvas, &CanvasWidget::zoomOutAnimated);
    
    QAction* fitView = viewMenu->addAction("&Fit to Window");
    fitView->setIcon(IconManager::icon("fit"));
    fitView->setShortcut(QKeySequence("Ctrl+0"));
    connect(fitView, &QAction::triggered, m_canvas, &CanvasWidget::fitToWindowAnimated);
    
    viewMenu->addSeparator();
    
    QAction* toggleGrid = viewMenu->addAction("Show &Grid");
    toggleGrid->setIcon(IconManager::icon("grid"));
    toggleGrid->setCheckable(true);
    toggleGrid->setChecked(true);
    connect(toggleGrid, &QAction::toggled, m_canvas, &CanvasWidget::setShowGrid);
    
    QAction* toggleLabels = viewMenu->addAction("Show &Labels");
    toggleLabels->setIcon(IconManager::icon("label"));
    toggleLabels->setCheckable(true);
    toggleLabels->setChecked(true);
    connect(toggleLabels, &QAction::toggled, m_canvas, &CanvasWidget::setShowLabels);
    QAction* toggleLenLabels = viewMenu->addAction("Show &Length Labels");
    toggleLenLabels->setCheckable(true);
    toggleLenLabels->setChecked(false);
    connect(toggleLenLabels, &QAction::toggled, m_canvas, &CanvasWidget::setShowLengthLabels);
    QAction* toggleCrosshair = viewMenu->addAction("Show &Crosshair");
    toggleCrosshair->setIcon(IconManager::icon("crosshair"));
    toggleCrosshair->setCheckable(true);
    toggleCrosshair->setChecked(true);
    connect(toggleCrosshair, &QAction::toggled, m_canvas, &CanvasWidget::setShowCrosshair);
    
    
    // Tools Menu
    QMenu* toolsMenu = menuBar()->addMenu("&Tools");
    m_toolsMenu = toolsMenu;
    
    // Modify tools (real tools)
    m_toolTrimAction = toolsMenu->addAction("&Trim");
    m_toolTrimAction->setIcon(IconManager::iconUnique("scissors", "mod_trim", "TR"));
    connect(m_toolTrimAction, &QAction::triggered, this, &MainWindow::toolTrim);
    m_toolExtendAction = toolsMenu->addAction("&Extend");
    m_toolExtendAction->setIcon(IconManager::iconUnique("arrow-right", "mod_extend", "EX"));
    connect(m_toolExtendAction, &QAction::triggered, this, &MainWindow::toolExtend);
    m_toolOffsetAction = toolsMenu->addAction("&Offset...");
    m_toolOffsetAction->setIcon(IconManager::iconUnique("offset", "mod_offset", "OF"));
    connect(m_toolOffsetAction, &QAction::triggered, this, &MainWindow::toolOffset);
    m_toolFilletZeroAction = toolsMenu->addAction("&Fillet (Zero Radius)");
    m_toolFilletZeroAction->setIcon(IconManager::iconUnique("corner-round", "mod_fillet", "FR"));
    connect(m_toolFilletZeroAction, &QAction::triggered, this, &MainWindow::toolFilletZero);
    m_toolChamferAction = toolsMenu->addAction("C&hamfer...");
    m_toolChamferAction->setIcon(IconManager::iconUnique("corner", "mod_chamfer", "CH"));
    connect(m_toolChamferAction, &QAction::triggered, this, &MainWindow::toolChamfer);
    QAction* polarInputAct = toolsMenu->addAction("&Polar Input...");
    polarInputAct->setIcon(IconManager::icon("polar"));
    connect(polarInputAct, &QAction::triggered, this, &MainWindow::showPolarInput);
    QAction* massPolarAct = toolsMenu->addAction("Mass &Polar Reductions...");
    massPolarAct->setIcon(IconManager::icon("polar"));
    connect(massPolarAct, &QAction::triggered, this, &MainWindow::showMassPolar);
    QAction* joinPolarAct = toolsMenu->addAction("&Join (Polar)...");
    joinPolarAct->setIcon(IconManager::icon("join"));
    connect(joinPolarAct, &QAction::triggered, this, &MainWindow::showJoinPolar);
    QAction* traverseAct = toolsMenu->addAction("&Traverse...");
    traverseAct->setIcon(IconManager::icon("compass"));
    connect(traverseAct, &QAction::triggered, this, &MainWindow::showTraverse);
    QAction* interResAct = toolsMenu->addAction("&Intersection / Resection...");
    interResAct->setIcon(IconManager::icon("compass"));
    connect(interResAct, &QAction::triggered, this, &MainWindow::showIntersectResection);
    QAction* levelingAct = toolsMenu->addAction("&Leveling Adjustment...");
    levelingAct->setIcon(IconManager::icon("ruler-measure"));
    connect(levelingAct, &QAction::triggered, this, &MainWindow::showLeveling);
    QAction* lsnetAct = toolsMenu->addAction("&Network LS (Point)...");
    lsnetAct->setIcon(IconManager::icon("square"));
    connect(lsnetAct, &QAction::triggered, this, &MainWindow::showLSNetwork);
    QAction* transformAct = toolsMenu->addAction("&Transformations...");
    transformAct->setIcon(IconManager::icon("transform"));
    connect(transformAct, &QAction::triggered, this, &MainWindow::showTransformations);
    toolsMenu->addSeparator();
    m_preferencesAction = toolsMenu->addAction("&Preferences...");
    m_preferencesAction->setIcon(IconManager::icon("settings"));
    if (m_preferencesAction) {
        m_preferencesAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+,")));
        connect(m_preferencesAction, &QAction::triggered, this, &MainWindow::showSettings);
    }
    // Keep Undo/Redo labels in sync with stack
    if (m_undoStack) {
        connect(m_undoStack, &QUndoStack::canUndoChanged, this, [this](bool en){ if (m_undoAction) m_undoAction->setEnabled(en); });
        connect(m_undoStack, &QUndoStack::canRedoChanged, this, [this](bool en){ if (m_redoAction) m_redoAction->setEnabled(en); });
        connect(m_undoStack, &QUndoStack::undoTextChanged, this, [this](const QString& t){ if (m_undoAction) m_undoAction->setText(t.isEmpty()?"Undo":QString("Undo %1").arg(t)); });
        connect(m_undoStack, &QUndoStack::redoTextChanged, this, [this](const QString& t){ if (m_redoAction) m_redoAction->setText(t.isEmpty()?"Redo":QString("Redo %1").arg(t)); });
        m_undoAction->setEnabled(m_undoStack->canUndo());
        m_redoAction->setEnabled(m_undoStack->canRedo());
    }
    
    // Panel visibility toggles
    viewMenu->addSeparator();
    viewMenu->addAction("Panels")->setEnabled(false); // Section header
    
    m_toggleLeftPanelAction = viewMenu->addAction("Toggle &Coordinates Panel");
    m_toggleLeftPanelAction->setShortcut(QKeySequence("Ctrl+L"));
    m_toggleLeftPanelAction->setCheckable(true);
    m_toggleLeftPanelAction->setChecked(true);
    connect(m_toggleLeftPanelAction, &QAction::triggered, this, &MainWindow::toggleLeftPanel);
    
    m_toggleRightPanelAction = viewMenu->addAction("Toggle &Layers/Properties Panel");
    m_toggleRightPanelAction->setShortcut(QKeySequence("Ctrl+R"));
    m_toggleRightPanelAction->setCheckable(true);
    m_toggleRightPanelAction->setChecked(true);
    // Use toggled(bool) to explicitly set visibility, keeping state in sync
    connect(m_toggleRightPanelAction, &QAction::toggled, this, &MainWindow::setRightPanelsVisible);
    
    m_toggleCommandPanelAction = viewMenu->addAction("Toggle &Command Line");
    m_toggleCommandPanelAction->setShortcut(QKeySequence("Ctrl+Shift+C"));
    m_toggleCommandPanelAction->setCheckable(true);
    m_toggleCommandPanelAction->setChecked(true);
    connect(m_toggleCommandPanelAction, &QAction::triggered, this, &MainWindow::toggleCommandPanel);
    // Project Plan panel toggle
    m_toggleProjectPlanAction = viewMenu->addAction("Toggle &Project Plan");
    m_toggleProjectPlanAction->setCheckable(true);
    m_toggleProjectPlanAction->setChecked(false);
    connect(m_toggleProjectPlanAction, &QAction::toggled, this, &MainWindow::toggleProjectPlanPanel);
    
    // Add layout/theme controls
    viewMenu->addSeparator();
    m_resetLayoutAction = viewMenu->addAction("Reset &Layout");
    connect(m_resetLayoutAction, &QAction::triggered, this, &MainWindow::resetLayout);

    // Settings (top-level) Menu: reuse the same Preferences action
    QMenu* settingsMenu = menuBar()->addMenu("&Settings");
    m_settingsMenu = settingsMenu;
    if (m_preferencesAction) {
        settingsMenu->addAction(m_preferencesAction);
    }

    // Help Menu
    QMenu* helpMenu = menuBar()->addMenu("&Help");
    m_helpMenu = helpMenu;
    
    QAction* aboutAction = helpMenu->addAction("&About SiteSurveyor");
    m_aboutAction = aboutAction;
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::setupToolbar()
{
    // Two-row toolbars; no overflow dropdowns. Keep icons compact.
    // Ensure we never duplicate bars if called multiple times
    {
        QList<QToolBar*> bars = findChildren<QToolBar*>();
        for (QToolBar* b : bars) {
            if (!b) continue;
            const QString obj = b->objectName();
            if (obj == QLatin1String("TopBarRow1") || obj == QLatin1String("TopBarRow2")) {
                removeToolBar(b);
                b->deleteLater();
            }
        }
    }
    // Quick Access (top-left)
    QToolBar* quickBar = addToolBar("QuickAccess");
    quickBar->setObjectName("QuickAccess");
    quickBar->setAllowedAreas(Qt::TopToolBarArea);
    quickBar->setMovable(true);
    quickBar->setFloatable(false);
    quickBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    quickBar->setIconSize(QSize(14, 14));
    // Hover/press visual feedback for quick bar
    quickBar->setStyleSheet(
        "QToolBar { background: transparent; }"
        " QToolBar QToolButton { border-radius: 4px; padding: 1px 3px; font-size: 10px; }"
        " QToolBar QToolButton:hover { background-color: rgba(74,144,217,0.18); }"
        " QToolBar QToolButton:pressed { background-color: rgba(74,144,217,0.30); }"
    );
    if (m_newProjectAction) quickBar->addAction(m_newProjectAction);
    if (m_openProjectAction) quickBar->addAction(m_openProjectAction);
    if (m_saveProjectAction) quickBar->addAction(m_saveProjectAction);
    quickBar->addSeparator();
    if (m_undoAction) quickBar->addAction(m_undoAction);
    if (m_redoAction) quickBar->addAction(m_redoAction);

    QToolBar* topBar = addToolBar("TopBarRow1");
    m_topBar = topBar;
    topBar->setObjectName("TopBarRow1");
    topBar->setAllowedAreas(Qt::TopToolBarArea);
    topBar->setMovable(true);
    topBar->setFloatable(false);
    topBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    topBar->setIconSize(QSize(12, 12));
    // Hover/press feedback for top bar
    topBar->setStyleSheet(
        "QToolBar { background: transparent; }"
        " QToolBar QToolButton { border-radius: 4px; padding: 1px 3px; font-size: 10px; }"
        " QToolBar QToolButton:hover { background-color: rgba(74,144,217,0.18); }"
        " QToolBar QToolButton:pressed { background-color: rgba(74,144,217,0.30); }"
    );

    auto addCategoryTo = [&](QToolBar* bar, const QString& /*name*/){
        // Keep a subtle separator between groups; no text labels for a cleaner, icon-only look
        bar->addSeparator();
    };
    auto addMenuGroup = [&](QToolBar* bar, const QString& title, const QIcon& icon = QIcon()){
        QToolButton* btn = new QToolButton(this);
        if (!icon.isNull()) btn->setIcon(icon);
        btn->setToolTip(title);
        btn->setPopupMode(QToolButton::InstantPopup);
        btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        btn->setStyleSheet("QToolButton { font-size: 10px; }");
        QMenu* menu = new QMenu(btn);
        menu->setTearOffEnabled(true);
        btn->setMenu(menu);
        bar->addWidget(btn);
        return menu;
    };

    auto makeGroupDock = [&](QMenu* menu, const QString& title){
        QDockWidget* dock = new QDockWidget(title, this);
        QToolBar* tb = new QToolBar(dock);
        tb->setIconSize(QSize(16, 16));
        for (QAction* a : menu->actions()) tb->addAction(a);
        QWidget* w = new QWidget(dock);
        QVBoxLayout* lay = new QVBoxLayout(w);
        lay->setContentsMargins(4,4,4,4);
        lay->addWidget(tb);
        w->setLayout(lay);
        dock->setWidget(w);
        addDockWidget(Qt::LeftDockWidgetArea, dock);
        dock->hide();
        if (QToolButton* btn = qobject_cast<QToolButton*>(menu->parentWidget())) {
            btn->setCheckable(true);
            connect(btn, &QToolButton::toggled, this, [dock](bool on){ dock->setVisible(on); });
            connect(dock, &QDockWidget::visibilityChanged, this, [btn](bool vis){ QSignalBlocker b(btn); btn->setChecked(vis); });
        }
        return dock;
    };

    // Primary CAD groups: Draw | Modify | Layers | View | Aids
    // Draw (primary)
    addCategoryTo(topBar, "Draw");
    QAction* drawLineAction = new QAction(IconManager::iconUnique("line", "draw_line", "L"), "Line", this);
    drawLineAction->setShortcut(QKeySequence("L"));
    drawLineAction->setCheckable(true);
    drawLineAction->setToolTip("Line (L) - Draw a line segment");
    connect(drawLineAction, &QAction::toggled, this, [this](bool on){ if (on) m_canvas->setToolMode(CanvasWidget::ToolMode::DrawLine); });
    topBar->addAction(drawLineAction);
    QAction* drawPolyAction = new QAction(IconManager::iconUnique("polygon", "draw_poly", "PL"), "Polyline", this);
    drawPolyAction->setShortcut(QKeySequence("PL"));
    drawPolyAction->setCheckable(true);
    drawPolyAction->setToolTip("Polyline (PL) - Draw connected segments");
    connect(drawPolyAction, &QAction::toggled, this, [this](bool on){ if (on) m_canvas->setToolMode(CanvasWidget::ToolMode::DrawPolygon); });
    topBar->addAction(drawPolyAction);
    QAction* drawCircleAction = new QAction(IconManager::iconUnique("circle", "draw_circle", "C"), "Circle", this);
    drawCircleAction->setCheckable(true);
    drawCircleAction->setToolTip("Circle - Click center, then radius (typed distance or D=diameter optional)");
    connect(drawCircleAction, &QAction::toggled, this, [this](bool on){ if (on) m_canvas->setToolMode(CanvasWidget::ToolMode::DrawCircle); });
    topBar->addAction(drawCircleAction);
    QAction* drawArcAction = new QAction(IconManager::iconUnique("arc", "draw_arc", "A"), "Arc", this);
    drawArcAction->setCheckable(true);
    drawArcAction->setToolTip("Arc (3-point) - Click start, second point, then end");
    connect(drawArcAction, &QAction::toggled, this, [this](bool on){ if (on) m_canvas->setToolMode(CanvasWidget::ToolMode::DrawArc); });
    topBar->addAction(drawArcAction);
    QAction* drawRectAction = new QAction(IconManager::iconUnique("rectangle", "draw_rectangle", "R"), "Rectangle", this);
    drawRectAction->setCheckable(true);
    drawRectAction->setToolTip("Rectangle - Click first corner, then opposite corner (Shift for square)");
    connect(drawRectAction, &QAction::toggled, this, [this](bool on){ if (on) m_canvas->setToolMode(CanvasWidget::ToolMode::DrawRectangle); });
    topBar->addAction(drawRectAction);
    QAction* drawRegPolyAction = new QAction(IconManager::iconUnique("regular-polygon", "draw_regpoly", "RP"), "Regular Polygon", this);
    drawRegPolyAction->setToolTip("Regular Polygon");
    connect(drawRegPolyAction, &QAction::triggered, this, &MainWindow::drawRegularPolygon);
    topBar->addAction(drawRegPolyAction);
    // Store for pinning/exclusivity
    m_drawLineToolAction = drawLineAction;
    m_drawPolyToolAction = drawPolyAction;
    m_drawCircleToolAction = drawCircleAction;
    m_drawArcToolAction = drawArcAction;
    m_drawRectToolAction = drawRectAction;
    m_drawRegularPolygonAction = drawRegPolyAction;
    // Pin button for Draw group
    m_drawPinButton = new QToolButton(this);
    m_drawPinButton->setIcon(IconManager::iconUnique("pin", "draw_pin"));
    m_drawPinButton->setCheckable(true);
    m_drawPinButton->setToolTip("Pin Draw group inline");
    topBar->addWidget(m_drawPinButton);
    connect(m_drawPinButton, &QToolButton::toggled, this, [this](bool on){ m_drawGroupPinned = on; updatePinnedGroupsUI(); });

    // Modify (Lengthen, Trim, Extend, Offset, Fillet, Chamfer, Delete)
    addCategoryTo(topBar, "Modify");
    QAction* lengthenAct = new QAction(IconManager::iconUnique("ruler-measure", "modify_lengthen", "LN"), "Lengthen", this);
    lengthenAct->setCheckable(true);
    lengthenAct->setToolTip("Lengthen - adjust line length by value or click");
    connect(lengthenAct, &QAction::toggled, this, [this](bool on){ if (on) m_canvas->setToolMode(CanvasWidget::ToolMode::Lengthen); });
    topBar->addAction(lengthenAct);
    QAction* trimAct = new QAction(IconManager::iconUnique("scissors", "modify_trim", "TR"), "Trim", this);
    trimAct->setToolTip("Trim");
    connect(trimAct, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->startTrim(); });
    topBar->addAction(trimAct);
    QAction* extendAct = new QAction(IconManager::iconUnique("arrow-right", "modify_extend", "EX"), "Extend", this);
    extendAct->setToolTip("Extend");
    connect(extendAct, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->startExtend(); });
    topBar->addAction(extendAct);
    QAction* offsetAct = new QAction(IconManager::iconUnique("ruler", "modify_offset", "OF"), "Offset", this);
    connect(offsetAct, &QAction::triggered, this, [this](){ bool ok=false; double d=QInputDialog::getDouble(this, "Offset", "Distance:", 1.0, 0.0, 1e9, 3, &ok); if (!ok) return; if (m_canvas) m_canvas->startOffset(d); });
    topBar->addAction(offsetAct);
    QAction* filletAct = new QAction(IconManager::iconUnique("corner-right-down", "modify_fillet0", "F0"), "Fillet0", this);
    connect(filletAct, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->startFilletZero(); });
    topBar->addAction(filletAct);
    QAction* chamferAct = new QAction(IconManager::iconUnique("crop", "modify_chamfer", "CH"), "Chamfer", this);
    connect(chamferAct, &QAction::triggered, this, [this](){ bool ok=false; double d=QInputDialog::getDouble(this, "Chamfer", "Distance:", 1.0, 0.0, 1e9, 3, &ok); if (!ok) return; if (m_canvas) m_canvas->startChamfer(d); });
    topBar->addAction(chamferAct);
    // Avoid duplicating Delete Selected (kept under Edit group/menu)

    // Layers (Hide/Isolate/Lock/ShowAll) + Layer Panel toggle
    addCategoryTo(topBar, "Layers");
    QAction* hideLayActTop = new QAction(IconManager::iconUnique("eye-off", "layer_hide", "HD"), "Hide", this);
    hideLayActTop->setToolTip("Hide Selected Layers (Ctrl+Shift+H)");
    connect(hideLayActTop, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->hideSelectedLayers(); });
    topBar->addAction(hideLayActTop);
    QAction* isoLayActTop = new QAction(IconManager::iconUnique("filter", "layer_isolate", "IS"), "Isolate", this);
    isoLayActTop->setToolTip("Isolate Selection Layers (Ctrl+Shift+I)");
    connect(isoLayActTop, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->isolateSelectionLayers(); });
    topBar->addAction(isoLayActTop);
    QAction* lockLayActTop = new QAction(IconManager::iconUnique("lock", "layer_lock", "LK"), "Lock", this);
    lockLayActTop->setToolTip("Lock Selected Layers (Ctrl+Shift+L)");
    connect(lockLayActTop, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->lockSelectedLayers(); });
    topBar->addAction(lockLayActTop);
    QAction* showAllActTop = new QAction(IconManager::iconUnique("eye", "layer_show_all", "SA"), "Show All", this);
    showAllActTop->setToolTip("Show All Layers (Ctrl+Shift+S)");
    connect(showAllActTop, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->showAllLayers(); });
    topBar->addAction(showAllActTop);
    QAction* layerPanelAct = topBar->addAction(IconManager::iconUnique("desktop", "layer_panel", "LP"), "Layer Panel");
    layerPanelAct->setToolTip("Show/Hide Layers panel");
    connect(layerPanelAct, &QAction::triggered, this, [this](){ if (m_layersDock) m_layersDock->setVisible(!m_layersDock->isVisible()); });
    // Inline Layer selector
    {
        m_layerCombo = new QComboBox(this);
        m_layerCombo->setMinimumWidth(120);
        m_layerCombo->setMaximumWidth(160);
        m_layerCombo->setToolTip("Current Layer");
        topBar->addWidget(m_layerCombo);
        refreshLayerCombo();
        connect(m_layerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onLayerComboChanged);
    }

    // View
    addCategoryTo(topBar, "View");
    m_selectToolAction = new QAction(IconManager::iconUnique("select", "view_select", "SL"), "Select", this);
    m_selectToolAction->setShortcut(QKeySequence("ESC"));
    m_selectToolAction->setToolTip("Select (Esc)");
    m_selectToolAction->setCheckable(true);
    m_selectToolAction->setChecked(true);
    connect(m_selectToolAction, &QAction::toggled, this, [this](bool on){ if(on) m_canvas->setToolMode(CanvasWidget::ToolMode::Select); });
    topBar->addAction(m_selectToolAction);
    m_panToolAction = new QAction(IconManager::iconUnique("pan", "view_pan", "PN"), "Pan", this);
    m_panToolAction->setShortcut(QKeySequence("P"));
    m_panToolAction->setToolTip("Pan (P)");
    m_panToolAction->setCheckable(true);
    connect(m_panToolAction, &QAction::toggled, this, [this](bool on){ if(on) m_canvas->setToolMode(CanvasWidget::ToolMode::Pan); });
    topBar->addAction(m_panToolAction);
    m_zoomWindowToolAction = new QAction(IconManager::iconUnique("zoom-window", "view_zoomwin", "ZW"), "Zoom Window", this);
    m_zoomWindowToolAction->setShortcut(QKeySequence("W"));
    m_zoomWindowToolAction->setToolTip("Zoom Window (W)");
    m_zoomWindowToolAction->setCheckable(true);
    connect(m_zoomWindowToolAction, &QAction::toggled, this, [this](bool on){ if(on) m_canvas->setToolMode(CanvasWidget::ToolMode::ZoomWindow); });
    topBar->addAction(m_zoomWindowToolAction);
    m_lassoToolAction = new QAction(IconManager::iconUnique("lasso", "view_lasso", "LS"), "Lasso", this);
    m_lassoToolAction->setToolTip("Lasso Select");
    m_lassoToolAction->setCheckable(true);
    connect(m_lassoToolAction, &QAction::toggled, this, [this](bool on){ if(on) m_canvas->setToolMode(CanvasWidget::ToolMode::LassoSelect); });
    topBar->addAction(m_lassoToolAction);
    // Zoom controls
    QAction* zoomInAction = new QAction(IconManager::iconUnique("zoom-in", "view_zoomin", "ZI"), "Zoom In", this);
    zoomInAction->setToolTip("Zoom In");
    connect(zoomInAction, &QAction::triggered, m_canvas, &CanvasWidget::zoomInAnimated);
    QAction* zoomOutAction = new QAction(IconManager::iconUnique("zoom-out", "view_zoomout", "ZO"), "Zoom Out", this);
    zoomOutAction->setToolTip("Zoom Out");
    connect(zoomOutAction, &QAction::triggered, m_canvas, &CanvasWidget::zoomOutAnimated);
    QAction* fitAction = new QAction(IconManager::iconUnique("fit", "view_fit", "FT"), "Extents", this);
    fitAction->setToolTip("Fit to Window");
    connect(fitAction, &QAction::triggered, m_canvas, &CanvasWidget::fitToWindowAnimated);
    QAction* homeAction = new QAction(IconManager::iconUnique("home", "view_home", "HM"), "Home", this);
    homeAction->setToolTip("Home");
    connect(homeAction, &QAction::triggered, m_canvas, &CanvasWidget::resetView);
    topBar->addAction(zoomInAction);
    topBar->addAction(zoomOutAction);
    topBar->addAction(fitAction);
    topBar->addAction(homeAction);
    // Display toggles inline with View panel
    {
        QAction* toggleGridTop = new QAction(IconManager::iconUnique("grid", "view_grid", "GD"), "Grid", this);
        toggleGridTop->setCheckable(true);
        toggleGridTop->setChecked(true);
        connect(toggleGridTop, &QAction::toggled, m_canvas, &CanvasWidget::setShowGrid);
        QAction* toggleLabelsTop = new QAction(IconManager::iconUnique("label", "view_labels", "LB"), "Labels", this);
        toggleLabelsTop->setCheckable(true);
        toggleLabelsTop->setChecked(true);
        connect(toggleLabelsTop, &QAction::toggled, m_canvas, &CanvasWidget::setShowLabels);
        if (!m_crosshairToggleAction) m_crosshairToggleAction = new QAction(IconManager::iconUnique("crosshair", "view_crosshair", "CR"), "Crosshair", this);
        m_crosshairToggleAction->setCheckable(true);
        m_crosshairToggleAction->setChecked(true);
        connect(m_crosshairToggleAction, &QAction::toggled, m_canvas, &CanvasWidget::setShowCrosshair);
        topBar->addAction(toggleGridTop);
        topBar->addAction(toggleLabelsTop);
        topBar->addAction(m_crosshairToggleAction);
    }

    // Aids group omitted on top bar (kept in status bar for AutoCAD-like UX)

    // Quick Project Planner toggle on top row
    addCategoryTo(topBar, "PlanToggle");
    if (!m_toggleProjectPlanAction) {
        m_toggleProjectPlanAction = new QAction(IconManager::iconUnique("plan-menu", "group_plan", "PL"), "Project Plan", this);
        m_toggleProjectPlanAction->setCheckable(true);
        connect(m_toggleProjectPlanAction, &QAction::triggered, this, &MainWindow::toggleProjectPlanPanel);
    }
    topBar->addAction(m_toggleProjectPlanAction);

    // Insert a separator between primary CAD groups and secondary groups
    topBar->addSeparator();

    // Break to second row
    addToolBarBreak(Qt::TopToolBarArea);

    QToolBar* bottomBar = addToolBar("TopBarRow2");
    m_bottomBar = bottomBar;
    bottomBar->setObjectName("TopBarRow2");
    bottomBar->setAllowedAreas(Qt::TopToolBarArea);
    bottomBar->setMovable(true);
    bottomBar->setFloatable(false);
    bottomBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    bottomBar->setIconSize(QSize(12, 12));
    // Hover/press feedback for second row
    bottomBar->setStyleSheet(
        "QToolBar { background: transparent; }"
        " QToolBar QToolButton { border-radius: 4px; padding: 1px 3px; font-size: 10px; }"
        " QToolBar QToolButton:hover { background-color: rgba(74,144,217,0.18); }"
        " QToolBar QToolButton:pressed { background-color: rgba(74,144,217,0.30); }"
    );

    // Secondary groups moved to second row: Data | Prefs | Plan

    QMenu* dataMenuTb = addMenuGroup(bottomBar, "Data", IconManager::iconUnique("data-menu", "group_data", "DT"));
    if (m_importPointsAction) dataMenuTb->addAction(m_importPointsAction);
    if (m_exportPointsAction) dataMenuTb->addAction(m_exportPointsAction);
    if (QToolButton* tb = qobject_cast<QToolButton*>(dataMenuTb->parentWidget())) tb->setToolTip("Import Coordinates, Export Coordinates");
    makeGroupDock(dataMenuTb, "Data");

    QMenu* prefsMenuTb = addMenuGroup(bottomBar, "Prefs", IconManager::iconUnique("prefs-menu", "group_prefs", "PR"));
    if (m_preferencesAction) prefsMenuTb->addAction(m_preferencesAction);
    if (m_resetLayoutAction) prefsMenuTb->addAction(m_resetLayoutAction);
    if (QToolButton* tb = qobject_cast<QToolButton*>(prefsMenuTb->parentWidget())) tb->setToolTip("Preferences, Reset Layout");
    makeGroupDock(prefsMenuTb, "Prefs");

    QMenu* planMenuTb = addMenuGroup(bottomBar, "Plan", IconManager::iconUnique("plan-menu", "group_plan", "PL"));
    if (!m_toggleProjectPlanAction) {
        m_toggleProjectPlanAction = new QAction(IconManager::iconUnique("plan-menu", "group_plan", "PL"), "Project Plan", this);
        m_toggleProjectPlanAction->setCheckable(true);
        connect(m_toggleProjectPlanAction, &QAction::triggered, this, &MainWindow::toggleProjectPlanPanel);
    }
    planMenuTb->addAction(m_toggleProjectPlanAction);
    if (QToolButton* tb = qobject_cast<QToolButton*>(planMenuTb->parentWidget())) tb->setToolTip("Show/Hide Project Plan panel");
    makeGroupDock(planMenuTb, "Plan");

    // Annotation group moved to second row (icon-only menu)
    addCategoryTo(bottomBar, "Annotation");
    {
        QMenu* annotMenu = addMenuGroup(bottomBar, "Annotation", IconManager::iconUnique("label", "group_annotation", "AN"));
        QAction* textAct = annotMenu->addAction("Text");
        QAction* dimAct = annotMenu->addAction("Dimension");
        connect(textAct, &QAction::triggered, this, [this](){
            bool ok=false;
            const QString txt = QInputDialog::getText(this, "Add Text", "Label:", QLineEdit::Normal, QString(), &ok);
            if (!ok || txt.trimmed().isEmpty()) return;
            ok=false;
            double h = QInputDialog::getDouble(this, "Add Text", "Height (world units):", 2.0, 0.1, 1e9, 2, &ok);
            if (!ok) return;
            ok=false;
            double ang = QInputDialog::getDouble(this, "Add Text", "Angle (deg):", 0.0, -360.0, 360.0, 1, &ok);
            if (!ok) return;
            if (m_canvas) m_canvas->addText(txt.trimmed(), m_lastMouseWorld, h, ang);
        });
        connect(dimAct, &QAction::triggered, this, [this](){
            bool ok=false;
            double h = QInputDialog::getDouble(this, "Add Dimension", "Text height:", 2.0, 0.1, 1e9, 2, &ok);
            if (!ok) return;
            int li = m_canvas ? m_canvas->selectedLineIndex() : -1;
            if (li >= 0) {
                QPointF a,b; if (!m_canvas->lineEndpoints(li,a,b)) return; QString layer = m_canvas->lineLayer(li); m_canvas->addDimension(a,b,h,layer); return;
            }
            const QStringList names = m_pointManager->getPointNames();
            if (names.size() < 2) { QMessageBox::information(this, "Dimension", "Need at least two points."); return; }
            QString aName = QInputDialog::getItem(this, "Dimension", "From point:", names, 0, false, &ok); if (!ok || aName.isEmpty()) return;
            QString bName = QInputDialog::getItem(this, "Dimension", "To point:", names, 1, false, &ok); if (!ok || bName.isEmpty()) return;
            Point pa = m_pointManager->getPoint(aName); Point pb = m_pointManager->getPoint(bName);
            if (pa.name.isEmpty() || pb.name.isEmpty()) return;
            m_canvas->addDimension(pa.toQPointF(), pb.toQPointF(), h);
        });
    }

    // Blocks group moved to second row (icon-only menu)
    addCategoryTo(bottomBar, "Blocks");
    {
        QMenu* blocksMenu = addMenuGroup(bottomBar, "Blocks", IconManager::iconUnique("square", "group_blocks", "BL"));
        QAction* insertBlk = blocksMenu->addAction("Insert Block...");
        QAction* createBlk = blocksMenu->addAction("Create Block from Selection");
        QAction* explodeBlk = blocksMenu->addAction("Explode");
        connect(insertBlk, &QAction::triggered, this, [this](){ QMessageBox::information(this, "Blocks", "Insert Block coming soon."); });
        connect(createBlk, &QAction::triggered, this, [this](){ QMessageBox::information(this, "Blocks", "Create Block coming soon."); });
        connect(explodeBlk, &QAction::triggered, this, [this](){ QMessageBox::information(this, "Blocks", "Explode coming soon."); });
    }

    // Properties quick access moved to second row (icon-only action)
    addCategoryTo(bottomBar, "Properties");
    QAction* propsAct = bottomBar->addAction(IconManager::iconUnique("settings", "group_properties", "PR"), "Properties");
    propsAct->setToolTip("Show Properties tab");
    connect(propsAct, &QAction::triggered, this, [this](){
        if (m_layersDock) { m_layersDock->setVisible(true); m_layersDock->raise(); int rpw = AppSettings::rightPanelWidth(); if (rpw < 200) rpw = 200; m_layersDock->resize(rpw, m_layersDock->height()); }
        if (m_rightTabs && m_propertiesPanel) m_rightTabs->setCurrentWidget(m_propertiesPanel);
    });

    // Utilities group moved to second row (icon-only menu)
    addCategoryTo(bottomBar, "Utilities");
    {
        QMenu* utilMenu = addMenuGroup(bottomBar, "Utilities", IconManager::iconUnique("prefs-menu", "group_utilities", "UT"));
        QAction* idPt = utilMenu->addAction("ID Point");
        QAction* measAng = utilMenu->addAction("Measure Angle");
        QAction* listProps = utilMenu->addAction("List Properties");
        connect(idPt, &QAction::triggered, this, [this](){
            const auto pts = m_pointManager->getAllPoints();
            if (pts.isEmpty()) { QMessageBox::information(this, "ID Point", "No coordinates defined."); return; }
            const QPointF w = m_lastMouseWorld;
            double bestD2 = std::numeric_limits<double>::max();
            int bestIdx = -1;
            for (int i=0;i<pts.size();++i) {
                const auto& p = pts[i];
                double dx = p.x - w.x(); double dy = p.y - w.y(); double d2 = dx*dx + dy*dy;
                if (d2 < bestD2) { bestD2 = d2; bestIdx = i; }
            }
            if (bestIdx >= 0) {
                const auto& p = pts[bestIdx];
                const QString msg = QString("Nearest point: %1\nX: %2  Y: %3  Z: %4\nDistance: %5")
                    .arg(p.name)
                    .arg(p.x, 0, 'f', 3)
                    .arg(p.y, 0, 'f', 3)
                    .arg(p.z, 0, 'f', 3)
                    .arg(qSqrt(bestD2), 0, 'f', 3);
                QMessageBox::information(this, "ID Point", msg);
            }
        });
        connect(measAng, &QAction::triggered, this, [this](){
            const QStringList names = m_pointManager->getPointNames();
            if (names.size() < 2) { QMessageBox::information(this, "Measure Angle", "Need at least two points."); return; }
            bool ok=false; QString a = QInputDialog::getItem(this, "Measure Angle", "From point:", names, 0, false, &ok); if (!ok || a.isEmpty()) return;
            QString b = QInputDialog::getItem(this, "Measure Angle", "To point:", names, 1, false, &ok); if (!ok || b.isEmpty()) return;
            Point p1 = m_pointManager->getPoint(a); Point p2 = m_pointManager->getPoint(b);
            if (p1.name.isEmpty() || p2.name.isEmpty()) { QMessageBox::information(this, "Measure Angle", "Invalid points."); return; }
            double az = SurveyCalculator::azimuth(p1, p2);
            QString dms = SurveyCalculator::toDMS(az);
            QMessageBox::information(this, "Measure Angle", QString("Azimuth %1 -> %2: %3° (%4)").arg(a, b).arg(az, 0, 'f', 4).arg(dms));
        });
        connect(listProps, &QAction::triggered, this, &MainWindow::showSelectedProperties);
    }
    
    // Survey group
    addCategoryTo(bottomBar, "Survey");
    QAction* polarToolbarAct = bottomBar->addAction(IconManager::iconUnique("polar", "survey_polar", "PO"), "Polar");
    polarToolbarAct->setToolTip("Polar Input");
    connect(polarToolbarAct, &QAction::triggered, this, &MainWindow::showPolarInput);
    QAction* joinToolbarAct = bottomBar->addAction(IconManager::iconUnique("join", "survey_join", "JN"), "Join");
    joinToolbarAct->setToolTip("Join (Polar)");
    connect(joinToolbarAct, &QAction::triggered, this, &MainWindow::showJoinPolar);
    QAction* traverseToolbarAct = bottomBar->addAction(IconManager::iconUnique("compass", "survey_traverse", "TV"), "Traverse");
    traverseToolbarAct->setToolTip("Traverse");
    connect(traverseToolbarAct, &QAction::triggered, this, &MainWindow::showTraverse);
    QAction* addPointAction = bottomBar->addAction(IconManager::iconUnique("add-point", "survey_add_point", "AP"), "Add Point");
    addPointAction->setToolTip("Add Coordinate");
    connect(addPointAction, &QAction::triggered, this, &MainWindow::showAddPointDialog);

    addCategoryTo(bottomBar, "Modify");
    if (m_toolTrimAction) bottomBar->addAction(m_toolTrimAction);
    if (m_toolExtendAction) bottomBar->addAction(m_toolExtendAction);
    if (m_toolOffsetAction) bottomBar->addAction(m_toolOffsetAction);
    if (m_toolFilletZeroAction) bottomBar->addAction(m_toolFilletZeroAction);
    if (m_toolChamferAction) bottomBar->addAction(m_toolChamferAction);

    // Display group moved to top View panel; keep second row compact

    // Selection tools
    addCategoryTo(bottomBar, "Select");
    QAction* selAllActTb = bottomBar->addAction(IconManager::iconUnique("square", "sel_all", "AL"), "All");
    selAllActTb->setToolTip("Select All (Ctrl+A)");
    connect(selAllActTb, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->selectAllVisible(); });
    QAction* invSelActTb = bottomBar->addAction(IconManager::iconUnique("clear", "sel_invert", "IV"), "Invert");
    invSelActTb->setToolTip("Invert Selection (Ctrl+I)");
    connect(invSelActTb, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->invertSelectionVisible(); });
    QAction* selByLayerActTb = bottomBar->addAction(IconManager::iconUnique("search", "sel_bylayer", "BL"), "ByLayer");
    selByLayerActTb->setToolTip("Select by Current Layer");
    connect(selByLayerActTb, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->selectByCurrentLayer(); });
    QAction* clearSelActTb = bottomBar->addAction(IconManager::iconUnique("eraser", "sel_clear", "CL"), "Clear");
    clearSelActTb->setToolTip("Clear Selection (Esc)");
    connect(clearSelActTb, &QAction::triggered, this, [this](){ if (m_canvas) { m_canvas->clearSelection(); m_canvas->update(); onSelectionChanged(0,0); } });

    // Avoid duplicating layer visibility tools on second row

    topBar->addSeparator();

    // Group tool selection actions for exclusivity
    QActionGroup* toolGroup = new QActionGroup(this);
    toolGroup->setExclusive(true);
    m_selectToolAction->setActionGroup(toolGroup);
    m_panToolAction->setActionGroup(toolGroup);
    m_zoomWindowToolAction->setActionGroup(toolGroup);
    if (m_lassoToolAction) m_lassoToolAction->setActionGroup(toolGroup);
    if (m_drawLineToolAction) m_drawLineToolAction->setActionGroup(toolGroup);
    if (m_drawPolyToolAction) m_drawPolyToolAction->setActionGroup(toolGroup);
    if (m_drawCircleToolAction) m_drawCircleToolAction->setActionGroup(toolGroup);
    if (m_drawArcToolAction) m_drawArcToolAction->setActionGroup(toolGroup);
    if (m_drawRectToolAction) m_drawRectToolAction->setActionGroup(toolGroup);
    // Include Lengthen mode in exclusivity
    lengthenAct->setActionGroup(toolGroup);
    enableOverflowTearOff(topBar);
    enableOverflowTearOff(bottomBar);
    
    // Initial pin UI state
    updatePinnedGroupsUI();
}

void MainWindow::setupConnections()
{
    // Canvas signals
    connect(m_canvas, &CanvasWidget::mouseWorldPosition, this, &MainWindow::updateCoordinates);
    // Debounce OSNAP hint to avoid flicker
    if (!m_osnapHintTimer) { m_osnapHintTimer = new QTimer(this); m_osnapHintTimer->setSingleShot(true); m_osnapHintTimer->setInterval(50); }
    connect(m_canvas, &CanvasWidget::osnapHintChanged, this, [this](const QString& hint){
        m_pendingOsnapHint = hint;
        if (m_osnapHintTimer) {
            disconnect(m_osnapHintTimer, nullptr, nullptr, nullptr);
            connect(m_osnapHintTimer, &QTimer::timeout, this, [this](){ if (m_measureLabel) m_measureLabel->setText(m_pendingOsnapHint); });
            m_osnapHintTimer->start();
        } else {
            if (m_measureLabel) m_measureLabel->setText(hint);
        }
    });
    connect(m_canvas, &CanvasWidget::canvasClicked, this, &MainWindow::handleCanvasClick);
    connect(m_canvas, &CanvasWidget::zoomChanged, this, &MainWindow::onZoomChanged);
    connect(m_canvas, &CanvasWidget::drawingDistanceChanged, this, &MainWindow::onDrawingDistanceChanged);
    connect(m_canvas, &CanvasWidget::drawingAngleChanged, this, &MainWindow::onDrawingAngleChanged);
    
    // Point manager signals
    connect(m_pointManager, &PointManager::pointAdded, this, &MainWindow::updatePointsTable);
    connect(m_pointManager, &PointManager::pointRemoved, this, &MainWindow::updatePointsTable);
    connect(m_pointManager, &PointManager::pointsCleared, this, &MainWindow::updatePointsTable);
    // Selection changes
    if (m_canvas) connect(m_canvas, &CanvasWidget::selectionChanged, this, &MainWindow::onSelectionChanged);
    
    // Table selection -> zoom to point
    connect(m_pointsTable, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::onPointsTableSelectionChanged);
    
    // Command processor signals
    connect(m_commandProcessor, &CommandProcessor::commandProcessed, 
            this, &MainWindow::appendToCommandOutput);

    // Layer manager signals
    connect(m_layerManager, &LayerManager::layersChanged, this, [this](){
        refreshLayerCombo();
        updateLayerStatusText();
        updatePointsTable();
        if (m_canvas) m_canvas->update();
    });
    connect(m_layerManager, &LayerManager::currentLayerChanged, this, [this](const QString&){
        refreshLayerCombo();
        updateLayerStatusText();
    });
}

void MainWindow::updatePinnedGroupsUI()
{
    if (!m_topBar) return;
    for (QAction* a : m_drawInlineActions) {
        if (!a) continue;
        m_topBar->removeAction(a);
    }
    m_drawInlineActions.clear();
    if (!m_drawGroupPinned) return;
    if (!m_drawAnchorAction) {
        m_drawAnchorAction = m_topBar->addSeparator();
    }
    auto insertInline = [&](QAction* act){
        if (act) {
            m_topBar->insertAction(m_drawAnchorAction, act);
            m_drawInlineActions.append(act);
        }
    };
    insertInline(m_drawLineToolAction);
    insertInline(m_drawPolyToolAction);
    insertInline(m_drawCircleToolAction);
    insertInline(m_drawArcToolAction);
    insertInline(m_drawRectToolAction);
}

void MainWindow::enableOverflowTearOff(QToolBar* bar)
{
    if (!bar) return;
    auto ensure = [bar]() {
        QToolButton* ext = bar->findChild<QToolButton*>("qt_toolbar_ext_button");
        if (!ext) return;
        ext->setPopupMode(QToolButton::InstantPopup);
        QMenu* m = ext->menu();
        if (m) m->setTearOffEnabled(true);
        if (!ext->property("tearoff_hooked").toBool()) {
            QObject::connect(ext, &QToolButton::pressed, bar, [ext]() {
                QMenu* mm = ext->menu();
                if (mm) mm->setTearOffEnabled(true);
            });
            ext->setProperty("tearoff_hooked", true);
        }
    };
    ensure();
    QTimer::singleShot(0, bar, ensure);
    QTimer::singleShot(200, bar, ensure);
}

void MainWindow::updateMoreDock()
{
    // This function is temporarily disabled due to stability issues
    // The overflow menu functionality has been removed to prevent crashes
    return;
}

void MainWindow::showAddPointDialog()
{
    bool ok;
    QString name = QInputDialog::getText(this, "Add Coordinate", "Coordinate name:", 
                                          QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty()) return;
    
    // Prompt one-by-one for coordinates
    const bool gauss = AppSettings::gaussMode();
    const bool use3D = AppSettings::use3D();
    double x = 0.0, y = 0.0, z = 0.0;

    if (gauss) {
        y = QInputDialog::getDouble(this, "Add Coordinate", "Enter Y:", 0.0, -1e12, 1e12, 3, &ok);
        if (!ok) return;
        x = QInputDialog::getDouble(this, "Add Coordinate", "Enter X:", 0.0, -1e12, 1e12, 3, &ok);
        if (!ok) return;
    } else {
        x = QInputDialog::getDouble(this, "Add Coordinate", "Enter X:", 0.0, -1e12, 1e12, 3, &ok);
        if (!ok) return;
        y = QInputDialog::getDouble(this, "Add Coordinate", "Enter Y:", 0.0, -1e12, 1e12, 3, &ok);
        if (!ok) return;
    }
    if (use3D) {
        z = QInputDialog::getDouble(this, "Add Coordinate", "Enter Z:", 0.0, -1e12, 1e12, 3, &ok);
        if (!ok) return;
    } else {
        z = 0.0; // default for 2D
    }

    // Build command string respecting Gauss input order expected by the command processor
    QString command;
    if (gauss) {
        command = QString("add %1 %2 %3 %4").arg(name).arg(y, 0, 'f', 3).arg(x, 0, 'f', 3).arg(z, 0, 'f', 3);
    } else {
        command = QString("add %1 %2 %3 %4").arg(name).arg(x, 0, 'f', 3).arg(y, 0, 'f', 3).arg(z, 0, 'f', 3);
    }
    m_commandInput->setText(command);
    executeCommand();
}

void MainWindow::clearAll()
{
    int ret = QMessageBox::question(this, "Clear All", 
                                   "Are you sure you want to clear all coordinates?",
                                   QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        m_commandInput->setText("clear");
        executeCommand();
    }
}

void MainWindow::newProject()
{
    int ret = QMessageBox::question(this, "New Project", 
                                   "Clear current project and start new?",
                                   QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        m_pointManager->clearAllPoints();
        m_canvas->clearAll();
        m_commandOutput->clear();
        updatePointsTable();
        updateStatus();
        if (m_centerStack && m_canvas) m_centerStack->setCurrentWidget(m_canvas);
    }
}

void MainWindow::openProject()
{
    const QString fileName = QFileDialog::getOpenFileName(this, "Open Project",
                                                          QString(),
                                                          "SiteSurveyor Project (*.ssproj *.json);;All Files (*)");
    if (fileName.isEmpty()) return;
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly)) { QMessageBox::warning(this, "Open Project", QString("Failed to open %1").arg(fileName)); return; }
    const QByteArray data = f.readAll(); f.close();
    QJsonParseError err; QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) { QMessageBox::warning(this, "Open Project", "Invalid project file"); return; }
    QJsonObject root = doc.object();
    if (root.value("type").toString() != QLatin1String("SiteSurveyorProject")) { QMessageBox::warning(this, "Open Project", "Unsupported project type"); return; }
    // Clear
    m_pointManager->clearAllPoints(); m_canvas->clearAll();
    // Layers
    if (m_layerManager) {
        // Remove all non-default layers
        const auto existing = m_layerManager->layers();
        for (const auto& L : existing) { if (L.name != "0") m_layerManager->removeLayer(L.name); }
        // Add from file
        QJsonArray jlayers = root.value("layers").toArray();
        for (const auto& v : jlayers) {
            QJsonObject o = v.toObject();
            QString name = o.value("name").toString();
            QColor color(o.value("r").toInt(200), o.value("g").toInt(200), o.value("b").toInt(200));
            bool vis = o.value("visible").toBool(true);
            bool lock = o.value("locked").toBool(false);
            if (name.isEmpty() || name == "0") continue;
            m_layerManager->addLayer(name, color);
            m_layerManager->setLayerVisible(name, vis);
            m_layerManager->setLayerLocked(name, lock);
        }
        QString cur = root.value("currentLayer").toString(); if (!cur.isEmpty()) m_layerManager->setCurrentLayer(cur);
    }
    // Points
    for (const auto& v : root.value("points").toArray()) {
        QJsonObject o = v.toObject(); Point p(o.value("name").toString(), o.value("x").toDouble(), o.value("y").toDouble(), o.value("z").toDouble());
        m_pointManager->addPoint(p); m_canvas->addPoint(p);
        QString layer = o.value("layer").toString(); if (!layer.isEmpty()) m_canvas->setPointLayer(p.name, layer);
    }
    // Lines
    for (const auto& v : root.value("lines").toArray()) {
        QJsonObject o = v.toObject(); QPointF a(o.value("ax").toDouble(), o.value("ay").toDouble()); QPointF b(o.value("bx").toDouble(), o.value("by").toDouble());
        m_canvas->addLine(a, b); QString layer = o.value("layer").toString(); int idx = m_canvas->lineCount()-1; if (idx>=0 && !layer.isEmpty()) m_canvas->setLineLayer(idx, layer);
    }
    // Texts
    for (const auto& v : root.value("texts").toArray()) {
        QJsonObject o = v.toObject();
        const QString t = o.value("text").toString();
        const QPointF pos(o.value("x").toDouble(), o.value("y").toDouble());
        const double h = o.value("height").toDouble(2.0);
        const double ang = o.value("angle").toDouble(0.0);
        const QString layer = o.value("layer").toString();
        if (!t.isEmpty()) m_canvas->addText(t, pos, h, ang, layer);
    }
    updatePointsTable(); updateStatus();
    showToast(QStringLiteral("Project loaded"));
    if (m_centerStack && m_canvas) m_centerStack->setCurrentWidget(m_canvas);
}

void MainWindow::saveProject()
{
    const QString fileName = QFileDialog::getSaveFileName(this, "Save Project As",
                                                          QString(),
                                                          "SiteSurveyor Project (*.ssproj *.json);;All Files (*)");
    if (fileName.isEmpty()) return;
    QJsonObject root; root["type"] = QStringLiteral("SiteSurveyorProject"); root["version"] = 1;
    // Settings (minimal)
    QJsonObject settings; settings["gaussMode"] = AppSettings::gaussMode(); settings["use3D"] = AppSettings::use3D(); settings["units"] = AppSettings::measurementUnits(); settings["angleFormat"] = AppSettings::angleFormat(); settings["crs"] = AppSettings::crs(); root["settings"] = settings;
    // Layers
    QJsonArray jlayers; if (m_layerManager) {
        for (const auto& L : m_layerManager->layers()) { QJsonObject o; o["name"] = L.name; o["r"] = L.color.red(); o["g"] = L.color.green(); o["b"] = L.color.blue(); o["visible"] = L.visible; o["locked"] = L.locked; jlayers.append(o); }
        root["currentLayer"] = m_layerManager->currentLayer();
    }
    root["layers"] = jlayers;
    // Points
    QJsonArray points; for (const auto& p : m_pointManager->getAllPoints()) { QJsonObject o; o["name"] = p.name; o["x"] = p.x; o["y"] = p.y; o["z"] = p.z; o["layer"] = m_canvas ? m_canvas->pointLayer(p.name) : QStringLiteral("0"); points.append(o); } root["points"] = points;
    // Lines
    QJsonArray lines; if (m_canvas) { for (int i=0;i<m_canvas->lineCount();++i) { QPointF a,b; if (!m_canvas->lineEndpoints(i,a,b)) continue; QJsonObject o; o["ax"] = a.x(); o["ay"] = a.y(); o["bx"] = b.x(); o["by"] = b.y(); o["layer"] = m_canvas->lineLayer(i); lines.append(o);} } root["lines"] = lines;
    // Texts
    QJsonArray texts; if (m_canvas) {
        const int n = m_canvas->textCount();
        for (int i=0;i<n;++i) {
            QString t; QPointF pos; double h=0.0, ang=0.0; QString layer;
            if (!m_canvas->textData(i, t, pos, h, ang, layer)) continue;
            QJsonObject o; o["text"] = t; o["x"] = pos.x(); o["y"] = pos.y(); o["height"] = h; o["angle"] = ang; o["layer"] = layer; texts.append(o);
        }
    } root["texts"] = texts;
    QSaveFile f(fileName); if (!f.open(QIODevice::WriteOnly)) { QMessageBox::warning(this, "Save Project", QString("Failed to write %1").arg(fileName)); return; }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented)); f.commit();
    showToast(QStringLiteral("Project saved"));
}

void MainWindow::importCoordinates()
{
    const QString fileName = QFileDialog::getOpenFileName(this, "Import Coordinates",
                                                          QString(),
                                                          "CSV Files (*.csv);;All Files (*)");
    if (fileName.isEmpty()) return;
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Import Coordinates", QString("Failed to open %1").arg(fileName));
        return;
    }
    QTextStream in(&f);
    int added = 0;
    int lineNo = 0;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        ++lineNo;
        if (line.trimmed().isEmpty()) continue;
        const QStringList parts = line.split(',');
        if (parts.size() < 3) continue;
        const QString name = parts.at(0).trimmed();
        bool okx=false, oky=false, okz=true;
        const double x = parts.at(1).trimmed().toDouble(&okx);
        const double y = parts.at(2).trimmed().toDouble(&oky);
        double z = 0.0;
        if (parts.size() > 3) z = parts.at(3).trimmed().toDouble(&okz);
        if (!okx || !oky || !okz || name.isEmpty()) continue;
        if (m_pointManager->addPoint(name, x, y, z)) {
            Point p(name, x, y, z);
            m_canvas->addPoint(p);
            ++added;
        }
    }
    f.close();
    updatePointsTable();
    updateStatus();
    AppSettings::addRecentFile(fileName);
    if (m_welcomeWidget) m_welcomeWidget->reload();
    if (m_commandOutput) m_commandOutput->append(QString("Imported %1 points from %2").arg(added).arg(QFileInfo(fileName).fileName()));
    showToast(QString("Imported %1 points").arg(added));
    if (m_centerStack && m_canvas) m_centerStack->setCurrentWidget(m_canvas);
}

void MainWindow::importCoordinatesFrom(const QString& filePath)
{
    if (filePath.trimmed().isEmpty()) return;
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Import Coordinates", QString("Failed to open %1").arg(filePath));
        return;
    }
    QTextStream in(&f);
    int added = 0;
    int lineNo = 0;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        ++lineNo;
        if (line.trimmed().isEmpty()) continue;
        const QStringList parts = line.split(',');
        if (parts.size() < 3) continue;
        const QString name = parts.at(0).trimmed();
        bool okx=false, oky=false, okz=true;
        const double x = parts.at(1).trimmed().toDouble(&okx);
        const double y = parts.at(2).trimmed().toDouble(&oky);
        double z = 0.0;
        if (parts.size() > 3) z = parts.at(3).trimmed().toDouble(&okz);
        if (!okx || !oky || !okz || name.isEmpty()) continue;
        if (m_pointManager->addPoint(name, x, y, z)) {
            Point p(name, x, y, z);
            m_canvas->addPoint(p);
            ++added;
        }
    }
    f.close();
    updatePointsTable();
    updateStatus();
    AppSettings::addRecentFile(filePath);
    if (m_welcomeWidget) m_welcomeWidget->reload();
    if (m_commandOutput) m_commandOutput->append(QString("Imported %1 points from %2").arg(added).arg(QFileInfo(filePath).fileName()));
    showToast(QString("Imported %1 points").arg(added));
    if (m_centerStack && m_canvas) m_centerStack->setCurrentWidget(m_canvas);
}

void MainWindow::exportCoordinates()
{
    const QString fileName = QFileDialog::getSaveFileName(this, "Export Coordinates",
                                                          QString(),
                                                          "CSV Files (*.csv);;All Files (*)");
    if (fileName.isEmpty()) return;
    QFile f(fileName);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export Coordinates", QString("Failed to write %1").arg(fileName));
        return;
    }
    QTextStream out(&f);
    out << "name,x,y,z\n";
    const QVector<Point> pts = m_pointManager->getAllPoints();
    for (const auto& p : pts) {
        out << p.name << "," << QString::number(p.x, 'f', 3) << ","
            << QString::number(p.y, 'f', 3) << "," << QString::number(p.z, 'f', 3) << "\n";
    }
    f.close();
    if (m_commandOutput) m_commandOutput->append(QString("Exported %1 points to %2").arg(pts.size()).arg(QFileInfo(fileName).fileName()));
    showToast(QString("Exported %1 points").arg(pts.size()));
}

void MainWindow::calculateDistance()
{
    const QStringList names = m_pointManager->getPointNames();
    if (names.size() < 2) { QMessageBox::information(this, "Distance", "Need at least two points."); return; }
    bool ok = false;
    const QString a = QInputDialog::getItem(this, "Distance", "From point:", names, 0, false, &ok);
    if (!ok || a.isEmpty()) return;
    const QString b = QInputDialog::getItem(this, "Distance", "To point:", names, 1, false, &ok);
    if (!ok || b.isEmpty()) return;
    const Point p1 = m_pointManager->getPoint(a);
    const Point p2 = m_pointManager->getPoint(b);
    double d = AppSettings::use3D() ? SurveyCalculator::distance3D(p1, p2)
                                    : SurveyCalculator::distance2D(p1, p2);
    const bool imperial = AppSettings::measurementUnits().compare(QStringLiteral("imperial"), Qt::CaseInsensitive) == 0;
    const double factor = imperial ? 3.28084 : 1.0; // m->ft
    const QString unit = imperial ? QStringLiteral("ft") : QStringLiteral("m");
    const QString msg = QString("Distance %1 -> %2 = %3 %4").arg(a, b).arg(d * factor, 0, 'f', 3).arg(unit);
    QMessageBox::information(this, "Distance", msg);
    if (m_commandOutput) m_commandOutput->append(msg);
}

void MainWindow::calculateArea()
{
    const QStringList names = m_pointManager->getPointNames();
    if (names.size() < 3) { QMessageBox::information(this, "Area", "Need at least three points."); return; }
    bool ok = false;
    const QString input = QInputDialog::getText(this, "Area", "Enter point names (comma separated):", QLineEdit::Normal, QString(), &ok);
    if (!ok || input.trimmed().isEmpty()) return;
    const QStringList list = input.split(',');
    QVector<Point> poly;
    for (const QString& n : list) {
        const QString name = n.trimmed();
        if (!m_pointManager->hasPoint(name)) continue;
        poly.push_back(m_pointManager->getPoint(name));
    }
    if (poly.size() < 3) { QMessageBox::information(this, "Area", "Not enough valid points."); return; }
    const double A_m2 = SurveyCalculator::area(poly);
    const bool imperial = AppSettings::measurementUnits().compare(QStringLiteral("imperial"), Qt::CaseInsensitive) == 0;
    const double factor2 = imperial ? (3.28084 * 3.28084) : 1.0; // m^2 -> ft^2
    const QString unit2 = imperial ? QStringLiteral("ft^2") : QStringLiteral("m^2");
    const QString msg = QString("Area (%1 points) = %2 %3").arg(poly.size()).arg(A_m2 * factor2, 0, 'f', 3).arg(unit2);
    QMessageBox::information(this, "Area", msg);
    if (m_commandOutput) m_commandOutput->append(msg);
}

void MainWindow::drawRegularPolygon()
{
    bool ok=false;
    int sides = QInputDialog::getInt(this, "Regular Polygon", "Number of sides:", 5, 3, 128, 0, &ok);
    if (!ok) return;
    if (m_canvas) {
        m_canvas->startRegularPolygonByEdge(sides);
        statusBar()->showMessage(QString("Regular polygon: pick first edge point (%1 sides)").arg(sides), 3000);
    }
}

void MainWindow::calculateAzimuth()
{
    const QStringList names = m_pointManager->getPointNames();
    if (names.size() < 2) { QMessageBox::information(this, "Bearing", "Need at least two points."); return; }
    bool ok = false;
    const QString a = QInputDialog::getItem(this, "Bearing", "From point:", names, 0, false, &ok);
    if (!ok || a.isEmpty()) return;
    const QString b = QInputDialog::getItem(this, "Bearing", "To point:", names, 1, false, &ok);
    if (!ok || b.isEmpty()) return;
    const Point p1 = m_pointManager->getPoint(a);
    const Point p2 = m_pointManager->getPoint(b);
    const double az = SurveyCalculator::azimuth(p1, p2);
    const QString dms = SurveyCalculator::toDMS(az);
    const QString msg = QString("Bearing %1 -> %2 = %3").arg(a, b, dms);
    QMessageBox::information(this, "Bearing", msg);
    if (m_commandOutput) m_commandOutput->append(msg);
}

void MainWindow::toolTrim()
{
    if (!m_canvas) return;
    m_canvas->startTrim();
    statusBar()->showMessage(QStringLiteral("Trim: select cutting edges and objects; press Esc to finish"), 3000);
}

void MainWindow::toolExtend()
{
    if (!m_canvas) return;
    m_canvas->startExtend();
    statusBar()->showMessage(QStringLiteral("Extend: select boundary edges and objects; press Esc to finish"), 3000);
}

void MainWindow::toolOffset()
{
    if (!m_canvas) return;
    const bool imperial = AppSettings::measurementUnits().compare(QStringLiteral("imperial"), Qt::CaseInsensitive) == 0;
    const QString unit = imperial ? QStringLiteral("ft") : QStringLiteral("m");
    bool ok = false;
    double val = QInputDialog::getDouble(this, QStringLiteral("Offset"),
                                         QStringLiteral("Distance (%1):").arg(unit),
                                         imperial ? (1.0/3.28084) : 1.0, -1e6, 1e6, 3, &ok);
    if (!ok) return;
    double meters = imperial ? (val * 0.3048) : val;
    m_canvas->startOffset(meters);
    statusBar()->showMessage(QStringLiteral("Offset: pick side; press Esc to finish"), 3000);
}

void MainWindow::toolFilletZero()
{
    if (!m_canvas) return;
    m_canvas->startFilletZero();
    statusBar()->showMessage(QStringLiteral("Fillet (0): select two lines; press Esc to finish"), 3000);
}

void MainWindow::toolChamfer()
{
    if (!m_canvas) return;
    const bool imperial = AppSettings::measurementUnits().compare(QStringLiteral("imperial"), Qt::CaseInsensitive) == 0;
    const QString unit = imperial ? QStringLiteral("ft") : QStringLiteral("m");
    bool ok = false;
    double val = QInputDialog::getDouble(this, QStringLiteral("Chamfer"),
                                         QStringLiteral("Distance (%1):").arg(unit),
                                         imperial ? (1.0/3.28084) : 1.0, -1e6, 1e6, 3, &ok);
    if (!ok) return;
    double meters = imperial ? (val * 0.3048) : val;
    m_canvas->startChamfer(meters);
    statusBar()->showMessage(QStringLiteral("Chamfer: select two lines; press Esc to finish"), 3000);
}

void MainWindow::updatePointsTable()
{
    if (!m_pointsTable || !m_pointManager) return;
    QSignalBlocker block1(m_pointsTable);
    m_updatingPointsTable = true;
    auto points = m_pointManager->getAllPoints();
    m_pointsTable->setRowCount(points.size());
    // Update headers based on Gauss mode
    {
        QStringList headers = AppSettings::gaussMode()
            ? (QStringList() << "Name" << "Y" << "X" << "Z" << "Layer")
            : (QStringList() << "Name" << "X" << "Y" << "Z" << "Layer");
        m_pointsTable->setHorizontalHeaderLabels(headers);
    }
    // Hide Z when in 2D mode
    m_pointsTable->setColumnHidden(3, !AppSettings::use3D());
    
    int row = 0;
    for (const auto& point : points) {
        // Name (editable)
        QTableWidgetItem* nameItem = new QTableWidgetItem(point.name);
        nameItem->setFlags((nameItem->flags() | Qt::ItemIsEditable));
        nameItem->setData(Qt::UserRole, point.name); // original name for rename tracking
        m_pointsTable->setItem(row, 0, nameItem);
        if (AppSettings::gaussMode()) {
            QTableWidgetItem* yi = new QTableWidgetItem(QString::number(point.y, 'f', 3)); yi->setFlags(yi->flags() | Qt::ItemIsEditable); yi->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter); m_pointsTable->setItem(row, 1, yi);
            QTableWidgetItem* xi = new QTableWidgetItem(QString::number(point.x, 'f', 3)); xi->setFlags(xi->flags() | Qt::ItemIsEditable); xi->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter); m_pointsTable->setItem(row, 2, xi);
        } else {
            QTableWidgetItem* xi = new QTableWidgetItem(QString::number(point.x, 'f', 3)); xi->setFlags(xi->flags() | Qt::ItemIsEditable); xi->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter); m_pointsTable->setItem(row, 1, xi);
            QTableWidgetItem* yi = new QTableWidgetItem(QString::number(point.y, 'f', 3)); yi->setFlags(yi->flags() | Qt::ItemIsEditable); yi->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter); m_pointsTable->setItem(row, 2, yi);
        }
        QTableWidgetItem* zi = new QTableWidgetItem(QString::number(point.z, 'f', 3)); zi->setFlags(zi->flags() | Qt::ItemIsEditable); zi->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter); m_pointsTable->setItem(row, 3, zi);

        // Layer combobox in column 4
        QComboBox* layerCombo = new QComboBox(m_pointsTable);
        if (m_layerManager) {
            for (const auto& L : m_layerManager->layers()) {
                QPixmap pm(12, 12); pm.fill(L.color);
                layerCombo->addItem(QIcon(pm), L.name);
            }
            // Select current layer for this point (from canvas), fallback to currentLayer
            QString current = m_canvas ? m_canvas->pointLayer(point.name) : m_layerManager->currentLayer();
            int idx = layerCombo->findText(current, Qt::MatchFixedString);
            if (idx >= 0) layerCombo->setCurrentIndex(idx);
        }
        // Capture the point name for this row
        const QString ptName = point.name;
        connect(layerCombo, &QComboBox::currentTextChanged, this, [this, ptName](const QString& layer){
            if (m_canvas) m_canvas->setPointLayer(ptName, layer);
        });
        m_pointsTable->setCellWidget(row, 4, layerCombo);
        row++;
    }
    m_updatingPointsTable = false;
    updateStatus();
}

void MainWindow::deleteSelectedCoordinates()
{
    if (!m_pointsTable || !m_pointManager) return;
    auto sel = m_pointsTable->selectionModel()->selectedRows();
    if (sel.isEmpty()) return;
    // Collect names first
    QStringList names;
    for (const auto& idx : sel) {
        int r = idx.row();
        if (r < 0 || r >= m_pointsTable->rowCount()) continue;
        QTableWidgetItem* it = m_pointsTable->item(r, 0);
        if (!it) continue;
        names << it->text();
    }
    for (const QString& name : names) {
        if (m_canvas) m_canvas->removePointByName(name);
        m_pointManager->removePoint(name);
    }
    updatePointsTable();
}

void MainWindow::onPointsCellChanged(QTableWidgetItem* item)
{
    if (!item || !m_pointsTable || !m_pointManager) return;
    if (m_updatingPointsTable) return;
    int row = item->row();
    int col = item->column();
    QTableWidgetItem* nameItem = m_pointsTable->item(row, 0);
    if (!nameItem) return;
    QString oldName = nameItem->data(Qt::UserRole).toString();
    if (oldName.isEmpty()) oldName = nameItem->text();
    // Column mapping
    const bool gauss = AppSettings::gaussMode();
    const int colX = gauss ? 2 : 1;
    const int colY = gauss ? 1 : 2;
    const int colZ = 3;

    if (col == 0) {
        // Rename
        const QString newName = nameItem->text().trimmed();
        if (newName.isEmpty() || newName == oldName) return;
        if (m_pointManager->hasPoint(newName)) {
            // Revert
            m_updatingPointsTable = true;
            nameItem->setText(oldName);
            m_updatingPointsTable = false;
            showToast(QString("Name '%1' exists.").arg(newName));
            return;
        }
        Point p = m_pointManager->getPoint(oldName);
        if (p.name.isEmpty()) return;
        // Update PM: remove old, add new
        m_pointManager->removePoint(oldName);
        p.name = newName;
        m_pointManager->addPoint(p);
        // Update canvas: rename point entity
        if (m_canvas) m_canvas->renamePoint(oldName, newName);
        // Update table's stored original name and layer connection will be refreshed on rebuild
        nameItem->setData(Qt::UserRole, newName);
        updatePointsTable();
        return;
    }

    // Coordinate edits: parse values
    auto parseCell = [&](int c, double& out){
        QTableWidgetItem* it = m_pointsTable->item(row, c);
        if (!it) return false;
        bool ok=false; double v = it->text().trimmed().toDouble(&ok);
        if (!ok) return false;
        out = v; return true;
    };
    double x=0.0,y=0.0,z=0.0;
    // Read from table for the edited cell and siblings
    if (!parseCell(colX, x)) return;
    if (!parseCell(colY, y)) return;
    if (!parseCell(colZ, z)) z = 0.0;
    // Update PointManager
    Point p = m_pointManager->getPoint(oldName);
    if (p.name.isEmpty()) return;
    p.x = x; p.y = y; p.z = z;
    m_pointManager->addPoint(p);
    // Update Canvas
    if (m_canvas) m_canvas->setPointXYZ(oldName, x, y, z);
    // Normalize formatting for edited cell
    m_updatingPointsTable = true;
    if (col == colX || col == colY) item->setText(QString::number((col==colX?x:y), 'f', 3));
    else if (col == colZ) item->setText(QString::number(z, 'f', 3));
    m_updatingPointsTable = false;
}

void MainWindow::updateCoordinates(const QPointF& worldPos)
{
    m_lastMouseWorld = worldPos;
    if (AppSettings::gaussMode()) {
        m_coordLabel->setText(QString("Y: %1  X: %2")
                             .arg(worldPos.y(), 0, 'f', 3)
                             .arg(worldPos.x(), 0, 'f', 3));
    } else {
        m_coordLabel->setText(QString("X: %1  Y: %2")
                             .arg(worldPos.x(), 0, 'f', 3)
                             .arg(worldPos.y(), 0, 'f', 3));
    }
}

void MainWindow::updateStatus()
{
    m_pointCountLabel->setText(QString("Points: %1").arg(m_pointManager->getAllPoints().size()));
}

void MainWindow::handleCanvasClick(const QPointF& worldPos)
{
    // Could implement point selection or adding points at click location
    QString message = QString("Canvas clicked at (%.3f, %.3f)")
                     .arg(worldPos.x()).arg(worldPos.y());
    m_statusBar->showMessage(message, 2000);
}

void MainWindow::appendToCommandOutput(const QString& text)
{
    // Command is already shown in executeCommand
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, "About SiteSurveyor Desktop",
        "<h2>SiteSurveyor Desktop</h2>"
        "<p>A modern Geomatics Software.</p>"
        "<p>Version 1.0.0</p>"
        "<p>© 2025 Eineva Incorporated. All rights reserved.</p>");
}

void MainWindow::showSettings()
{
    if (!m_settingsDialog) {
        m_settingsDialog = new SettingsDialog(m_canvas, this);
        if (m_settingsDialog) {
            connect(m_settingsDialog, &SettingsDialog::settingsApplied, this, [this](){
                updateStatus();
                refreshLayerCombo();
                if (m_canvas) m_canvas->update();
                updateMeasureLabelText();
            });
            connect(m_settingsDialog, &SettingsDialog::signOutRequested, this, [this](){
                if (m_welcomeWidget) m_welcomeWidget->signOut();
                updateLicenseStateUI();
                if (m_centerStack && m_welcomeWidget) m_centerStack->setCurrentWidget(m_welcomeWidget);
            });
        }
    }
    m_settingsDialog->reload();
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
    fadeInWidget(m_settingsDialog);
}


void MainWindow::showJoinPolar()
{
    if (!m_joinDialog) {
        m_joinDialog = new JoinPolarDialog(m_pointManager, m_canvas, this);
        // Keep dialog lists fresh when points change
        // (placeholder removed)
        connect(m_pointManager, &PointManager::pointAdded, m_joinDialog, &JoinPolarDialog::reload);
        connect(m_pointManager, &PointManager::pointRemoved, m_joinDialog, &JoinPolarDialog::reload);
        connect(m_pointManager, &PointManager::pointsCleared, m_joinDialog, &JoinPolarDialog::reload);
    }
    m_joinDialog->reload();
    m_joinDialog->show();
    m_joinDialog->raise();
    m_joinDialog->activateWindow();
    fadeInWidget(m_joinDialog);
}

void MainWindow::showPolarInput()
{
    if (!m_polarDialog) {
        m_polarDialog = new PolarInputDialog(m_pointManager, m_canvas, this);
        // Keep dialog sources fresh when points change
        connect(m_pointManager, &PointManager::pointAdded, m_polarDialog, &PolarInputDialog::reload);
        connect(m_pointManager, &PointManager::pointRemoved, m_polarDialog, &PolarInputDialog::reload);
        connect(m_pointManager, &PointManager::pointsCleared, m_polarDialog, &PolarInputDialog::reload);
    }
    m_polarDialog->reload();
    m_polarDialog->show();
    m_polarDialog->raise();
    m_polarDialog->activateWindow();
    fadeInWidget(m_polarDialog);
}

void MainWindow::showMassPolar()
{
    if (!m_massPolarDlg) {
        m_massPolarDlg = new MassPolarDialog(m_pointManager, m_canvas, this);
        if (m_pointManager) {
            connect(m_pointManager, &PointManager::pointAdded, m_massPolarDlg, &MassPolarDialog::reload);
            connect(m_pointManager, &PointManager::pointRemoved, m_massPolarDlg, &MassPolarDialog::reload);
            connect(m_pointManager, &PointManager::pointsCleared, m_massPolarDlg, &MassPolarDialog::reload);
        }
    }
    if (m_massPolarDlg) {
        m_massPolarDlg->reload();
        m_massPolarDlg->show();
        m_massPolarDlg->raise();
        m_massPolarDlg->activateWindow();
        fadeInWidget(m_massPolarDlg);
    }
}

void MainWindow::showTraverse()
{
    if (!m_traverseDialog) {
        m_traverseDialog = new TraverseDialog(m_pointManager, m_canvas, this);
        if (m_pointManager) {
            connect(m_pointManager, &PointManager::pointAdded, m_traverseDialog, &TraverseDialog::reload);
            connect(m_pointManager, &PointManager::pointRemoved, m_traverseDialog, &TraverseDialog::reload);
            connect(m_pointManager, &PointManager::pointsCleared, m_traverseDialog, &TraverseDialog::reload);
        }
    }
    if (m_traverseDialog) {
        m_traverseDialog->reload();
        m_traverseDialog->show();
        m_traverseDialog->raise();
        m_traverseDialog->activateWindow();
        fadeInWidget(m_traverseDialog);
    }
}

void MainWindow::showIntersectResection()
{
    if (!m_intersectResectionDlg) {
        m_intersectResectionDlg = new IntersectResectionDialog(m_pointManager, m_canvas, this);
        if (m_pointManager) {
            connect(m_pointManager, &PointManager::pointAdded, m_intersectResectionDlg, &IntersectResectionDialog::reload);
            connect(m_pointManager, &PointManager::pointRemoved, m_intersectResectionDlg, &IntersectResectionDialog::reload);
            connect(m_pointManager, &PointManager::pointsCleared, m_intersectResectionDlg, &IntersectResectionDialog::reload);
        }
    }
    if (m_intersectResectionDlg) {
        m_intersectResectionDlg->reload();
        m_intersectResectionDlg->show();
        m_intersectResectionDlg->raise();
        m_intersectResectionDlg->activateWindow();
        fadeInWidget(m_intersectResectionDlg);
    }
}

void MainWindow::showLeveling()
{
    if (!m_levelingDlg) {
        m_levelingDlg = new LevelingDialog(m_pointManager, m_canvas, this);
        if (m_pointManager) {
            connect(m_pointManager, &PointManager::pointAdded, m_levelingDlg, &LevelingDialog::reload);
            connect(m_pointManager, &PointManager::pointRemoved, m_levelingDlg, &LevelingDialog::reload);
            connect(m_pointManager, &PointManager::pointsCleared, m_levelingDlg, &LevelingDialog::reload);
        }
    }
    if (m_levelingDlg) {
        m_levelingDlg->reload();
        m_levelingDlg->show();
        m_levelingDlg->raise();
        m_levelingDlg->activateWindow();
        fadeInWidget(m_levelingDlg);
    }
}

void MainWindow::showLSNetwork()
{
    if (!m_lsNetworkDlg) {
        m_lsNetworkDlg = new LSNetworkDialog(m_pointManager, m_canvas, this);
        if (m_pointManager) {
            connect(m_pointManager, &PointManager::pointAdded, m_lsNetworkDlg, &LSNetworkDialog::reload);
            connect(m_pointManager, &PointManager::pointRemoved, m_lsNetworkDlg, &LSNetworkDialog::reload);
            connect(m_pointManager, &PointManager::pointsCleared, m_lsNetworkDlg, &LSNetworkDialog::reload);
        }
    }
    if (m_lsNetworkDlg) {
        m_lsNetworkDlg->reload();
        m_lsNetworkDlg->show();
        m_lsNetworkDlg->raise();
        m_lsNetworkDlg->activateWindow();
        fadeInWidget(m_lsNetworkDlg);
    }
}

void MainWindow::showTransformations()
{
    if (!m_transformDlg) {
        m_transformDlg = new TransformDialog(m_pointManager, m_canvas, this);
        if (m_pointManager) {
            connect(m_pointManager, &PointManager::pointAdded, m_transformDlg, &TransformDialog::reload);
            connect(m_pointManager, &PointManager::pointRemoved, m_transformDlg, &TransformDialog::reload);
            connect(m_pointManager, &PointManager::pointsCleared, m_transformDlg, &TransformDialog::reload);
        }
    }
    if (m_transformDlg) {
        m_transformDlg->reload();
        m_transformDlg->show();
        m_transformDlg->raise();
        m_transformDlg->activateWindow();
        fadeInWidget(m_transformDlg);
    }
}

// --- Animation helpers ---
void MainWindow::fadeInWidget(QWidget* w, int duration)
{
    if (!w) return;
    QPointer<QWidget> wGuard = w;
    auto* effRaw = new QGraphicsOpacityEffect(w);
    QPointer<QGraphicsOpacityEffect> eff = effRaw;
    w->setGraphicsEffect(effRaw);
    effRaw->setOpacity(0.0);
    auto* anim = new QPropertyAnimation(effRaw, "opacity", w);
    anim->setDuration(duration);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QPropertyAnimation::finished, this, [wGuard, eff](){
        if (wGuard) wGuard->setGraphicsEffect(nullptr);
        if (eff) eff->deleteLater();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::showToast(const QString& msg, int durationMs)
{
    QLabel* toast = new QLabel(msg, this);
    toast->setObjectName("ToastOverlay");
    toast->setStyleSheet(
        "QLabel#ToastOverlay {"
        "  background-color: rgba(20,20,20,220);"
        "  color: #ffffff;"
        "  border: 1px solid rgba(255,255,255,30);"
        "  border-radius: 6px;"
        "  padding: 6px 12px;"
        "}"
    );
    toast->setAttribute(Qt::WA_TransparentForMouseEvents);
    toast->setWindowFlags(Qt::FramelessWindowHint | Qt::ToolTip);
    toast->adjustSize();
    const int tb = m_statusBar ? m_statusBar->height() : 24;
    const int x = (width() - toast->width()) / 2;
    const int y = height() - tb - toast->height() - 12;
    toast->move(x, y);
    toast->show();

    auto* eff = new QGraphicsOpacityEffect(toast);
    toast->setGraphicsEffect(eff);
    eff->setOpacity(0.0);
    auto* animIn = new QPropertyAnimation(eff, "opacity", toast);
    animIn->setDuration(160);
    animIn->setStartValue(0.0);
    animIn->setEndValue(1.0);
    animIn->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(animIn, &QPropertyAnimation::finished, this, [this, toast, eff, durationMs](){
        QTimer::singleShot(durationMs, this, [toast, eff](){
            auto* animOut = new QPropertyAnimation(eff, "opacity", toast);
            animOut->setDuration(220);
            animOut->setStartValue(1.0);
            animOut->setEndValue(0.0);
            animOut->setEasingCurve(QEasingCurve::InCubic);
            QObject::connect(animOut, &QPropertyAnimation::finished, toast, [toast](){ toast->deleteLater(); });
            animOut->start(QAbstractAnimation::DeleteWhenStopped);
        });
    });
    animIn->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::pulseLabel(QWidget* w, int duration)
{
    if (!w) return;
    QPointer<QWidget> wGuard = w;
    auto* effRaw = new QGraphicsOpacityEffect(w);
    QPointer<QGraphicsOpacityEffect> eff = effRaw;
    w->setGraphicsEffect(effRaw);
    effRaw->setOpacity(0.4);
    auto* anim = new QPropertyAnimation(effRaw, "opacity", w);
    anim->setDuration(duration);
    anim->setStartValue(0.4);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QPropertyAnimation::finished, this, [wGuard, eff](){
        if (wGuard) wGuard->setGraphicsEffect(nullptr);
        if (eff) eff->deleteLater();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::animateRightDockToWidth(int targetWidth)
{
    if (!m_layersDock) return;
    m_layersDock->setVisible(true);
    m_layersDock->raise();
    QRect g = m_layersDock->geometry();
    if (g.width() <= 1) { m_layersDock->resize(1, g.height()); g = m_layersDock->geometry(); }
    QRect end(g);
    end.setWidth(targetWidth);
    end.moveLeft(g.right() - targetWidth + 1);
    auto* anim = new QPropertyAnimation(m_layersDock, "geometry", this);
    anim->setDuration(180);
    anim->setStartValue(g);
    anim->setEndValue(end);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::animateRightDockClose()
{
    if (!m_layersDock) return;
    QRect g = m_layersDock->geometry();
    QRect end(g);
    end.setWidth(1);
    end.moveLeft(g.right());
    auto* anim = new QPropertyAnimation(m_layersDock, "geometry", this);
    anim->setDuration(160);
    anim->setStartValue(g);
    anim->setEndValue(end);
    anim->setEasingCurve(QEasingCurve::InCubic);
    connect(anim, &QPropertyAnimation::finished, this, [this](){ if (m_layersDock) m_layersDock->hide(); });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::onZoomChanged(double zoom)
{
    // Show as percentage (rounded)
    const QString text = QString("Zoom: %1%")
                             .arg(QString::number(zoom * 100.0, 'f', 0));
    if (m_zoomLabel) {
        m_zoomLabel->setText(text);
    }
}

void MainWindow::onPointsTableSelectionChanged()
{
    if (!m_pointsTable || !m_pointManager || !m_canvas) return;
    QList<QTableWidgetSelectionRange> ranges = m_pointsTable->selectedRanges();
    if (ranges.isEmpty()) return;
    int row = ranges.first().topRow();
    if (row < 0 || row >= m_pointsTable->rowCount()) return;
    QTableWidgetItem* nameItem = m_pointsTable->item(row, 0);
    if (!nameItem) return;
    const QString name = nameItem->text();
    if (!m_pointManager->hasPoint(name)) return;
    const Point p = m_pointManager->getPoint(name);
    // Center on point without changing the current zoom level
    m_canvas->centerOnPoint(QPointF(p.x, p.y));
}

void MainWindow::refreshLayerCombo()
{
    if (!m_layerCombo || !m_layerManager) return;
    const QString current = m_layerManager->currentLayer();
    m_layerCombo->blockSignals(true);
    m_layerCombo->clear();
    for (const auto& L : m_layerManager->layers()) {
        QPixmap pm(16, 16);
        pm.fill(L.color);
        m_layerCombo->addItem(QIcon(pm), L.name);
        if (L.name.compare(current, Qt::CaseInsensitive) == 0) {
            m_layerCombo->setCurrentIndex(m_layerCombo->count()-1);
        }
    }
    m_layerCombo->blockSignals(false);
}

void MainWindow::onLayerComboChanged(int index)
{
    if (!m_layerCombo || !m_layerManager) return;
    if (index < 0 || index >= m_layerCombo->count()) return;
    const QString name = m_layerCombo->itemText(index);
    m_layerManager->setCurrentLayer(name);
    updateLayerStatusText();
}

void MainWindow::onDrawingDistanceChanged(double meters)
{
    if (!m_measureLabel) return;
    m_liveDistanceMeters = meters;
    updateMeasureLabelText();
    pulseLabel(m_measureLabel);
}

void MainWindow::onDrawingAngleChanged(double degrees)
{
    if (!m_measureLabel) return;
    m_liveAngleDegrees = degrees;
    updateMeasureLabelText();
}

void MainWindow::updateMeasureLabelText()
{
    if (!m_measureLabel) return;
    const bool imperial = AppSettings::measurementUnits().compare(QStringLiteral("imperial"), Qt::CaseInsensitive) == 0;
    const double factor = imperial ? 3.28084 : 1.0; // m->ft
    const QString unit = imperial ? QStringLiteral("ft") : QStringLiteral("m");
    QString text;
    if (m_canvas && (m_canvas->toolMode() == CanvasWidget::ToolMode::DrawCircle)) {
        text = QString("R: %1 %2  D: %3 %4").arg(m_liveDistanceMeters * factor, 0, 'f', 3).arg(unit)
                                             .arg(m_liveDistanceMeters * 2.0 * factor, 0, 'f', 3).arg(unit);
    } else if (m_canvas && (m_canvas->toolMode() == CanvasWidget::ToolMode::DrawLine)) {
        // For line drawing, show L and Brg (bearing) in DMS
        text = QString("L=%1 %2").arg(m_liveDistanceMeters * factor, 0, 'f', 3).arg(unit);
        text += QString("  Brg=%1").arg(SurveyCalculator::toDMS(m_liveAngleDegrees));
    } else {
        // Default (other tools): show length and bearing
        text = QString("Len: %1 %2").arg(m_liveDistanceMeters * factor, 0, 'f', 3).arg(unit);
        text += QString("  Brg=%1").arg(SurveyCalculator::toDMS(m_liveAngleDegrees));
    }
    m_measureLabel->setText(text);
}

void MainWindow::updateLayerStatusText()
{
    if (!m_layerStatusLabel || !m_layerManager) return;
    const QString name = m_layerManager->currentLayer();
    m_layerStatusLabel->setText(QString("Layer: %1").arg(name.isEmpty() ? QStringLiteral("0") : name));
}

void MainWindow::toggleLeftPanel()
{
    // Toggle visibility of left panel (Coordinates)
    if (!m_pointsDock) return;

    const bool visible = m_pointsDock->isVisible();

    if (visible) {
        m_pointsDock->hide();
        if (m_leftPanelButton) {
            m_leftPanelButton->setArrowType(Qt::RightArrow);
            m_leftPanelButton->setToolTip("Show coordinates panel (Ctrl+L)");
        }
    } else {
        m_pointsDock->show();
        // Make sure it's properly sized (wider)
        m_pointsDock->resize(350, m_pointsDock->height());
        m_pointsDock->raise();
        if (m_leftPanelButton) {
            m_leftPanelButton->setArrowType(Qt::LeftArrow);
            m_leftPanelButton->setToolTip("Hide coordinates panel (Ctrl+L)");
        }
    }

    updatePanelToggleStates();
}

void MainWindow::onRightDockVisibilityChanged(bool visible)
{
    Q_UNUSED(visible);
    if (m_syncingRightDock) return;
    m_syncingRightDock = true;
    if (m_toggleRightPanelAction) m_toggleRightPanelAction->setChecked(m_layersDock && m_layersDock->isVisible());
    updatePanelToggleStates();
    m_rightDockClosingByUser = false;
    m_syncingRightDock = false;
}

void MainWindow::setRightPanelsVisible(bool visible)
{
    // Control visibility of the merged right sidebar
    if (!m_layersDock) return;
    if (m_syncingRightDock) return;
    m_syncingRightDock = true;

    if (visible) {
        int rpw = AppSettings::rightPanelWidth(); if (rpw < 200) rpw = 200;
        animateRightDockToWidth(rpw);
    } else {
        animateRightDockClose();
    }

    if (m_toggleRightPanelAction) m_toggleRightPanelAction->setChecked(visible);
    updatePanelToggleStates();
    m_syncingRightDock = false;
}

void MainWindow::toggleRightPanel()
{
    // Toggle visibility of merged right panel
    if (!m_layersDock) return;
    const bool rightVisible = m_layersDock->isVisible();
    setRightPanelsVisible(!rightVisible);
}

void MainWindow::toggleCommandPanel()
{
    // Toggle visibility of command panel at bottom
    if (!m_commandDock) return;
    
    bool visible = m_commandDock->isVisible();
    m_commandDock->setVisible(!visible);
    
    // Update menu action state
    if (m_toggleCommandPanelAction) {
        m_toggleCommandPanelAction->setChecked(!visible);
    }
    updatePanelToggleStates();
}

void MainWindow::createPanelToggleButtons()
{
    // Create toggle button for left panels
    m_leftPanelButton = new QToolButton(this);
    m_leftPanelButton->setArrowType(Qt::LeftArrow);
    m_leftPanelButton->setAutoRaise(false);
    m_leftPanelButton->setFixedSize(18, 80);
    m_leftPanelButton->setToolTip("Hide coordinates panel (Ctrl+L)");
    m_leftPanelButton->setStyleSheet(
        "QToolButton {"
        "  background-color: #4a4a4a;"
        "  border: 1px solid #666;"
        "  border-top-right-radius: 4px;"
        "  border-bottom-right-radius: 4px;"
        "  border-left: none;"
        "}"
        "QToolButton:hover {"
        "  background-color: #5a5a5a;"
        "}"
        "QToolButton:pressed {"
        "  background-color: #6a6a6a;"
        "}"
    );
    connect(m_leftPanelButton, &QToolButton::clicked, this, &MainWindow::toggleLeftPanel);
    
    // Create toggle button for right panel
    m_rightPanelButton = new QToolButton(this);
    m_rightPanelButton->setArrowType(Qt::RightArrow);
    m_rightPanelButton->setAutoRaise(false);
    m_rightPanelButton->setFixedSize(18, 80);
    m_rightPanelButton->setToolTip("Hide right panels (Ctrl+R)");
    m_rightPanelButton->setStyleSheet(
        "QToolButton {"
        "  background-color: #4a4a4a;"
        "  border: 1px solid #666;"
        "  border-top-left-radius: 4px;"
        "  border-bottom-left-radius: 4px;"
        "  border-right: none;"
        "}"
        "QToolButton:hover {"
        "  background-color: #5a5a5a;"
        "}"
        "QToolButton:pressed {"
        "  background-color: #6a6a6a;"
        "}"
    );
    connect(m_rightPanelButton, &QToolButton::clicked, this, &MainWindow::toggleRightPanel);
    
    // Position buttons at the edges of the central widget
    // These will be repositioned in resizeEvent
    m_leftPanelButton->setParent(centralWidget());
    m_rightPanelButton->setParent(centralWidget());
    
    // Make sure buttons stay on top
    m_leftPanelButton->raise();
    m_rightPanelButton->raise();
    
    // Initially position them
    updateToggleButtonPositions();
    updatePanelToggleStates();
}

void MainWindow::updateToggleButtonPositions()
{
    if (!centralWidget()) return;
    
    int centerY = centralWidget()->height() / 2;
    
    // Position left toggle button at left edge of central area
    if (m_leftPanelButton) {
        m_leftPanelButton->move(0, centerY - 40);
        m_leftPanelButton->raise();
    }
    
    // Position right toggle button at right edge of central area
    if (m_rightPanelButton) {
        m_rightPanelButton->move(centralWidget()->width() - m_rightPanelButton->width(), centerY - 40);
        m_rightPanelButton->raise();
    }
}

void MainWindow::updatePanelToggleStates()
{
    const bool leftVisible = m_pointsDock && m_pointsDock->isVisible();
    const bool rightVisible = (m_layersDock && m_layersDock->isVisible());
    const bool cmdVisible = m_commandDock && m_commandDock->isVisible();
    const bool planVisible = m_projectPlanDock && m_projectPlanDock->isVisible();
    
    // Keep edge toggle buttons always visible, change arrow and tooltip based on state
    if (m_leftPanelButton) {
        m_leftPanelButton->setVisible(true);
        m_leftPanelButton->setArrowType(leftVisible ? Qt::LeftArrow : Qt::RightArrow);
        m_leftPanelButton->setToolTip(leftVisible ? "Hide coordinates panel (Ctrl+L)" : "Show coordinates panel (Ctrl+L)");
    }
    if (m_rightPanelButton) {
        m_rightPanelButton->setVisible(true);
        m_rightPanelButton->setArrowType(rightVisible ? Qt::LeftArrow : Qt::RightArrow);
        m_rightPanelButton->setToolTip(rightVisible ? "Hide right panels (Ctrl+R)" : "Show right panels (Ctrl+R)");
    }
    
    // Sync menu items (block signals to avoid re-entrant toggles)
    if (m_toggleLeftPanelAction) { QSignalBlocker b(m_toggleLeftPanelAction); m_toggleLeftPanelAction->setChecked(leftVisible); }
    if (m_toggleRightPanelAction) { QSignalBlocker b(m_toggleRightPanelAction); m_toggleRightPanelAction->setChecked(rightVisible); }
    if (m_toggleCommandPanelAction) { QSignalBlocker b(m_toggleCommandPanelAction); m_toggleCommandPanelAction->setChecked(cmdVisible); }
    if (m_toggleProjectPlanAction) { QSignalBlocker b(m_toggleProjectPlanAction); m_toggleProjectPlanAction->setChecked(planVisible); }
    
    // Reposition buttons
    updateToggleButtonPositions();
}

void MainWindow::setupProjectPlanDock()
{
    if (m_projectPlanDock) return;
    m_projectPlanPanel = new ProjectPlanPanel(this);
    m_projectPlanDock = new QDockWidget("Project Plan", this);
    m_projectPlanDock->setObjectName("ProjectPlanDock");
    m_projectPlanDock->setWidget(m_projectPlanPanel);
    m_projectPlanDock->setAllowedAreas(Qt::LeftDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, m_projectPlanDock);
    if (m_pointsDock) {
        tabifyDockWidget(m_pointsDock, m_projectPlanDock);
        m_pointsDock->raise();
    }
    // Start hidden; user can toggle from View menu or toolbar
    m_projectPlanDock->hide();
    connect(m_projectPlanDock, &QDockWidget::visibilityChanged, this, [this](bool vis){
        if (m_toggleProjectPlanAction) {
            QSignalBlocker b(m_toggleProjectPlanAction);
            m_toggleProjectPlanAction->setChecked(vis);
        }
        updatePanelToggleStates();
    });
}

void MainWindow::toggleProjectPlanPanel()
{
    if (!m_projectPlanDock) return;
    const bool vis = m_projectPlanDock->isVisible();
    m_projectPlanDock->setVisible(!vis);
    if (!vis) {
        // Ensure it remains tabified with coordinates panel
        if (m_pointsDock) {
            tabifyDockWidget(m_pointsDock, m_projectPlanDock);
            m_projectPlanDock->raise();
        }
    }
    updatePanelToggleStates();
}
