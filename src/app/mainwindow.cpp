#include "app/mainwindow.h"
#include "app/settingsdialog.h"
#include "auth/authmanager.h"
#include "auth/cloudmanager.h"
#include "auth/cloudfiledialog.h"
#include "auth/versionhistorydialog.h"
#include "auth/shareprojectdialog.h"
#include "auth/conflictdialog.h"
#include "canvas/canvaswidget.h"
#include "dxf/dxfreader.h"
#include "gdal/gdalreader.h"
#include "gdal/gdalwriter.h"
#include "gdal/gdalgeosloader.h"
#include "gdal/geosbridge.h"
#include "gama/gamaexporter.h"
#include "gama/gamarunner.h"
#include "gama/network_adjustment_dialog.h"
#include "tools/snapper.h"
#include "tools/levellingdialog.h"
#include "tools/resection_dialog.h"
#include "tools/check_geometry_dialog.h"
#include "tools/intersection_dialog.h"
#include "tools/polar_dialog.h"
#include "tools/join_dialog.h"
#include "tools/traverse_dialog.h"
#include "tools/volume_dialog.h"
#include "tools/contour_dialog.h"
#include "tools/station_setup_dialog.h"


#include "categories/category_manager.h"
#include "console/command_console.h"



#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QStatusBar>
#include <QLabel>
#include <QMessageBox>
#include <QApplication>
#include <QDockWidget>
#include <QListWidget>
#include <QTreeWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QInputDialog>
#include <QColorDialog>
#include <QVBoxLayout>
#include <QStackedLayout>
#include <QPushButton>
#include <QFileInfo>
#include <QDir>
#include <QToolBar>
#include <QSettings>
#include <QDesktopServices>
#include <QTextStream>
#include <QRegularExpression>

#include <QUrl>
#include <QTextBrowser>
#include <QCloseEvent>
#include <ogr_spatialref.h>

MainWindow::MainWindow(AuthManager* auth, QWidget *parent)
    : QMainWindow(parent)
    , m_auth(auth)
{
    m_cloudManager = new CloudManager(m_auth, this);
    setWindowTitle("SiteSurveyor");
    setWindowIcon(QIcon(":/branding/logo.ico"));
    resize(1200, 800);

    // Initialize GDAL
    GdalReader::initialize();

    // Create canvas as central widget
    m_canvas = new CanvasWidget(this);
    setCentralWidget(m_canvas);

    // Setup menus
    setupMenus();
    
    // Setup toolbar
    setupToolbar();
    
    // Setup status bar
    setupStatusBar();
    
    // Setup layer panel
    setupLayerPanel();
    
    // Setup properties panel
    setupPropertiesPanel();
    
    // Connect signals
    connect(m_canvas, &CanvasWidget::mouseWorldPosition, this, &MainWindow::updateCoordinates);
    connect(m_canvas, &CanvasWidget::zoomChanged, this, &MainWindow::updateZoom);
    connect(m_canvas, &CanvasWidget::layersChanged, this, &MainWindow::updateLayerPanel);
    connect(m_canvas, &CanvasWidget::statusMessage, this, [this](const QString& msg) {
        statusBar()->showMessage(msg, 5000);
    });
    connect(m_canvas, &CanvasWidget::pegDeleted, this, &MainWindow::updatePegPanel);
    connect(m_canvas, &CanvasWidget::pegAdded, this, &MainWindow::updatePegPanel);

    
    // Load saved settings

    QSettings settings;
    if (m_canvas) {
        m_canvas->setSouthAzimuth(settings.value("coordinates/southAzimuth", false).toBool());
        m_canvas->setSwapXY(settings.value("coordinates/swapXY", false).toBool());
        
        // Load theme
        QString theme = settings.value("appearance/theme", "light").toString();
        QString themePath = (theme == "dark") ? ":/styles/dark-theme.qss" : ":/styles/light-theme.qss";
        QFile styleFile(themePath);
        if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
            qApp->setStyleSheet(QLatin1String(styleFile.readAll()));
            styleFile.close();
        }
    }
}

MainWindow::~MainWindow()
{
}

void MainWindow::setCategory(SurveyCategory category)
{
    m_category = category;
    applyMenuFilters();
    
    // Update window title to show category
    QString categoryStr = CategoryManager::getCategoryDisplayName(category);
    statusBar()->showMessage(QString("Project type: %1").arg(categoryStr), 3000);
}

void MainWindow::applyMenuFilters()
{
    using CM = CategoryManager;
    
    // Show/hide actions based on category availability
    if (m_offsetAction) {
        m_offsetAction->setVisible(CM::isFeatureAvailable(m_category, CM::Offset));
    }
    if (m_partitionAction) {
        m_partitionAction->setVisible(CM::isFeatureAvailable(m_category, CM::Partition));
    }
    if (m_levellingAction) {
        m_levellingAction->setVisible(CM::isFeatureAvailable(m_category, CM::Levelling));
    }
    if (m_gamaAction) {
        m_gamaAction->setVisible(CM::isFeatureAvailable(m_category, CM::GamaAdjust));
    }
    if (m_stakeoutAction) {
        m_stakeoutAction->setVisible(
            CategoryManager::isFeatureAvailable(m_category, CategoryManager::Stakeout));
    }
    
    if (m_resectionAction) {
        m_resectionAction->setVisible(
            CategoryManager::isFeatureAvailable(m_category, CategoryManager::Resection));
    }
    
    if (m_intersectionAction) {
        m_intersectionAction->setVisible(
            CategoryManager::isFeatureAvailable(m_category, CategoryManager::Intersection));
    }
    
    if (m_polarAction) {
        m_polarAction->setVisible(
            CategoryManager::isFeatureAvailable(m_category, CategoryManager::PolarCalculation));
    }
    
    if (m_joinCalcAction) {
        m_joinCalcAction->setVisible(
            CategoryManager::isFeatureAvailable(m_category, CategoryManager::JoinCalculation));
    }
}

