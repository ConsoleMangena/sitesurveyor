#include "tools/check_point_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QtMath>
#include <QSettings>

CheckPointDialog::CheckPointDialog(const CanvasStation& station, 
                                   const QPointF& checkPos, 
                                   const QString& checkName, 
                                   QWidget *parent)
    : QDialog(parent), m_station(station), m_checkPos(checkPos), m_checkName(checkName)
{
    setupUi();
    
    // Calculate Theoretical Values
    double dE = m_checkPos.x() - m_station.stationPos.x();
    double dN = m_checkPos.y() - m_station.stationPos.y();
    
    // Bearing (from North clockwise)
    double rad = std::atan2(dE, dN);
    m_theoBearing = qRadiansToDegrees(rad);
    if (m_theoBearing < 0) m_theoBearing += 360.0;
    
    m_theoDist = std::sqrt(dE*dE + dN*dN);
    
    // Backsight Bearing
    if (m_station.hasBacksight) {
        double bsE = m_station.backsightPos.x() - m_station.stationPos.x();
        double bsN = m_station.backsightPos.y() - m_station.stationPos.y();
        double bsRad = std::atan2(bsE, bsN);
        m_bsBearing = qRadiansToDegrees(bsRad);
        if (m_bsBearing < 0) m_bsBearing += 360.0;
    } else {
        m_bsBearing = 0.0;
    }
    
    // Calculate Turn Angle (Theo Bearing - BS Bearing)
    double turnAngle = m_theoBearing - m_bsBearing;
    if (turnAngle < 0) turnAngle += 360.0;
    
    // Format Display
    QString info = QString("Checking Point: %1\n\n"
                           "Theoretical Data (from %2):\n"
                           "  Bearing: %3°\n"
                           "  Distance: %4 m\n"
                           "  Turn Angle: %5°")
                           .arg(m_checkName)
                           .arg(m_station.stationName)
                           .arg(m_theoBearing, 0, 'f', 4)
                           .arg(m_theoDist, 0, 'f', 3)
                           .arg(turnAngle, 0, 'f', 4);
                           
    m_targetInfoLabel->setText(info);
    
    // Init Observations with Theoretical values (for easy check)
    m_obsBearing->setValue(m_theoBearing);
    m_obsDist->setValue(m_theoDist);
    
    calculateResiduals();
}

void CheckPointDialog::setupUi()
{
    setWindowTitle("Verify Check Point");
    resize(400, 500);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Target Info
    QGroupBox* targetGroup = new QGroupBox("Target Information");
    QVBoxLayout* targetLayout = new QVBoxLayout(targetGroup);
    m_targetInfoLabel = new QLabel();
    m_targetInfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    targetLayout->addWidget(m_targetInfoLabel);
    mainLayout->addWidget(targetGroup);
    
    // Observation Input
    QGroupBox* obsGroup = new QGroupBox("Actual Observation");
    QFormLayout* obsLayout = new QFormLayout(obsGroup);
    
    m_obsBearing = new QDoubleSpinBox();
    m_obsBearing->setRange(0.0, 360.0);
    m_obsBearing->setDecimals(4);
    m_obsBearing->setSuffix("°");
    connect(m_obsBearing, &QDoubleSpinBox::valueChanged, this, &CheckPointDialog::calculateResiduals);
    obsLayout->addRow("Observed Bearing:", m_obsBearing);
    
    m_obsDist = new QDoubleSpinBox();
    m_obsDist->setRange(0.0, 999999.999);
    m_obsDist->setDecimals(3);
    m_obsDist->setSuffix(" m");
    connect(m_obsDist, &QDoubleSpinBox::valueChanged, this, &CheckPointDialog::calculateResiduals);
    obsLayout->addRow("Observed Distance:", m_obsDist);
    
    mainLayout->addWidget(obsGroup);
    
    // Results
    QGroupBox* resGroup = new QGroupBox("Residuals (Observed - Theoretical)");
    QFormLayout* resLayout = new QFormLayout(resGroup);
    
    m_resdAngle = new QLineEdit();
    m_resdAngle->setReadOnly(true);
    resLayout->addRow("dAngle:", m_resdAngle);
    
    m_resdDist = new QLineEdit();
    m_resdDist->setReadOnly(true);
    resLayout->addRow("dDist:", m_resdDist);
    
    QSettings settings;
    bool swapXY = settings.value("coordinates/swapXY", false).toBool();
    QString xLabel = swapXY ? "dY:" : "dX:";
    QString yLabel = swapXY ? "dX:" : "dY:";
    
    m_resdEast = new QLineEdit();
    m_resdEast->setReadOnly(true);
    resLayout->addRow("dEasting (" + xLabel + "):", m_resdEast);
    
    m_resdNorth = new QLineEdit();
    m_resdNorth->setReadOnly(true);
    resLayout->addRow("dNorthing (" + yLabel + "):", m_resdNorth);
    
    mainLayout->addWidget(resGroup);
    
    // Buttons
    QPushButton* closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    mainLayout->addWidget(closeBtn);
}

void CheckPointDialog::calculateResiduals()
{
    double obsBrg = m_obsBearing->value();
    double obsDist = m_obsDist->value();
    
    // Angular Residual
    double dAng = obsBrg - m_theoBearing;
    while (dAng > 180.0) dAng -= 360.0;
    while (dAng < -180.0) dAng += 360.0;
    
    // Distance Residual
    double dDist = obsDist - m_theoDist;
    
    // Coordinate Residuals
    // Calculate Observed Coordinates based on Station + Obs
    double obsRad = qDegreesToRadians(obsBrg);
    double obsE = m_station.stationPos.x() + obsDist * std::sin(obsRad);
    double obsN = m_station.stationPos.y() + obsDist * std::cos(obsRad);
    
    double dE = obsE - m_checkPos.x();
    double dN = obsN - m_checkPos.y();
    
    // Update UI
    m_resdAngle->setText(QString::number(dAng, 'f', 4) + "°");
    m_resdDist->setText(QString::number(dDist, 'f', 3) + " m");
    m_resdEast->setText(QString::number(dE, 'f', 3));
    m_resdNorth->setText(QString::number(dN, 'f', 3));
    
    // Color coding
    QPalette p = m_resdAngle->palette();
    if (std::abs(dE) > 0.05 || std::abs(dN) > 0.05) {
        m_resdEast->setStyleSheet("color: red; font-weight: bold;");
        m_resdNorth->setStyleSheet("color: red; font-weight: bold;");
    } else {
        m_resdEast->setStyleSheet("color: green;");
        m_resdNorth->setStyleSheet("color: green;");
    }
}
