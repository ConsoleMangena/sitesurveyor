#include "app/settingsdialog.h"
#include "app/mainwindow.h"
#include "canvas/canvaswidget.h"
#include "tools/snapper.h"
#include "auth/authmanager.h"

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
#include <QIcon>
#include <QDesktopServices>
#include <QUrl>

SettingsDialog::SettingsDialog(CanvasWidget* canvas, AuthManager* auth, QWidget* parent)
    : QDialog(parent)
    , m_canvas(canvas)
    , m_auth(auth)
    , m_stationColor(Qt::green)
    , m_backsightColor(Qt::cyan)
    , m_pegColor(Qt::red)
{
    setWindowTitle("Settings");
    setMinimumSize(550, 450);
    resize(700, 550);
    
    setupUI();
    loadCurrentSettings();
}

void SettingsDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    
    // Get theme for styling
    QSettings themeSettings;
    bool isDark = (themeSettings.value("appearance/theme", "light").toString() == "dark");
    
    // Style group boxes - compact sizing
    QString groupStyle = isDark ?
        "QGroupBox { font-weight: bold; font-size: 11px; border: 1px solid #3c3c3c; border-radius: 3px; margin-top: 10px; padding: 6px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 6px; padding: 0 4px; }" :
        "QGroupBox { font-weight: bold; font-size: 11px; border: 1px solid #d0d0d0; border-radius: 3px; margin-top: 10px; padding: 6px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 6px; padding: 0 4px; }";
    
    QTabWidget* tabs = new QTabWidget();
    tabs->setUsesScrollButtons(true);
    tabs->setElideMode(Qt::ElideNone);
    tabs->setStyleSheet(isDark ?
        "QTabWidget::pane { border: 1px solid #3c3c3c; border-radius: 4px; padding: 8px; }"
        "QTabBar::tab { padding: 8px 16px; margin-right: 2px; }"
        "QTabBar::tab:selected { background: #3c3c3c; color: white; border-bottom: 2px solid #0078d4; }"
        "QTabBar::tab:!selected { background: transparent; color: #aaa; }"
        "QTabBar::tab:hover:!selected { background: #2d2d30; }"
        "QTabBar::scroller { width: 30px; }"
        "QTabBar QToolButton { background: transparent; border: none; }" :
        "QTabWidget::pane { border: 1px solid #d0d0d0; border-radius: 4px; padding: 8px; }"
        "QTabBar::tab { padding: 8px 16px; margin-right: 2px; }"
        "QTabBar::tab:selected { background: white; color: #333; border-bottom: 2px solid #0078d4; }"
        "QTabBar::tab:!selected { background: transparent; color: #666; }"
        "QTabBar::tab:hover:!selected { background: #f5f5f5; }"
        "QTabBar::scroller { width: 30px; }"
        "QTabBar QToolButton { background: transparent; border: none; }");

    
    // ========== General Tab (Units + CRS + Theme) ==========
    QWidget* generalTab = new QWidget();
    QVBoxLayout* generalLayout = new QVBoxLayout(generalTab);
    generalLayout->setSpacing(10);
    
    // Units Group
    QGroupBox* lengthGroup = new QGroupBox("Length Units");
    lengthGroup->setStyleSheet(groupStyle);
    QFormLayout* lengthForm = new QFormLayout(lengthGroup);
    m_lengthUnitCombo = new QComboBox();
    m_lengthUnitCombo->addItems({"Meters", "Feet", "US Survey Feet", "Millimeters"});
    lengthForm->addRow("Type:", m_lengthUnitCombo);
    m_lengthPrecision = new QSpinBox();
    m_lengthPrecision->setRange(0, 8);
    m_lengthPrecision->setValue(3);
    lengthForm->addRow("Precision:", m_lengthPrecision);
    generalLayout->addWidget(lengthGroup);
    
    QGroupBox* angleGroup = new QGroupBox("Angle Units");
    angleGroup->setStyleSheet(groupStyle);
    QFormLayout* angleForm = new QFormLayout(angleGroup);
    m_angleUnitCombo = new QComboBox();
    m_angleUnitCombo->addItems({"Decimal Degrees", "Deg/Min/Sec", "Gradians", "Radians", "Surveyor's Units"});
    angleForm->addRow("Type:", m_angleUnitCombo);
    m_anglePrecision = new QSpinBox();
    m_anglePrecision->setRange(0, 8);
    m_anglePrecision->setValue(4);
    angleForm->addRow("Precision:", m_anglePrecision);
    
    m_clockwiseCheck = new QCheckBox("Clockwise Direction");
    angleForm->addRow(m_clockwiseCheck);
    
    m_directionBaseCombo = new QComboBox();
    m_directionBaseCombo->addItem("East (0)", 0);
    m_directionBaseCombo->addItem("North (90)", 1);
    m_directionBaseCombo->addItem("West (180)", 2);
    m_directionBaseCombo->addItem("South (0) [Zero South]", 3);
    angleForm->addRow("Base Angle:", m_directionBaseCombo);
    
    m_swapXY = new QCheckBox("Swap X/Y (Y-X Convention)");
    m_swapXY->setToolTip("Swaps X and Y coordinates (used in Zimbabwe and some other countries)");
    angleForm->addRow(m_swapXY);
    
    generalLayout->addWidget(angleGroup);
    
    // Theme (moved from Appearance)
    QGroupBox* themeGroup = new QGroupBox("Appearance");
    themeGroup->setStyleSheet(groupStyle);
    QFormLayout* themeForm = new QFormLayout(themeGroup);
    m_themeCombo = new QComboBox();
    m_themeCombo->addItem("Light", "light");
    m_themeCombo->addItem("Dark", "dark");
    QSettings settings;
    QString currentTheme = settings.value("appearance/theme", "light").toString();
    m_themeCombo->setCurrentIndex(currentTheme == "dark" ? 1 : 0);
    themeForm->addRow("Theme:", m_themeCombo);
    generalLayout->addWidget(themeGroup);
    
    generalLayout->addStretch();
    tabs->addTab(generalTab, QIcon(":/icons/settings.svg"), "General");

    // ========== Editor Tab (Snapping + Drafting + Selection) ==========
    QWidget* editorTab = new QWidget();
    QVBoxLayout* editorLayout = new QVBoxLayout(editorTab);
    editorLayout->setSpacing(10);
    
    // Snapping Group
    QGroupBox* snapGroup = new QGroupBox("Snapping");
    snapGroup->setStyleSheet(groupStyle);
    QFormLayout* snapForm = new QFormLayout(snapGroup);
    
    m_snapEnabled = new QCheckBox("Enable Snapping");
    snapForm->addRow(m_snapEnabled);
    
    m_snapEndpoint = new QCheckBox("Snap to Endpoints");
    snapForm->addRow(m_snapEndpoint);
    
    m_snapMidpoint = new QCheckBox("Snap to Midpoints");
    snapForm->addRow(m_snapMidpoint);
    
    m_snapEdge = new QCheckBox("Snap to Edges");
    snapForm->addRow(m_snapEdge);
    
    m_snapIntersection = new QCheckBox("Snap to Intersections");
    snapForm->addRow(m_snapIntersection);
    
    m_snapTolerance = new QDoubleSpinBox();
    m_snapTolerance->setRange(1.0, 50.0);
    m_snapTolerance->setValue(10.0);
    m_snapTolerance->setSuffix(" px");
    snapForm->addRow("Tolerance:", m_snapTolerance);
    
    editorLayout->addWidget(snapGroup);
    
    // AutoSnap/Drafting Group
    QGroupBox* draftGroup = new QGroupBox("AutoSnap");
    draftGroup->setStyleSheet(groupStyle);
    QFormLayout* draftForm = new QFormLayout(draftGroup);
    
    m_autoSnapCheck = new QCheckBox("Display AutoSnap Marker");
    draftForm->addRow(m_autoSnapCheck);
    
    m_snapMarkerSizeSlider = new QSlider(Qt::Horizontal);
    m_snapMarkerSizeSlider->setRange(1, 20);
    m_snapMarkerSizeSlider->setValue(5);
    draftForm->addRow("Marker Size:", m_snapMarkerSizeSlider);
    
    m_apertureSizeSlider = new QSlider(Qt::Horizontal);
    m_apertureSizeSlider->setRange(1, 50);
    m_apertureSizeSlider->setValue(10);
    draftForm->addRow("Aperture Size:", m_apertureSizeSlider);
    
    editorLayout->addWidget(draftGroup);
    
    // Selection Group
    QGroupBox* selectGroup = new QGroupBox("Selection");
    selectGroup->setStyleSheet(groupStyle);
    QFormLayout* selectForm = new QFormLayout(selectGroup);
    
    m_pickboxSizeSlider = new QSlider(Qt::Horizontal);
    m_pickboxSizeSlider->setRange(1, 50);
    m_pickboxSizeSlider->setValue(5);
    selectForm->addRow("Pickbox Size:", m_pickboxSizeSlider);
    
    m_gripSizeSlider = new QSlider(Qt::Horizontal);
    m_gripSizeSlider->setRange(1, 20);
    m_gripSizeSlider->setValue(5);
    selectForm->addRow("Grip Size:", m_gripSizeSlider);
    
    editorLayout->addWidget(selectGroup);
    editorLayout->addStretch();
    tabs->addTab(editorTab, QIcon(":/icons/edit.svg"), "Editor");

    
    // ========== Display Tab ==========
    QWidget* displayTab = new QWidget();
    QVBoxLayout* displayLayout = new QVBoxLayout(displayTab);
    displayLayout->setSpacing(12);
    
    QGroupBox* displayGroup = new QGroupBox("Display Settings");
    displayGroup->setStyleSheet(groupStyle);
    QFormLayout* displayForm = new QFormLayout(displayGroup);
    displayForm->setSpacing(10);
    displayForm->setContentsMargins(12, 20, 12, 12);
    
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
    
    m_crosshairSizeSlider = new QSlider(Qt::Horizontal);
    m_crosshairSizeSlider->setRange(1, 100); // 1% to 100% of screen/canvas
    m_crosshairSizeSlider->setValue(5);
    displayForm->addRow("Crosshair Size:", m_crosshairSizeSlider);
    
    displayLayout->addWidget(displayGroup);
    
    // Colors
    QGroupBox* colorGroup = new QGroupBox("Colors");
    colorGroup->setStyleSheet(groupStyle);
    QFormLayout* colorForm = new QFormLayout(colorGroup);
    colorForm->setSpacing(10);
    colorForm->setContentsMargins(12, 20, 12, 12);
    
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
    tabs->addTab(displayTab, QIcon(":/icons/grid.svg"), "Display");
    
    // ========== Survey Tab (Stakeout + Coordinates + GAMA) ==========
    QWidget* surveyTab = new QWidget();
    QVBoxLayout* surveyLayout = new QVBoxLayout(surveyTab);
    surveyLayout->setSpacing(10);
    
    // Stakeout Group
    QGroupBox* stakeoutGroup = new QGroupBox("Stakeout");
    stakeoutGroup->setStyleSheet(groupStyle);
    QFormLayout* stakeoutForm = new QFormLayout(stakeoutGroup);
    
    m_bearingPrecision = new QSpinBox();
    m_bearingPrecision->setRange(0, 4);
    m_bearingPrecision->setValue(2);
    m_bearingPrecision->setSuffix(" dp");
    stakeoutForm->addRow("Bearing Precision:", m_bearingPrecision);
    
    m_distancePrecision = new QSpinBox();
    m_distancePrecision->setRange(0, 4);
    m_distancePrecision->setValue(3);
    m_distancePrecision->setSuffix(" dp");
    stakeoutForm->addRow("Distance Precision:", m_distancePrecision);
    
    surveyLayout->addWidget(stakeoutGroup);
    
    // Coordinates Group
    QGroupBox* crsGroup = new QGroupBox("Coordinate System");
    crsGroup->setStyleSheet(groupStyle);
    QFormLayout* crsForm = new QFormLayout(crsGroup);
    
    m_crsCombo = new QComboBox();
    m_crsCombo->addItem("Local (No CRS)", "LOCAL");
    m_crsCombo->addItem("WGS 84 (EPSG:4326)", "EPSG:4326");
    m_crsCombo->addItem("UTM Zone 35S (EPSG:32735)", "EPSG:32735");
    m_crsCombo->addItem("UTM Zone 36S (EPSG:32736)", "EPSG:32736");
    m_crsCombo->addItem("Hartebeesthoek94 Lo25 (EPSG:2048)", "EPSG:2048");
    m_crsCombo->addItem("Hartebeesthoek94 Lo27 (EPSG:2049)", "EPSG:2049");
    m_crsCombo->addItem("Hartebeesthoek94 Lo29 (EPSG:2050)", "EPSG:2050");
    m_crsCombo->addItem("Custom EPSG...", "CUSTOM");
    crsForm->addRow("Project CRS:", m_crsCombo);
    
    m_customEpsg = new QLineEdit();
    m_customEpsg->setPlaceholderText("Enter EPSG code");
    m_customEpsg->setEnabled(false);
    crsForm->addRow("Custom EPSG:", m_customEpsg);
    
    connect(m_crsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        m_customEpsg->setEnabled(m_crsCombo->currentData().toString() == "CUSTOM");
    });
    
    m_targetCrsCombo = new QComboBox();
    m_targetCrsCombo->addItem("No Transformation", "NONE");
    m_targetCrsCombo->addItem("WGS 84 (EPSG:4326)", "EPSG:4326");
    m_targetCrsCombo->addItem("UTM Zone 35S", "EPSG:32735");
    crsForm->addRow("Transform To:", m_targetCrsCombo);
    
    m_scaleFactor = new QDoubleSpinBox();
    m_scaleFactor->setDecimals(8);
    m_scaleFactor->setRange(0.99000000, 1.01000000);
    m_scaleFactor->setValue(1.00000000);
    m_scaleFactor->setSingleStep(0.00001);
    crsForm->addRow("Scale Factor:", m_scaleFactor);
    
    surveyLayout->addWidget(crsGroup);
    
    // GAMA Group
    QGroupBox* gamaGroup = new QGroupBox("Network Adjustment (GAMA)");
    gamaGroup->setStyleSheet(groupStyle);
    QFormLayout* gamaForm = new QFormLayout(gamaGroup);
    
    m_gamaEnabled = new QCheckBox("Enable GAMA Integration");
    m_gamaEnabled->setChecked(false);
    gamaForm->addRow(m_gamaEnabled);
    
    m_gamaPath = new QLineEdit();
    m_gamaPath->setPlaceholderText("gama-local (default)");
    m_gamaPath->setText("gama-local");
    gamaForm->addRow("Executable:", m_gamaPath);
    
    surveyLayout->addWidget(gamaGroup);
    surveyLayout->addStretch();
    tabs->addTab(surveyTab, QIcon(":/icons/target.svg"), "Survey");
    
    // ========== Account Tab ==========
    QWidget* accountTab = new QWidget();
    QVBoxLayout* accountLayout = new QVBoxLayout(accountTab);
    accountLayout->setSpacing(10);
    
    // User Profile Group
    QGroupBox* userGroup = new QGroupBox("Profile");
    userGroup->setStyleSheet(groupStyle);
    QFormLayout* profileForm = new QFormLayout(userGroup);
    profileForm->setSpacing(6);
    
    if (m_auth && m_auth->isAuthenticated()) {
        const UserProfile& profile = m_auth->userProfile();
        
        QString displayName = profile.name.isEmpty() ? profile.username : profile.name;
        if (!displayName.isEmpty()) {
            QLabel* nameVal = new QLabel(displayName);
            nameVal->setStyleSheet("font-weight: bold;");
            profileForm->addRow("Name:", nameVal);
        }
        
        if (!profile.username.isEmpty()) {
            profileForm->addRow("Username:", new QLabel("@" + profile.username));
        }
        
        if (!profile.email.isEmpty()) {
            profileForm->addRow("Email:", new QLabel(profile.email));
        }
        
        if (!profile.organization.isEmpty()) {
            profileForm->addRow("Organization:", new QLabel(profile.organization));
        }
        
        if (!profile.userType.isEmpty()) {
            profileForm->addRow("Type:", new QLabel(profile.userType));
        }
        
        QString location;
        if (!profile.city.isEmpty() && !profile.country.isEmpty()) {
            location = profile.city + ", " + profile.country;
        } else if (!profile.city.isEmpty()) {
            location = profile.city;
        } else if (!profile.country.isEmpty()) {
            location = profile.country;
        }
        if (!location.isEmpty()) {
            profileForm->addRow("Location:", new QLabel(location));
        }
    } else {
        profileForm->addRow("", new QLabel("Not logged in"));
    }
    
    accountLayout->addWidget(userGroup);
    
    // Logout Button
    QPushButton* logoutBtn = new QPushButton("Sign Out");
    logoutBtn->setStyleSheet(isDark ?
        "QPushButton { padding: 8px 16px; }" :
        "QPushButton { padding: 8px 16px; }");
    connect(logoutBtn, &QPushButton::clicked, this, [this]() {
        if (m_auth) {
            m_auth->logout();
            accept();
        }
    });
    accountLayout->addWidget(logoutBtn, 0, Qt::AlignLeft);
    
    accountLayout->addStretch();
    tabs->addTab(accountTab, QIcon(":/icons/settings.svg"), "Account");
    
    mainLayout->addWidget(tabs);
    
    // ========== Buttons ==========
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    QPushButton* resetBtn = new QPushButton("Reset to Defaults");
    connect(resetBtn, &QPushButton::clicked, this, &SettingsDialog::resetToDefaults);
    buttonLayout->addWidget(resetBtn);
    
    buttonLayout->addStretch();
    
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::applySettings);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &SettingsDialog::applyChanges);
    buttonLayout->addWidget(buttonBox);
    
    mainLayout->addLayout(buttonLayout);
}

