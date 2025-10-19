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
#include "licensedialog.h"
#include "welcomewidget.h"
#include "surveycalculator.h"
#include "traversedialog.h"
#include "projectplanpanel.h"
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
#include <QFile>
#include <QTextStream>
#include <QEvent>
#include <QSignalBlocker>

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
    m_licenseDialog = nullptr;
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
    
    setupUI();
    // Apply last-used theme (default is light when unset)
    toggleDarkMode(AppSettings::darkMode());
    setupMenus();
    setupToolbar();
    // Apply Engineering Surveying preset (layers, units) if needed
    applyEngineeringPresetIfNeeded();
    setupConnections();
    // Determine initial UI state based on license
    updateLicenseStateUI();
    updatePanelToggleStates();
    
    updateStatus();
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
        if (m_layersDock) resizeDocks({m_layersDock}, {70}, Qt::Horizontal);
        if (m_pointsDock) resizeDocks({m_pointsDock}, {360}, Qt::Horizontal);
        if (m_commandDock) resizeDocks({m_commandDock}, {200}, Qt::Vertical);
    }
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
        // Force white text across widgets; allow specific widgets to override if needed
        style += "\n* { color: #ffffff; }\nQGroupBox::title { color: #ffffff; }\nQMenu::item { color: #ffffff; }\nQToolBar { color: #ffffff; }\n";
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
        IconManager::setMonochrome(false);
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
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    // Detect user-initiated close on right docks so we can close both in sync
    if ((obj == m_layersDock || obj == m_propertiesDock) && event->type() == QEvent::Close) {
        m_rightDockClosingByUser = true;
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
    setupLayersDock();     // Layers on right
    setupPropertiesDock(); // Properties on right (tabbed with Layers)
    setupPointsDock();     // Coordinates on left (wider)
    setupProjectPlanDock(); // Project planning panel (left, tabbed with coordinates)
    
    // Initial dock arrangement
    // Left side (now Coordinates): Right side (Layers/Properties tabified)
    if (m_layersDock && m_propertiesDock) {
        // Set minimum sizes before tabifying (narrower right sidebar)
        m_layersDock->setMinimumWidth(60);
        m_propertiesDock->setMinimumWidth(60);
        
        // Tabify the dock widgets
        tabifyDockWidget(m_layersDock, m_propertiesDock);
        m_layersDock->raise();  // Show layers tab by default
    }
    
    // Set initial widths: left (Coordinates) wide, right (Layers/Properties) very narrow
    if (m_layersDock) resizeDocks({m_layersDock}, {70}, Qt::Horizontal);
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
    const bool licensed = AppSettings::hasLicense();

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
    if (m_propertiesDock) m_propertiesDock->setVisible(licensed);
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
    // Disable all toolbuttons explicitly (status bar or others)
    const auto buttons = findChildren<QToolButton*>();
    for (QToolButton* b : buttons) {
        if (!b) continue;
        b->setEnabled(licensed);
    }

    // When unlicensed, re-enable only License entry (and optionally Exit/About)
    if (!licensed) {
        if (m_leftPanelButton) m_leftPanelButton->setVisible(false);
        if (m_rightPanelButton) m_rightPanelButton->setVisible(false);

        if (m_settingsMenu && m_settingsMenu->menuAction()) m_settingsMenu->menuAction()->setEnabled(true);
        if (m_licenseAction) m_licenseAction->setEnabled(true);
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

void MainWindow::setupLayersDock()
{
    m_layersDock = new QDockWidget("Layers", this);
    m_layersDock->setObjectName("LayersDock");
    m_layersDock->setAllowedAreas(Qt::RightDockWidgetArea);
    m_layersDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    m_layersDock->setMinimumWidth(80);
    m_layerPanel = new LayerPanel(this);
    m_layerPanel->setLayerManager(m_layerManager);
    // Keep panel in sync on layer changes
    connect(m_layerManager, &LayerManager::layersChanged, m_layerPanel, &LayerPanel::reload);
    connect(m_layerPanel, &LayerPanel::requestSetCurrent, this, [this](const QString& name){
        if (m_layerManager) m_layerManager->setCurrentLayer(name);
    });
    m_layersDock->setWidget(m_layerPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_layersDock);
    // Keep right-side docks synced (close one -> close both)
    connect(m_layersDock, &QDockWidget::visibilityChanged, this, &MainWindow::onRightDockVisibilityChanged);
    m_layersDock->installEventFilter(this);
}

void MainWindow::setupPropertiesDock()
{
    // Properties panel on right
    m_propertiesDock = new QDockWidget("Properties", this);
    m_propertiesDock->setObjectName("PropertiesDock");
    m_propertiesDock->setAllowedAreas(Qt::RightDockWidgetArea);
    m_propertiesDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    m_propertiesDock->setMinimumWidth(80);
    m_propertiesPanel = new PropertiesPanel(this);
    m_propertiesPanel->setLayerManager(m_layerManager);
    m_propertiesPanel->setCanvas(m_canvas);
    m_propertiesDock->setWidget(m_propertiesPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);
    connect(m_propertiesDock, &QDockWidget::visibilityChanged, this, &MainWindow::onRightDockVisibilityChanged);
    m_propertiesDock->installEventFilter(this);
}

void MainWindow::setupMenus()
{
    // File Menu
    QMenu* fileMenu = menuBar()->addMenu("&File");
    m_fileMenu = fileMenu;
    
    QAction* newProject = fileMenu->addAction("&New Project");
    newProject->setIcon(IconManager::icon("file-plus"));
    newProject->setShortcut(QKeySequence::New);
    connect(newProject, &QAction::triggered, this, &MainWindow::newProject);
    m_newProjectAction = newProject;
    
    QAction* openProject = fileMenu->addAction("&Open Project...");
    openProject->setIcon(IconManager::icon("folder-open"));
    openProject->setShortcut(QKeySequence::Open);
    connect(openProject, &QAction::triggered, this, &MainWindow::openProject);
    m_openProjectAction = openProject;
    
    QAction* saveProject = fileMenu->addAction("&Save Project");
    saveProject->setIcon(IconManager::icon("device-floppy"));
    saveProject->setShortcut(QKeySequence::Save);
    connect(saveProject, &QAction::triggered, this, &MainWindow::saveProject);
    m_saveProjectAction = saveProject;
    
    fileMenu->addSeparator();
    
    QAction* importPoints = fileMenu->addAction("&Import Coordinates...");
    importPoints->setIcon(IconManager::icon("file-import"));
    connect(importPoints, &QAction::triggered, this, &MainWindow::importCoordinates);
    m_importPointsAction = importPoints;
    QAction* exportPoints = fileMenu->addAction("&Export Coordinates...");
    exportPoints->setIcon(IconManager::icon("file-export"));
    connect(exportPoints, &QAction::triggered, this, &MainWindow::exportCoordinates);
    m_exportPointsAction = exportPoints;
    
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
    deletePointAction->setShortcut(QKeySequence::Delete);
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
    m_toolsMenu = toolsMenu;
    
    QAction* calcDistance = toolsMenu->addAction("Calculate &Distance...");
    calcDistance->setIcon(IconManager::icon("ruler-measure"));
    connect(calcDistance, &QAction::triggered, this, &MainWindow::calculateDistance);
    m_calcDistanceAction = calcDistance;
    QAction* calcArea = toolsMenu->addAction("Calculate &Area...");
    calcArea->setIcon(IconManager::icon("square"));
    connect(calcArea, &QAction::triggered, this, &MainWindow::calculateArea);
    m_calcAreaAction = calcArea;
    QAction* calcAzimuth = toolsMenu->addAction("Calculate A&zimuth...");
    calcAzimuth->setIcon(IconManager::icon("compass"));
    connect(calcAzimuth, &QAction::triggered, this, &MainWindow::calculateAzimuth);
    m_calcAzimuthAction = calcAzimuth;
    QAction* polarInputAct = toolsMenu->addAction("&Polar Input...");
    polarInputAct->setIcon(IconManager::icon("polar"));
    connect(polarInputAct, &QAction::triggered, this, &MainWindow::showPolarInput);
    QAction* joinPolarAct = toolsMenu->addAction("&Join (Polar)...");
    joinPolarAct->setIcon(IconManager::icon("join"));
    connect(joinPolarAct, &QAction::triggered, this, &MainWindow::showJoinPolar);
    QAction* traverseAct = toolsMenu->addAction("&Traverse...");
    traverseAct->setIcon(IconManager::icon("compass"));
    connect(traverseAct, &QAction::triggered, this, &MainWindow::showTraverse);
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
    m_darkModeAction = viewMenu->addAction("&Dark Mode");
    m_darkModeAction->setIcon(IconManager::icon("moon"));
    m_darkModeAction->setCheckable(true);
    connect(m_darkModeAction, &QAction::toggled, this, &MainWindow::toggleDarkMode);
    // Reflect persisted preference
    m_darkModeAction->setChecked(AppSettings::darkMode());

    // Settings (top-level) Menu: reuse the same Preferences action
    QMenu* settingsMenu = menuBar()->addMenu("&Settings");
    m_settingsMenu = settingsMenu;
    if (m_preferencesAction) {
        settingsMenu->addAction(m_preferencesAction);
    }
    QAction* licenseAction = settingsMenu->addAction("&Enter License Key...");
    m_licenseAction = licenseAction;
    licenseAction->setIcon(IconManager::icon("key"));
    connect(licenseAction, &QAction::triggered, this, &MainWindow::showLicense);

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
    quickBar->setIconSize(QSize(16, 16));
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
    topBar->setIconSize(QSize(16, 16));

    auto addCategoryTo = [&](QToolBar* bar, const QString& name){
        QLabel* chip = new QLabel(QString(" %1 ").arg(name), this);
        chip->setStyleSheet("QLabel { border: 1px solid #999; border-radius: 3px; padding: 1px 6px; }");
        bar->addSeparator();
        bar->addWidget(chip);
        bar->addSeparator();
    };
    auto addMenuGroup = [&](QToolBar* bar, const QString& title, const QIcon& icon = QIcon()){
        QToolButton* btn = new QToolButton(this);
        btn->setText(title + " ▾");
        if (!icon.isNull()) btn->setIcon(icon);
        btn->setPopupMode(QToolButton::InstantPopup);
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        QMenu* menu = new QMenu(btn);
        btn->setMenu(menu);
        bar->addWidget(btn);
        return menu;
    };

    // Primary CAD groups: Draw | Modify | Layers | View | Aids
    // Draw menu (primary)
    QMenu* drawMenuTb = addMenuGroup(topBar, "Draw", IconManager::icon("line"));
    QAction* drawLineAction = new QAction(IconManager::icon("line"), "Line", this);
    drawLineAction->setShortcut(QKeySequence("L"));
    drawLineAction->setCheckable(true);
    drawLineAction->setToolTip("Line (L) - Draw a line segment");
    connect(drawLineAction, &QAction::toggled, this, [this](bool on){ if (on) m_canvas->setToolMode(CanvasWidget::ToolMode::DrawLine); });
    drawMenuTb->addAction(drawLineAction);
    QAction* drawPolyAction = new QAction(IconManager::icon("polygon"), "Polyline", this);
    drawPolyAction->setShortcut(QKeySequence("PL"));
    drawPolyAction->setCheckable(true);
    drawPolyAction->setToolTip("Polyline (PL) - Draw connected segments");
    connect(drawPolyAction, &QAction::toggled, this, [this](bool on){ if (on) m_canvas->setToolMode(CanvasWidget::ToolMode::DrawPolygon); });
    drawMenuTb->addAction(drawPolyAction);
    QAction* drawCircleAction = new QAction(IconManager::icon("circle"), "Circle", this);
    drawCircleAction->setCheckable(true);
    drawCircleAction->setToolTip("Circle - Click center, then radius (typed distance or D=diameter optional)");
    connect(drawCircleAction, &QAction::toggled, this, [this](bool on){ if (on) m_canvas->setToolMode(CanvasWidget::ToolMode::DrawCircle); });
    drawMenuTb->addAction(drawCircleAction);
    QAction* drawArcAction = new QAction(IconManager::icon("arc"), "Arc", this);
    drawArcAction->setCheckable(true);
    drawArcAction->setToolTip("Arc (3-point) - Click start, second point, then end");
    connect(drawArcAction, &QAction::toggled, this, [this](bool on){ if (on) m_canvas->setToolMode(CanvasWidget::ToolMode::DrawArc); });
    drawMenuTb->addAction(drawArcAction);
    QAction* drawRectAction = new QAction(IconManager::icon("rectangle"), "Rectangle", this);
    drawRectAction->setCheckable(true);
    drawRectAction->setToolTip("Rectangle - Click first corner, then opposite corner (Shift for square)");
    connect(drawRectAction, &QAction::toggled, this, [this](bool on){ if (on) m_canvas->setToolMode(CanvasWidget::ToolMode::DrawRectangle); });
    drawMenuTb->addAction(drawRectAction);
    if (QToolButton* tb = qobject_cast<QToolButton*>(drawMenuTb->parentWidget())) tb->setToolTip("Line, Polyline, Circle, Arc, Rectangle");
    // Store for pinning/exclusivity
    m_drawLineToolAction = drawLineAction;
    m_drawPolyToolAction = drawPolyAction;
    m_drawCircleToolAction = drawCircleAction;
    m_drawArcToolAction = drawArcAction;
    m_drawRectToolAction = drawRectAction;
    // Pin button for Draw group
    m_drawPinButton = new QToolButton(this);
    m_drawPinButton->setIcon(IconManager::icon("pin"));
    m_drawPinButton->setCheckable(true);
    m_drawPinButton->setToolTip("Pin Draw group inline");
    topBar->addWidget(m_drawPinButton);
    connect(m_drawPinButton, &QToolButton::toggled, this, [this](bool on){ m_drawGroupPinned = on; updatePinnedGroupsUI(); });

    // Modify menu (Lengthen, Delete)
    QMenu* modifyMenuTb = addMenuGroup(topBar, "Modify", IconManager::icon("ruler-measure"));
    QAction* lengthenAct = new QAction(IconManager::icon("ruler-measure"), "Lengthen", this);
    lengthenAct->setCheckable(true);
    lengthenAct->setToolTip("Lengthen - adjust line length by value or click");
    connect(lengthenAct, &QAction::toggled, this, [this](bool on){ if (on) m_canvas->setToolMode(CanvasWidget::ToolMode::Lengthen); });
    modifyMenuTb->addAction(lengthenAct);
    if (m_deleteSelectedAction) modifyMenuTb->addAction(m_deleteSelectedAction);
    if (QToolButton* tb = qobject_cast<QToolButton*>(modifyMenuTb->parentWidget())) tb->setToolTip("Lengthen, Delete Selected (more coming: Move, Copy, Rotate, Scale, Mirror, Trim, Extend, Offset)");

    // Layers menu (Hide/Isolate/Lock/ShowAll) + Layer Panel toggle
    QMenu* layersMenuTb = addMenuGroup(topBar, "Layers", IconManager::icon("label"));
    QAction* hideLayActTop = new QAction(IconManager::icon("eye-off"), "Hide", this);
    hideLayActTop->setToolTip("Hide Selected Layers (Ctrl+Shift+H)");
    connect(hideLayActTop, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->hideSelectedLayers(); });
    layersMenuTb->addAction(hideLayActTop);
    QAction* isoLayActTop = new QAction(IconManager::icon("filter"), "Isolate", this);
    isoLayActTop->setToolTip("Isolate Selection Layers (Ctrl+Shift+I)");
    connect(isoLayActTop, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->isolateSelectionLayers(); });
    layersMenuTb->addAction(isoLayActTop);
    QAction* lockLayActTop = new QAction(IconManager::icon("lock"), "Lock", this);
    lockLayActTop->setToolTip("Lock Selected Layers (Ctrl+Shift+L)");
    connect(lockLayActTop, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->lockSelectedLayers(); });
    layersMenuTb->addAction(lockLayActTop);
    QAction* showAllActTop = new QAction(IconManager::icon("eye"), "Show All Layers", this);
    connect(showAllActTop, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->showAllLayers(); });
    layersMenuTb->addAction(showAllActTop);
    if (QToolButton* tb = qobject_cast<QToolButton*>(layersMenuTb->parentWidget())) tb->setToolTip("Hide, Isolate, Lock, Show All");
    QAction* layerPanelAct = topBar->addAction(IconManager::icon("label"), "Layer Panel");
    layerPanelAct->setToolTip("Show/Hide Layers panel");
    connect(layerPanelAct, &QAction::triggered, this, [this](){ if (m_layersDock) m_layersDock->setVisible(!m_layersDock->isVisible()); });

    // View menu
    QMenu* viewMenuTb = addMenuGroup(topBar, "View", IconManager::icon("zoom-in"));
    m_selectToolAction = new QAction(IconManager::icon("select"), "Select", this);
    m_selectToolAction->setShortcut(QKeySequence("ESC"));
    m_selectToolAction->setCheckable(true);
    m_selectToolAction->setChecked(true);
    connect(m_selectToolAction, &QAction::toggled, this, [this](bool on){ if(on) m_canvas->setToolMode(CanvasWidget::ToolMode::Select); });
    viewMenuTb->addAction(m_selectToolAction);
    m_panToolAction = new QAction(IconManager::icon("pan"), "Pan", this);
    m_panToolAction->setShortcut(QKeySequence("P"));
    m_panToolAction->setCheckable(true);
    connect(m_panToolAction, &QAction::toggled, this, [this](bool on){ if(on) m_canvas->setToolMode(CanvasWidget::ToolMode::Pan); });
    viewMenuTb->addAction(m_panToolAction);
    m_zoomWindowToolAction = new QAction(IconManager::icon("zoom-window"), "Zoom Window", this);
    m_zoomWindowToolAction->setShortcut(QKeySequence("W"));
    m_zoomWindowToolAction->setCheckable(true);
    connect(m_zoomWindowToolAction, &QAction::toggled, this, [this](bool on){ if(on) m_canvas->setToolMode(CanvasWidget::ToolMode::ZoomWindow); });
    viewMenuTb->addAction(m_zoomWindowToolAction);
    m_lassoToolAction = new QAction(IconManager::icon("lasso"), "Lasso", this);
    m_lassoToolAction->setCheckable(true);
    connect(m_lassoToolAction, &QAction::toggled, this, [this](bool on){ if(on) m_canvas->setToolMode(CanvasWidget::ToolMode::LassoSelect); });
    viewMenuTb->addAction(m_lassoToolAction);
    if (QToolButton* tb = qobject_cast<QToolButton*>(viewMenuTb->parentWidget())) tb->setToolTip("Select, Pan, Zoom Window, Lasso, Zoom In/Out, Extents, Home");
    // Add zoom controls into View menu
    QAction* zoomInAction = new QAction(IconManager::icon("zoom-in"), "Zoom In", this);
    connect(zoomInAction, &QAction::triggered, m_canvas, &CanvasWidget::zoomIn);
    QAction* zoomOutAction = new QAction(IconManager::icon("zoom-out"), "Zoom Out", this);
    connect(zoomOutAction, &QAction::triggered, m_canvas, &CanvasWidget::zoomOut);
    QAction* fitAction = new QAction(IconManager::icon("fit"), "Extents", this);
    connect(fitAction, &QAction::triggered, m_canvas, &CanvasWidget::fitToWindow);
    QAction* homeAction = new QAction(IconManager::icon("home"), "Home", this);
    connect(homeAction, &QAction::triggered, m_canvas, &CanvasWidget::resetView);
    viewMenuTb->addSeparator();
    viewMenuTb->addAction(zoomInAction);
    viewMenuTb->addAction(zoomOutAction);
    viewMenuTb->addAction(fitAction);
    viewMenuTb->addAction(homeAction);

    // Aids menu (Snaps/Ortho/Polar/Dynamic)
    QMenu* aidsMenuTb = addMenuGroup(topBar, "Aids", IconManager::icon("osnap"));
    if (m_osnapAction) { m_osnapAction->setIcon(IconManager::icon("osnap")); aidsMenuTb->addAction(m_osnapAction); }
    if (m_snapAction)  { m_snapAction->setIcon(IconManager::icon("snap"));  aidsMenuTb->addAction(m_snapAction); }
    if (m_orthoAction) { m_orthoAction->setIcon(IconManager::icon("ortho")); aidsMenuTb->addAction(m_orthoAction); }
    if (m_polarAction) { m_polarAction->setIcon(IconManager::icon("polar")); aidsMenuTb->addAction(m_polarAction); }
    if (m_dynAction)   { m_dynAction->setIcon(IconManager::icon("desktop")); aidsMenuTb->addAction(m_dynAction); }
    if (QToolButton* tb = qobject_cast<QToolButton*>(aidsMenuTb->parentWidget())) tb->setToolTip("OSNAP, SNAP, ORTHO, POLAR, DYN");

    // Insert a separator between primary CAD groups and secondary groups
    topBar->addSeparator();

    // Secondary groups: File | Edit | Data | Prefs | Plan
    QMenu* fileMenuTb = addMenuGroup(topBar, "File", IconManager::icon("folder-open"));
    if (m_newProjectAction) fileMenuTb->addAction(m_newProjectAction);
    if (m_openProjectAction) fileMenuTb->addAction(m_openProjectAction);
    if (m_saveProjectAction) fileMenuTb->addAction(m_saveProjectAction);
    if (m_exitAction) fileMenuTb->addAction(m_exitAction);
    if (QToolButton* tb = qobject_cast<QToolButton*>(fileMenuTb->parentWidget())) tb->setToolTip("New, Open, Save, Exit");

    QMenu* editMenuTb = addMenuGroup(topBar, "Edit", IconManager::icon("arrow-back-up"));
    if (m_undoAction) editMenuTb->addAction(m_undoAction);
    if (m_redoAction) editMenuTb->addAction(m_redoAction);
    if (m_deleteSelectedAction) editMenuTb->addAction(m_deleteSelectedAction);
    if (QToolButton* tb = qobject_cast<QToolButton*>(editMenuTb->parentWidget())) tb->setToolTip("Undo, Redo, Delete Selected");

    QMenu* dataMenuTb = addMenuGroup(topBar, "Data", IconManager::icon("file-import"));
    if (m_importPointsAction) dataMenuTb->addAction(m_importPointsAction);
    if (m_exportPointsAction) dataMenuTb->addAction(m_exportPointsAction);
    if (QToolButton* tb = qobject_cast<QToolButton*>(dataMenuTb->parentWidget())) tb->setToolTip("Import Coordinates, Export Coordinates");

    QMenu* prefsMenuTb = addMenuGroup(topBar, "Prefs", IconManager::icon("settings"));
    if (m_preferencesAction) prefsMenuTb->addAction(m_preferencesAction);
    if (m_darkModeAction) prefsMenuTb->addAction(m_darkModeAction);
    if (m_resetLayoutAction) prefsMenuTb->addAction(m_resetLayoutAction);
    if (QToolButton* tb = qobject_cast<QToolButton*>(prefsMenuTb->parentWidget())) tb->setToolTip("Preferences, Dark Mode, Reset Layout");

    QMenu* planMenuTb = addMenuGroup(topBar, "Plan", IconManager::icon("label"));
    if (!m_toggleProjectPlanAction) {
        m_toggleProjectPlanAction = new QAction(IconManager::icon("label"), "Project Plan", this);
        m_toggleProjectPlanAction->setCheckable(true);
        connect(m_toggleProjectPlanAction, &QAction::triggered, this, &MainWindow::toggleProjectPlanPanel);
    }
    planMenuTb->addAction(m_toggleProjectPlanAction);
    if (QToolButton* tb = qobject_cast<QToolButton*>(planMenuTb->parentWidget())) tb->setToolTip("Show/Hide Project Plan panel");

    // Annotation group (menu)
    addCategoryTo(topBar, "Annotation");
    {
        QToolButton* annotBtn = new QToolButton(this);
        annotBtn->setText("Annot");
        annotBtn->setPopupMode(QToolButton::InstantPopup);
        QMenu* annotMenu = new QMenu(annotBtn);
        QAction* textAct = annotMenu->addAction("Text");
        QAction* dimAct = annotMenu->addAction("Dimension");
        connect(textAct, &QAction::triggered, this, [this](){ QMessageBox::information(this, "Text", "Text tool coming soon."); });
        connect(dimAct, &QAction::triggered, this, [this](){ QMessageBox::information(this, "Dimension", "Dimension tool coming soon."); });
        annotBtn->setMenu(annotMenu);
        topBar->addWidget(annotBtn);
    }

    // Blocks group (menu)
    addCategoryTo(topBar, "Blocks");
    {
        QToolButton* blocksBtn = new QToolButton(this);
        blocksBtn->setText("Blocks");
        blocksBtn->setPopupMode(QToolButton::InstantPopup);
        QMenu* blocksMenu = new QMenu(blocksBtn);
        QAction* insertBlk = blocksMenu->addAction("Insert Block...");
        QAction* createBlk = blocksMenu->addAction("Create Block from Selection");
        QAction* explodeBlk = blocksMenu->addAction("Explode");
        connect(insertBlk, &QAction::triggered, this, [this](){ QMessageBox::information(this, "Blocks", "Insert Block coming soon."); });
        connect(createBlk, &QAction::triggered, this, [this](){ QMessageBox::information(this, "Blocks", "Create Block coming soon."); });
        connect(explodeBlk, &QAction::triggered, this, [this](){ QMessageBox::information(this, "Blocks", "Explode coming soon."); });
        blocksBtn->setMenu(blocksMenu);
        topBar->addWidget(blocksBtn);
    }

    // Properties quick access
    addCategoryTo(topBar, "Properties");
    QAction* propsAct = topBar->addAction(IconManager::icon("settings"), "Properties");
    propsAct->setToolTip("Show/Hide Properties panel");
    connect(propsAct, &QAction::triggered, this, [this](){ if (m_propertiesDock) m_propertiesDock->setVisible(!m_propertiesDock->isVisible()); });

    // Utilities group (menu)
    addCategoryTo(topBar, "Utilities");
    {
        QToolButton* utilBtn = new QToolButton(this);
        utilBtn->setText("Utils");
        utilBtn->setPopupMode(QToolButton::InstantPopup);
        QMenu* utilMenu = new QMenu(utilBtn);
        QAction* idPt = utilMenu->addAction("ID Point");
        QAction* measAng = utilMenu->addAction("Measure Angle");
        QAction* listProps = utilMenu->addAction("List Properties");
        connect(idPt, &QAction::triggered, this, [this](){ QMessageBox::information(this, "Utilities", "ID Point coming soon."); });
        connect(measAng, &QAction::triggered, this, [this](){ QMessageBox::information(this, "Utilities", "Measure Angle coming soon."); });
        connect(listProps, &QAction::triggered, this, [this](){ QMessageBox::information(this, "Utilities", "List Properties coming soon."); });
        utilBtn->setMenu(utilMenu);
        topBar->addWidget(utilBtn);
    }

    // Break to second row
    addToolBarBreak(Qt::TopToolBarArea);

    // Row 2: Draw | Survey | Measure | Display
    QToolBar* bottomBar = addToolBar("TopBarRow2");
    bottomBar->setObjectName("TopBarRow2");
    bottomBar->setAllowedAreas(Qt::TopToolBarArea);
    bottomBar->setMovable(true);
    bottomBar->setFloatable(false);
    bottomBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    bottomBar->setIconSize(QSize(16, 16));
    
    // Survey group
    addCategoryTo(bottomBar, "Survey");
    QAction* polarToolbarAct = bottomBar->addAction(IconManager::icon("polar"), "Polar");
    connect(polarToolbarAct, &QAction::triggered, this, &MainWindow::showPolarInput);
    QAction* joinToolbarAct = bottomBar->addAction(IconManager::icon("join"), "Join");
    connect(joinToolbarAct, &QAction::triggered, this, &MainWindow::showJoinPolar);
    QAction* traverseToolbarAct = bottomBar->addAction(IconManager::icon("compass"), "Traverse");
    connect(traverseToolbarAct, &QAction::triggered, this, &MainWindow::showTraverse);
    QAction* addPointAction = bottomBar->addAction(IconManager::icon("add-point"), "Add Point");
    connect(addPointAction, &QAction::triggered, this, &MainWindow::showAddPointDialog);

    addCategoryTo(bottomBar, "Measure");
    if (m_calcDistanceAction) bottomBar->addAction(m_calcDistanceAction);
    if (m_calcAreaAction) bottomBar->addAction(m_calcAreaAction);
    if (m_calcAzimuthAction) bottomBar->addAction(m_calcAzimuthAction);

    addCategoryTo(bottomBar, "Display");
    QLabel* layerLabel = new QLabel(" Layer: ");
    layerLabel->setStyleSheet("font-weight: bold;");
    bottomBar->addWidget(layerLabel);
    m_layerCombo = new QComboBox(this);
    m_layerCombo->setMinimumWidth(140);
    m_layerCombo->setMaximumWidth(220);
    bottomBar->addWidget(m_layerCombo);
    refreshLayerCombo();
    connect(m_layerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onLayerComboChanged);

    QAction* toggleGrid = bottomBar->addAction(IconManager::icon("grid"), "Grid");
    toggleGrid->setCheckable(true);
    toggleGrid->setChecked(true);
    connect(toggleGrid, &QAction::toggled, m_canvas, &CanvasWidget::setShowGrid);

    QAction* toggleLabels = bottomBar->addAction(IconManager::icon("label"), "Labels");
    toggleLabels->setCheckable(true);
    toggleLabels->setChecked(true);
    connect(toggleLabels, &QAction::toggled, m_canvas, &CanvasWidget::setShowLabels);

    m_crosshairToggleAction = bottomBar->addAction(IconManager::icon("crosshair"), "Crosshair");
    m_crosshairToggleAction->setCheckable(true);
    m_crosshairToggleAction->setChecked(true);
    connect(m_crosshairToggleAction, &QAction::toggled, m_canvas, &CanvasWidget::setShowCrosshair);

    // Selection tools
    addCategoryTo(bottomBar, "Select");
    QAction* selAllActTb = bottomBar->addAction(IconManager::icon("square"), "All");
    selAllActTb->setToolTip("Select All (Ctrl+A)");
    connect(selAllActTb, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->selectAllVisible(); });
    QAction* invSelActTb = bottomBar->addAction(IconManager::icon("clear"), "Invert");
    invSelActTb->setToolTip("Invert Selection (Ctrl+I)");
    connect(invSelActTb, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->invertSelectionVisible(); });
    QAction* selByLayerActTb = bottomBar->addAction(IconManager::icon("label"), "ByLayer");
    selByLayerActTb->setToolTip("Select by Current Layer");
    connect(selByLayerActTb, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->selectByCurrentLayer(); });
    QAction* clearSelActTb = bottomBar->addAction(IconManager::icon("eraser"), "Clear");
    clearSelActTb->setToolTip("Clear Selection (Esc)");
    connect(clearSelActTb, &QAction::triggered, this, [this](){ if (m_canvas) { m_canvas->clearSelection(); m_canvas->update(); onSelectionChanged(0,0); } });

    QAction* hideSelLayersTb = bottomBar->addAction(IconManager::icon("eye-off"), "Hide");
    hideSelLayersTb->setToolTip("Hide Selected Layers (Ctrl+Shift+H)");
    connect(hideSelLayersTb, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->hideSelectedLayers(); });
    QAction* isoSelLayersTb = bottomBar->addAction(IconManager::icon("filter"), "Isolate");
    isoSelLayersTb->setToolTip("Isolate Selection Layers (Ctrl+Shift+I)");
    connect(isoSelLayersTb, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->isolateSelectionLayers(); });
    QAction* lockSelLayersTb = bottomBar->addAction(IconManager::icon("lock"), "Lock");
    lockSelLayersTb->setToolTip("Lock Selected Layers (Ctrl+Shift+L)");
    connect(lockSelLayersTb, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->lockSelectedLayers(); });
    QAction* showAllLayersTb = bottomBar->addAction(IconManager::icon("eye"), "ShowAll");
    showAllLayersTb->setToolTip("Show All Layers (Ctrl+Shift+S)");
    connect(showAllLayersTb, &QAction::triggered, this, [this](){ if (m_canvas) m_canvas->showAllLayers(); });

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
    // Initial pin UI state
    updatePinnedGroupsUI();
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
    // Remove any existing inline Draw actions
    for (QAction* a : m_drawInlineActions) {
        if (!a) continue;
        m_topBar->removeAction(a);
    }
    m_drawInlineActions.clear();

    if (!m_drawGroupPinned) return;
    // Ensure we have an anchor to insert before (placed after the pin button)
    if (!m_drawAnchorAction) {
        // Fallback: add a separator at end as anchor
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

void MainWindow::openProject()
{
    // Alias to Import for now
    importCoordinates();
}

void MainWindow::saveProject()
{
    // Alias to Export for now
    exportCoordinates();
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
        if (m_pointManager->addPoint(name, x, y, z)) ++added;
    }
    f.close();
    updatePointsTable();
    updateStatus();
    AppSettings::addRecentFile(fileName);
    if (m_welcomeWidget) m_welcomeWidget->reload();
    if (m_commandOutput) m_commandOutput->append(QString("Imported %1 points from %2").arg(added).arg(QFileInfo(fileName).fileName()));
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
        if (m_pointManager->addPoint(name, x, y, z)) ++added;
    }
    f.close();
    updatePointsTable();
    updateStatus();
    AppSettings::addRecentFile(filePath);
    if (m_welcomeWidget) m_welcomeWidget->reload();
    if (m_commandOutput) m_commandOutput->append(QString("Imported %1 points from %2").arg(added).arg(QFileInfo(filePath).fileName()));
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

