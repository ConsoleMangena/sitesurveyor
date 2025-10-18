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

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("SiteSurveyor - Professional Geomatics Software");
    // Ensure the menubar appears inside the window (useful across desktops)
    if (menuBar()) {
        menuBar()->setNativeMenuBar(false);
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
    m_layerPanel = nullptr;
    m_layerCombo = nullptr;
    m_preferencesAction = nullptr;
    m_undoStack = new QUndoStack(this);
    m_canvas->setUndoStack(m_undoStack);
    
    setupUI();
    setupMenus();
    setupToolbar();
    setupConnections();
    
    updateStatus();
}

void MainWindow::resetLayout()
{
    if (!m_defaultLayoutState.isEmpty()) {
        restoreState(m_defaultLayoutState);
        // Nudge default sizes again for good measure (keep Layers smaller)
        if (m_layersDock && m_pointsDock) resizeDocks({m_layersDock, m_pointsDock}, {160, 360}, Qt::Horizontal);
        if (m_commandDock) resizeDocks({m_commandDock}, {200}, Qt::Vertical);
    }
}

void MainWindow::toggleDarkMode(bool on)
{
    m_darkMode = on;
    QPalette pal;
    if (on) {
        pal.setColor(QPalette::Window, QColor(37, 37, 38));
        pal.setColor(QPalette::WindowText, Qt::white);
        pal.setColor(QPalette::Base, QColor(30, 30, 30));
        pal.setColor(QPalette::AlternateBase, QColor(45, 45, 48));
        pal.setColor(QPalette::ToolTipBase, QColor(255, 255, 220));
        pal.setColor(QPalette::ToolTipText, Qt::black);
        pal.setColor(QPalette::Text, Qt::white);
        pal.setColor(QPalette::Button, QColor(45, 45, 48));
        pal.setColor(QPalette::ButtonText, Qt::white);
        pal.setColor(QPalette::BrightText, Qt::red);
        pal.setColor(QPalette::Highlight, QColor(0, 120, 215));
        pal.setColor(QPalette::HighlightedText, Qt::white);
        qApp->setStyle("Fusion");
    } else {
        pal = QPalette();
    }
    qApp->setPalette(pal);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    // Central widget with canvas
    setCentralWidget(m_canvas);
    
    // Create dock widgets
    setupPointsDock();
    setupCommandDock();
    setupLayersDock();
    setupPropertiesDock();
    // Arrange initial dock sizes
    if (m_layersDock && m_pointsDock) {
        // Make Layers (left) a bit smaller than Coordinates (right)
        resizeDocks({m_layersDock, m_pointsDock}, {160, 360}, Qt::Horizontal);
    }
    if (m_commandDock) {
        resizeDocks({m_commandDock}, {200}, Qt::Vertical);
    }
    // Save default layout state for reset
    m_defaultLayoutState = saveState();
    
    // Status bar
    m_statusBar = statusBar();
    m_coordLabel = new QLabel(AppSettings::gaussMode() ? "Y: 0.000  X: 0.000" : "X: 0.000  Y: 0.000");
    m_pointCountLabel = new QLabel("Coordinates: 0");
    m_zoomLabel = new QLabel("Zoom: 100%");
    
    m_statusBar->addWidget(m_coordLabel);
    m_statusBar->addWidget(new QLabel(" | "));
    m_statusBar->addWidget(m_pointCountLabel);
    m_statusBar->addWidget(new QLabel(" | "));
    m_statusBar->addWidget(m_zoomLabel);
    m_statusBar->addWidget(new QLabel(" | "));
    m_layerStatusLabel = new QLabel("Layer: 0");
    m_statusBar->addWidget(m_layerStatusLabel);
    m_statusBar->addWidget(new QLabel(" | "));
    m_measureLabel = new QLabel("Len: 0.000 m");
    m_statusBar->addWidget(m_measureLabel);
    updateLayerStatusText();
    // ORTHO toggle (F8)
    m_orthoAction = new QAction(IconManager::icon("ortho"), "ORTHO", this);
    m_orthoAction->setCheckable(true);
    m_orthoAction->setToolTip("Toggle Ortho mode (F8)");
    m_orthoAction->setShortcut(QKeySequence(Qt::Key_F8));
    addAction(m_orthoAction); // enable shortcut
    connect(m_orthoAction, &QAction::toggled, m_canvas, &CanvasWidget::setOrthoMode);
    m_orthoButton = new QToolButton(this);
    m_orthoButton->setDefaultAction(m_orthoAction);
    m_orthoButton->setAutoRaise(true);
    m_statusBar->addWidget(m_orthoButton);
    // SNAP toggle (F9)
    m_snapAction = new QAction(IconManager::icon("snap"), "SNAP", this);
    m_snapAction->setCheckable(true);
    m_snapAction->setToolTip("Toggle Snap to Grid (F9)");
    m_snapAction->setShortcut(QKeySequence(Qt::Key_F9));
    addAction(m_snapAction);
    connect(m_snapAction, &QAction::toggled, m_canvas, &CanvasWidget::setSnapMode);
    m_snapButton = new QToolButton(this);
    m_snapButton->setDefaultAction(m_snapAction);
    m_snapButton->setAutoRaise(true);
    m_statusBar->addWidget(m_snapButton);
    // OSNAP toggle (F3)
    m_osnapAction = new QAction(IconManager::icon("osnap"), "OSNAP", this);
    m_osnapAction->setCheckable(true);
    m_osnapAction->setToolTip("Toggle Object Snap (F3)");
    m_osnapAction->setShortcut(QKeySequence(Qt::Key_F3));
    addAction(m_osnapAction);
    connect(m_osnapAction, &QAction::toggled, m_canvas, &CanvasWidget::setOsnapMode);
    m_osnapButton = new QToolButton(this);
    m_osnapButton->setDefaultAction(m_osnapAction);
    m_osnapButton->setAutoRaise(true);
    m_statusBar->addWidget(m_osnapButton);
}

