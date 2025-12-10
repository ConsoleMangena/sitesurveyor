#include "app/mainwindow.h"
#include "app/settingsdialog.h"
#include "canvas/canvaswidget.h"
#include "dxf/dxfreader.h"
#include "gdal/gdalreader.h"
#include "gdal/gdalgeosloader.h"
#include "gdal/geosbridge.h"
#include "gama/gamaexporter.h"
#include "gama/gamarunner.h"
#include "tools/levellingdialog.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QStatusBar>
#include <QLabel>
#include <QMessageBox>
#include <QApplication>
#include <QDockWidget>
#include <QListWidget>
#include <QTreeWidget>
#include <QInputDialog>
#include <QColorDialog>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFileInfo>
#include <QDir>
#include <QToolBar>
#include <QSettings>
#include <QTextBrowser>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("SiteSurveyor");
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
    
    // Setup tools panel
    setupToolsPanel();
    
    // Connect signals
    connect(m_canvas, &CanvasWidget::mouseWorldPosition, this, &MainWindow::updateCoordinates);
    connect(m_canvas, &CanvasWidget::zoomChanged, this, &MainWindow::updateZoom);
    connect(m_canvas, &CanvasWidget::layersChanged, this, &MainWindow::updateLayerPanel);
    connect(m_canvas, &CanvasWidget::statusMessage, this, [this](const QString& msg) {
        statusBar()->showMessage(msg, 5000);
    });
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupMenus()
{
    // File menu
    QMenu* fileMenu = menuBar()->addMenu("&File");
    
    QAction* newAction = fileMenu->addAction("&New Project");
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, [this]() {
        if (QMessageBox::question(this, "New Project", 
                "Clear current project and start new?") == QMessageBox::Yes) {
            m_canvas->clearAll();
            m_canvas->setProjectFilePath("");
            setWindowTitle("SiteSurveyor - Untitled");
            updateLayerPanel();
        }
    });
    
    QAction* openAction = fileMenu->addAction("&Open Project...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this,
            "Open Project", QString(),
            "SiteSurveyor Project (*.ssp);;All Files (*)");
        if (!filePath.isEmpty()) {
            if (m_canvas->loadProject(filePath)) {
                setWindowTitle(QString("SiteSurveyor - %1").arg(QFileInfo(filePath).fileName()));
                addToRecentProjects(filePath);
                updateLayerPanel();
            } else {
                QMessageBox::warning(this, "Error", "Failed to load project file.");
            }
        }
    });
    
    // Recent Projects submenu
    m_recentMenu = fileMenu->addMenu("Recent &Projects");
    updateRecentProjectsMenu();
    
    QAction* saveAction = fileMenu->addAction("&Save Project");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, [this]() {
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
                setWindowTitle(QString("SiteSurveyor - %1").arg(QFileInfo(filePath).fileName()));
                statusBar()->showMessage("Project saved.", 3000);
            } else {
                QMessageBox::warning(this, "Error", "Failed to save project file.");
            }
        }
    });
    
    QAction* saveAsAction = fileMenu->addAction("Save Project &As...");
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
        SettingsDialog dialog(m_canvas, this);
        dialog.exec();
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
    
    // Tools menu
    QMenu* toolsMenu = menuBar()->addMenu("&Tools");
    
    m_snapAction = toolsMenu->addAction("Enable &Snapping");
    m_snapAction->setShortcut(QKeySequence("S"));
    m_snapAction->setCheckable(true);
    m_snapAction->setChecked(false);  // Snapping disabled by default (can be slow)
    connect(m_snapAction, &QAction::toggled, this, &MainWindow::toggleSnapping);
    
    toolsMenu->addSeparator();
    
    QAction* offsetAction = toolsMenu->addAction("&Offset Polyline...");
    offsetAction->setShortcut(QKeySequence("O"));
    connect(offsetAction, &QAction::triggered, this, &MainWindow::offsetPolyline);
    
    QAction* partitionAction = toolsMenu->addAction("&Partition to Offset...");
    partitionAction->setShortcut(QKeySequence("P"));
    connect(partitionAction, &QAction::triggered, this, [this]() {
        if (m_canvas) {
            m_canvas->projectPartitionToOffset("PART");
        }
    });
    
    toolsMenu->addSeparator();
    
    // Polyline Edit submenu
    QMenu* polyEditMenu = toolsMenu->addMenu("Polyline &Edit");
    
    QAction* selectAction = polyEditMenu->addAction("&Select Mode");
    selectAction->setShortcut(QKeySequence("V"));
    connect(selectAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startSelectMode();
    });
    
    polyEditMenu->addSeparator();
    
    QAction* explodeAction = polyEditMenu->addAction("&Explode");
    explodeAction->setShortcut(QKeySequence("E"));
    connect(explodeAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->explodeSelectedPolyline();
    });
    
    QAction* splitAction = polyEditMenu->addAction("Sp&lit at Point");
    splitAction->setShortcut(QKeySequence("X"));
    connect(splitAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startSplitMode();
    });
    
    QAction* joinAction = polyEditMenu->addAction("&Join Selected");
    joinAction->setShortcut(QKeySequence("J"));
    connect(joinAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->joinPolylines();
    });
    
    polyEditMenu->addSeparator();
    
    QAction* closeAction = polyEditMenu->addAction("&Close/Open");
    closeAction->setShortcut(QKeySequence("Ctrl+Shift+C"));
    connect(closeAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->closeSelectedPolyline();
    });
    
    QAction* reverseAction = polyEditMenu->addAction("&Reverse Direction");
    reverseAction->setShortcut(QKeySequence("R"));
    connect(reverseAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->reverseSelectedPolyline();
    });
    
    QAction* deletePolyAction = polyEditMenu->addAction("&Delete");
    deletePolyAction->setShortcut(QKeySequence::Delete);
    connect(deletePolyAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->deleteSelectedPolyline();
    });
    
    QAction* panAction = toolsMenu->addAction("&Pan Mode");
    panAction->setShortcut(QKeySequence("H"));
    panAction->setCheckable(true);
    panAction->setChecked(false);
    connect(panAction, &QAction::toggled, this, [this](bool checked) {
        if (m_canvas) {
            m_canvas->setPanMode(checked);
        }
    });
    
    toolsMenu->addSeparator();
    
    // Station setup submenu
    QMenu* stationMenu = toolsMenu->addMenu("&Station Setup");
    
    QAction* setStationAction = stationMenu->addAction("Set &Station Point");
    setStationAction->setShortcut(QKeySequence("1"));
    connect(setStationAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startSetStationMode();
    });
    
    QAction* setBacksightAction = stationMenu->addAction("Set &Backsight Point");
    setBacksightAction->setShortcut(QKeySequence("2"));
    connect(setBacksightAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startSetBacksightMode();
    });
    
    stationMenu->addSeparator();
    
    QAction* clearStationAction = stationMenu->addAction("&Clear Station");
    connect(clearStationAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->clearStation();
    });
    
    stationMenu->addSeparator();
    
    QAction* stakeoutAction = stationMenu->addAction("Sta&keout from Station");
    stakeoutAction->setShortcut(QKeySequence("K"));
    connect(stakeoutAction, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startStakeoutMode();
    });
    
    toolsMenu->addSeparator();
    
    // Network Adjustment (GAMA)
    QAction* gamaAction = toolsMenu->addAction("Adjust &Network (GAMA)...");
    gamaAction->setShortcut(QKeySequence("Ctrl+J"));
    connect(gamaAction, &QAction::triggered, this, &MainWindow::runGamaAdjustment);
    
    // Levelling Tool
    QAction* levelAction = toolsMenu->addAction("&Levelling Tool...");
    levelAction->setShortcut(QKeySequence("Ctrl+L"));
    connect(levelAction, &QAction::triggered, this, [this]() {
        LevellingDialog dialog(m_canvas, this);
        dialog.exec();
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
            "<p>Version 1.07</p>"
            "<p>Developed by <b>Eineva Incorporated</b></p>");
    });
}