void MainWindow::setupMenus()
{
    // File menu
    QMenu* fileMenu = menuBar()->addMenu("&File");
    
    QAction* newAction = fileMenu->addAction("&New Project");
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, [this]() {
        try {
            if (!m_canvas) {
                QMessageBox::warning(this, "Error", "Canvas not initialized");
                return;
            }
            if (QMessageBox::question(this, "New Project", 
                    "Clear current project and start new?") == QMessageBox::Yes) {
                m_canvas->clearAll();
                m_canvas->setProjectFilePath("");
                setWindowTitle("SiteSurveyor - Untitled");
                updateLayerPanel();
            }
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "Error", QString("New Project failed: %1").arg(e.what()));
        } catch (...) {
            QMessageBox::critical(this, "Error", "New Project failed with unknown error");
        }
    });
    
    QAction* openAction = fileMenu->addAction(QIcon(":/icons/open.png"), "&Open Project...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this]() {
        QString fileName = QFileDialog::getOpenFileName(this, "Open Project", "", "SiteSurveyor Project (*.ssp)");
        if (!fileName.isEmpty()) {
            if (m_canvas->loadProject(fileName)) {
                m_canvas->setProjectFilePath(fileName);
                setWindowTitle(QString("SiteSurveyor - %1").arg(QFileInfo(fileName).fileName()));
                statusBar()->showMessage("Project loaded", 3000);
                addToRecentProjects(fileName);
                updateLayerPanel();
            } else {
                QMessageBox::critical(this, "Error", "Failed to load project");
            }
        }
    });

    QAction* openCloudAction = fileMenu->addAction(QIcon(":/icons/cloud-download.png"), "Ope&n from Cloud...");
    connect(openCloudAction, &QAction::triggered, this, &MainWindow::openFromCloud);
    
    m_recentMenu = fileMenu->addMenu("Recent &Projects");
    updateRecentProjectsMenu();
    
    QAction* saveAction = fileMenu->addAction(QIcon(":/icons/save.png"), "&Save Project");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, [this]() {
        if (m_canvas->projectFilePath().isEmpty()) {
            QString fileName = QFileDialog::getSaveFileName(this, "Save Project", "", "SiteSurveyor Project (*.ssp)");
            if (!fileName.isEmpty()) {
                if (m_canvas->saveProject(fileName)) {
                    m_canvas->setProjectFilePath(fileName);
                    setWindowTitle(QString("SiteSurveyor - %1").arg(QFileInfo(fileName).fileName()));
                    statusBar()->showMessage("Project saved", 3000);
                } else {
                    QMessageBox::critical(this, "Error", "Failed to save project");
                }
            }
        } else {
            if (m_canvas->saveProject(m_canvas->projectFilePath())) {
                statusBar()->showMessage("Project saved", 3000);
            } else {
                QMessageBox::critical(this, "Error", "Failed to save project");
            }
        }
    });

    QAction* saveCloudAction = fileMenu->addAction(QIcon(":/icons/cloud-upload.png"), "Save to &Cloud...");
    connect(saveCloudAction, &QAction::triggered, this, &MainWindow::saveToCloud);
    
    fileMenu->addSeparator();

    QAction* versionAction = fileMenu->addAction(QIcon(":/icons/history.svg"), "Version &History");
    connect(versionAction, &QAction::triggered, this, &MainWindow::showVersionHistory);
    
    QAction* shareAction = fileMenu->addAction(QIcon(":/icons/share.svg"), "S&hare Project");
    connect(shareAction, &QAction::triggered, this, &MainWindow::shareProject);
    
    fileMenu->addSeparator();
    
    QAction* saveAsAction = fileMenu->addAction("Save &As...");
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, [this]() {
        QString filePath = QFileDialog::getSaveFileName(this,
            "Save Project As", QString(),
            "SiteSurveyor Project (*.ssp);;All Files (*)");
        if (!filePath.isEmpty()) {
            if (!filePath.endsWith(".ssp")) filePath += ".ssp";
            if (m_canvas->saveProject(filePath)) {
                m_canvas->setProjectFilePath(filePath);
                setWindowTitle(QString("SiteSurveyor - %1").arg(QFileInfo(filePath).fileName()));
                statusBar()->showMessage("Project saved.", 3000);
            } else {
                QMessageBox::warning(this, "Error", "Failed to save project file.");
            }
        }
    });
    
    fileMenu->addSeparator();
    
    QAction* importDxfAction = fileMenu->addAction("Import &DXF...");
    importDxfAction->setShortcut(QKeySequence("Ctrl+D"));
    connect(importDxfAction, &QAction::triggered, this, &MainWindow::importDXF);
    
    QAction* importGisAction = fileMenu->addAction("Import &GIS Data...");
    importGisAction->setShortcut(QKeySequence("Ctrl+G"));
    connect(importGisAction, &QAction::triggered, this, &MainWindow::importGDAL);
    
    QAction* importCsvAction = fileMenu->addAction("Import &CSV Points...");
    connect(importCsvAction, &QAction::triggered, this, &MainWindow::importCSVPoints);
    
    fileMenu->addSeparator();

    
    // Export submenu
    QMenu* exportMenu = fileMenu->addMenu("E&xport");
    
    QAction* exportShpAction = exportMenu->addAction("Export to &Shapefile...");
    exportShpAction->setToolTip("Export polylines and pegs to ESRI Shapefile format");
    connect(exportShpAction, &QAction::triggered, this, [this]() {
        if (!m_canvas) return;
        QString filePath = QFileDialog::getSaveFileName(this, "Export to Shapefile",
            QString(), "Shapefile (*.shp)");
        if (filePath.isEmpty()) return;
        if (!filePath.endsWith(".shp")) filePath += ".shp";
        
        GdalWriter writer;
        bool success = true;
        
        // Export polylines
        if (!m_canvas->polylines().isEmpty()) {
            if (!writer.exportToShapefile(m_canvas->polylines(), filePath, m_canvas->crs())) {
                success = false;
            }
        }
        
        // Export pegs to separate file
        if (!m_canvas->pegs().isEmpty()) {
            QString pegPath = filePath;
            pegPath.replace(".shp", "_pegs.shp");
            if (!writer.exportPegsToShapefile(m_canvas->pegs(), pegPath, m_canvas->crs())) {
                success = false;
            }
        }
        
        if (success) {
            statusBar()->showMessage("Exported to Shapefile successfully", 3000);
        } else {
            QMessageBox::warning(this, "Export Error", writer.lastError());
        }
    });
    
    QAction* exportGeoJsonAction = exportMenu->addAction("Export to &GeoJSON...");
    exportGeoJsonAction->setToolTip("Export to GeoJSON format for web mapping");
    connect(exportGeoJsonAction, &QAction::triggered, this, [this]() {
        if (!m_canvas) return;
        QString filePath = QFileDialog::getSaveFileName(this, "Export to GeoJSON",
            QString(), "GeoJSON (*.geojson)");
        if (filePath.isEmpty()) return;
        if (!filePath.endsWith(".geojson")) filePath += ".geojson";
        
        GdalWriter writer;
        bool success = true;
        
        if (!m_canvas->polylines().isEmpty()) {
            if (!writer.exportToGeoJSON(m_canvas->polylines(), filePath, m_canvas->crs())) {
                success = false;
            }
        }
        
        if (!m_canvas->pegs().isEmpty()) {
            QString pegPath = filePath;
            pegPath.replace(".geojson", "_pegs.geojson");
            if (!writer.exportPegsToGeoJSON(m_canvas->pegs(), pegPath, m_canvas->crs())) {
                success = false;
            }
        }
        
        if (success) {
            statusBar()->showMessage("Exported to GeoJSON successfully", 3000);
        } else {
            QMessageBox::warning(this, "Export Error", writer.lastError());
        }
    });
    
    QAction* exportKmlAction = exportMenu->addAction("Export to &KML (Google Earth)...");
    exportKmlAction->setToolTip("Export for viewing in Google Earth");
    connect(exportKmlAction, &QAction::triggered, this, [this]() {
        if (!m_canvas) return;
        QString filePath = QFileDialog::getSaveFileName(this, "Export to KML",
            QString(), "KML (*.kml)");
        if (filePath.isEmpty()) return;
        if (!filePath.endsWith(".kml")) filePath += ".kml";
        
        GdalWriter writer;
        if (writer.exportToKML(m_canvas->polylines(), m_canvas->pegs(), filePath)) {
            statusBar()->showMessage("Exported to KML successfully", 3000);
        } else {
            QMessageBox::warning(this, "Export Error", writer.lastError());
        }
    });
    
    fileMenu->addSeparator();
    
    QAction* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);
    
    // Edit menu
    QMenu* editMenu = menuBar()->addMenu("&Edit");
    
    QAction* undoAction = editMenu->addAction("&Undo");
    undoAction->setShortcut(QKeySequence::Undo);
    undoAction->setEnabled(false);
    connect(undoAction, &QAction::triggered, m_canvas, &CanvasWidget::undo);
    
    QAction* redoAction = editMenu->addAction("&Redo");
    redoAction->setShortcut(QKeySequence::Redo);
    redoAction->setEnabled(false);
    connect(redoAction, &QAction::triggered, m_canvas, &CanvasWidget::redo);
    
    // Update undo/redo action enabled state
    connect(m_canvas, &CanvasWidget::undoRedoChanged, this, [undoAction, redoAction, this]() {
        undoAction->setEnabled(m_canvas->canUndo());
        redoAction->setEnabled(m_canvas->canRedo());
    });
    
    editMenu->addSeparator();
    
    QAction* settingsAction = editMenu->addAction("&Settings...");
    settingsAction->setShortcut(QKeySequence("Ctrl+,"));
    connect(settingsAction, &QAction::triggered, this, [this]() {
        SettingsDialog dialog(m_canvas, m_auth, this);
        dialog.exec();
        updatePegPanel();
    });
    
    // View menu
    QMenu* viewMenu = menuBar()->addMenu("&View");
    
    QAction* zoomInAction = viewMenu->addAction("Zoom &In");
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(zoomInAction, &QAction::triggered, m_canvas, &CanvasWidget::zoomIn);
    
    QAction* zoomOutAction = viewMenu->addAction("Zoom &Out");
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOutAction, &QAction::triggered, m_canvas, &CanvasWidget::zoomOut);
    
    QAction* fitAction = viewMenu->addAction("&Fit to Window");
    fitAction->setShortcut(QKeySequence("Ctrl+0"));
    connect(fitAction, &QAction::triggered, m_canvas, &CanvasWidget::fitToWindow);
    
    viewMenu->addSeparator();
    
    QAction* gridAction = viewMenu->addAction("Show &Grid");
    gridAction->setCheckable(true);
    gridAction->setChecked(m_canvas->showGrid());
    connect(gridAction, &QAction::toggled, m_canvas, &CanvasWidget::setShowGrid);
    
    viewMenu->addSeparator();
    
    QAction* layersAction = viewMenu->addAction("Show &Layers Panel");
    layersAction->setCheckable(true);
    layersAction->setChecked(true);
    connect(layersAction, &QAction::toggled, this, [this](bool checked) {
        if (m_layerDock) m_layerDock->setVisible(checked);
    });
    
    QAction* pegListAction = viewMenu->addAction("Show &Coordinates/Pegs Panel");
    pegListAction->setCheckable(true);
    pegListAction->setChecked(true);
    connect(pegListAction, &QAction::toggled, this, [this](bool checked) {
        if (m_pegDock) m_pegDock->setVisible(checked);
    });
    
    QAction* consoleAction = viewMenu->addAction("Show &Command Console");
    consoleAction->setShortcut(QKeySequence("Ctrl+Shift+C"));
    consoleAction->setCheckable(true);
    consoleAction->setChecked(true);
    connect(consoleAction, &QAction::toggled, this, [this](bool checked) {
        if (m_consoleDock) m_consoleDock->setVisible(checked);
    });
    
    viewMenu->addSeparator();
    
    QAction* maximizeAction = viewMenu->addAction("&Maximize Window");
    maximizeAction->setShortcut(QKeySequence("F11"));
    connect(maximizeAction, &QAction::triggered, this, [this]() {
        showMaximized();
    });
    
    QAction* restoreAction = viewMenu->addAction("&Restore Window");
    restoreAction->setShortcut(QKeySequence("Shift+Escape"));
    restoreAction->setToolTip("Exit fullscreen/maximized mode (Shift+Escape)");
    connect(restoreAction, &QAction::triggered, this, [this]() {
        showNormal();
    });
    
    QAction* fullscreenAction = viewMenu->addAction("F&ullscreen");
    fullscreenAction->setShortcut(QKeySequence("Shift+F11"));
    fullscreenAction->setCheckable(true);
    connect(fullscreenAction, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            showFullScreen();
        } else {
            showNormal();
        }
    });
    
    // Tools menu
    QMenu* toolsMenu = menuBar()->addMenu("&Tools");
    
    // === FUNCTIONALITIES submenu ===
    QMenu* funcMenu = toolsMenu->addMenu("&Functionalities");
    
    // Selection mode (ESC or V)
    QAction* selectAction = funcMenu->addAction("&Select Mode");
    selectAction->setShortcut(QKeySequence("V"));
    connect(selectAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startSelectMode();
    });
    
    funcMenu->addSeparator();
    
    m_snapAction = funcMenu->addAction("Enable &Snapping");
    m_snapAction->setShortcut(QKeySequence("S"));
    m_snapAction->setCheckable(true);
    m_snapAction->setChecked(false);
    connect(m_snapAction, &QAction::toggled, this, &MainWindow::toggleSnapping);
    
    QAction* panAction = funcMenu->addAction("&Pan Mode");
    panAction->setShortcut(QKeySequence("H"));
    panAction->setCheckable(true);
    panAction->setChecked(false);
    connect(panAction, &QAction::toggled, this, [this](bool checked) {
        if (m_canvas) {
            m_canvas->setPanMode(checked);
        }
    });
    
    funcMenu->addSeparator();
    
    QAction* measureAction = funcMenu->addAction("&Measure Distance");
    measureAction->setShortcut(QKeySequence("M"));
    connect(measureAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startMeasureMode();
    });
    
    funcMenu->addSeparator();
    
    QAction* fitAction2 = funcMenu->addAction("&Zoom to Fit");
    fitAction2->setShortcut(QKeySequence("F"));
    connect(fitAction2, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->fitToWindow();
    });
    
    QAction* zoomIn = funcMenu->addAction("Zoom &In");
    zoomIn->setShortcut(QKeySequence("+"));
    connect(zoomIn, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->zoomIn();
    });
    
    QAction* zoomOut = funcMenu->addAction("Zoom &Out");
    zoomOut->setShortcut(QKeySequence("-"));
    connect(zoomOut, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->zoomOut();
    });
    
    // === DRAW submenu ===
    QMenu* drawMenu = toolsMenu->addMenu("&Draw");
    
    QAction* lineAction = drawMenu->addAction("&Line");
    lineAction->setShortcut(QKeySequence("L"));
    connect(lineAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startDrawLineMode();
    });
    
    QAction* polylineAction = drawMenu->addAction("&Polyline");
    polylineAction->setShortcut(QKeySequence("P"));
    connect(polylineAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startDrawPolylineMode();
    });
    
    QAction* rectAction = drawMenu->addAction("&Rectangle");
    rectAction->setShortcut(QKeySequence("R"));
    connect(rectAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startDrawRectMode();
    });
    
    QAction* circleAction = drawMenu->addAction("&Circle");
    circleAction->setShortcut(QKeySequence("C"));
    connect(circleAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startDrawCircleMode();
    });
    
    QAction* arcAction = drawMenu->addAction("&Arc (3-point)");
    arcAction->setShortcut(QKeySequence("A"));
    connect(arcAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startDrawArcMode();
    });
    
    drawMenu->addSeparator();
    
    QAction* textAction = drawMenu->addAction("&Text...");
    connect(textAction, &QAction::triggered, this, [this]() {
        if (!m_canvas) return;
        bool ok;
        QString text = QInputDialog::getText(this, "Add Text", "Enter text:", 
                                              QLineEdit::Normal, "", &ok);
        if (ok && !text.isEmpty()) {
            m_canvas->startDrawTextMode(text, 2.5);
        }
    });
    
    // === CREATE PEGS submenu ===
    QMenu* pegMenu = toolsMenu->addMenu("Create &Pegs");
    
    QAction* addPegClickAction = pegMenu->addAction("Add &Peg (Click)...");
    addPegClickAction->setShortcut(QKeySequence("Shift+P"));
    addPegClickAction->setToolTip("Click on canvas to place pegs with Z coordinate");
    connect(addPegClickAction, &QAction::triggered, this, [this]() {
        if (!m_canvas) return;
        bool ok;
        QString input = QInputDialog::getText(this, "Add Peg", 
            "Enter peg name [, Z] (e.g. P1, 150.0):", QLineEdit::Normal, "P1", &ok);
        if (ok && !input.isEmpty()) {
            QStringList parts = input.split(",");
            QString name = parts[0].trimmed();
            double z = (parts.size() >= 2) ? parts[1].trimmed().toDouble() : 0.0;
            m_canvas->startAddPegMode(name, z);
        }
    });

    
    QAction* addPegCoordsAction = pegMenu->addAction("Add Peg by &Coordinates...");
    addPegCoordsAction->setToolTip("Enter X,Y,Z coordinates to create a peg");
    connect(addPegCoordsAction, &QAction::triggered, this, [this]() {
        if (!m_canvas) return;
        bool ok;
        QString input = QInputDialog::getText(this, "Add Peg by Coordinates", 
            "Enter: Name, X, Y [, Z] (e.g. P1, 1000.00, 2000.00, 150.00):", QLineEdit::Normal, "", &ok);
        if (ok && !input.isEmpty()) {
            QStringList parts = input.split(",");
            if (parts.size() >= 3) {
                QString name = parts[0].trimmed();
                double x = parts[1].trimmed().toDouble();
                double y = parts[2].trimmed().toDouble();
                double z = (parts.size() >= 4) ? parts[3].trimmed().toDouble() : 0.0;
                m_canvas->addPegAtPosition(QPointF(x, y), name, z);
                updatePegPanel();
            } else {
                statusBar()->showMessage("Invalid format. Use: Name, X, Y [, Z]", 3000);
            }
        }
    });

    
    pegMenu->addSeparator();
    
    QAction* clearPegsAction = pegMenu->addAction("&Clear All Pegs");
    connect(clearPegsAction, &QAction::triggered, this, [this]() {
        if (m_canvas) {
            m_canvas->clearPegs();
            statusBar()->showMessage("All pegs cleared", 3000);
        }
    });
    
    // === SURVEY TOOLS submenu ===
    QMenu* surveyToolsMenu = toolsMenu->addMenu("&Survey Tools");
    
    // -- Land Survey/Cadastral Tools --
    m_offsetAction = surveyToolsMenu->addAction("&Offset Polyline...");
    m_offsetAction->setShortcut(QKeySequence("O"));
    connect(m_offsetAction, &QAction::triggered, this, &MainWindow::offsetPolyline);
    
    m_partitionAction = surveyToolsMenu->addAction("Pro&ject to Offset");
    m_partitionAction->setToolTip("Project partition wall intersections to offset polylines");
    connect(m_partitionAction, &QAction::triggered, this, [this]() {
        if (!m_canvas) return;
        bool ok;
        QString prefix = QInputDialog::getText(this, "Partition Projection", 
            "Enter peg name prefix:", QLineEdit::Normal, "PP", &ok);
        if (ok && !prefix.isEmpty()) {
            m_canvas->projectPartitionToOffset(prefix);
        }
    });
    
    surveyToolsMenu->addSeparator();
    
    // -- Geometry Calculations --
    QMenu* geomCalcMenu = surveyToolsMenu->addMenu("&Geometry Calculations");
    
    QAction* calcAreaAction = geomCalcMenu->addAction("Calculate &Area");
    calcAreaAction->setToolTip("Calculate area of selected closed polygon");
    connect(calcAreaAction, &QAction::triggered, this, [this]() {
        if (!m_canvas || !m_canvas->hasSelection()) {
            statusBar()->showMessage("Select a closed polygon first", 3000);
            return;
        }
        const CanvasPolyline* poly = m_canvas->selectedPolyline();
        if (!poly || !poly->closed) {
            statusBar()->showMessage("Selected geometry must be a closed polygon", 3000);
            return;
        }
        double area = GeosBridge::calculateArea(poly->points);
        QMessageBox::information(this, "Area Calculation",
            QString("<b>Polygon Area</b><br><br>"
                   "Area: <b>%1 m²</b><br>"
                   "Area: <b>%2 ha</b>")
            .arg(area, 0, 'f', 3)
            .arg(area / 10000.0, 0, 'f', 4));
    });
    
    QAction* calcPerimAction = geomCalcMenu->addAction("Calculate &Perimeter/Length");
    calcPerimAction->setToolTip("Calculate perimeter of polygon or length of polyline");
    connect(calcPerimAction, &QAction::triggered, this, [this]() {
        if (!m_canvas || !m_canvas->hasSelection()) {
            statusBar()->showMessage("Select a polyline or polygon first", 3000);
            return;
        }
        const CanvasPolyline* poly = m_canvas->selectedPolyline();
        if (!poly) {
            statusBar()->showMessage("No polyline selected", 3000);
            return;
        }
        double length = GeosBridge::calculatePerimeter(poly->points, poly->closed);
        QString type = poly->closed ? "Perimeter" : "Length";
        QMessageBox::information(this, type + " Calculation",
            QString("<b>%1</b><br><br>"
                   "%1: <b>%2 m</b>")
            .arg(type)
            .arg(length, 0, 'f', 3));
    });
    
    geomCalcMenu->addSeparator();
    
    QAction* checkGeomAction = geomCalcMenu->addAction("Check &Geometry");
    checkGeomAction->setToolTip("Analyze geometry validity and repair errors");
    connect(checkGeomAction, &QAction::triggered, this, [this]() {
        if (!m_canvas) return;
        
        // Determine scope
        CheckGeometryDialog::CheckMode mode = CheckGeometryDialog::CheckAll;
        
        if (m_canvas->hasSelection()) {
            QMessageBox::StandardButton reply = QMessageBox::question(this, "Check Geometry", 
                "You have objects selected.\n\n"
                "Do you want to check ONLY the selected objects?\n"
                "(Select 'No' to check the entire drawing)",
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
            
            if (reply == QMessageBox::Cancel) return;
            if (reply == QMessageBox::Yes) mode = CheckGeometryDialog::CheckSelected;
        }
        
        // Open the detailed check dialog
        CheckGeometryDialog* dlg = new CheckGeometryDialog(m_canvas, mode, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    
    QAction* checkOverlapAction = geomCalcMenu->addAction("Check &Overlap (2 polygons)");
    checkOverlapAction->setToolTip("Check if two selected polygons overlap");
    connect(checkOverlapAction, &QAction::triggered, this, [this]() {
        if (!m_canvas) {
            statusBar()->showMessage("No canvas", 3000);
            return;
        }
        auto selected = m_canvas->getSelectedIndices();
        if (selected.size() < 2) {
            statusBar()->showMessage("Select exactly two polygons (Ctrl+Click to multi-select)", 3000);
            return;
        }
        const CanvasPolyline& poly1 = m_canvas->polylines()[selected[0]];
        const CanvasPolyline& poly2 = m_canvas->polylines()[selected[1]];
        
        bool overlap = GeosBridge::polygonsOverlap(poly1.points, poly2.points);
        if (overlap) {
            QMessageBox::warning(this, "Overlap Check", 
                "<span style='color:orange;font-size:18px;'>⚠</span> Polygons <b>OVERLAP</b>");
        } else {
            QMessageBox::information(this, "Overlap Check", 
                "<span style='color:green;font-size:18px;'>✓</span> Polygons are <b>SEPARATE</b> (no overlap)");
        }
    });
    
    geomCalcMenu->addSeparator();
    
    QAction* bufferAction = geomCalcMenu->addAction("Create &Buffer Zone...");
    bufferAction->setToolTip("Create buffer/setback zone around polyline or polygon");
    connect(bufferAction, &QAction::triggered, this, [this]() {
        if (!m_canvas || !m_canvas->hasSelection()) {
            statusBar()->showMessage("Select a polyline or polygon first", 3000);
            return;
        }
        const CanvasPolyline* poly = m_canvas->selectedPolyline();
        if (!poly) {
            statusBar()->showMessage("No polyline selected", 3000);
            return;
        }
        bool ok;
        double distance = QInputDialog::getDouble(this, "Buffer Zone", 
            "Enter buffer distance (meters):", 3.0, 0.1, 1000.0, 2, &ok);
        if (!ok) return;
        
        QVector<QPointF> bufferPoints = GeosBridge::createBuffer(poly->points, distance, poly->closed);
        if (bufferPoints.isEmpty()) {
            QMessageBox::warning(this, "Buffer Error", 
                QString("Buffer creation failed: %1").arg(GeosBridge::lastError()));
            return;
        }
        
        // Add buffer as new polyline
        CanvasPolyline bufferPoly;
        bufferPoly.points = bufferPoints;
        bufferPoly.layer = poly->layer + "_buffer";
        bufferPoly.color = QColor(255, 165, 0);  // Orange
        bufferPoly.closed = true;
        m_canvas->addPolyline(bufferPoly);
        
        statusBar()->showMessage(QString("Buffer zone created (%1m)").arg(distance), 3000);
    });
    
    surveyToolsMenu->addSeparator();
    
    // -- Calculation Tools --
    m_polarAction = surveyToolsMenu->addAction("&Polar Calculation...");
    m_polarAction->setToolTip("Calculate point from bearing and distance");
    connect(m_polarAction, &QAction::triggered, this, [this]() {
        PolarDialog dialog(m_canvas, this);
        dialog.exec();
    });
    
    m_joinCalcAction = surveyToolsMenu->addAction("&Join Calculation...");
    m_joinCalcAction->setToolTip("Calculate bearing and distance between two points");
    connect(m_joinCalcAction, &QAction::triggered, this, [this]() {
        JoinDialog dialog(m_canvas, this);
        dialog.exec();
    });
    
    m_intersectionAction = surveyToolsMenu->addAction("&Intersection...");
    m_intersectionAction->setToolTip("Calculate intersection point");
    connect(m_intersectionAction, &QAction::triggered, this, [this]() {
        IntersectionDialog dialog(m_canvas, this);
        dialog.exec();
    });
    
    surveyToolsMenu->addSeparator();
    
    // -- Traverse & Levelling --
    QAction* traverseAction = surveyToolsMenu->addAction("&Traverse Calculation...");
    traverseAction->setShortcut(QKeySequence("T"));
    traverseAction->setToolTip("Full traverse computation with Bowditch/Transit adjustment");
    connect(traverseAction, &QAction::triggered, this, [this]() {
        TraverseDialog dialog(m_canvas, this);
        dialog.exec();
    });
    
    m_levellingAction = surveyToolsMenu->addAction("&Levelling Tool...");
    m_levellingAction->setToolTip("Rise & Fall or HPC levelling calculations");
    connect(m_levellingAction, &QAction::triggered, this, [this]() {
        LevellingDialog dialog(m_canvas, this);
        dialog.exec();
    });
    
    QAction* volumeAction = surveyToolsMenu->addAction("&Volume Calculation...");
    volumeAction->setToolTip("Calculate cut/fill volumes between survey surface and design level");
    connect(volumeAction, &QAction::triggered, this, [this]() {
        VolumeDialog dialog(m_canvas, this);
        dialog.exec();
    });
    
    QAction* contourAction = surveyToolsMenu->addAction("&Contour Generator...");
    contourAction->setToolTip("Generate contour lines from survey points");
    connect(contourAction, &QAction::triggered, this, [this]() {
        ContourDialog dialog(m_canvas, this);
        dialog.exec();
    });
    
    surveyToolsMenu->addSeparator();


    
    // -- Network Adjustment --
    m_gamaAction = surveyToolsMenu->addAction("Adjust &Network (GAMA)...");
    m_gamaAction->setToolTip("Least squares network adjustment using GNU GAMA");
    connect(m_gamaAction, &QAction::triggered, this, &MainWindow::runGamaAdjustment);
    
    m_resectionAction = surveyToolsMenu->addAction("&Resection (Free Station)...");
    m_resectionAction->setToolTip("Calculate station position from known points");
    connect(m_resectionAction, &QAction::triggered, this, [this]() {
        ResectionDialog dialog(m_canvas, this);
        dialog.exec();
    });
    
    surveyToolsMenu->addSeparator();
    
    // -- CRS Transformations --
    QMenu* crsMenu = surveyToolsMenu->addMenu("CRS &Transformations");
    
    QAction* gpsToLocalAction = crsMenu->addAction("Convert &GPS → Local Grid...");
    gpsToLocalAction->setToolTip("Convert WGS84 GPS coordinates to local survey grid");
    connect(gpsToLocalAction, &QAction::triggered, this, [this]() {
        if (!m_canvas) return;
        
        // Select target CRS
        QStringList crsList;
        crsList << "EPSG:32733 - UTM Zone 33S (South Africa East)"
                << "EPSG:32734 - UTM Zone 34S (South Africa Central)"
                << "EPSG:32735 - UTM Zone 35S (South Africa West)"
                << "EPSG:22285 - SA Lo29 (Cape Town)"
                << "EPSG:22287 - SA Lo31 (Port Elizabeth)"
                << "EPSG:22289 - SA Lo33 (Durban)"
                << "EPSG:22291 - SA Lo35 (East London)"
                << "EPSG:4326 - WGS84 (GPS coordinates)";
        
        bool ok;
        QString selected = QInputDialog::getItem(this, "Select Target CRS",
            "Convert GPS (WGS84) coordinates to:", crsList, 0, false, &ok);
        if (!ok) return;
        
        QString targetEpsg = selected.split(" - ")[0];
        
        // Get GPS coordinates
        QString coordInput = QInputDialog::getText(this, "Enter GPS Coordinates",
            "Enter Latitude, Longitude (e.g. -26.2041, 28.0473):", QLineEdit::Normal, "", &ok);
        if (!ok || coordInput.isEmpty()) return;
        
        QStringList parts = coordInput.split(",");
        if (parts.size() < 2) {
            QMessageBox::warning(this, "Error", "Invalid format. Use: Latitude, Longitude");
            return;
        }
        
        double lat = parts[0].trimmed().toDouble();
        double lon = parts[1].trimmed().toDouble();
        
        // Transform using PROJ
        OGRSpatialReference srcSRS, dstSRS;
        srcSRS.importFromEPSG(4326);  // WGS84
        
        int epsgCode = targetEpsg.mid(5).toInt();
        dstSRS.importFromEPSG(epsgCode);
        
        OGRCoordinateTransformation* transform = 
            OGRCreateCoordinateTransformation(&srcSRS, &dstSRS);
        
        if (!transform) {
            QMessageBox::warning(this, "Error", "Failed to create coordinate transformation");
            return;
        }
        
        double x = lon;  // X = Longitude in WGS84
        double y = lat;  // Y = Latitude in WGS84
        
        if (transform->Transform(1, &x, &y)) {
            // Add as peg
            bool addPeg = QMessageBox::question(this, "Conversion Result",
                QString("<b>GPS → %1</b><br><br>"
                       "Input (Lat/Lon): %2, %3<br><br>"
                       "<b>Result:</b><br>"
                       "X (Easting): %4<br>"
                       "Y (Northing): %5<br><br>"
                       "Add as peg?")
                .arg(targetEpsg)
                .arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6)
                .arg(x, 0, 'f', 3).arg(y, 0, 'f', 3),
                QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
            
            if (addPeg) {
                bool nameOk;
                QString pegName = QInputDialog::getText(this, "Peg Name",
                    "Enter peg name:", QLineEdit::Normal, "GPS1", &nameOk);
                if (nameOk && !pegName.isEmpty()) {
                    m_canvas->addPegAtPosition(QPointF(x, y), pegName);
                }
            }
        } else {
            QMessageBox::warning(this, "Error", "Coordinate transformation failed");
        }
        
        OGRCoordinateTransformation::DestroyCT(transform);
    });
    
    QAction* localToGpsAction = crsMenu->addAction("Convert &Local → GPS...");
    localToGpsAction->setToolTip("Convert local grid coordinates to WGS84 GPS");
    connect(localToGpsAction, &QAction::triggered, this, [this]() {
        if (!m_canvas) return;
        
        // Select source CRS
        QStringList crsList;
        crsList << "EPSG:32733 - UTM Zone 33S"
                << "EPSG:32734 - UTM Zone 34S"
                << "EPSG:32735 - UTM Zone 35S"
                << "EPSG:22285 - SA Lo29"
                << "EPSG:22287 - SA Lo31"
                << "EPSG:22289 - SA Lo33"
                << "EPSG:22291 - SA Lo35";
        
        bool ok;
        QString selected = QInputDialog::getItem(this, "Select Source CRS",
            "Convert from (current project CRS):", crsList, 0, false, &ok);
        if (!ok) return;
        
        QString sourceEpsg = selected.split(" - ")[0];
        
        // Get local coordinates
        QString coordInput = QInputDialog::getText(this, "Enter Local Coordinates",
            "Enter X (Easting), Y (Northing):", QLineEdit::Normal, "", &ok);
        if (!ok || coordInput.isEmpty()) return;
        
        QStringList parts = coordInput.split(",");
        if (parts.size() < 2) {
            QMessageBox::warning(this, "Error", "Invalid format. Use: X, Y");
            return;
        }
        
        double x = parts[0].trimmed().toDouble();
        double y = parts[1].trimmed().toDouble();
        
        // Transform using PROJ
        OGRSpatialReference srcSRS, dstSRS;
        int epsgCode = sourceEpsg.mid(5).toInt();
        srcSRS.importFromEPSG(epsgCode);
        dstSRS.importFromEPSG(4326);  // WGS84
        
        OGRCoordinateTransformation* transform = 
            OGRCreateCoordinateTransformation(&srcSRS, &dstSRS);
        
        if (!transform) {
            QMessageBox::warning(this, "Error", "Failed to create coordinate transformation");
            return;
        }
        
        double lon = x;
        double lat = y;
        
        if (transform->Transform(1, &lon, &lat)) {
            QMessageBox::information(this, "Conversion Result",
                QString("<b>%1 → GPS (WGS84)</b><br><br>"
                       "Input (X/Y): %2, %3<br><br>"
                       "<b>Result:</b><br>"
                       "Latitude: %4<br>"
                       "Longitude: %5<br><br>"
                       "<small>Copy for Google Maps: %4, %5</small>")
                .arg(sourceEpsg)
                .arg(x, 0, 'f', 3).arg(y, 0, 'f', 3)
                .arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6));
        } else {
            QMessageBox::warning(this, "Error", "Coordinate transformation failed");
        }
        
        OGRCoordinateTransformation::DestroyCT(transform);
    });
    
    crsMenu->addSeparator();
    
    QAction* transformPegsAction = crsMenu->addAction("Transform All &Pegs...");
    transformPegsAction->setToolTip("Transform all pegs from one CRS to another");
    connect(transformPegsAction, &QAction::triggered, this, [this]() {
        if (!m_canvas || m_canvas->pegs().isEmpty()) {
            QMessageBox::information(this, "Info", "No pegs to transform");
            return;
        }
        
        QMessageBox::information(this, "Transform Pegs",
            QString("This will transform all %1 pegs from the current project CRS to a new CRS.\n\n"
                   "Current CRS: %2\n\n"
                   "This feature will be available in the next update.")
            .arg(m_canvas->pegs().size())
            .arg(m_canvas->crs()));
    });
    
    // === MODIFY submenu ===
    QMenu* polyEditMenu = toolsMenu->addMenu("&Modify");
    
    QAction* copyAction = polyEditMenu->addAction("&Copy");
    copyAction->setShortcut(QKeySequence("Ctrl+C"));
    connect(copyAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->copySelectedPolyline();
    });
    
    QAction* moveAction = polyEditMenu->addAction("&Move");
    moveAction->setShortcut(QKeySequence("Ctrl+M"));
    connect(moveAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startMoveMode();
    });
    
    QAction* mirrorAction = polyEditMenu->addAction("M&irror");
    mirrorAction->setShortcut(QKeySequence("Ctrl+I"));
    connect(mirrorAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startMirrorMode();
    });
    
    polyEditMenu->addSeparator();
    
    QAction* explodeAction = polyEditMenu->addAction("&Explode");
    explodeAction->setShortcut(QKeySequence("Ctrl+E"));
    connect(explodeAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->explodeSelectedPolyline();
    });
    
    QAction* splitAction = polyEditMenu->addAction("&Break");
    splitAction->setShortcut(QKeySequence("Ctrl+B"));
    connect(splitAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startSplitMode();
    });
    
    QAction* mergeAction = polyEditMenu->addAction("Merge &Polylines");
    mergeAction->setShortcut(QKeySequence("Ctrl+J"));
    connect(mergeAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->joinPolylines();
    });
    
    polyEditMenu->addSeparator();
    
    QAction* closeAction = polyEditMenu->addAction("C&lose/Open");
    closeAction->setShortcut(QKeySequence("Ctrl+Shift+C"));
    connect(closeAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->closeSelectedPolyline();
    });
    
    QAction* reverseAction = polyEditMenu->addAction("&Reverse Direction");
    reverseAction->setShortcut(QKeySequence("Ctrl+R"));
    connect(reverseAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->reverseSelectedPolyline();
    });
    
    polyEditMenu->addSeparator();
    
    QAction* deletePolyAction = polyEditMenu->addAction("&Erase");
    deletePolyAction->setShortcut(QKeySequence::Delete);
    connect(deletePolyAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->deleteSelectedPolyline();
    });
    
    polyEditMenu->addSeparator();
    
    QAction* scaleAction = polyEditMenu->addAction("&Scale...");
    connect(scaleAction, &QAction::triggered, this, [this]() {
        if (!m_canvas || !m_canvas->hasSelection()) {
            statusBar()->showMessage("Select object(s) first", 3000);
            return;
        }
        bool ok;
        double factor = QInputDialog::getDouble(this, "Scale", "Scale factor:", 
                                                 1.0, 0.001, 1000.0, 3, &ok);
        if (ok) {
            m_canvas->startScaleMode(factor);
        }
    });
    
    QAction* rotateAction = polyEditMenu->addAction("Ro&tate...");
    connect(rotateAction, &QAction::triggered, this, [this]() {
        if (!m_canvas || !m_canvas->hasSelection()) {
            statusBar()->showMessage("Select object(s) first", 3000);
            return;
        }
        bool ok;
        double angle = QInputDialog::getDouble(this, "Rotate", "Rotation angle (degrees):", 
                                                0.0, -360.0, 360.0, 2, &ok);
        if (ok) {
            m_canvas->startRotateMode(angle);
        }
    });
    
    // === STATION SETUP submenu ===
    QMenu* stationMenu = toolsMenu->addMenu("&Station Setup");
    
    QAction* setStationAction = stationMenu->addAction("Set &Station Point (Click)");
    setStationAction->setShortcut(QKeySequence("1"));
    setStationAction->setToolTip("Click anywhere to set instrument position (snaps to peg if near)");
    connect(setStationAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startSetStationMode();
    });
    
    QAction* setStationCoordsAction = stationMenu->addAction("Enter Station &Coordinates...");
    setStationCoordsAction->setToolTip("Enter station coordinates manually");
    connect(setStationCoordsAction, &QAction::triggered, this, [this]() {
        if (!m_canvas) return;
        bool ok;
        QString coords = QInputDialog::getText(this, "Station Coordinates", 
            "Enter station X,Y coordinates (e.g. 1000.000,2000.000):", 
            QLineEdit::Normal, "", &ok);
        if (ok && !coords.isEmpty()) {
            QStringList parts = coords.split(",");
            if (parts.size() >= 2) {
                double x = parts[0].trimmed().toDouble();
                double y = parts[1].trimmed().toDouble();
                m_canvas->setStationPoint(QPointF(x, y), "STN");
            } else {
                statusBar()->showMessage("Invalid format. Use: X,Y", 3000);
            }
        }
    });
    
    stationMenu->addSeparator();
    
    QAction* setBacksightAction = stationMenu->addAction("Set &Backsight Point (Click)");
    setBacksightAction->setShortcut(QKeySequence("2"));
    setBacksightAction->setToolTip("Click anywhere to set backsight (snaps to peg if near)");
    connect(setBacksightAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startSetBacksightMode();
    });
    
    QAction* setBacksightCoordsAction = stationMenu->addAction("Enter Backsight C&oordinates...");
    setBacksightCoordsAction->setToolTip("Enter backsight coordinates manually");
    connect(setBacksightCoordsAction, &QAction::triggered, this, [this]() {
        if (!m_canvas) return;
        bool ok;
        QString coords = QInputDialog::getText(this, "Backsight Coordinates", 
            "Enter backsight X,Y coordinates (e.g. 1000.000,2500.000):", 
            QLineEdit::Normal, "", &ok);
        if (ok && !coords.isEmpty()) {
            QStringList parts = coords.split(",");
            if (parts.size() >= 2) {
                double x = parts[0].trimmed().toDouble();
                double y = parts[1].trimmed().toDouble();
                m_canvas->setBacksightPoint(QPointF(x, y), "BS");
            } else {
                statusBar()->showMessage("Invalid format. Use: X,Y", 3000);
            }
        }
    });
    
    stationMenu->addSeparator();
    
    QAction* checkPointAction = stationMenu->addAction("&Verify Check Point");
    checkPointAction->setShortcut(QKeySequence("3"));
    checkPointAction->setToolTip("Click on a point to verify station setup error");
    connect(checkPointAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startSetCheckPointMode();
    });
    
    stationMenu->addSeparator();
    
    QAction* clearStationAction = stationMenu->addAction("&Clear Station");
    clearStationAction->setToolTip("Clear station and backsight setup");
    connect(clearStationAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->clearStation();
    });
    
    stationMenu->addSeparator();
    
    m_stakeoutAction = stationMenu->addAction("Sta&keout from Station");
    m_stakeoutAction->setShortcut(QKeySequence("K"));
    m_stakeoutAction->setToolTip("Show bearing and distance from station to cursor/pegs");
    connect(m_stakeoutAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startStakeoutMode();
    });
    
    // Help menu
    QMenu* helpMenu = menuBar()->addMenu("&Help");
    
    QAction* shortcutsAction = helpMenu->addAction("&Keyboard Shortcuts...");
    shortcutsAction->setShortcut(QKeySequence("?"));
    connect(shortcutsAction, &QAction::triggered, this, &MainWindow::showKeyboardShortcuts);
    
    helpMenu->addSeparator();
    
    
    QAction* aboutAction = helpMenu->addAction("&About SiteSurveyor");
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "About SiteSurveyor",
            "<h2>SiteSurveyor</h2>"
            "<p>Professional Land Surveying Software</p>"
            "<p>Official Site: <a href='https://sitesurveyor.dev'>sitesurveyor.dev</a></p>"
            "<p>Developed by <b>Eineva Incorporated</b></p>");
    });
}