void MainWindow::setupPointsDock()
{
    m_pointsDock = new QDockWidget("Coordinates", this);
    m_pointsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    
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
    addDockWidget(Qt::RightDockWidgetArea, m_pointsDock);
}

void MainWindow::setupCommandDock()
{
    QDockWidget* commandDock = new QDockWidget("Command Console", this);
    commandDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    
    QWidget* commandWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(commandWidget);
    
    // Command output
    m_commandOutput = new QTextEdit();
    m_commandOutput->setReadOnly(true);
    m_commandOutput->setMaximumHeight(150);
    m_commandOutput->setFont(QFont("Consolas", 9));
    
    // Command input
    QHBoxLayout* inputLayout = new QHBoxLayout();
    m_commandInput = new QLineEdit();
    m_commandInput->setPlaceholderText("Enter command (type 'help' for commands)");
    
    QPushButton* executeBtn = new QPushButton("Execute");
    executeBtn->setMaximumWidth(80);
    
    inputLayout->addWidget(new QLabel("Command:"));
    inputLayout->addWidget(m_commandInput);
    inputLayout->addWidget(executeBtn);
    
    layout->addWidget(m_commandOutput);
    layout->addLayout(inputLayout);
    
    commandDock->setWidget(commandWidget);
    addDockWidget(Qt::BottomDockWidgetArea, commandDock);
    m_commandDock = commandDock;
    
    // Connect execute button
    connect(executeBtn, &QPushButton::clicked, this, &MainWindow::executeCommand);
    connect(m_commandInput, &QLineEdit::returnPressed, this, &MainWindow::executeCommand);
}

void MainWindow::setupLayersDock()
{
    m_layersDock = new QDockWidget("Layers", this);
    m_layersDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_layerPanel = new LayerPanel(this);
    m_layerPanel->setLayerManager(m_layerManager);
    // Keep panel in sync on layer changes
    connect(m_layerManager, &LayerManager::layersChanged, m_layerPanel, &LayerPanel::reload);
    connect(m_layerPanel, &LayerPanel::requestSetCurrent, this, [this](const QString& name){
        if (m_layerManager) m_layerManager->setCurrentLayer(name);
    });
    m_layersDock->setWidget(m_layerPanel);
    addDockWidget(Qt::LeftDockWidgetArea, m_layersDock);
}