void MainWindow::setupStatusBar()
{
    m_coordLabel = new QLabel("X: 0.000  Y: 0.000");
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
}

void MainWindow::updateLayerPanel()
{
    if (!m_layerList) return;
    
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
        
        // Gray out locked layers
        if (layer.locked) {
            item->setForeground(Qt::gray);
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
    
    // Update properties when selection changes
    connect(m_canvas, &CanvasWidget::selectionChanged, this, [this](int) {
        updatePropertiesPanel();
    });
}

void MainWindow::setupToolsPanel()
{
    m_toolsDock = new QDockWidget("Quick Tools", this);
    m_toolsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    
    QWidget* dockContents = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(dockContents);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    
    // Tool buttons with icons
    auto addToolButton = [&](const QString& text, const QString& tooltip, auto slot) {
        QPushButton* btn = new QPushButton(text);
        btn->setToolTip(tooltip);
        btn->setMinimumHeight(32);
        connect(btn, &QPushButton::clicked, this, slot);
        layout->addWidget(btn);
        return btn;
    };
    
    addToolButton("Select (V)", "Select polylines", [this]() { if (m_canvas) m_canvas->startSelectMode(); });
    addToolButton("Offset (O)", "Offset polyline", [this]() { offsetPolyline(); });
    addToolButton("Split (X)", "Split polyline at point", [this]() { if (m_canvas) m_canvas->startSplitMode(); });
    addToolButton("Join (J)", "Join selected polylines", [this]() { if (m_canvas) m_canvas->joinPolylines(); });
    addToolButton("Explode (E)", "Explode polyline", [this]() { if (m_canvas) m_canvas->explodeSelectedPolyline(); });
    
    layout->addStretch();
    
    // Station section
    QLabel* stationLabel = new QLabel("Station:");
    stationLabel->setStyleSheet("font-weight: bold; margin-top: 10px;");
    layout->addWidget(stationLabel);
    
    addToolButton("Set Station (1)", "Set instrument station", [this]() { if (m_canvas) m_canvas->startSetStationMode(); });
    addToolButton("Stakeout (K)", "Start stakeout mode", [this]() { if (m_canvas) m_canvas->startStakeoutMode(); });
    
    layout->addStretch();
    
    m_toolsDock->setWidget(dockContents);
    addDockWidget(Qt::LeftDockWidgetArea, m_toolsDock);
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

void MainWindow::updateCoordinates(const QPointF& pos)
{
    m_coordLabel->setText(QString("X: %1  Y: %2")
        .arg(pos.x(), 0, 'f', 3)
        .arg(pos.y(), 0, 'f', 3));
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
    
    // Check if GAMA is available
    GamaRunner runner;
    if (!runner.isAvailable()) {
        QMessageBox::warning(this, "GNU GAMA Not Found",
            "The gama-local executable was not found.\n\n"
            "Please install GNU GAMA:\n"
            "  wget https://ftp.gnu.org/gnu/gama/gama-2.28.tar.gz\n"
            "  tar -xzf gama-2.28.tar.gz\n"
            "  cd gama-2.28 && ./configure && make && sudo make install\n\n"
            "Or set the path in Settings.");
        return;
    }
    
    // Get pegs from canvas to create network
    const auto& pegs = m_canvas->pegs();
    if (pegs.size() < 2) {
        QMessageBox::information(this, "Insufficient Points",
            "At least 2 peg points are required for network adjustment.");
        return;
    }
    
    // Create GAMA network from pegs
    GamaExporter exporter;
    GamaNetwork network;
    network.description = "SiteSurveyor Network Adjustment";
    
    // First peg is fixed (control point)
    bool first = true;
    for (const auto& peg : pegs) {
        GamaPoint pt;
        pt.id = peg.name;
        pt.x = peg.position.x();
        pt.y = peg.position.y();
        
        if (first) {
            pt.fixX = true;
            pt.fixY = true;
            pt.adjust = false;
            first = false;
        } else {
            pt.adjust = true;
        }
        
        network.points.append(pt);
    }
    
    // Create distance observations between consecutive pegs
    for (int i = 0; i < pegs.size() - 1; ++i) {
        GamaDistance dist;
        dist.from = pegs[i].name;
        dist.to = pegs[i + 1].name;
        
        // Calculate distance
        QPointF diff = pegs[i + 1].position - pegs[i].position;
        dist.value = qSqrt(diff.x() * diff.x() + diff.y() * diff.y());
        dist.stddev = 0.002; // 2mm precision
        
        network.distances.append(dist);
    }
    
    exporter.setNetwork(network);
    
    // Save to temp file  
    QString tempPath = QDir::temp().filePath("sitesurveyor_gama.xml");
    if (!exporter.exportToXml(tempPath)) {
        QMessageBox::warning(this, "Export Failed", "Failed to export network to XML.");
        return;
    }
    
    // Run adjustment
    statusBar()->showMessage("Running GAMA adjustment...", 0);
    QApplication::processEvents();
    
    GamaAdjustmentResults results = runner.runAdjustment(tempPath);
    
    if (!results.success) {
        QMessageBox::warning(this, "Adjustment Failed", 
            QString("GAMA adjustment failed:\n%1").arg(results.errorMessage));
        statusBar()->showMessage("Adjustment failed.", 3000);
        return;
    }
    
    // Show results
    QString resultText = QString(
        "Network Adjustment Results\n"
        "===========================\n\n"
        "Degrees of Freedom: %1\n"
        "Sigma0 (m0): %2\n"
        "Chi-Square Test: %3\n\n"
        "Adjusted Coordinates:\n")
        .arg(results.degreesOfFreedom)
        .arg(results.sigma0, 0, 'f', 4)
        .arg(results.chiSquarePassed ? "PASSED" : "FAILED");
    
    for (const auto& pt : results.adjustedPoints) {
        resultText += QString("  %1: X=%2, Y=%3\n")
            .arg(pt.id)
            .arg(pt.x, 0, 'f', 4)
            .arg(pt.y, 0, 'f', 4);
    }
    
    QMessageBox::information(this, "GAMA Adjustment Results", resultText);
    statusBar()->showMessage("Adjustment complete.", 3000);
}

void MainWindow::setupToolbar()
{
    m_toolbar = addToolBar("Main Toolbar");
    m_toolbar->setMovable(false);
    m_toolbar->setIconSize(QSize(16, 16));
    
    // New Project
    QAction* newAct = m_toolbar->addAction(QIcon(":/icons/file-plus.svg"), "New");
    newAct->setToolTip("New Project (Ctrl+N)");
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
    QAction* openAct = m_toolbar->addAction(QIcon(":/icons/folder-open.svg"), "Open");
    openAct->setToolTip("Open Project (Ctrl+O)");
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
    QAction* saveAct = m_toolbar->addAction(QIcon(":/icons/save.svg"), "Save");
    saveAct->setToolTip("Save Project (Ctrl+S)");
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
    QAction* importAct = m_toolbar->addAction(QIcon(":/icons/layers.svg"), "Import");
    importAct->setToolTip("Import DXF (Ctrl+D)");
    connect(importAct, &QAction::triggered, this, &MainWindow::importDXF);
    
    m_toolbar->addSeparator();
    
    // Zoom In
    QAction* zoomInAct = m_toolbar->addAction(QIcon(":/icons/zoom-in.svg"), "");
    zoomInAct->setToolTip("Zoom In (+)");
    connect(zoomInAct, &QAction::triggered, m_canvas, &CanvasWidget::zoomIn);
    
    // Zoom Out
    QAction* zoomOutAct = m_toolbar->addAction(QIcon(":/icons/zoom-out.svg"), "");
    zoomOutAct->setToolTip("Zoom Out (-)");
    connect(zoomOutAct, &QAction::triggered, m_canvas, &CanvasWidget::zoomOut);
    
    // Fit to Window
    QAction* fitAct = m_toolbar->addAction(QIcon(":/icons/zoom-fit.svg"), "");
    fitAct->setToolTip("Fit to Window (Ctrl+0)");
    connect(fitAct, &QAction::triggered, m_canvas, &CanvasWidget::fitToWindow);
    
    m_toolbar->addSeparator();
    
    // Select Mode
    QAction* selectAct = m_toolbar->addAction(QIcon(":/icons/select.svg"), "");
    selectAct->setToolTip("Select Mode (V)");
    connect(selectAct, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startSelectMode();
    });
    
    // Offset Tool
    QAction* offsetAct = m_toolbar->addAction(QIcon(":/icons/offset.svg"), "");
    offsetAct->setToolTip("Offset Polyline (O)");
    connect(offsetAct, &QAction::triggered, this, &MainWindow::offsetPolyline);
    
    // Split Tool
    QAction* splitAct = m_toolbar->addAction(QIcon(":/icons/split.svg"), "");
    splitAct->setToolTip("Split Polyline (X)");
    connect(splitAct, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startSplitMode();
    });
    
    // Join Tool
    QAction* joinAct = m_toolbar->addAction(QIcon(":/icons/join.svg"), "");
    joinAct->setToolTip("Join Selected (J)");
    connect(joinAct, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->joinPolylines();
    });
    
    m_toolbar->addSeparator();
    
    // Station Setup
    QAction* stationAct = m_toolbar->addAction(QIcon(":/icons/station.svg"), "");
    stationAct->setToolTip("Set Station (1)");
    connect(stationAct, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startSetStationMode();
    });
    
    // Stakeout
    QAction* stakeoutAct = m_toolbar->addAction(QIcon(":/icons/stakeout.svg"), "");
    stakeoutAct->setToolTip("Stakeout Mode (K)");
    connect(stakeoutAct, &QAction::triggered, this, [this]() {
        if (m_canvas) m_canvas->startStakeoutMode();
    });
    
    m_toolbar->addSeparator();
    
    // Snap Toggle
    m_snapAction = m_toolbar->addAction(QIcon(":/icons/snap.svg"), "");
    m_snapAction->setCheckable(true);
    m_snapAction->setToolTip("Toggle Snapping (S)");
    connect(m_snapAction, &QAction::toggled, this, &MainWindow::toggleSnapping);
    
    // Grid Toggle
    QAction* gridAct = m_toolbar->addAction(QIcon(":/icons/grid.svg"), "");
    gridAct->setCheckable(true);
    gridAct->setChecked(true);
    gridAct->setToolTip("Toggle Grid (G)");
    connect(gridAct, &QAction::toggled, this, [this](bool enabled) {
        if (m_canvas) m_canvas->setShowGrid(enabled);
    });
    
    m_toolbar->addSeparator();
    
    // Settings
    QAction* settingsAct = m_toolbar->addAction(QIcon(":/icons/settings.svg"), "");
    settingsAct->setToolTip("Settings (Ctrl+,)");
    connect(settingsAct, &QAction::triggered, this, [this]() {
        SettingsDialog dialog(m_canvas, this);
        dialog.exec();
    });
}