void MainWindow::calculateAzimuth()
{
    const QStringList names = m_pointManager->getPointNames();
    if (names.size() < 2) { QMessageBox::information(this, "Azimuth", "Need at least two points."); return; }
    bool ok = false;
    const QString a = QInputDialog::getItem(this, "Azimuth", "From point:", names, 0, false, &ok);
    if (!ok || a.isEmpty()) return;
    const QString b = QInputDialog::getItem(this, "Azimuth", "To point:", names, 1, false, &ok);
    if (!ok || b.isEmpty()) return;
    const Point p1 = m_pointManager->getPoint(a);
    const Point p2 = m_pointManager->getPoint(b);
    const double az = SurveyCalculator::azimuth(p1, p2);
    const bool useDecimal = AppSettings::angleFormat().compare(QStringLiteral("decimal"), Qt::CaseInsensitive) == 0;
    QString msg;
    if (useDecimal) {
        msg = QString("Azimuth %1 -> %2 = %3°").arg(a, b).arg(az, 0, 'f', 4);
    } else {
        const QString dms = SurveyCalculator::toDMS(az);
        msg = QString("Azimuth %1 -> %2 = %3").arg(a, b, dms);
    }
    QMessageBox::information(this, "Azimuth", msg);
    if (m_commandOutput) m_commandOutput->append(msg);
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
            });
        }
    }
    m_settingsDialog->reload();
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
}