void MainWindow::setupPropertiesDock()
{
    m_propertiesDock = new QDockWidget("Properties", this);
    m_propertiesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_propertiesPanel = new PropertiesPanel(this);
    m_propertiesPanel->setLayerManager(m_layerManager);
    m_propertiesPanel->setCanvas(m_canvas);
    m_propertiesDock->setWidget(m_propertiesPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);
    // Tabify with Coordinates dock for a tidy right side
    if (m_pointsDock) {
        tabifyDockWidget(m_pointsDock, m_propertiesDock);
        // Show Coordinates tab by default so users immediately see the table
        m_pointsDock->raise();
    }
}

void MainWindow::setupMenus()
{
    // File Menu
    QMenu* fileMenu = menuBar()->addMenu("&File");
    
    QAction* newProject = fileMenu->addAction("&New Project");
    newProject->setShortcut(QKeySequence::New);
    connect(newProject, &QAction::triggered, this, &MainWindow::newProject);
    
    QAction* openProject = fileMenu->addAction("&Open Project...");
    openProject->setShortcut(QKeySequence::Open);
    
    QAction* saveProject = fileMenu->addAction("&Save Project");
    saveProject->setShortcut(QKeySequence::Save);
    
    fileMenu->addSeparator();
    
    QAction* importPoints = fileMenu->addAction("&Import Coordinates...");
    QAction* exportPoints = fileMenu->addAction("&Export Coordinates...");
    
    fileMenu->addSeparator();
    
    QAction* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    
    // Edit Menu
    QMenu* editMenu = menuBar()->addMenu("&Edit");
    // Undo/Redo actions
    m_undoAction = editMenu->addAction("Undo");
    m_undoAction->setShortcut(QKeySequence::Undo);
    connect(m_undoAction, &QAction::triggered, this, [this](){ if (m_undoStack) m_undoStack->undo(); });
    m_redoAction = editMenu->addAction("Redo");
    m_redoAction->setShortcut(QKeySequence::Redo);
    connect(m_redoAction, &QAction::triggered, this, [this](){ if (m_undoStack) m_undoStack->redo(); });
    editMenu->addSeparator();
    
    QAction* addPointAction = editMenu->addAction("&Add Coordinate...");
    addPointAction->setShortcut(QKeySequence("Ctrl+N"));
    connect(addPointAction, &QAction::triggered, this, &MainWindow::showAddPointDialog);
    
    QAction* deletePointAction = editMenu->addAction("&Delete Coordinate");
    deletePointAction->setShortcut(QKeySequence::Delete);
    
    editMenu->addSeparator();
    
    QAction* clearAllAction = editMenu->addAction("Clear &All");
    connect(clearAllAction, &QAction::triggered, this, &MainWindow::clearAll);

    // Also expose Preferences under Edit for easy discovery
    editMenu->addSeparator();
    QAction* editPreferencesAction = editMenu->addAction("&Preferences...");
    if (editPreferencesAction) {
        editPreferencesAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+,")));
        connect(editPreferencesAction, &QAction::triggered, this, &MainWindow::showSettings);
    }
    
    // View Menu
    QMenu* viewMenu = menuBar()->addMenu("&View");
    
    QAction* zoomIn = viewMenu->addAction("Zoom &In");
    zoomIn->setIcon(IconManager::icon("zoom-in"));
    zoomIn->setShortcut(QKeySequence::ZoomIn);
    connect(zoomIn, &QAction::triggered, m_canvas, &CanvasWidget::zoomIn);
    
    QAction* zoomOut = viewMenu->addAction("Zoom &Out");
    zoomOut->setIcon(IconManager::icon("zoom-out"));
    zoomOut->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOut, &QAction::triggered, m_canvas, &CanvasWidget::zoomOut);
    
    QAction* fitView = viewMenu->addAction("&Fit to Window");
    fitView->setIcon(IconManager::icon("fit"));
    fitView->setShortcut(QKeySequence("Ctrl+0"));
    connect(fitView, &QAction::triggered, m_canvas, &CanvasWidget::fitToWindow);
    
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
    QAction* toggleCrosshair = viewMenu->addAction("Show &Crosshair");
    toggleCrosshair->setIcon(IconManager::icon("crosshair"));
    toggleCrosshair->setCheckable(true);
    toggleCrosshair->setChecked(true);
    connect(toggleCrosshair, &QAction::toggled, m_canvas, &CanvasWidget::setShowCrosshair);
    
    
    // Tools Menu
    QMenu* toolsMenu = menuBar()->addMenu("&Tools");
    
    QAction* calcDistance = toolsMenu->addAction("Calculate &Distance...");
    QAction* calcArea = toolsMenu->addAction("Calculate &Area...");
    QAction* calcAzimuth = toolsMenu->addAction("Calculate A&zimuth...");
    QAction* polarInputAct = toolsMenu->addAction("&Polar Input...");
    polarInputAct->setIcon(IconManager::icon("polar"));
    connect(polarInputAct, &QAction::triggered, this, &MainWindow::showPolarInput);
    QAction* joinPolarAct = toolsMenu->addAction("&Join (Polar)...");
    joinPolarAct->setIcon(IconManager::icon("join"));
    connect(joinPolarAct, &QAction::triggered, this, &MainWindow::showJoinPolar);
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
    
    // Add layout/theme controls
    viewMenu->addSeparator();
    m_resetLayoutAction = viewMenu->addAction("Reset &Layout");
    connect(m_resetLayoutAction, &QAction::triggered, this, &MainWindow::resetLayout);
    m_darkModeAction = viewMenu->addAction("&Dark Mode");
    m_darkModeAction->setCheckable(true);
    connect(m_darkModeAction, &QAction::toggled, this, &MainWindow::toggleDarkMode);

    // Settings (top-level) Menu: reuse the same Preferences action
    QMenu* settingsMenu = menuBar()->addMenu("&Settings");
    if (m_preferencesAction) {
        settingsMenu->addAction(m_preferencesAction);
    }

    // Help Menu
    QMenu* helpMenu = menuBar()->addMenu("&Help");
    
    QAction* aboutAction = helpMenu->addAction("&About SiteSurveyor");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::setupToolbar()
{
    QToolBar* mainToolbar = addToolBar("Main");
    mainToolbar->setMovable(false);
    mainToolbar->setFloatable(false);
    mainToolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    mainToolbar->setIconSize(QSize(24, 24));
    
    // Add actions
    QAction* homeAction = mainToolbar->addAction(IconManager::icon("home"), "Home");
    homeAction->setToolTip("Reset view to origin");
    homeAction->setStatusTip("Reset view to default position");
    connect(homeAction, &QAction::triggered, m_canvas, &CanvasWidget::resetView);
    
    mainToolbar->addSeparator();
    
    QAction* zoomInAction = mainToolbar->addAction(IconManager::icon("zoom-in"), "Zoom In");
    zoomInAction->setToolTip("Zoom in (Ctrl+=)");
    connect(zoomInAction, &QAction::triggered, m_canvas, &CanvasWidget::zoomIn);
    
    QAction* zoomOutAction = mainToolbar->addAction(IconManager::icon("zoom-out"), "Zoom Out");
    zoomOutAction->setToolTip("Zoom out (Ctrl+-)");
    connect(zoomOutAction, &QAction::triggered, m_canvas, &CanvasWidget::zoomOut);
    
    QAction* fitAction = mainToolbar->addAction(IconManager::icon("fit"), "Fit");
    fitAction->setToolTip("Fit to window (Ctrl+0)");
    connect(fitAction, &QAction::triggered, m_canvas, &CanvasWidget::fitToWindow);
    
    mainToolbar->addSeparator();
    // Tool mode group (Select / Pan / Zoom Window)
    QActionGroup* toolGroup = new QActionGroup(this);
    toolGroup->setExclusive(true);

    m_selectToolAction = mainToolbar->addAction(IconManager::icon("select"), "Select");
    m_selectToolAction->setShortcut(QKeySequence("S"));
    m_selectToolAction->setToolTip("Select tool (S)");
    m_selectToolAction->setCheckable(true);
    m_selectToolAction->setChecked(true);
    m_selectToolAction->setActionGroup(toolGroup);
    connect(m_selectToolAction, &QAction::toggled, this, [this](bool on){ if(on) m_canvas->setToolMode(CanvasWidget::ToolMode::Select); });

    m_panToolAction = mainToolbar->addAction(IconManager::icon("pan"), "Pan");
    m_panToolAction->setShortcut(QKeySequence("P"));
    m_panToolAction->setToolTip("Pan tool (P) or hold Space");
    m_panToolAction->setCheckable(true);
    m_panToolAction->setActionGroup(toolGroup);
    connect(m_panToolAction, &QAction::toggled, this, [this](bool on){ if(on) m_canvas->setToolMode(CanvasWidget::ToolMode::Pan); });

    m_zoomWindowToolAction = mainToolbar->addAction(IconManager::icon("zoom-window"), "Zoom Window");
    m_zoomWindowToolAction->setShortcut(QKeySequence("W"));
    m_zoomWindowToolAction->setToolTip("Zoom window (W). ESC to cancel");
    m_zoomWindowToolAction->setCheckable(true);
    m_zoomWindowToolAction->setActionGroup(toolGroup);
    connect(m_zoomWindowToolAction, &QAction::toggled, this, [this](bool on){ if(on) m_canvas->setToolMode(CanvasWidget::ToolMode::ZoomWindow); });

    // Drawing tools
    QAction* drawLineAction = mainToolbar->addAction(IconManager::icon("line"), "Draw Line");
    drawLineAction->setCheckable(true);
    drawLineAction->setActionGroup(toolGroup);
    drawLineAction->setToolTip("Draw lines with OSNAP/ORTHO/SNAP support");
    drawLineAction->setStatusTip("Draw line segments with snapping and ortho constraints");
    connect(drawLineAction, &QAction::toggled, this, [this](bool on){ if(on) m_canvas->setToolMode(CanvasWidget::ToolMode::DrawLine); });

    QAction* drawPolyAction = mainToolbar->addAction(IconManager::icon("polygon"), "Draw Polygon");
    drawPolyAction->setCheckable(true);
    drawPolyAction->setActionGroup(toolGroup);
    drawPolyAction->setToolTip("Draw polygons; right-click or double-click to finish");
    drawPolyAction->setStatusTip("Draw polygons with snapping; right-click or double-click to finish");
    connect(drawPolyAction, &QAction::toggled, this, [this](bool on){ if(on) m_canvas->setToolMode(CanvasWidget::ToolMode::DrawPolygon); });

    // Crosshair toggle button
    m_crosshairToggleAction = mainToolbar->addAction(IconManager::icon("crosshair"), "Crosshair");
    m_crosshairToggleAction->setToolTip("Toggle crosshair");
    m_crosshairToggleAction->setCheckable(true);
    m_crosshairToggleAction->setChecked(true);
    connect(m_crosshairToggleAction, &QAction::toggled, m_canvas, &CanvasWidget::setShowCrosshair);

    // Join (Polar) toolbar button
    mainToolbar->addSeparator();
    QAction* joinToolbarAct = mainToolbar->addAction(IconManager::icon("join"), "Join (Polar)");
    joinToolbarAct->setToolTip("Join (polar) between two coordinates");
    joinToolbarAct->setStatusTip("Compute join between two coordinates");
    connect(joinToolbarAct, &QAction::triggered, this, &MainWindow::showJoinPolar);
    QAction* polarToolbarAct = mainToolbar->addAction(IconManager::icon("polar"), "Polar Input");
    polarToolbarAct->setToolTip("Add coordinate using polar data");
    polarToolbarAct->setStatusTip("Add coordinate using polar distance/azimuth");
    connect(polarToolbarAct, &QAction::triggered, this, &MainWindow::showPolarInput);

    // Layer selector
    mainToolbar->addSeparator();
    mainToolbar->addWidget(new QLabel("Layer:"));
    m_layerCombo = new QComboBox(this);
    mainToolbar->addWidget(m_layerCombo);
    refreshLayerCombo();
    connect(m_layerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onLayerComboChanged);
}
void MainWindow::setupConnections()
{
    // Canvas signals
    connect(m_canvas, &CanvasWidget::mouseWorldPosition, this, &MainWindow::updateCoordinates);
    connect(m_canvas, &CanvasWidget::canvasClicked, this, &MainWindow::handleCanvasClick);
    connect(m_canvas, &CanvasWidget::zoomChanged, this, &MainWindow::onZoomChanged);
    connect(m_canvas, &CanvasWidget::drawingDistanceChanged, this, &MainWindow::onDrawingDistanceChanged);
    
    // Point manager signals
    connect(m_pointManager, &PointManager::pointAdded, this, &MainWindow::updatePointsTable);
    connect(m_pointManager, &PointManager::pointRemoved, this, &MainWindow::updatePointsTable);
    connect(m_pointManager, &PointManager::pointsCleared, this, &MainWindow::updatePointsTable);
    
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

void MainWindow::executeCommand()
{
    QString command = m_commandInput->text();
    if (command.isEmpty()) return;
    
    // Show command in output
    m_commandOutput->append(QString("> %1").arg(command));
    
    // Process command
    QString result = m_commandProcessor->processCommand(command);
    
    // Show result
    if (!result.isEmpty()) {
        m_commandOutput->append(result);
    }
    
    // Clear input
    m_commandInput->clear();
    
    // Update UI
    updatePointsTable();
    updateStatus();
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
    }
}