void MainWindow::setupStatusBar()
{
    QSettings coordSettings;
    bool swapXY = coordSettings.value("coordinates/swapXY", false).toBool();
    m_coordLabel = new QLabel(swapXY ? "Y: 0.000  X: 0.000" : "X: 0.000  Y: 0.000");
    m_coordLabel->setMinimumWidth(200);
    statusBar()->addWidget(m_coordLabel);
    
    m_crsLabel = new QLabel("");
    m_crsLabel->setMinimumWidth(100);
    statusBar()->addWidget(m_crsLabel);
    
    m_zoomLabel = new QLabel("Zoom: 100%");
    m_zoomLabel->setMinimumWidth(100);
    statusBar()->addPermanentWidget(m_zoomLabel);
}

void MainWindow::setupLayerPanel()
{
    m_layerDock = new QDockWidget("Layers", this);
    m_layerDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    
    QWidget* dockContents = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(dockContents);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    
    // Add layer button
    QPushButton* addLayerBtn = new QPushButton("+ Add Layer");
    connect(addLayerBtn, &QPushButton::clicked, this, [this]() {
        bool ok;
        QString name = QInputDialog::getText(this, "Add Layer",
            "Layer name:", QLineEdit::Normal, "", &ok);
        if (ok && !name.isEmpty()) {
            QColor color = QColorDialog::getColor(Qt::white, this, "Layer Color");
            if (color.isValid()) {
                m_canvas->addLayer(name, color);
                updateLayerPanel();
            }
        }
    });
    layout->addWidget(addLayerBtn);
    
    // Layer list
    m_layerList = new QListWidget();
    m_layerList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_layerList->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_layerList);
    
    m_layerDock->setWidget(dockContents);
    addDockWidget(Qt::RightDockWidgetArea, m_layerDock);
    
    connect(m_layerList, &QListWidget::itemChanged, this, &MainWindow::onLayerItemChanged);
    connect(m_layerList, &QListWidget::customContextMenuRequested, this, &MainWindow::onLayerContextMenu);
    
    // Handle Active Layer Change
    connect(m_layerList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current, QListWidgetItem* previous) {
        Q_UNUSED(previous);
        if (current) {
            QString layerName = current->data(Qt::UserRole).toString();
            m_canvas->setActiveLayer(layerName);
            // Updating the panel here might be recursive or unnecessary if we just highlight local item.
            // But we should rely on Canvas signal to update panel for consistency.
            // Wait, setActiveLayer emits layersChanged(). updateLayerPanel() clears list.
            // This would cause infinite loop or loss of selection focus!
            // Solution: setActiveLayer should ONLY emit if changed. 
            // And updateLayerPanel should restore selection? 
            // Or better: updateLayerPanel is called on layersChanged.
            // If we select item -> active layer -> layers changed -> update panel -> list clear -> selection lost.
            // This is BAD.
            // We should block signals or check if layer changed.
            // Or better: updateLayerPanel should NOT clear list if only active status changed?
            // Actually, if we just want to Highlight, we can do it locally?
            // But active layer is a global state.
            // If I implemented setActiveLayer to emit signal, it will re-trigger updateLayerPanel.
            // I should modify updateLayerPanel to preserve selection or not clear if unnecessary.
        }
    });
}