void SettingsDialog::loadCurrentSettings()
{
    if (!m_canvas) return;
    QSettings settings;

    // Load snapping settings
    m_snapEnabled->setChecked(m_canvas->isSnappingEnabled());
    m_showGrid->setChecked(m_canvas->showGrid());
    
    // Load Units settings
    m_lengthUnitCombo->setCurrentIndex(settings.value("units/lengthUnit", 0).toInt());
    m_lengthPrecision->setValue(settings.value("units/lengthPrecision", 3).toInt());
    m_angleUnitCombo->setCurrentIndex(settings.value("units/angleUnit", 0).toInt());
    m_anglePrecision->setValue(settings.value("units/anglePrecision", 4).toInt());
    m_clockwiseCheck->setChecked(settings.value("units/clockwise", false).toBool());
    
    // Logic to sync South Azimuth with Base Angle
    bool south = settings.value("coordinates/southAzimuth", false).toBool();
    if (south) {
        m_directionBaseCombo->setCurrentIndex(3); // South
    } else {
        m_directionBaseCombo->setCurrentIndex(settings.value("units/baseAngle", 1).toInt()); // Default North (1)
    }

    // Load Drafting settings
    m_autoSnapCheck->setChecked(settings.value("drafting/autoSnap", true).toBool());
    m_snapMarkerSizeSlider->setValue(settings.value("drafting/markerSize", 5).toInt());
    m_apertureSizeSlider->setValue(settings.value("drafting/apertureSize", 10).toInt());

    // Load Selection settings
    m_pickboxSizeSlider->setValue(settings.value("selection/pickboxSize", 5).toInt());
    m_gripSizeSlider->setValue(settings.value("selection/gripSize", 5).toInt());

    // Load Display settings (Crosshair)
    m_crosshairSizeSlider->setValue(settings.value("display/crosshairSize", 5).toInt());
    
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
    

    m_swapXY->setChecked(settings.value("coordinates/swapXY", false).toBool());
}