void MainWindow::showLicense()
{
    if (!m_licenseDialog) {
        m_licenseDialog = new LicenseDialog(this);
        // React to license saves and discipline changes
        connect(m_licenseDialog, &LicenseDialog::licenseSaved, this, &MainWindow::onLicenseActivated);
        connect(m_licenseDialog, &LicenseDialog::disciplineChanged, this, &MainWindow::onLicenseActivated);
    }
    if (m_licenseDialog) {
        m_licenseDialog->reload();
        m_licenseDialog->show();
        m_licenseDialog->raise();
        m_licenseDialog->activateWindow();
    }
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
    }
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
        const bool imperial = AppSettings::measurementUnits().compare(QStringLiteral("imperial"), Qt::CaseInsensitive) == 0;
        const double factor = imperial ? 3.28084 : 1.0; // m->ft
        const QString unit = imperial ? QStringLiteral("ft") : QStringLiteral("m");
        m_measureLabel->setText(QString("Len: %1 %2").arg(meters * factor, 0, 'f', 3).arg(unit));
    }
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
    // Distinguish between tab switches (one dock hidden, the other visible)
    // and closing the last visible dock (both become hidden). Keep action state consistent.
    if (m_syncingRightDock) return;
    m_syncingRightDock = true;

    QDockWidget* senderDock = qobject_cast<QDockWidget*>(sender());
    QDockWidget* otherDock = nullptr;
    if (senderDock == m_layersDock) otherDock = m_propertiesDock;
    else if (senderDock == m_propertiesDock) otherDock = m_layersDock;

    const bool otherVisible = otherDock && otherDock->isVisible();

    if (!visible && !otherVisible) {
        // Closing the last visible tab: ensure both are hidden and action unchecked
        if (m_layersDock) m_layersDock->hide();
        if (m_propertiesDock) m_propertiesDock->hide();
        if (m_toggleRightPanelAction) m_toggleRightPanelAction->setChecked(false);
    } else {
        // Either a tab switch (one hidden, one visible) or a show event
        if (m_layersDock && m_propertiesDock) {
            tabifyDockWidget(m_layersDock, m_propertiesDock);
        }
        const bool anyVisible = (m_layersDock && m_layersDock->isVisible()) || (m_propertiesDock && m_propertiesDock->isVisible());
        if (m_toggleRightPanelAction) m_toggleRightPanelAction->setChecked(anyVisible);
    }

    updatePanelToggleStates();
    m_rightDockClosingByUser = false;
    m_syncingRightDock = false;
}