void MainWindow::updateLayerPanel()
{
    if (!m_layerList || !m_canvas) return;
    
    m_layerList->blockSignals(true);
    m_layerList->clear();
    
    for (const auto& layer : m_canvas->layers()) {
        // Display name with lock indicator
        QString displayName = layer.locked ? 
            QString("🔒 %1").arg(layer.name) : layer.name;
        
        QListWidgetItem* item = new QListWidgetItem(displayName);
        item->setData(Qt::UserRole, layer.name);  // Store actual name
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(layer.visible ? Qt::Checked : Qt::Unchecked);
        
        // Color indicator
        QPixmap pm(16, 16);
        pm.fill(layer.color);
        item->setIcon(QIcon(pm));
        
        if (layer.locked) {
            item->setForeground(Qt::gray);
        }
        
        if (layer.name == m_canvas->activeLayer()) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            // item->setBackground(QColor(230, 240, 255)); // Optional highlight
            m_layerList->setCurrentItem(item); // Restore selection
        }
        
        m_layerList->addItem(item);
    }
    
    m_layerList->blockSignals(false);
}

void MainWindow::onLayerItemChanged()
{
    if (!m_layerList || !m_canvas) return;
    
    for (int i = 0; i < m_layerList->count(); ++i) {
        QListWidgetItem* item = m_layerList->item(i);
        QString layerName = item->data(Qt::UserRole).toString();
        bool visible = (item->checkState() == Qt::Checked);
        m_canvas->setLayerVisible(layerName, visible);
    }
}

