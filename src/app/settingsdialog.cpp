#include "app/settingsdialog.h"
#include "canvas/canvaswidget.h"
#include "tools/snapper.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QTabWidget>
#include <QFile>
#include <QSettings>
#include <QApplication>

SettingsDialog::SettingsDialog(CanvasWidget* canvas, QWidget* parent)
    : QDialog(parent)
    , m_canvas(canvas)
    , m_stationColor(Qt::green)
    , m_backsightColor(Qt::cyan)
    , m_pegColor(Qt::red)
{
    setWindowTitle("Settings");
    setMinimumWidth(400);
    setupUI();
    loadCurrentSettings();
}

void SettingsDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    QTabWidget* tabs = new QTabWidget();
    
    // ========== Snapping Tab ==========
    QWidget* snapTab = new QWidget();
    QVBoxLayout* snapLayout = new QVBoxLayout(snapTab);
    
    QGroupBox* snapGroup = new QGroupBox("Snap Settings");
    QFormLayout* snapForm = new QFormLayout(snapGroup);
    
    m_snapEnabled = new QCheckBox("Enable Snapping");
    snapForm->addRow(m_snapEnabled);
    
    m_snapEndpoint = new QCheckBox("Snap to Endpoints");
    snapForm->addRow(m_snapEndpoint);
    
    m_snapMidpoint = new QCheckBox("Snap to Midpoints");
    snapForm->addRow(m_snapMidpoint);
    
    m_snapEdge = new QCheckBox("Snap to Edges");
    snapForm->addRow(m_snapEdge);
    
    m_snapIntersection = new QCheckBox("Snap to Intersections (slow)");
    m_snapIntersection->setToolTip("Warning: This can be slow with many lines");
    snapForm->addRow(m_snapIntersection);
    
    m_snapTolerance = new QDoubleSpinBox();
    m_snapTolerance->setRange(1.0, 50.0);
    m_snapTolerance->setValue(10.0);
    m_snapTolerance->setSuffix(" px");
    snapForm->addRow("Snap Tolerance:", m_snapTolerance);
    
    snapLayout->addWidget(snapGroup);
    snapLayout->addStretch();
    tabs->addTab(snapTab, "Snapping");
    
    // ========== Display Tab ==========
    QWidget* displayTab = new QWidget();
    QVBoxLayout* displayLayout = new QVBoxLayout(displayTab);
    
    QGroupBox* displayGroup = new QGroupBox("Display Settings");
    QFormLayout* displayForm = new QFormLayout(displayGroup);
    
    m_showGrid = new QCheckBox("Show Grid");
    displayForm->addRow(m_showGrid);
    
    m_gridSize = new QSpinBox();
    m_gridSize->setRange(1, 100);
    m_gridSize->setValue(10);
    m_gridSize->setSuffix(" units");
    displayForm->addRow("Grid Size:", m_gridSize);
    
    m_pegMarkerSize = new QDoubleSpinBox();
    m_pegMarkerSize->setRange(0.1, 5.0);
    m_pegMarkerSize->setValue(0.5);
    m_pegMarkerSize->setSuffix(" units");
    displayForm->addRow("Peg Marker Size:", m_pegMarkerSize);
    
    displayLayout->addWidget(displayGroup);
    
    // Colors
    QGroupBox* colorGroup = new QGroupBox("Colors");
    QFormLayout* colorForm = new QFormLayout(colorGroup);
    
    m_stationColorBtn = new QPushButton();
    m_stationColorBtn->setFixedSize(60, 24);
    m_stationColorBtn->setStyleSheet(QString("background-color: %1").arg(m_stationColor.name()));
    connect(m_stationColorBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(m_stationColor, this, "Station Color");
        if (c.isValid()) {
            m_stationColor = c;
            m_stationColorBtn->setStyleSheet(QString("background-color: %1").arg(c.name()));
        }
    });
    colorForm->addRow("Station Color:", m_stationColorBtn);
    
    m_backsightColorBtn = new QPushButton();
    m_backsightColorBtn->setFixedSize(60, 24);
    m_backsightColorBtn->setStyleSheet(QString("background-color: %1").arg(m_backsightColor.name()));
    connect(m_backsightColorBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(m_backsightColor, this, "Backsight Color");
        if (c.isValid()) {
            m_backsightColor = c;
            m_backsightColorBtn->setStyleSheet(QString("background-color: %1").arg(c.name()));
        }
    });
    colorForm->addRow("Backsight Color:", m_backsightColorBtn);
    
    m_pegColorBtn = new QPushButton();
    m_pegColorBtn->setFixedSize(60, 24);
    m_pegColorBtn->setStyleSheet(QString("background-color: %1").arg(m_pegColor.name()));
    connect(m_pegColorBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(m_pegColor, this, "Default Peg Color");
        if (c.isValid()) {
            m_pegColor = c;
            m_pegColorBtn->setStyleSheet(QString("background-color: %1").arg(c.name()));
        }
    });
    colorForm->addRow("Default Peg Color:", m_pegColorBtn);
    
    displayLayout->addWidget(colorGroup);
    displayLayout->addStretch();
    tabs->addTab(displayTab, "Display");
    
    // ========== Stakeout Tab ==========
    QWidget* stakeoutTab = new QWidget();
    QVBoxLayout* stakeoutLayout = new QVBoxLayout(stakeoutTab);
    
    QGroupBox* stakeoutGroup = new QGroupBox("Stakeout Settings");
    QFormLayout* stakeoutForm = new QFormLayout(stakeoutGroup);
    
    m_bearingPrecision = new QSpinBox();
    m_bearingPrecision->setRange(0, 4);
    m_bearingPrecision->setValue(2);
    m_bearingPrecision->setSuffix(" decimal places");
    stakeoutForm->addRow("Bearing Precision:", m_bearingPrecision);
    
    m_distancePrecision = new QSpinBox();
    m_distancePrecision->setRange(0, 4);
    m_distancePrecision->setValue(3);
    m_distancePrecision->setSuffix(" decimal places");
    stakeoutForm->addRow("Distance Precision:", m_distancePrecision);
    
    stakeoutLayout->addWidget(stakeoutGroup);
    stakeoutLayout->addStretch();
    tabs->addTab(stakeoutTab, "Stakeout");
    
    // ========== Coordinates Tab ==========
    QWidget* coordTab = new QWidget();
    QVBoxLayout* coordLayout = new QVBoxLayout(coordTab);
    
    QGroupBox* crsGroup = new QGroupBox("Coordinate Reference System");
    QFormLayout* crsForm = new QFormLayout(crsGroup);
    
    m_crsCombo = new QComboBox();
    m_crsCombo->addItem("Local (No CRS)", "LOCAL");
    m_crsCombo->addItem("WGS 84 (EPSG:4326)", "EPSG:4326");
    m_crsCombo->addItem("UTM Zone 35S (EPSG:32735)", "EPSG:32735");
    m_crsCombo->addItem("UTM Zone 36S (EPSG:32736)", "EPSG:32736");
    m_crsCombo->addItem("Hartebeesthoek94 Lo25 (EPSG:2048)", "EPSG:2048");
    m_crsCombo->addItem("Hartebeesthoek94 Lo27 (EPSG:2049)", "EPSG:2049");
    m_crsCombo->addItem("Hartebeesthoek94 Lo29 (EPSG:2050)", "EPSG:2050");
    m_crsCombo->addItem("Cape Lo25 (EPSG:22275)", "EPSG:22275");
    m_crsCombo->addItem("Cape Lo27 (EPSG:22277)", "EPSG:22277");
    m_crsCombo->addItem("Cape Lo29 (EPSG:22279)", "EPSG:22279");
    m_crsCombo->addItem("Custom EPSG...", "CUSTOM");
    crsForm->addRow("Project CRS:", m_crsCombo);
    
    m_customEpsg = new QLineEdit();
    m_customEpsg->setPlaceholderText("Enter EPSG code (e.g., 32735)");
    m_customEpsg->setEnabled(false);
    crsForm->addRow("Custom EPSG:", m_customEpsg);
    
    connect(m_crsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        m_customEpsg->setEnabled(m_crsCombo->currentData().toString() == "CUSTOM");
    });
    
    coordLayout->addWidget(crsGroup);
    
    // Transformation
    QGroupBox* transformGroup = new QGroupBox("Coordinate Transformation");
    QFormLayout* transformForm = new QFormLayout(transformGroup);
    
    m_targetCrsCombo = new QComboBox();
    m_targetCrsCombo->addItem("No Transformation", "NONE");
    m_targetCrsCombo->addItem("WGS 84 (EPSG:4326)", "EPSG:4326");
    m_targetCrsCombo->addItem("UTM Zone 35S (EPSG:32735)", "EPSG:32735");
    m_targetCrsCombo->addItem("UTM Zone 36S (EPSG:32736)", "EPSG:32736");
    m_targetCrsCombo->addItem("Hartebeesthoek94 Lo25 (EPSG:2048)", "EPSG:2048");
    m_targetCrsCombo->addItem("Hartebeesthoek94 Lo27 (EPSG:2049)", "EPSG:2049");
    m_targetCrsCombo->addItem("Hartebeesthoek94 Lo29 (EPSG:2050)", "EPSG:2050");
    transformForm->addRow("Transform To:", m_targetCrsCombo);
    
    QLabel* transformNote = new QLabel("Note: Transformation applies when exporting coordinates");
    transformNote->setStyleSheet("color: gray; font-size: 10px;");
    transformForm->addRow(transformNote);
    
    coordLayout->addWidget(transformGroup);
    
    // Scale Factor
    QGroupBox* scaleGroup = new QGroupBox("Scale Factor (Ground to Grid)");
    QFormLayout* scaleForm = new QFormLayout(scaleGroup);
    
    m_scaleFactor = new QDoubleSpinBox();
    m_scaleFactor->setDecimals(8);
    m_scaleFactor->setRange(0.99000000, 1.01000000);
    m_scaleFactor->setValue(1.00000000);
    m_scaleFactor->setSingleStep(0.00001);
    scaleForm->addRow("Scale Factor:", m_scaleFactor);
    
    QLabel* scaleNote = new QLabel("1.0 = no scaling. Typical values: 0.9996 - 1.0004");
    scaleNote->setStyleSheet("color: gray; font-size: 10px;");
    scaleForm->addRow(scaleNote);
    
    coordLayout->addWidget(scaleGroup);
    coordLayout->addStretch();
    tabs->addTab(coordTab, "Coordinates");
    
    // ========== Appearance Tab ==========
    QWidget* appearanceTab = new QWidget();
    QVBoxLayout* appearanceLayout = new QVBoxLayout(appearanceTab);
    
    QGroupBox* themeGroup = new QGroupBox("Theme");
    QFormLayout* themeForm = new QFormLayout(themeGroup);
    
    m_themeCombo = new QComboBox();
    m_themeCombo->addItem("Light", "light");
    m_themeCombo->addItem("Dark", "dark");
    
    // Load current theme setting
    QSettings settings;
    QString currentTheme = settings.value("appearance/theme", "light").toString();
    m_themeCombo->setCurrentIndex(currentTheme == "dark" ? 1 : 0);
    
    themeForm->addRow("Color Theme:", m_themeCombo);
    
    QLabel* themeNote = new QLabel(
        "Theme changes take effect immediately when applied.\n"
        "Light mode is recommended for fieldwork visibility.");
    themeNote->setStyleSheet("color: gray; font-size: 10px;");
    themeNote->setWordWrap(true);
    themeForm->addRow(themeNote);
    
    appearanceLayout->addWidget(themeGroup);
    appearanceLayout->addStretch();
    tabs->addTab(appearanceTab, "Appearance");
    
    // ========== GAMA Tab ==========
    QWidget* gamaTab = new QWidget();
    QVBoxLayout* gamaLayout = new QVBoxLayout(gamaTab);
    
    QGroupBox* gamaGroup = new QGroupBox("GNU GAMA Network Adjustment");
    QFormLayout* gamaForm = new QFormLayout(gamaGroup);
    
    m_gamaEnabled = new QCheckBox("Enable GAMA Integration");
    m_gamaEnabled->setChecked(false);
    gamaForm->addRow(m_gamaEnabled);
    
    m_gamaPath = new QLineEdit();
    m_gamaPath->setPlaceholderText("gama-local (default)");
    m_gamaPath->setText("gama-local");
    gamaForm->addRow("Executable Path:", m_gamaPath);
    
    QLabel* gamaNote = new QLabel(
        "GNU GAMA is used for least-squares network adjustment.\n"
        "Install from: https://www.gnu.org/software/gama/");
    gamaNote->setStyleSheet("color: gray; font-size: 10px;");
    gamaNote->setWordWrap(true);
    gamaForm->addRow(gamaNote);
    
    gamaLayout->addWidget(gamaGroup);
    gamaLayout->addStretch();
    tabs->addTab(gamaTab, "GAMA");
    
    mainLayout->addWidget(tabs);
    
    // ========== Buttons ==========
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    QPushButton* resetBtn = new QPushButton("Reset to Defaults");
    connect(resetBtn, &QPushButton::clicked, this, &SettingsDialog::resetToDefaults);
    buttonLayout->addWidget(resetBtn);
    
    buttonLayout->addStretch();
    
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::applySettings);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    buttonLayout->addWidget(buttonBox);
    
    mainLayout->addLayout(buttonLayout);
}