void MainWindow::setRightPanelsVisible(bool visible)
{
    // Explicitly control the visibility of the combined right sidebar
    if (!m_layersDock && !m_propertiesDock) return;
    if (m_syncingRightDock) return;
    m_syncingRightDock = true;

    if (m_layersDock) m_layersDock->setVisible(visible);
    if (m_propertiesDock) m_propertiesDock->setVisible(visible);

    if (visible && m_layersDock && m_propertiesDock) {
        tabifyDockWidget(m_layersDock, m_propertiesDock);
        m_layersDock->raise();
        // Ensure narrow width when shown
        m_layersDock->resize(70, m_layersDock->height());
        m_propertiesDock->resize(70, m_propertiesDock->height());
    }

    if (m_toggleRightPanelAction) m_toggleRightPanelAction->setChecked(visible);
    updatePanelToggleStates();
    m_syncingRightDock = false;
}

void MainWindow::toggleRightPanel()
{
    // Toggle visibility of right panels (Layers/Properties tabs)
    if (!m_layersDock && !m_propertiesDock) return;

    const bool rightVisible = (m_layersDock && m_layersDock->isVisible()) || (m_propertiesDock && m_propertiesDock->isVisible());

    if (rightVisible) {
        if (m_layersDock) m_layersDock->hide();
        if (m_propertiesDock) m_propertiesDock->hide();
        if (m_rightPanelButton) {
            m_rightPanelButton->setArrowType(Qt::RightArrow);
            m_rightPanelButton->setToolTip("Show right panels (Ctrl+R)");
        }
    } else {
        if (m_layersDock) m_layersDock->show();
        if (m_propertiesDock) m_propertiesDock->show();
        if (m_layersDock && m_propertiesDock) {
            // Re-tabify defensively in case user undocked them
            tabifyDockWidget(m_layersDock, m_propertiesDock);
            m_layersDock->raise();
        }
        // Make sure it's properly sized (very narrow)
        if (m_layersDock) m_layersDock->resize(70, m_layersDock->height());
        if (m_propertiesDock) m_propertiesDock->resize(70, m_propertiesDock->height());
        if (m_rightPanelButton) {
            m_rightPanelButton->setArrowType(Qt::LeftArrow);
            m_rightPanelButton->setToolTip("Hide right panels (Ctrl+R)");
        }
    }

    updatePanelToggleStates();
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
    const bool rightVisible = (m_layersDock && m_layersDock->isVisible()) || (m_propertiesDock && m_propertiesDock->isVisible());
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