void MainWindow::onLayerContextMenu(const QPoint& pos)
{
    if (!m_layerList || !m_canvas) return;
    
    QListWidgetItem* item = m_layerList->itemAt(pos);
    if (!item) return;
    
    QString layerName = item->data(Qt::UserRole).toString();
    
    QMenu menu(this);
    
    // Lock/Unlock action
    bool isLocked = m_canvas->isLayerLocked(layerName);
    QAction* lockAction = menu.addAction(isLocked ? "🔓 Unlock Layer" : "🔒 Lock Layer");
    connect(lockAction, &QAction::triggered, this, [this, layerName, isLocked]() {
        m_canvas->setLayerLocked(layerName, !isLocked);
        updateLayerPanel();
    });
    
    menu.addSeparator();
    
    // Change color
    QAction* colorAction = menu.addAction("🎨 Change Color...");
    connect(colorAction, &QAction::triggered, this, [this, layerName]() {
        CanvasLayer* layer = m_canvas->getLayer(layerName);
        if (layer) {
            QColor newColor = QColorDialog::getColor(layer->color, this, "Layer Color");
            if (newColor.isValid()) {
                m_canvas->setLayerColor(layerName, newColor);
                updateLayerPanel();
            }
        }
    });
    
    // Rename
    QAction* renameAction = menu.addAction("✏️ Rename...");
    connect(renameAction, &QAction::triggered, this, [this, layerName]() {
        bool ok;
        QString newName = QInputDialog::getText(this, "Rename Layer",
            "New layer name:", QLineEdit::Normal, layerName, &ok);
        if (ok && !newName.isEmpty() && newName != layerName) {
            m_canvas->renameLayer(layerName, newName);
            updateLayerPanel();
        }
    });
    
    menu.addSeparator();
    
    // Delete
    QAction* deleteAction = menu.addAction("🗑️ Delete Layer");
    connect(deleteAction, &QAction::triggered, this, [this, layerName]() {
        if (QMessageBox::question(this, "Delete Layer",
                QString("Delete layer '%1'?").arg(layerName)) == QMessageBox::Yes) {
            m_canvas->removeLayer(layerName);
            updateLayerPanel();
        }
    });
    
    menu.exec(m_layerList->mapToGlobal(pos));
}

void MainWindow::setupPropertiesPanel()
{
    m_propertiesDock = new QDockWidget("Properties", this);
    m_propertiesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    
    QWidget* dockContents = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(dockContents);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    
    // Properties tree
    m_propertiesTree = new QTreeWidget();
    m_propertiesTree->setHeaderLabels({"Property", "Value"});
    m_propertiesTree->setColumnCount(2);
    m_propertiesTree->setAlternatingRowColors(true);
    m_propertiesTree->setRootIsDecorated(false);
    layout->addWidget(m_propertiesTree);
    
    // Populate with some basic info
    QTreeWidgetItem* projItem = new QTreeWidgetItem({"Project", ""});
    projItem->setFlags(projItem->flags() & ~Qt::ItemIsEditable);
    QTreeWidgetItem* nameItem = new QTreeWidgetItem({"Name", "Untitled"});
    QTreeWidgetItem* crsItem = new QTreeWidgetItem({"CRS", "Local"});
    projItem->addChild(nameItem);
    projItem->addChild(crsItem);
    m_propertiesTree->addTopLevelItem(projItem);
    projItem->setExpanded(true);
    
    QTreeWidgetItem* selItem = new QTreeWidgetItem({"Selection", ""});
    QTreeWidgetItem* typeItem = new QTreeWidgetItem({"Type", "None"});
    QTreeWidgetItem* pointsItem = new QTreeWidgetItem({"Points", "0"});
    QTreeWidgetItem* layerItem = new QTreeWidgetItem({"Layer", "-"});
    selItem->addChild(typeItem);
    selItem->addChild(pointsItem);
    selItem->addChild(layerItem);
    m_propertiesTree->addTopLevelItem(selItem);
    selItem->setExpanded(true);
    
    m_propertiesDock->setWidget(dockContents);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);
    
    // Tab with layers
    tabifyDockWidget(m_layerDock, m_propertiesDock);
    
    m_layerDock->raise();
    
    // Add Command Console at bottom
    m_consoleDock = new CommandConsole(m_canvas, this, this);
    addDockWidget(Qt::BottomDockWidgetArea, m_consoleDock);
    
    // Update properties when selection changes
    connect(m_canvas, &CanvasWidget::selectionChanged, this, [this](int) {
        updatePropertiesPanel();
    });
    
    // Setup peg panel
    setupPegPanel();
}

void MainWindow::setupPegPanel()
{
    m_pegDock = new QDockWidget("Coordinates / Pegs", this);
    m_pegDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    
    QWidget* dockContents = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(dockContents);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    
    // Info label
    QLabel* infoLabel = new QLabel("Double-click to zoom to peg");
    infoLabel->setStyleSheet("color: gray; font-size: 10px;");
    layout->addWidget(infoLabel);
    
    // Peg table
    m_pegTable = new QTableWidget();
    m_pegTable->setColumnCount(4);
    m_pegTable->setHorizontalHeaderLabels({"Name", "X", "Y", "Z"});
    m_pegTable->horizontalHeader()->setStretchLastSection(true);
    m_pegTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pegTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pegTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_pegTable->setSortingEnabled(true);
    layout->addWidget(m_pegTable);
    
    // Connect cell editing to update peg
    connect(m_pegTable, &QTableWidget::cellChanged, this, [this](int row, int column) {
        if (!m_canvas || row < 0 || row >= m_canvas->pegs().size()) return;
        
        // Get current values
        QString name = m_pegTable->item(row, 0) ? m_pegTable->item(row, 0)->text() : "";
        double x = m_pegTable->item(row, 1) ? m_pegTable->item(row, 1)->text().toDouble() : 0.0;
        double y = m_pegTable->item(row, 2) ? m_pegTable->item(row, 2)->text().toDouble() : 0.0;
        double z = m_pegTable->item(row, 3) ? m_pegTable->item(row, 3)->text().toDouble() : 0.0;
        
        // Check if SwapXY is enabled (columns may be swapped)
        QSettings settings;
        bool swapXY = settings.value("coordinates/swapXY", false).toBool();
        if (swapXY) {
            std::swap(x, y);
        }
        
        // Update peg
        m_canvas->updatePeg(row, name, x, y, z);
    });

    
    // Buttons layout
    QHBoxLayout* btnLayout = new QHBoxLayout();
    
    // Refresh button
    QPushButton* refreshBtn = new QPushButton("↻ Refresh");
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::updatePegPanel);
    btnLayout->addWidget(refreshBtn);
    
    // Select on canvas button
    QPushButton* selectBtn = new QPushButton("Select");
    selectBtn->setToolTip("Select peg on canvas");
    connect(selectBtn, &QPushButton::clicked, this, [this]() {
        if (!m_canvas || !m_pegTable) return;
        int row = m_pegTable->currentRow();
        if (row >= 0 && row < m_canvas->pegs().size()) {
            m_canvas->selectPeg(row);
            m_canvas->zoomToPoint(m_canvas->pegs()[row].position);
        } else {
            statusBar()->showMessage("Select a peg from the table first", 2000);
        }
    });
    btnLayout->addWidget(selectBtn);
    
    // Delete button
    QPushButton* deleteBtn = new QPushButton("Delete");
    deleteBtn->setToolTip("Delete selected peg");
    deleteBtn->setStyleSheet("QPushButton { color: #c0392b; }");
    connect(deleteBtn, &QPushButton::clicked, this, [this]() {
        if (!m_canvas || !m_pegTable) return;
        int row = m_pegTable->currentRow();
        if (row >= 0 && row < m_canvas->pegs().size()) {
            QString name = m_canvas->pegs()[row].name;
            m_canvas->selectPeg(row);
            m_canvas->deleteSelectedPeg();
            updatePegPanel();
            statusBar()->showMessage(QString("Deleted peg '%1'").arg(name), 2000);
        } else {
            statusBar()->showMessage("Select a peg from the table first", 2000);
        }
    });
    btnLayout->addWidget(deleteBtn);
    
    layout->addLayout(btnLayout);

    
    m_pegDock->setWidget(dockContents);
    addDockWidget(Qt::LeftDockWidgetArea, m_pegDock);
    
    // Double-click to zoom to peg
    connect(m_pegTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (!m_canvas || row < 0) return;
        const auto& pegs = m_canvas->pegs();
        if (row < pegs.size()) {
            const auto& peg = pegs[row];
            m_canvas->zoomToPoint(peg.position);
            statusBar()->showMessage(QString("Zoomed to peg '%1'").arg(peg.name), 2000);
        }
    });
    
    // Initial populate
    updatePegPanel();
}