void SettingsDialog::applySettings()
{
    applyChanges();
    accept();
}

void SettingsDialog::applyChanges()
{
    if (!m_canvas) return;
    QSettings settings;
    
    // Apply snapping settings
    m_canvas->setSnappingEnabled(m_snapEnabled->isChecked());
    m_canvas->setShowGrid(m_showGrid->isChecked());
    
    // Save Units settings
    settings.setValue("units/lengthUnit", m_lengthUnitCombo->currentIndex());
    settings.setValue("units/lengthPrecision", m_lengthPrecision->value());
    settings.setValue("units/angleUnit", m_angleUnitCombo->currentIndex());
    settings.setValue("units/anglePrecision", m_anglePrecision->value());
    settings.setValue("units/clockwise", m_clockwiseCheck->isChecked());
    settings.setValue("units/baseAngle", m_directionBaseCombo->currentIndex());

    // Save Drafting settings
    settings.setValue("drafting/autoSnap", m_autoSnapCheck->isChecked());
    settings.setValue("drafting/markerSize", m_snapMarkerSizeSlider->value());
    settings.setValue("drafting/apertureSize", m_apertureSizeSlider->value());

    // Save Selection settings
    settings.setValue("selection/pickboxSize", m_pickboxSizeSlider->value());
    settings.setValue("selection/gripSize", m_gripSizeSlider->value());

    // Save Display settings (Crosshair)
    settings.setValue("display/crosshairSize", m_crosshairSizeSlider->value());
    
    // Apply CRS settings
    QString crs = m_crsCombo->currentData().toString();
    if (crs == "CUSTOM" && !m_customEpsg->text().isEmpty()) {
        crs = QString("EPSG:%1").arg(m_customEpsg->text());
    }
    m_canvas->setCRS(crs);
    m_canvas->setTargetCRS(m_targetCrsCombo->currentData().toString());
    m_canvas->setScaleFactor(m_scaleFactor->value());
    bool south = (m_directionBaseCombo->currentData().toInt() == 3);
    m_canvas->setSouthAzimuth(south);
    m_canvas->setSwapXY(m_swapXY->isChecked());
    
    // Save settings
    settings.setValue("coordinates/southAzimuth", south);
    settings.setValue("units/baseAngle", m_directionBaseCombo->currentIndex());
    settings.setValue("coordinates/swapXY", m_swapXY->isChecked());
    
    // Apply theme settings
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
    
    // Update toolbar icons to match theme
    MainWindow* mainWindow = qobject_cast<MainWindow*>(parentWidget());
    if (mainWindow) {
        mainWindow->updateToolbarIcons();
    }
}