void SettingsDialog::loadCurrentSettings()
{
    if (!m_canvas) return;
    
    // Load snapping settings
    m_snapEnabled->setChecked(m_canvas->isSnappingEnabled());
    m_showGrid->setChecked(m_canvas->showGrid());
    
    // Load CRS settings
    QString crs = m_canvas->crs();
    int crsIndex = m_crsCombo->findData(crs);
    if (crsIndex >= 0) {
        m_crsCombo->setCurrentIndex(crsIndex);
    } else if (crs.startsWith("EPSG:")) {
        m_crsCombo->setCurrentIndex(m_crsCombo->findData("CUSTOM"));
        m_customEpsg->setText(crs.mid(5));
    }
    
    QString targetCrs = m_canvas->targetCRS();
    int targetIndex = m_targetCrsCombo->findData(targetCrs);
    if (targetIndex >= 0) {
        m_targetCrsCombo->setCurrentIndex(targetIndex);
    }
    
    m_scaleFactor->setValue(m_canvas->scaleFactor());
}

void SettingsDialog::applySettings()
{
    if (!m_canvas) {
        accept();
        return;
    }
    
    // Apply snapping settings
    m_canvas->setSnappingEnabled(m_snapEnabled->isChecked());
    m_canvas->setShowGrid(m_showGrid->isChecked());
    
    // Apply CRS settings
    QString crs = m_crsCombo->currentData().toString();
    if (crs == "CUSTOM" && !m_customEpsg->text().isEmpty()) {
        crs = QString("EPSG:%1").arg(m_customEpsg->text());
    }
    m_canvas->setCRS(crs);
    m_canvas->setTargetCRS(m_targetCrsCombo->currentData().toString());
    m_canvas->setScaleFactor(m_scaleFactor->value());
    
    // Apply theme settings
    QSettings settings;
    QString selectedTheme = m_themeCombo->currentData().toString();
    settings.setValue("appearance/theme", selectedTheme);
    
    // Apply the stylesheet immediately
    QString themePath = (selectedTheme == "dark") 
        ? ":/styles/dark-theme.qss" 
        : ":/styles/light-theme.qss";
    QFile styleFile(themePath);
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = QLatin1String(styleFile.readAll());
        qApp->setStyleSheet(styleSheet);
        styleFile.close();
    }
    
    accept();
}