void MainWindow::updatePegPanel()
{
    if (!m_pegTable || !m_canvas) return;
    
    m_pegTable->setSortingEnabled(false);
    m_pegTable->setRowCount(0);
    
    const auto& pegs = m_canvas->pegs();
    m_pegTable->setRowCount(pegs.size());
    
    QSettings settings;
    bool swapXY = settings.value("coordinates/swapXY", false).toBool();
    
    if (swapXY) {
        m_pegTable->setHorizontalHeaderLabels({"Name", "Y", "X", "Z"});
    } else {
        m_pegTable->setHorizontalHeaderLabels({"Name", "X", "Y", "Z"});
    }
    
    for (int i = 0; i < pegs.size(); ++i) {
        const auto& peg = pegs[i];
        
        QTableWidgetItem* nameItem = new QTableWidgetItem(peg.name);
        nameItem->setForeground(peg.color);
        
        QString xStr, yStr;
        if (swapXY) {
            xStr = QString::number(peg.position.y(), 'f', 3);
            yStr = QString::number(peg.position.x(), 'f', 3);
        } else {
            xStr = QString::number(peg.position.x(), 'f', 3);
            yStr = QString::number(peg.position.y(), 'f', 3);
        }
        QString zStr = QString::number(peg.z, 'f', 3);
        
        m_pegTable->setItem(i, 0, nameItem);
        m_pegTable->setItem(i, 1, new QTableWidgetItem(xStr));
        m_pegTable->setItem(i, 2, new QTableWidgetItem(yStr));
        m_pegTable->setItem(i, 3, new QTableWidgetItem(zStr));
    }
    
    m_pegTable->setSortingEnabled(true);
    m_pegTable->resizeColumnsToContents();
}

void MainWindow::updatePropertiesPanel()
{
    if (!m_propertiesTree || !m_canvas) return;
    
    // Find selection item
    QTreeWidgetItem* selItem = nullptr;
    for (int i = 0; i < m_propertiesTree->topLevelItemCount(); ++i) {
        if (m_propertiesTree->topLevelItem(i)->text(0) == "Selection") {
            selItem = m_propertiesTree->topLevelItem(i);
            break;
        }
    }
    
    if (!selItem) return;
    
    const CanvasPolyline* poly = m_canvas->selectedPolyline();
    if (poly) {
        for (int i = 0; i < selItem->childCount(); ++i) {
            QString prop = selItem->child(i)->text(0);
            if (prop == "Type") {
                selItem->child(i)->setText(1, poly->closed ? "Closed Polyline" : "Polyline");
            } else if (prop == "Points") {
                selItem->child(i)->setText(1, QString::number(poly->points.size()));
            } else if (prop == "Layer") {
                selItem->child(i)->setText(1, poly->layer.isEmpty() ? "Default" : poly->layer);
            }
        }
    } else {
        for (int i = 0; i < selItem->childCount(); ++i) {
            QString prop = selItem->child(i)->text(0);
            if (prop == "Type") selItem->child(i)->setText(1, "None");
            else if (prop == "Points") selItem->child(i)->setText(1, "0");
            else if (prop == "Layer") selItem->child(i)->setText(1, "-");
        }
    }
}

void MainWindow::importDXF()
{
    QString fileName = QFileDialog::getOpenFileName(this, 
        "Import DXF", QString(), 
        "DXF Files (*.dxf);;All Files (*)");
    
    if (fileName.isEmpty()) return;
    
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    // Use the new GDAL + GEOS loader
    GdalGeosLoader loader;
    DxfData data;
    bool success = loader.loadDxf(fileName, data);
    
    QApplication::restoreOverrideCursor();
    
    if (success) {
        m_canvas->loadDxfData(data);
        setWindowTitle(QString("SiteSurveyor - %1").arg(QFileInfo(fileName).fileName()));
        m_crsLabel->setText("DXF");
        
        statusBar()->showMessage(QString("Loaded: %1 polylines, %2 texts | Processed: %3, Repaired: %4, Failed: %5")
            .arg(data.polylines.size())
            .arg(data.texts.size())
            .arg(loader.geometriesProcessed())
            .arg(loader.geometriesRepaired())
            .arg(loader.geometriesFailed()), 5000);
        
        // Show geometry issues dialog if there were any repairs or failures
        QStringList issues = loader.issueLog();
        if (!issues.isEmpty()) {
            QString summary = QString("<b>Geometry Validation Report</b><br><br>"
                                       "Processed: %1<br>"
                                       "Invalid (Loaded): <span style='color:red'>%3</span><br><br>"
                                       "<b>Note:</b> Invalid geometries were loaded as-is to preserve data integrity. Use 'Check Geometry' to fix them manually.<br><br>"
                                       "<b>Details:</b><br>")
                .arg(loader.geometriesProcessed())
                .arg(loader.geometriesFailed());
            
            // Limit to first 20 issues to avoid huge dialogs
            int maxShow = qMin(issues.size(), 20);
            for (int i = 0; i < maxShow; ++i) {
                QString issue = issues[i];
                if (issue.startsWith("[FIXED]")) {
                    summary += QString("<span style='color:green'>✓</span> %1<br>").arg(issue.mid(8));
                } else {
                    summary += QString("<span style='color:red'>✗</span> %1<br>").arg(issue.mid(12));
                }
            }
            if (issues.size() > 20) {
                summary += QString("<br>... and %1 more issues").arg(issues.size() - 20);
            }
            
            QMessageBox::information(this, "Geometry Validation", summary);
        }
    } else {
        QMessageBox::warning(this, "Import DXF", 
            QString("Failed to load DXF file:\n%1\n\nError: %2")
                .arg(fileName)
                .arg(loader.lastError()));
    }
}

void MainWindow::importGDAL()
{
    QString fileName = QFileDialog::getOpenFileName(this, 
        "Import GIS Data", QString(), 
        GdalReader::fileFilter());
    
    if (fileName.isEmpty()) return;
    
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    GdalReader reader;
    bool success = reader.readFile(fileName);
    
    QApplication::restoreOverrideCursor();
    
    if (success) {
        m_canvas->loadGdalData(reader.data());
        setWindowTitle(QString("SiteSurveyor - %1").arg(QFileInfo(fileName).fileName()));
        
        // Show CRS in status bar
        if (!reader.data().crs.isEmpty()) {
            m_crsLabel->setText(reader.data().crs);
        } else {
            m_crsLabel->setText("Unknown CRS");
        }
        
        const auto& data = reader.data();
        statusBar()->showMessage(QString("Loaded: %1 points, %2 lines, %3 polygons, %4 rasters, %5 layers")
            .arg(data.points.size())
            .arg(data.lineStrings.size())
            .arg(data.polygons.size())
            .arg(data.rasters.size())
            .arg(data.layers.size()), 5000);
    } else {
        QMessageBox::warning(this, "Import GIS Data", 
            QString("Failed to load GIS file:\n%1\n\nError: %2")
                .arg(fileName)
                .arg(reader.lastError()));
    }
}


void MainWindow::importCSVPoints()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Import Points", "", "CSV Files (*.csv);;Text Files (*.txt);;All Files (*)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Could not open file");
        return;
    }

    QTextStream in(&file);
    int imported = 0;
    int lineNum = 0;
    
    // Simple header detection
    bool hasHeader = false;
    bool headerIndicatesYXZ = false;
    
    QString firstLine = in.readLine().trimmed();
    lineNum++;
    QString lowerFirst = firstLine.toLower();
    
    if (lowerFirst.contains("name") || lowerFirst.contains("point") ||
        lowerFirst.contains("x") || lowerFirst.contains("east") ||
        lowerFirst.contains("y") || lowerFirst.contains("north")) {
        hasHeader = true;
        
        QStringList headerParts = firstLine.split(QRegularExpression("[,;\\t]"));
        int xCol = -1, yCol = -1;
        for (int i = 0; i < headerParts.size(); ++i) {
            QString col = headerParts[i].toLower().trimmed();
            if (xCol < 0 && (col.contains("x") || col.contains("east"))) xCol = i;
            if (yCol < 0 && (col.contains("y") || col.contains("north"))) yCol = i;
        }
        if (yCol >= 0 && xCol >= 0 && yCol < xCol) headerIndicatesYXZ = true;
    } else {
        in.seek(0);
        lineNum = 0;
    }
    
    QSettings settings;
    bool swapXY = settings.value("coordinates/swapXY", false).toBool();
    if (hasHeader) swapXY = headerIndicatesYXZ;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        lineNum++;
        if (line.isEmpty() || line.startsWith("#")) continue;
        
        QStringList parts = line.split(QRegularExpression("[,;\\t]"));
        if (parts.size() < 2) continue;
        
        QString name;
        double x = 0, y = 0, z = 0;
        bool okX = false, okY = false, okZ = false;
        
        double v0 = parts[0].trimmed().toDouble(&okX);
        if (okX && parts.size() >= 2) {
            double v1 = parts[1].trimmed().toDouble(&okY);
            if (okY) {
                x = swapXY ? v1 : v0;
                y = swapXY ? v0 : v1;
                if (parts.size() >= 3) {
                    z = parts[2].trimmed().toDouble(&okZ);
                    if (!okZ) { name = parts[2].trimmed(); z = 0; }
                    else if (parts.size() >= 4) name = parts[3].trimmed();
                }
            }
        } else {
            name = parts[0].trimmed();
            if (parts.size() >= 3) {
                double v1 = parts[1].trimmed().toDouble(&okX);
                double v2 = parts[2].trimmed().toDouble(&okY);
                if (okX && okY) {
                    x = swapXY ? v2 : v1;
                    y = swapXY ? v1 : v2;
                    if (parts.size() >= 4) z = parts[3].trimmed().toDouble(&okZ);
                }
            }
        }
        
        if (name.isEmpty()) name = QString("P%1").arg(m_canvas->pegs().size() + imported + 1);
        
        if (okX || okY) {
            CanvasPeg peg;
            peg.position = QPointF(x, y);
            peg.z = z;
            peg.name = name;
            peg.color = Qt::red;
            peg.layer = "IMPORTED";
            m_canvas->addPeg(peg);
            imported++;
        }
    }
    file.close();
    
    updatePegPanel();
    m_canvas->update();
    statusBar()->showMessage(QString("Imported %1 points from CSV").arg(imported), 5000);
    if (imported > 0) m_canvas->fitToWindow();
}


void MainWindow::updateCoordinates(const QPointF& pos)
{

    // Check if Swap X/Y is enabled via QSettings (for Zimbabwe/Y-X convention)
    QSettings settings;
    bool swapXY = settings.value("coordinates/swapXY", false).toBool();
    
    if (swapXY) {
        // Y-X convention: First value is Y (Northing), second is X (Easting)
        m_coordLabel->setText(QString("Y: %1  X: %2")
            .arg(pos.y(), 0, 'f', 3)
            .arg(pos.x(), 0, 'f', 3));
    } else {
        // Standard X-Y convention
        m_coordLabel->setText(QString("X: %1  Y: %2")
            .arg(pos.x(), 0, 'f', 3)
            .arg(pos.y(), 0, 'f', 3));
    }
}

void MainWindow::updateZoom(double zoom)
{
    m_zoomLabel->setText(QString("Zoom: %1%").arg(static_cast<int>(zoom * 100)));
}

void MainWindow::toggleSnapping(bool enabled)
{
    if (m_canvas) {
        m_canvas->setSnappingEnabled(enabled);
        statusBar()->showMessage(enabled ? "Snapping enabled" : "Snapping disabled", 2000);
    }
}

void MainWindow::offsetPolyline()
{
    if (!m_canvas) return;
    
    // Check if a polyline is selected
    if (!m_canvas->hasSelection()) {
        QMessageBox::information(this, "Offset Polyline", 
            "Please select a polyline first by clicking on it.");
        return;
    }
    
    // Get offset distance from user
    bool ok;
    double distance = QInputDialog::getDouble(this, "Offset Polyline",
        "Enter offset distance (in drawing units):",
        1.0, 0.001, 1000.0, 3, &ok);
    
    if (!ok || distance <= 0) return;
    
    // Start offset tool - user will click to indicate which side
    m_canvas->startOffsetTool(distance);
}