void MainWindow::showKeyboardShortcuts()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Keyboard Shortcuts");
    dialog.resize(450, 500);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QTextBrowser* browser = new QTextBrowser();
    browser->setHtml(R"(
        <h2>Keyboard Shortcuts</h2>
        
        <h3>File</h3>
        <table>
        <tr><td><b>Ctrl+N</b></td><td>New Project</td></tr>
        <tr><td><b>Ctrl+O</b></td><td>Open Project</td></tr>
        <tr><td><b>Ctrl+S</b></td><td>Save Project</td></tr>
        <tr><td><b>Ctrl+Shift+S</b></td><td>Save As</td></tr>
        <tr><td><b>Ctrl+D</b></td><td>Import DXF</td></tr>
        <tr><td><b>Ctrl+G</b></td><td>Import GIS Data</td></tr>
        </table>
        
        <h3>View</h3>
        <table>
        <tr><td><b>+ / =</b></td><td>Zoom In</td></tr>
        <tr><td><b>-</b></td><td>Zoom Out</td></tr>
        <tr><td><b>Ctrl+0</b></td><td>Fit to Window</td></tr>
        <tr><td><b>Mouse Wheel</b></td><td>Zoom</td></tr>
        <tr><td><b>Middle Click + Drag</b></td><td>Pan</td></tr>
        </table>
        
        <h3>Tools</h3>
        <table>
        <tr><td><b>S</b></td><td>Toggle Snapping</td></tr>
        <tr><td><b>O</b></td><td>Offset Polyline</td></tr>
        <tr><td><b>P</b></td><td>Partition to Offset</td></tr>
        <tr><td><b>H</b></td><td>Pan Mode</td></tr>
        </table>
        
        <h3>Station Setup</h3>
        <table>
        <tr><td><b>1</b></td><td>Set Station Point</td></tr>
        <tr><td><b>2</b></td><td>Set Backsight</td></tr>
        <tr><td><b>K</b></td><td>Stakeout Mode</td></tr>
        </table>
        
        <h3>Edit</h3>
        <table>
        <tr><td><b>Ctrl+Z</b></td><td>Undo</td></tr>
        <tr><td><b>Ctrl+Y</b></td><td>Redo</td></tr>
        <tr><td><b>Ctrl+,</b></td><td>Settings</td></tr>
        <tr><td><b>Ctrl+J</b></td><td>GAMA Adjustment</td></tr>
        </table>
        
        <h3>General</h3>
        <table>
        <tr><td><b>?</b></td><td>Show This Help</td></tr>
        <tr><td><b>Escape</b></td><td>Cancel Current Tool</td></tr>
        </table>
    )");
    
    layout->addWidget(browser);
    
    QPushButton* closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeBtn);
    
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
