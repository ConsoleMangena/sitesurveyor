#include "settingsdialog.h"
#include "canvaswidget.h"
#include "appsettings.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

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

    m_applyButton = new QPushButton("Apply", this);
    m_closeButton = new QPushButton("Close", this);

    connect(m_applyButton, &QPushButton::clicked, this, &SettingsDialog::applyChanges);
    connect(m_closeButton, &QPushButton::clicked, this, &SettingsDialog::close);

    QHBoxLayout* gridSizeLayout = new QHBoxLayout();
    gridSizeLayout->addWidget(gridSizeLabel);
    gridSizeLayout->addWidget(m_gridSizeSpin);

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
    emit settingsApplied();
}