void MainWindow::runGamaAdjustment()
{
    if (!m_canvas) return;
    
    // Open the full network adjustment dialog
    NetworkAdjustmentDialog dialog(m_canvas, this);
    dialog.exec();
    
    // Refresh peg panel if results were applied
    updatePegPanel();
}

void MainWindow::setupToolbar()
{
    m_toolbar = addToolBar("Main Toolbar");
    m_toolbar->setMovable(false);
    m_toolbar->setIconSize(QSize(18, 18));
    
    // Check if dark mode is active
    QSettings themeSettings;
    QString theme = themeSettings.value("appearance/theme", "light").toString();
    QString iconPrefix = (theme == "dark") ? ":/icons/dark/" : ":/icons/";
    
    // Helper lambda to create action and store icon name
    auto addToolbarAction = [&](const QString& iconName, const QString& text, const QString& tooltip) -> QAction* {
        QAction* action = m_toolbar->addAction(QIcon(iconPrefix + iconName), text);
        action->setToolTip(tooltip);
        action->setData(iconName);  // Store icon name for theme switching
        return action;
    };
    
    // New Project
    QAction* newAct = addToolbarAction("file-plus.svg", "New", "New Project (Ctrl+N)");
    connect(newAct, &QAction::triggered, this, [this]() {
        if (QMessageBox::question(this, "New Project", 
                "Clear current project and start new?") == QMessageBox::Yes) {
            m_canvas->clearAll();
            m_canvas->setProjectFilePath("");
            setWindowTitle("SiteSurveyor - Untitled");
            updateLayerPanel();
        }
    });
    
    // Open Project
    QAction* openAct = m_toolbar->addAction(QIcon(iconPrefix + "folder-open.svg"), "Open");
    openAct->setToolTip("Open Project (Ctrl+O)");
    openAct->setData("folder-open.svg");
    connect(openAct, &QAction::triggered, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this,
            "Open Project", QString(),
            "SiteSurveyor Project (*.ssp);;All Files (*)");
        if (!filePath.isEmpty()) {
            if (m_canvas->loadProject(filePath)) {
                setWindowTitle(QString("SiteSurveyor - %1").arg(QFileInfo(filePath).fileName()));
                addToRecentProjects(filePath);
                updateLayerPanel();
            }
        }
    });
    
    // Save Project
    QAction* saveAct = m_toolbar->addAction(QIcon(iconPrefix + "save.svg"), "Save");
    saveAct->setToolTip("Save Project (Ctrl+S)");
    saveAct->setData("save.svg");
    connect(saveAct, &QAction::triggered, this, [this]() {
        QString filePath = m_canvas->projectFilePath();
        if (filePath.isEmpty()) {
            filePath = QFileDialog::getSaveFileName(this,
                "Save Project", QString(),
                "SiteSurveyor Project (*.ssp);;All Files (*)");
        }
        if (!filePath.isEmpty()) {
            if (!filePath.endsWith(".ssp")) filePath += ".ssp";
            if (m_canvas->saveProject(filePath)) {
                m_canvas->setProjectFilePath(filePath);
                addToRecentProjects(filePath);
                setWindowTitle(QString("SiteSurveyor - %1").arg(QFileInfo(filePath).fileName()));
                statusBar()->showMessage("Project saved.", 3000);
            }
        }
    });
    
    m_toolbar->addSeparator();
    
    // Import DXF
    // Import DXF
    QAction* importAct = addToolbarAction("import.svg", "Import", "Import DXF (Ctrl+D)");
    connect(importAct, &QAction::triggered, this, &MainWindow::importDXF);
    
    m_toolbar->addSeparator();
    
    // Zoom In
    QAction* zoomInAct = m_toolbar->addAction(QIcon(iconPrefix + "zoom-in.svg"), "");
    zoomInAct->setToolTip("Zoom In (+)");
    zoomInAct->setData("zoom-in.svg");
    connect(zoomInAct, &QAction::triggered, m_canvas, &CanvasWidget::zoomIn);
    
    // Zoom Out
    QAction* zoomOutAct = m_toolbar->addAction(QIcon(iconPrefix + "zoom-out.svg"), "");
    zoomOutAct->setToolTip("Zoom Out (-)");
    zoomOutAct->setData("zoom-out.svg");
    connect(zoomOutAct, &QAction::triggered, m_canvas, &CanvasWidget::zoomOut);
    
    // Fit to Window
    QAction* fitAct = m_toolbar->addAction(QIcon(iconPrefix + "zoom-fit.svg"), "");
    fitAct->setToolTip("Fit to Window (Ctrl+0)");
    fitAct->setData("zoom-fit.svg");
    connect(fitAct, &QAction::triggered, m_canvas, &CanvasWidget::fitToWindow);
    
    m_toolbar->addSeparator();
    
    // Select Mode
    QAction* selectAct = m_toolbar->addAction(QIcon(iconPrefix + "select.svg"), "");
    selectAct->setToolTip("Select Mode (V)");
    selectAct->setData("select.svg");
    connect(selectAct, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startSelectMode();
    });
    
    // Offset Tool
    QAction* offsetAct = m_toolbar->addAction(QIcon(iconPrefix + "offset.svg"), "");
    offsetAct->setToolTip("Offset Polyline (O)");
    offsetAct->setData("offset.svg");
    connect(offsetAct, &QAction::triggered, this, &MainWindow::offsetPolyline);
    
    // Split Tool
    QAction* splitAct = m_toolbar->addAction(QIcon(iconPrefix + "split.svg"), "");
    splitAct->setToolTip("Split Polyline (X)");
    splitAct->setData("split.svg");
    connect(splitAct, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startSplitMode();
    });
    
    // Merge Tool
    QAction* mergeAct = m_toolbar->addAction(QIcon(iconPrefix + "join.svg"), "");
    mergeAct->setToolTip("Merge Selected (M)");
    mergeAct->setData("join.svg");
    connect(mergeAct, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->joinPolylines();
    });
    
    // Copy Tool
    QAction* copyAct = m_toolbar->addAction(QIcon(iconPrefix + "copy.svg"), "");
    copyAct->setToolTip("Copy Selected (C)");
    copyAct->setData("copy.svg");
    connect(copyAct, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->copySelectedPolyline();
    });
    
    // Move Tool  
    // Move Tool  
    QAction* moveAct = addToolbarAction("move.svg", "Move", "Move Selected");
    connect(moveAct, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startMoveMode();
    });
    
    // Mirror Tool
    // Mirror Tool
    QAction* mirrorAct = addToolbarAction("mirror.svg", "Mirror", "Mirror Selected");
    connect(mirrorAct, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startMirrorMode();
    });
    
    // Explode Tool
    // Explode Tool
    QAction* explodeAct = addToolbarAction("explode.svg", "", "Explode (E)");
    connect(explodeAct, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->explodeSelectedPolyline();
    });
    
    // Trim Tool
    // Trim Tool
    QAction* trimAct = addToolbarAction("trim.svg", "Trim", "Trim");
    connect(trimAct, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startTrimMode();
    });
    
    // Extend Tool
    // Extend Tool
    QAction* extendAct = addToolbarAction("extend.svg", "Extend", "Extend");
    connect(extendAct, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startExtendMode();
    });
    
    // Fillet Tool
    QAction* filletAct = m_toolbar->addAction("");
    filletAct->setText("Fillet");
    filletAct->setToolTip("Fillet");
    connect(filletAct, &QAction::triggered, this, [this]() {
        if (m_canvas) {
            bool ok;
            double radius = QInputDialog::getDouble(this, "Fillet Radius",
                "Enter fillet radius:", 1.0, 0.001, 1000.0, 3, &ok);
            if (ok) m_canvas->startFilletMode(radius);
        }
    });
    
    // Measure Tool
    QAction* measureAct = m_toolbar->addAction("");
    measureAct->setText("Measure");
    measureAct->setToolTip("Measure Distance");
    connect(measureAct, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startMeasureMode();
    });
    
    m_toolbar->addSeparator();
    
    // Station Setup
    QAction* stationAct = m_toolbar->addAction(QIcon(iconPrefix + "station.svg"), "");
    stationAct->setToolTip("Set Station (1)");
    stationAct->setData("station.svg");
    connect(stationAct, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startSetStationMode();
    });
    
    // Stakeout
    QAction* stakeoutAct = m_toolbar->addAction(QIcon(iconPrefix + "stakeout.svg"), "");
    stakeoutAct->setToolTip("Stakeout Mode (K)");
    stakeoutAct->setData("stakeout.svg");
    connect(stakeoutAct, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startStakeoutMode();
    });
    
    m_toolbar->addSeparator();
    
    // Snap Toggle
    m_snapAction = m_toolbar->addAction(QIcon(iconPrefix + "snap.svg"), "");
    m_snapAction->setCheckable(true);
    m_snapAction->setToolTip("Toggle Snapping (S)");
    m_snapAction->setData("snap.svg");
    connect(m_snapAction, &QAction::toggled, this, &MainWindow::toggleSnapping);
    
    // Grid Toggle
    QAction* gridAct = m_toolbar->addAction(QIcon(iconPrefix + "grid.svg"), "");
    gridAct->setCheckable(true);
    gridAct->setChecked(true);
    gridAct->setToolTip("Toggle Grid (G)");
    gridAct->setData("grid.svg");
    connect(gridAct, &QAction::toggled, this, [this](bool enabled) {
        if (m_canvas) m_canvas->setShowGrid(enabled);
    });
    
    m_toolbar->addSeparator();
    
    // Settings
    QAction* settingsAct = m_toolbar->addAction(QIcon(iconPrefix + "settings.svg"), "");
    settingsAct->setToolTip("Settings (Ctrl+,)");
    settingsAct->setData("settings.svg");
    connect(settingsAct, &QAction::triggered, this, [this]() {
        SettingsDialog dialog(m_canvas, m_auth, this);
        dialog.exec();
        // Update icons after settings dialog closes (in case theme changed)
        updateToolbarIcons();
    });
}

void MainWindow::updateToolbarIcons()
{
    // Get current theme
    QSettings settings;
    QString theme = settings.value("appearance/theme", "light").toString();
    QString iconPrefix = (theme == "dark") ? ":/icons/dark/" : ":/icons/";
    
    // Update all toolbar action icons using stored icon name in data
    for (QAction* action : m_toolbar->actions()) {
        if (action->isSeparator()) continue;
        
        QString iconName = action->data().toString();
        if (!iconName.isEmpty()) {
            action->setIcon(QIcon(iconPrefix + iconName));
        }
    }
}