void SettingsDialog::resetToDefaults()
{
    // Snapping
    m_snapEnabled->setChecked(false);
    m_snapEndpoint->setChecked(true);
    m_snapMidpoint->setChecked(false);
    m_snapEdge->setChecked(false);
    m_snapIntersection->setChecked(false);
    m_snapTolerance->setValue(10.0);
    
    // Display
    m_showGrid->setChecked(true);
    m_gridSize->setValue(10);
    m_pegMarkerSize->setValue(0.5);
    
    // Colors
    m_stationColor = Qt::green;
    m_backsightColor = Qt::cyan;
    m_pegColor = Qt::red;
    m_stationColorBtn->setStyleSheet("background-color: green");
    m_backsightColorBtn->setStyleSheet("background-color: cyan");
    m_pegColorBtn->setStyleSheet("background-color: red");
    
    // Stakeout
    m_bearingPrecision->setValue(2);
    m_distancePrecision->setValue(3);
    
    // Coordinates
    m_crsCombo->setCurrentIndex(0);  // Local
    m_targetCrsCombo->setCurrentIndex(0);  // No Transformation
    m_scaleFactor->setValue(1.0);
    m_customEpsg->clear();
    
    // GAMA
    m_gamaEnabled->setChecked(false);
    m_gamaPath->setText("gama-local");
}