void MainWindow::updatePointsTable()
{
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
        m_pointsTable->setItem(row, 0, new QTableWidgetItem(point.name));
        if (AppSettings::gaussMode()) {
            m_pointsTable->setItem(row, 1, new QTableWidgetItem(QString::number(point.y, 'f', 3)));
            m_pointsTable->setItem(row, 2, new QTableWidgetItem(QString::number(point.x, 'f', 3)));
        } else {
            m_pointsTable->setItem(row, 1, new QTableWidgetItem(QString::number(point.x, 'f', 3)));
            m_pointsTable->setItem(row, 2, new QTableWidgetItem(QString::number(point.y, 'f', 3)));
        }
        m_pointsTable->setItem(row, 3, new QTableWidgetItem(QString::number(point.z, 'f', 3)));

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
    
    updateStatus();
}

void MainWindow::updateCoordinates(const QPointF& worldPos)
{
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
    m_pointCountLabel->setText(QString("Coordinates: %1").arg(m_pointManager->getAllPoints().size()));
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
    QMessageBox::about(this, "About SiteSurveyor",
        "<h2>SiteSurveyor</h2>"
        "<p>Professional Geomatics Software</p>"
        "<p>Version 1.0.0</p>"
        "<p>A comprehensive surveying and spatial data management application "
        "for professional surveyors and GIS professionals.</p>"
        "<p> 2024 - Built with Qt6 and C++</p>");
}

