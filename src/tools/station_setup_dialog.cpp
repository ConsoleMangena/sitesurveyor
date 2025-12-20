#include "tools/station_setup_dialog.h"
#include "canvas/canvaswidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QtMath>

#include <QSettings>

StationSetupDialog::StationSetupDialog(CanvasWidget* canvas, QWidget *parent)
    : QDialog(parent), m_canvas(canvas)
{
    setupUi();
    loadCurrent();
}

void StationSetupDialog::setupUi()
{
    setWindowTitle("Station Setup");
    resize(400, 500);
    
    QSettings settings;
    bool swapXY = settings.value("coordinates/swapXY", false).toBool();
    QString xLabel = swapXY ? "Y:" : "X:";
    QString yLabel = swapXY ? "X:" : "Y:";
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // --- Station Group ---
    QGroupBox* stnGroup = new QGroupBox("Instrument Station");
    QFormLayout* stnLayout = new QFormLayout(stnGroup);
    
    m_stnName = new QLineEdit();
    stnLayout->addRow("Station Name:", m_stnName);
    
    m_stnE = new QDoubleSpinBox();
    m_stnE->setRange(-9999999.999, 9999999.999);
    m_stnE->setDecimals(3);
    stnLayout->addRow("Easting (" + xLabel + "):", m_stnE);
    
    m_stnN = new QDoubleSpinBox();
    m_stnN->setRange(-9999999.999, 9999999.999);
    m_stnN->setDecimals(3);
    stnLayout->addRow("Northing (" + yLabel + "):", m_stnN);
    
    m_stnZ = new QDoubleSpinBox();
    m_stnZ->setRange(-9999.999, 9999.999);
    m_stnZ->setDecimals(3);
    stnLayout->addRow("Elevation (Z):", m_stnZ);
    
    mainLayout->addWidget(stnGroup);
    
    // --- Backsight Group ---
    QGroupBox* bsGroup = new QGroupBox("Backsight / Orientation");
    QVBoxLayout* bsGroupLayout = new QVBoxLayout(bsGroup);
    
    m_useBsCoords = new QCheckBox("Use Backsight Coordinates");
    m_useBsCoords->setChecked(true);
    connect(m_useBsCoords, &QCheckBox::toggled, this, &StationSetupDialog::onModeChanged);
    bsGroupLayout->addWidget(m_useBsCoords);
    
    QFormLayout* bsForm = new QFormLayout();
    
    m_bsName = new QLineEdit();
    bsForm->addRow("Backsight Name:", m_bsName);
    
    m_bsE = new QDoubleSpinBox();
    m_bsE->setRange(-9999999.999, 9999999.999);
    m_bsE->setDecimals(3);
    bsForm->addRow("BS " + xLabel + ":", m_bsE);
    
    m_bsN = new QDoubleSpinBox();
    m_bsN->setRange(-9999999.999, 9999999.999);
    m_bsN->setDecimals(3);
    bsForm->addRow("BS " + yLabel + ":", m_bsN);
    
    bsGroupLayout->addLayout(bsForm);
    
    // Bearing
    QHBoxLayout* azLayout = new QHBoxLayout();
    m_azimuth = new QDoubleSpinBox();
    m_azimuth->setRange(0.0, 360.0);
    m_azimuth->setDecimals(4);
    m_azimuth->setSuffix("°");
    
    m_calcAzimuthBtn = new QPushButton("Calc Bearing");
    connect(m_calcAzimuthBtn, &QPushButton::clicked, this, &StationSetupDialog::onCalculateAzimuth);
    
    azLayout->addWidget(new QLabel("Bearing:"));
    azLayout->addWidget(m_azimuth);
    azLayout->addWidget(m_calcAzimuthBtn);
    
    bsGroupLayout->addLayout(azLayout);
    
    mainLayout->addWidget(bsGroup);
    
    // --- Buttons ---
    QHBoxLayout* btnLayout = new QHBoxLayout();
    
    QPushButton* clearBtn = new QPushButton("Clear Station");
    connect(clearBtn, &QPushButton::clicked, this, &StationSetupDialog::onClearStation);
    
    QPushButton* cancelBtn = new QPushButton("Cancel");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    
    QPushButton* setBtn = new QPushButton("Set Station");
    setBtn->setDefault(true);
    connect(setBtn, &QPushButton::clicked, this, &StationSetupDialog::onSetStation);
    
    btnLayout->addWidget(clearBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(setBtn);
    
    mainLayout->addLayout(btnLayout);
    
    onModeChanged(true); // Init state
}

void StationSetupDialog::loadCurrent()
{
    if (!m_canvas) return;
    const auto& s = m_canvas->station();
    
    if (s.hasStation) {
        m_stnName->setText(s.stationName);
        m_stnE->setValue(s.stationPos.x());
        m_stnN->setValue(s.stationPos.y());
        // Z not stored in CanvasStation struct currently (only 2D visualization), but inputs exist.
        // We'll leave Z at 0 or previously entered.
    } else {
        m_stnName->setText("STN1");
        m_stnE->setValue(1000.0);
        m_stnN->setValue(1000.0);
    }
    
    if (s.hasBacksight) {
        m_bsName->setText(s.backsightName);
        m_bsE->setValue(s.backsightPos.x());
        m_bsN->setValue(s.backsightPos.y());
        onCalculateAzimuth(); // Update azimuth display
    }
}

void StationSetupDialog::onModeChanged(bool useCoords)
{
    m_bsE->setEnabled(useCoords);
    m_bsN->setEnabled(useCoords);
    m_calcAzimuthBtn->setEnabled(useCoords);
    m_azimuth->setReadOnly(useCoords); // If using coords, azimuth is derived. If not, user enters it.
}

void StationSetupDialog::onCalculateAzimuth()
{
    double dE = m_bsE->value() - m_stnE->value();
    double dN = m_bsN->value() - m_stnN->value();
    
    // Qt uses Y down for screen, but survey coords are typically Y (North) up.
    // However, our internal logic often treats Y as North directly if we are in mathematical coords?
    // Let's assume math coords: Atan2(dN, dE) is math angle.
    // Survey Bearing/Azimuth is from North (Y axis) clockwise.
    // Azimuth = atan2(dE, dN).
    // Note: atan2(y, x) in C++ is math convention. 
    // For North-Zero Clockwise: atan2(dE, dN).
    
    double rad = std::atan2(dE, dN);
    double deg = qRadiansToDegrees(rad);
    
    if (deg < 0) deg += 360.0;
    
    m_azimuth->setValue(deg);
}

void StationSetupDialog::onSetStation()
{
    if (!m_canvas) return;
    
    CanvasStation s;
    s.hasStation = true;
    s.stationName = m_stnName->text();
    s.stationPos = QPointF(m_stnE->value(), m_stnN->value());
    
    s.hasBacksight = true;
    s.backsightName = m_bsName->text();
    
    if (m_useBsCoords->isChecked()) {
        s.backsightPos = QPointF(m_bsE->value(), m_bsN->value());
        // Ensure azimuth matches (re-calc just in case)
        // onCalculateAzimuth(); // Update UI
    } else {
        // Compute BS pos from Azimuth (visual only, say 100m away)
        double dist = 100.0;
        double azRad = qDegreesToRadians(m_azimuth->value());
        double dE = dist * std::sin(azRad);
        double dN = dist * std::cos(azRad);
        s.backsightPos = QPointF(s.stationPos.x() + dE, s.stationPos.y() + dN);
    }
    
    m_canvas->setStation(s);
    
    QMessageBox::information(this, "Station Setup", "Station updated successfully.");
    accept();
}

void StationSetupDialog::onClearStation()
{
    if (!m_canvas) return;
    CanvasStation s; // Default empty
    m_canvas->setStation(s);
    loadCurrent(); // UI reset
}
