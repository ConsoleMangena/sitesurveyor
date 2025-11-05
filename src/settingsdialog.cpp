#include "settingsdialog.h"
#include "canvaswidget.h"
#include "appsettings.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QFormLayout>

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

    // Profile group
    QGroupBox* profileBox = new QGroupBox("Profile", this);
    QFormLayout* profileForm = new QFormLayout(profileBox);
    m_firstEdit = new QLineEdit(profileBox);
    m_lastEdit = new QLineEdit(profileBox);
    m_emailEdit = new QLineEdit(profileBox);
    m_emailEdit->setPlaceholderText("you@example.com");
    profileForm->addRow(new QLabel("First name:", profileBox), m_firstEdit);
    profileForm->addRow(new QLabel("Surname:", profileBox), m_lastEdit);
    profileForm->addRow(new QLabel("Email:", profileBox), m_emailEdit);
    m_signOutButton = new QPushButton("Sign Out", profileBox);
    profileForm->addRow(new QLabel("" , profileBox), m_signOutButton);

    m_applyButton = new QPushButton("Apply", this);
    m_closeButton = new QPushButton("Close", this);

    connect(m_applyButton, &QPushButton::clicked, this, &SettingsDialog::applyChanges);
    connect(m_closeButton, &QPushButton::clicked, this, &SettingsDialog::close);
    connect(m_signOutButton, &QPushButton::clicked, this, [this](){ emit signOutRequested(); close(); });

    QHBoxLayout* gridSizeLayout = new QHBoxLayout();
    gridSizeLayout->addWidget(gridSizeLabel);
    gridSizeLayout->addWidget(m_gridSizeSpin);

    QHBoxLayout* unitsLayout = new QHBoxLayout();
    unitsLayout->addWidget(unitsLabel);
    unitsLayout->addWidget(m_unitsCombo);

    QHBoxLayout* angleLayout = new QHBoxLayout();
    angleLayout->addWidget(angleLabel);
    angleLayout->addWidget(m_angleCombo);

    QHBoxLayout* crsLayout = new QHBoxLayout();
    crsLayout->addWidget(crsLabel);
    crsLayout->addWidget(m_crsEdit);


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
    mainLayout->addLayout(crsLayout);
    mainLayout->addWidget(profileBox);
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
    // Profile from cached settings
    m_firstEdit->setText(AppSettings::userFirstName());
    m_lastEdit->setText(AppSettings::userLastName());
    m_emailEdit->setText(AppSettings::userEmail());
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
    // Profile save (local cache)
    const QString first = m_firstEdit->text().trimmed();
    const QString last  = m_lastEdit->text().trimmed();
    const QString email = m_emailEdit->text().trimmed();
    AppSettings::setUserFirstName(first);
    AppSettings::setUserLastName(last);
    AppSettings::setUserEmail(email);
    if (!first.isEmpty() || !last.isEmpty()) AppSettings::setUserName((first + " " + last).trimmed());
    // Update grid suffix immediately to reflect units choice
    const QString units = AppSettings::measurementUnits();
    m_gridSizeSpin->setSuffix(units.compare("imperial", Qt::CaseInsensitive) == 0 ? " ft" : " m");
    emit settingsApplied();
}