void MainWindow::showSettings()
{
    if (!m_settingsDialog) {
        m_settingsDialog = new SettingsDialog(m_canvas, this);
    }
    connect(m_settingsDialog, &SettingsDialog::settingsApplied, this, [this]() {
        updatePointsTable();
        updateStatus();
        updateCoordinates(QPointF(0, 0));
    }, Qt::UniqueConnection);
    m_settingsDialog->reload();
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
}

void MainWindow::showJoinPolar()
{
    if (!m_joinDialog) {
        m_joinDialog = new JoinPolarDialog(m_pointManager, m_canvas, this);
        // Keep dialog lists fresh when points change
        connect(m_pointManager, &PointManager::pointAdded, m_joinDialog, &JoinPolarDialog::reload);
        connect(m_pointManager, &PointManager::pointRemoved, m_joinDialog, &JoinPolarDialog::reload);
        connect(m_pointManager, &PointManager::pointsCleared, m_joinDialog, &JoinPolarDialog::reload);
    }
    m_joinDialog->reload();
    m_joinDialog->show();
    m_joinDialog->raise();
    m_joinDialog->activateWindow();
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
    if (m_measureLabel) {
        m_measureLabel->setText(QString("Len: %1 m").arg(meters, 0, 'f', 3));
    }
}

void MainWindow::updateLayerStatusText()
{
    if (!m_layerStatusLabel || !m_layerManager) return;
    const QString name = m_layerManager->currentLayer();
    m_layerStatusLabel->setText(QString("Layer: %1").arg(name.isEmpty() ? QStringLiteral("0") : name));
}