void SettingsDialog::resetToDefaults()
{
    // Units
    m_lengthUnitCombo->setCurrentIndex(0);
    m_lengthPrecision->setValue(3);
    m_angleUnitCombo->setCurrentIndex(0);
    m_anglePrecision->setValue(4);
    m_clockwiseCheck->setChecked(false);
    m_directionBaseCombo->setCurrentIndex(1); // North Default

    // Drafting
    m_autoSnapCheck->setChecked(true);
    m_snapMarkerSizeSlider->setValue(5);
    m_apertureSizeSlider->setValue(10);

    // Selection
    m_pickboxSizeSlider->setValue(5);
    m_gripSizeSlider->setValue(5);

    // Display
    m_crosshairSizeSlider->setValue(5);

    // Snapping
    m_snapEnabled->setChecked(false);
    m_snapEndpoint->setChecked(true);
    m_snapMidpoint->setChecked(false);
    m_snapEdge->setChecked(false);
    m_snapIntersection->setChecked(false);
    m_snapTolerance->setValue(10.0);
    
    // Display (Existing)
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
    QSettings settings; 
    m_gamaEnabled->setChecked(settings.value("gama/enabled", false).toBool());
    m_gamaPath->setText(settings.value("gama/path", "gama-local").toString());
    
    // Reset Direction/Coordinate Axis preferences
    m_directionBaseCombo->setCurrentIndex(1); // North
    m_swapXY->setChecked(false);
}
