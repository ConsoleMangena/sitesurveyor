#include "settingsdialog.h"
#include "canvaswidget.h"
#include "appsettings.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFormLayout>
#include <QGroupBox>

SettingsDialog::SettingsDialog(CanvasWidget* canvas, QWidget *parent)
    : QDialog(parent), m_canvas(canvas)
{
    setWindowTitle("Settings");
    setModal(true);
    setMinimumWidth(300);

    m_showGridCheck = new QCheckBox("Show grid", this);
    m_showLabelsCheck = new QCheckBox("Show coordinate labels", this);
    m_gaussModeCheck = new QCheckBox("Use Gauss/Zimbabwe orientation (0° South, swap X/Y)", this);
    m_use3DCheck = new QCheckBox("Use 3D coordinates (show Z)", this);

    m_gridSizeSpin = new QDoubleSpinBox(this);
    m_gridSizeSpin->setRange(1.0, 1000.0);
    m_gridSizeSpin->setDecimals(2);
    m_gridSizeSpin->setSuffix(" m");

    QLabel* gridSizeLabel = new QLabel("Grid spacing:", this);

    // Engineering preferences
    QLabel* unitsLabel = new QLabel("Units:", this);
    m_unitsCombo = new QComboBox(this);
    m_unitsCombo->addItem("Metric (m, m^2)", "metric");
    m_unitsCombo->addItem("Imperial (ft, ft^2)", "imperial");

    QLabel* angleLabel = new QLabel("Angle format:", this);
    m_angleCombo = new QComboBox(this);
    m_angleCombo->addItem("DMS (deg°min'sec\" )", "dms");
    m_angleCombo->addItem("Decimal Degrees", "decimal");

    QLabel* crsLabel = new QLabel("CRS:", this);
    m_crsEdit = new QLineEdit(this);
    m_crsEdit->setPlaceholderText("EPSG:4326");

    // Autosave controls
    m_autosaveCheck = new QCheckBox("Enable autosave", this);
    m_autosaveIntervalSpin = new QSpinBox(this);
    m_autosaveIntervalSpin->setRange(1, 60);
    m_autosaveIntervalSpin->setSuffix(" min");


    m_applyButton = new QPushButton("Apply", this);
    m_closeButton = new QPushButton("Close", this);

    connect(m_applyButton, &QPushButton::clicked, this, &SettingsDialog::applyChanges);
    connect(m_closeButton, &QPushButton::clicked, this, &SettingsDialog::close);

    QHBoxLayout* gridSizeLayout = new QHBoxLayout();
    gridSizeLayout->addWidget(gridSizeLabel);
    gridSizeLayout->addWidget(m_gridSizeSpin);

    QHBoxLayout* unitsLayout = new QHBoxLayout();
    unitsLayout->addWidget(unitsLabel);
    unitsLayout->addWidget(m_unitsCombo);

    QHBoxLayout* angleLayout = new QHBoxLayout();
    angleLayout->addWidget(angleLabel);
    angleLayout->addWidget(m_angleCombo);

    // Group: Survey
    QGroupBox* surveyBox = new QGroupBox("Survey", this);
    QFormLayout* surveyForm = new QFormLayout(surveyBox);
    surveyForm->addRow(crsLabel, m_crsEdit);

    // Group: Autosave
    QGroupBox* autosaveBox = new QGroupBox("Autosave", this);
    QFormLayout* autosaveForm = new QFormLayout(autosaveBox);
    autosaveForm->addRow(m_autosaveCheck);
    autosaveForm->addRow(new QLabel("Interval:", this), m_autosaveIntervalSpin);


    QHBoxLayout* buttonsLayout = new QHBoxLayout();
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(m_applyButton);
    buttonsLayout->addWidget(m_closeButton);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_showGridCheck);
    mainLayout->addWidget(m_showLabelsCheck);
    mainLayout->addWidget(m_gaussModeCheck);
    mainLayout->addWidget(m_use3DCheck);
    mainLayout->addLayout(gridSizeLayout);
    mainLayout->addLayout(unitsLayout);
    mainLayout->addLayout(angleLayout);
    mainLayout->addWidget(surveyBox);
    mainLayout->addWidget(autosaveBox);
    mainLayout->addStretch();
    mainLayout->addLayout(buttonsLayout);

    loadFromCanvas();
}

void SettingsDialog::reload()
{
    loadFromCanvas();
}

void SettingsDialog::loadFromCanvas()
{
    if (!m_canvas) {
        return;
    }
    m_showGridCheck->setChecked(m_canvas->showGrid());
    m_showLabelsCheck->setChecked(m_canvas->showLabels());
    m_gridSizeSpin->setValue(m_canvas->gridSize());
    m_gaussModeCheck->setChecked(AppSettings::gaussMode());
    m_use3DCheck->setChecked(AppSettings::use3D());

    // Preferences
    const QString units = AppSettings::measurementUnits();
    int ui = m_unitsCombo->findData(units);
    if (ui < 0) ui = 0;
    m_unitsCombo->setCurrentIndex(ui);
    // Adjust grid suffix
    m_gridSizeSpin->setSuffix(units.compare("imperial", Qt::CaseInsensitive) == 0 ? " ft" : " m");

    const QString ang = AppSettings::angleFormat();
    int ai = m_angleCombo->findData(ang);
    if (ai < 0) ai = 0;
    m_angleCombo->setCurrentIndex(ai);

    m_crsEdit->setText(AppSettings::crs());

    // Autosave
    m_autosaveCheck->setChecked(AppSettings::autosaveEnabled());
    m_autosaveIntervalSpin->setValue(AppSettings::autosaveIntervalMinutes());
}

void SettingsDialog::applyChanges()
{
    if (!m_canvas) {
        return;
    }
    m_canvas->setShowGrid(m_showGridCheck->isChecked());
    m_canvas->setShowLabels(m_showLabelsCheck->isChecked());
    m_canvas->setGridSize(m_gridSizeSpin->value());
    AppSettings::setGaussMode(m_gaussModeCheck->isChecked());
    AppSettings::setUse3D(m_use3DCheck->isChecked());
    m_canvas->setGaussMode(m_gaussModeCheck->isChecked());

    // Save engineering preferences
    AppSettings::setMeasurementUnits(m_unitsCombo->currentData().toString());
    AppSettings::setAngleFormat(m_angleCombo->currentData().toString());
    AppSettings::setCrs(m_crsEdit->text().trimmed());

    // Autosave
    AppSettings::setAutosaveEnabled(m_autosaveCheck->isChecked());
    AppSettings::setAutosaveIntervalMinutes(m_autosaveIntervalSpin->value());

    // Update grid suffix immediately to reflect units choice
    const QString units = AppSettings::measurementUnits();
    m_gridSizeSpin->setSuffix(units.compare("imperial", Qt::CaseInsensitive) == 0 ? " ft" : " m");
    emit settingsApplied();
}