void MainWindow::showKeyboardShortcuts()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Keyboard Shortcuts");
    dialog.resize(500, 600);
    
    // Theme detection
    QSettings settings;
    bool isDark = (settings.value("appearance/theme", "light").toString() == "dark");
    QString bgCol = isDark ? "#1e1e2e" : "#ffffff";
    QString textCol = isDark ? "#cdd6f4" : "#333333";
    QString headCol = isDark ? "#89b4fa" : "#0066cc";
    QString keyBg = isDark ? "#313244" : "#f0f0f0";
    QString keyBorder = isDark ? "#45475a" : "#cccccc";
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QTextBrowser* browser = new QTextBrowser();
    browser->setOpenExternalLinks(false);
    
    // Modern CSS for shortcuts table
    QString css = QString(R"(
        <style>
            body { background-color: %1; color: %2; font-family: sans-serif; }
            h2 { color: %3; border-bottom: 2px solid %3; padding-bottom: 5px; }
            h3 { color: %3; margin-top: 20px; }
            table { width: 100%; border-collapse: collapse; margin-bottom: 15px; }
            td { padding: 6px; vertical-align: middle; }
            td.key { width: 140px; }
            kbd { 
                background-color: %4; 
                border: 1px solid %5; 
                border-radius: 4px; 
                padding: 3px 8px; 
                font-family: monospace; 
                font-weight: bold; 
                color: %2;
                display: inline-block;
            }
        </style>
    )").arg(bgCol, textCol, headCol, keyBg, keyBorder);
    
    QString html = R"(
        <h2>Keyboard Shortcuts</h2>
        
        <h3>File Operations</h3>
        <table>
        <tr><td class="key"><kbd>Ctrl+N</kbd></td><td>New Project</td></tr>
        <tr><td class="key"><kbd>Ctrl+O</kbd></td><td>Open Project</td></tr>
        <tr><td class="key"><kbd>Ctrl+S</kbd></td><td>Save Project</td></tr>
        <tr><td class="key"><kbd>Ctrl+Shift+S</kbd></td><td>Save As...</td></tr>
        <tr><td class="key"><kbd>Ctrl+D</kbd></td><td>Import DXF</td></tr>
        <tr><td class="key"><kbd>Ctrl+G</kbd></td><td>Import GIS Data</td></tr>
        </table>
        
        <h3>View & Navigation</h3>
        <table>
        <tr><td class="key"><kbd>+</kbd> / <kbd>=</kbd></td><td>Zoom In</td></tr>
        <tr><td class="key"><kbd>-</kbd></td><td>Zoom Out</td></tr>
        <tr><td class="key"><kbd>Ctrl+0</kbd></td><td>Fit to Window</td></tr>
        <tr><td class="key"><kbd>Ctrl+Shift+C</kbd></td><td>Toggle Command Console</td></tr>
        <tr><td class="key"><kbd>F11</kbd></td><td>Maximize Window</td></tr>
        <tr><td class="key"><kbd>Shift+F11</kbd></td><td>Toggle Fullscreen</td></tr>
        </table>
        
        <h3>Tools & Editing</h3>
        <table>
        <tr><td class="key"><kbd>S</kbd></td><td>Toggle Snapping</td></tr>
        <tr><td class="key"><kbd>O</kbd></td><td>Offset Polyline</td></tr>
        <tr><td class="key"><kbd>P</kbd></td><td>Partition to Offset</td></tr>
        <tr><td class="key"><kbd>H</kbd></td><td>Pan Mode</td></tr>
        <tr><td class="key"><kbd>Ctrl+Z</kbd></td><td>Undo</td></tr>
        <tr><td class="key"><kbd>Ctrl+Y</kbd></td><td>Redo</td></tr>
        <tr><td class="key"><kbd>Del</kbd></td><td>Delete Selected</td></tr>
        </table>
        
        <h3>Station Setup</h3>
        <table>
        <tr><td class="key"><kbd>1</kbd></td><td>Set Station Point</td></tr>
        <tr><td class="key"><kbd>2</kbd></td><td>Set Backsight</td></tr>
        <tr><td class="key"><kbd>K</kbd></td><td>Stakeout Mode</td></tr>
        </table>
    )";
    
    browser->setHtml(css + html);
    layout->addWidget(browser);
    
    QDialogButtonBox* btnBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(btnBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(btnBox);
    
    dialog.exec();
}

void MainWindow::updateRecentProjectsMenu()
{
    if (!m_recentMenu) return;
    
    m_recentMenu->clear();
    
    QSettings settings("SiteSurveyor", "SiteSurveyor");
    QStringList recentFiles = settings.value("recentProjects").toStringList();
    
    if (recentFiles.isEmpty()) {
        QAction* emptyAction = m_recentMenu->addAction("(No recent projects)");
        emptyAction->setEnabled(false);
        return;
    }
    
    for (const QString& filePath : recentFiles) {
        if (QFile::exists(filePath)) {
            QAction* action = m_recentMenu->addAction(QFileInfo(filePath).fileName());
            action->setData(filePath);
            action->setToolTip(filePath);
            connect(action, &QAction::triggered, this, &MainWindow::openRecentProject);
        }
    }
    
    m_recentMenu->addSeparator();
    
    QAction* clearAction = m_recentMenu->addAction("Clear Recent Projects");
    connect(clearAction, &QAction::triggered, this, [this]() {
        QSettings settings("SiteSurveyor", "SiteSurveyor");
        settings.setValue("recentProjects", QStringList());
        updateRecentProjectsMenu();
    });
}

void MainWindow::openRecentProject()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (!action) return;
    
    QString filePath = action->data().toString();
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        QMessageBox::warning(this, "Error", "Project file not found.");
        return;
    }
    
    if (m_canvas->loadProject(filePath)) {
        setWindowTitle(QString("SiteSurveyor - %1").arg(QFileInfo(filePath).fileName()));
        addToRecentProjects(filePath);
        updateLayerPanel();
    } else {
        QMessageBox::warning(this, "Error", "Failed to load project file.");
    }
}

void MainWindow::addToRecentProjects(const QString& filePath)
{
    QSettings settings("SiteSurveyor", "SiteSurveyor");
    QStringList recentFiles = settings.value("recentProjects").toStringList();
    
    // Remove if already exists (to reorder)
    recentFiles.removeAll(filePath);
    
    // Add to beginning
    recentFiles.prepend(filePath);
    
    // Keep only last 10
    while (recentFiles.size() > 10) {
        recentFiles.removeLast();
    }
    
    settings.setValue("recentProjects", recentFiles);
    updateRecentProjectsMenu();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Exit SiteSurveyor",
        "Are you sure you want to exit?\n\nAny unsaved changes will be lost.",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::saveToCloud()
{
    if (!m_auth->isAuthenticated()) {
        QMessageBox::warning(this, "Cloud Save", "You must be logged in to save to the cloud.");
        return;
    }

    CloudFileDialog dialog(m_cloudManager, CloudFileDialog::Save, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString name = dialog.selectedFileName();
        // Ensure extension
        if (!name.endsWith(".ssp")) name += ".ssp";
        
        QByteArray data = m_canvas->saveProjectToJson();
        
        // Helper to perform upload
        auto performUpload = [this, name, data]() {
            statusBar()->showMessage("Uploading to cloud...", 0);
            
            QMetaObject::Connection* conn = new QMetaObject::Connection;
            *conn = connect(m_cloudManager, &CloudManager::uploadFinished, this, 
                [this, conn, name](bool success, const QString& msg) {
                    statusBar()->showMessage(success ? "Upload successful" : "Upload failed: " + msg, 5000);
                    if (success) {
                        // After successful save, this becomes the current cloud file
                        // Note: We don't have the ID yet unless we list. 
                        // For simplicity, we invalidate current ID if handle changes or just leave it.
                        // Ideally we should fetch the new ID.
                    } else {
                        QMessageBox::warning(this, "Upload Error", msg);
                    }
                    disconnect(*conn);
                    delete conn;
                });
                
            // Use uploadNewVersion to maintain history
            // Strip .ssp and version suffix if any to get base name
            QString baseName = name;
            baseName.remove(QRegularExpression("\\.ssp$"));
            m_cloudManager->uploadNewVersion(baseName, data);
        };

        // Check for conflict if we are overwriting the currently open cloud file
        // We assume we are overwriting if we have an ID and an Etag
        if (!m_currentCloudFileId.isEmpty() && !m_currentCloudEtag.isEmpty()) {
            
            // Check if the selected name matches the current file name would be better
            // But for now, let's just check conflict on the ID we have.
            // If the user picked a DIFFERENT file in the dialog, this logic is slightly flawed 
            // as we are checking the OLD file.
            // But CloudFileDialog typically returns a name. 
            // If we assume the user is saving the current project:
            
            m_cloudManager->checkForConflict(m_currentCloudFileId, m_currentCloudEtag);
            
            // Wait for signal? We need to connect to conflictDetected signal.
            // This requires a nested event loop or separating logic.
            // For simplicity in this iteration:
            // We connect to conflict signal and inside that slot we decide to upload or not.
            
            QMetaObject::Connection* connConflict = new QMetaObject::Connection;
            QMetaObject::Connection* connNoConflict = new QMetaObject::Connection;
            
            auto cleanup = [connConflict, connNoConflict]() {
                disconnect(*connConflict);
                disconnect(*connNoConflict);
                delete connConflict;
                delete connNoConflict;
            };

            *connConflict = connect(m_cloudManager, &CloudManager::conflictDetected, this,
                [this, performUpload, cleanup](const QString& id, const CloudFile& remote) {
                    cleanup();
                    
                    ConflictDialog dialog("Local Changes", QDateTime::currentDateTime(), 
                                        remote.name, remote.updatedAt, this);
                    if (dialog.exec() == QDialog::Accepted) {
                        if (dialog.resolution() == ConflictDialog::OverwriteRemote) {
                            performUpload();
                        } else if (dialog.resolution() == ConflictDialog::StartNewProject) {
                            // User wants to save as new, CloudFileDialog already gave us a name?
                            // Actually if we want "New Project", we should probably ask for a new name 
                            // or append "copy".
                            // For now, OverwriteRemote = Upload standard (which creates version).
                            // StartNewProject logic is tricky here without re-running dialog.
                            // Let's treat OverwriteRemote as "Proceed with Versioned Upload"
                            // And Cancel as Cancel.
                        }
                    }
                });

            *connNoConflict = connect(m_cloudManager, &CloudManager::noConflict, this,
                [performUpload, cleanup](const QString& id) {
                    cleanup();
                    performUpload();
                });
                
            return; // Return and wait for signal
        }

        // No current cloud file or new save
        performUpload();
    }
}

void MainWindow::openFromCloud()
{
    if (!m_auth->isAuthenticated()) {
        QMessageBox::warning(this, "Cloud Open", "You must be logged in to access cloud projects.");
        return;
    }

    CloudFileDialog dialog(m_cloudManager, CloudFileDialog::Open, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString fileId = dialog.selectedFileId();
        
        statusBar()->showMessage("Downloading from cloud...", 0);
        
        QMetaObject::Connection* conn = new QMetaObject::Connection;
        *conn = connect(m_cloudManager, &CloudManager::downloadFinished, this, 
            [this, conn, fileId](bool success, const QByteArray& data, const QString& msg) {
                statusBar()->showMessage(success ? "Download successful" : "Download failed: " + msg, 5000);
                if (success) {
                    if (m_canvas->loadProjectFromJson(data)) {
                        m_canvas->setProjectFilePath("cloud://" + fileId);
                        statusBar()->showMessage("Project loaded from cloud", 3000);
                        
                        // Track current file
                        m_currentCloudFileId = fileId;
                        // We need Etag for conflict detection. 
                        // The download signal doesn't provide it, but listProjects does.
                        // We could fetch metadata. For now, clear etag until we implement better tracking.
                        m_currentCloudEtag.clear(); 
                        
                        // Refresh etag via separate call? 
                        // Or modify downloadFinished to include metadata?
                        // For now we skip etag update, conflict detection will rely on next save check fetching it.
                    } else {
                        QMessageBox::critical(this, "Error", "Failed to parse project data");
                    }
                } else {
                    QMessageBox::warning(this, "Download Error", msg);
                }
                disconnect(*conn);
                delete conn;
            });
            
        m_cloudManager->downloadProject(fileId);
    }
}

void MainWindow::showVersionHistory()
{
    if (m_currentCloudFileId.isEmpty()) {
        // If not editing a cloud file, maybe ask user to pick one?
        // Or show for "currently open" file name?
        // For now, simplistic:
        QMessageBox::information(this, "Version History", "Please open a cloud project first to view its history.");
        return;
    }
    
    // We need base name.
    // Assuming m_canvas->projectFilePath() has some info or we track name.
    // Let's assume we can derive it or ask user to pick project from list.
    // Better: Open File Dialog to pick project to view history for?
    // Let's use that approach as it's more flexible.
    
    CloudFileDialog dialog(m_cloudManager, CloudFileDialog::Open, this);
    dialog.setWindowTitle("Select Project for History");
    if (dialog.exec() == QDialog::Accepted) {
        QString name = dialog.selectedFileName();
        QString baseName = name.remove(QRegularExpression("(_v\\d+)?\\.ssp$"));
        
        VersionHistoryDialog historyDialog(m_cloudManager, baseName, this);
        connect(&historyDialog, &VersionHistoryDialog::restoreRequested, this, 
            [this](const QString& fileId, int version) {
                // Load the restored version
                 m_cloudManager->downloadProject(fileId);
                 // We need to connect to downloadFinished to load it... 
                 // Reuse openFromCloud logic logic basically.
                 // For now, simple notification
                 QMessageBox::information(this, "Restore", "Version restored. Please use 'Open from Cloud' to open the specific version ID: " + fileId);
            });
        historyDialog.exec();
    }
}

void MainWindow::shareProject()
{
    if (m_currentCloudFileId.isEmpty()) {
        QMessageBox::information(this, "Share Project", "Please open a cloud project first to share it.");
        return;
    }
    
    // We need project name.
    QString name = "Project"; // Placeholder
    
    ShareProjectDialog dialog(m_cloudManager, m_currentCloudFileId, name, this);
    dialog.exec();
}

