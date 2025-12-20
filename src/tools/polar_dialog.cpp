#include "tools/polar_dialog.h"
#include "canvas/canvaswidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QSettings>
#include <cmath>

PolarDialog::PolarDialog(CanvasWidget* canvas, QWidget* parent)
    : QDialog(parent)
    , m_canvas(canvas)
{
    setWindowTitle("Polar Calculation");
    setMinimumWidth(350);
    setupUi();
}

void PolarDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Check if Swap X/Y is enabled
    QSettings settings;
    bool swapXY = settings.value("coordinates/swapXY", false).toBool();
    QString firstLabel = swapXY ? "Y:" : "X:";
    QString secondLabel = swapXY ? "X:" : "Y:";
    
    // Start Point Group
    QGroupBox* startGroup = new QGroupBox("Start Point (Known)");
    QGridLayout* startLayout = new QGridLayout(startGroup);
    
    m_startE = new QDoubleSpinBox();
    m_startE->setRange(-9999999.999, 9999999.999);
    m_startE->setDecimals(3);
    m_startN = new QDoubleSpinBox();
    m_startN->setRange(-9999999.999, 9999999.999);
    m_startN->setDecimals(3);
    
    QPushButton* pickBtn = new QPushButton("Pick");
    connect(pickBtn, &QPushButton::clicked, this, &PolarDialog::pickStartPoint);
    
    startLayout->addWidget(new QLabel(firstLabel), 0, 0);
    startLayout->addWidget(swapXY ? m_startN : m_startE, 0, 1);
    startLayout->addWidget(new QLabel(secondLabel), 1, 0);
    startLayout->addWidget(swapXY ? m_startE : m_startN, 1, 1);
    startLayout->addWidget(pickBtn, 0, 2, 2, 1);
    
    mainLayout->addWidget(startGroup);
    
    // Bearing Group
    QGroupBox* bearingGroup = new QGroupBox("Bearing (D° M' S\")");
    QHBoxLayout* bearingLayout = new QHBoxLayout(bearingGroup);
    
    m_bearingDeg = new QDoubleSpinBox();
    m_bearingDeg->setRange(0, 360);
    m_bearingDeg->setDecimals(0);
    m_bearingDeg->setSuffix("°");
    
    m_bearingMin = new QDoubleSpinBox();
    m_bearingMin->setRange(0, 59);
    m_bearingMin->setDecimals(0);
    m_bearingMin->setSuffix("'");
    
    m_bearingSec = new QDoubleSpinBox();
    m_bearingSec->setRange(0, 59.999);
    m_bearingSec->setDecimals(3);
    m_bearingSec->setSuffix("\"");
    
    bearingLayout->addWidget(m_bearingDeg);
    bearingLayout->addWidget(m_bearingMin);
    bearingLayout->addWidget(m_bearingSec);
    
    mainLayout->addWidget(bearingGroup);
    
    // Distance Input
    QGroupBox* distGroup = new QGroupBox("Distance");
    QHBoxLayout* distLayout = new QHBoxLayout(distGroup);
    
    m_distance = new QDoubleSpinBox();
    m_distance->setRange(0.001, 9999999.999);
    m_distance->setDecimals(3);
    m_distance->setValue(100.0);
    m_distance->setSuffix(" m");
    
    distLayout->addWidget(m_distance);
    mainLayout->addWidget(distGroup);
    
    // Calculate Button
    QPushButton* calcBtn = new QPushButton("Calculate");
    calcBtn->setStyleSheet("font-weight: bold; padding: 8px;");
    connect(calcBtn, &QPushButton::clicked, this, &PolarDialog::calculate);
    mainLayout->addWidget(calcBtn);
    
    // Result Label
    m_resultLabel = new QLabel("End Point: Not Calculated");
    m_resultLabel->setStyleSheet("font-weight: bold; padding: 10px; background-color: #2d2d30; border-radius: 4px;");
    mainLayout->addWidget(m_resultLabel);
    
    // Add to Canvas Button
    QPushButton* addBtn = new QPushButton("Add Point to Canvas");
    connect(addBtn, &QPushButton::clicked, this, &PolarDialog::addToCanvas);
    mainLayout->addWidget(addBtn);
    
    // Dialog buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void PolarDialog::calculate()
{
    // Convert bearing to decimal degrees
    double bearing = m_bearingDeg->value() + 
                     m_bearingMin->value() / 60.0 + 
                     m_bearingSec->value() / 3600.0;
    
    // Check if South Azimuth is enabled
    QSettings settings;
    bool southAzimuth = settings.value("coordinates/southAzimuth", false).toBool();
    bool swapXY = settings.value("coordinates/swapXY", false).toBool();
    
    // Convert bearing to radians
    double bearingRad;
    if (southAzimuth) {
        // South Azimuth: 0° = South, 90° = West
        bearingRad = (180.0 - bearing) * M_PI / 180.0;
    } else {
        // Standard: 0° = North, 90° = East
        bearingRad = bearing * M_PI / 180.0;
    }
    
    double distance = m_distance->value();
    
    // Calculate delta coordinates
    double dE = distance * sin(bearingRad);
    double dN = distance * cos(bearingRad);
    
    // Calculate end point
    m_resultE = m_startE->value() + dE;
    m_resultN = m_startN->value() + dN;
    
    // Display result
    if (swapXY) {
        m_resultLabel->setText(QString("End Point:\nY: %1\nX: %2")
                               .arg(m_resultN, 0, 'f', 3)
                               .arg(m_resultE, 0, 'f', 3));
    } else {
        m_resultLabel->setText(QString("End Point:\nX: %1\nY: %2")
                               .arg(m_resultE, 0, 'f', 3)
                               .arg(m_resultN, 0, 'f', 3));
    }
}

void PolarDialog::pickStartPoint()
{
    // TODO: Implement pick from canvas
}

void PolarDialog::addToCanvas()
{
    if (!m_canvas || (m_resultE == 0.0 && m_resultN == 0.0)) return;
    
    CanvasPeg peg;
    peg.position = QPointF(m_resultE, m_resultN);
    peg.name = "POLAR";
    peg.color = Qt::red;
    m_canvas->addPeg(peg);
}
