#include "tools/join_dialog.h"
#include "canvas/canvaswidget.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QSettings>
#include <cmath>

JoinDialog::JoinDialog(CanvasWidget* canvas, QWidget* parent)
    : QDialog(parent)
    , m_canvas(canvas)
{
    setWindowTitle("Join Calculation (Inverse)");
    setMinimumWidth(400);
    setupUi();
}

void JoinDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Check if Swap X/Y is enabled
    QSettings settings;
    bool swapXY = settings.value("coordinates/swapXY", false).toBool();
    QString firstLabel = swapXY ? "Y:" : "X:";
    QString secondLabel = swapXY ? "X:" : "Y:";
    
    // Point A Group
    QGroupBox* groupA = new QGroupBox("Point A (From)");
    QGridLayout* layoutA = new QGridLayout(groupA);
    
    m_eastA = new QDoubleSpinBox();
    m_eastA->setRange(-9999999.999, 9999999.999);
    m_eastA->setDecimals(3);
    m_northA = new QDoubleSpinBox();
    m_northA->setRange(-9999999.999, 9999999.999);
    m_northA->setDecimals(3);
    
    QPushButton* pickABtn = new QPushButton("Pick");
    connect(pickABtn, &QPushButton::clicked, this, &JoinDialog::pickPointA);
    
    layoutA->addWidget(new QLabel(firstLabel), 0, 0);
    layoutA->addWidget(swapXY ? m_northA : m_eastA, 0, 1);
    layoutA->addWidget(new QLabel(secondLabel), 1, 0);
    layoutA->addWidget(swapXY ? m_eastA : m_northA, 1, 1);
    layoutA->addWidget(pickABtn, 0, 2, 2, 1);
    
    mainLayout->addWidget(groupA);
    
    // Point B Group
    QGroupBox* groupB = new QGroupBox("Point B (To)");
    QGridLayout* layoutB = new QGridLayout(groupB);
    
    m_eastB = new QDoubleSpinBox();
    m_eastB->setRange(-9999999.999, 9999999.999);
    m_eastB->setDecimals(3);
    m_northB = new QDoubleSpinBox();
    m_northB->setRange(-9999999.999, 9999999.999);
    m_northB->setDecimals(3);
    
    QPushButton* pickBBtn = new QPushButton("Pick");
    connect(pickBBtn, &QPushButton::clicked, this, &JoinDialog::pickPointB);
    
    layoutB->addWidget(new QLabel(firstLabel), 0, 0);
    layoutB->addWidget(swapXY ? m_northB : m_eastB, 0, 1);
    layoutB->addWidget(new QLabel(secondLabel), 1, 0);
    layoutB->addWidget(swapXY ? m_eastB : m_northB, 1, 1);
    layoutB->addWidget(pickBBtn, 0, 2, 2, 1);
    
    mainLayout->addWidget(groupB);
    
    // Calculate Button
    QPushButton* calcBtn = new QPushButton("Calculate");
    calcBtn->setStyleSheet("font-weight: bold; padding: 8px;");
    connect(calcBtn, &QPushButton::clicked, this, &JoinDialog::calculate);
    mainLayout->addWidget(calcBtn);
    
    // Result Label
    m_resultLabel = new QLabel("Bearing: Not Calculated\nDistance: Not Calculated");
    m_resultLabel->setStyleSheet("font-weight: bold; padding: 10px; background-color: #2d2d30; border-radius: 4px;");
    mainLayout->addWidget(m_resultLabel);
    
    // Dialog buttons
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void JoinDialog::calculate()
{
    double dE = m_eastB->value() - m_eastA->value();
    double dN = m_northB->value() - m_northA->value();
    
    // Calculate distance
    double distance = sqrt(dE * dE + dN * dN);
    
    // Calculate bearing (azimuth)
    double bearingRad = atan2(dE, dN);  // atan2(E, N) gives bearing from North
    double bearingDeg = bearingRad * 180.0 / M_PI;
    
    // Normalize to 0-360
    if (bearingDeg < 0) {
        bearingDeg += 360.0;
    }
    
    // Check if South Azimuth is enabled
    QSettings settings;
    bool southAzimuth = settings.value("coordinates/southAzimuth", false).toBool();
    
    if (southAzimuth) {
        // Convert to South Azimuth: 0° = South
        bearingDeg = fmod(bearingDeg + 180.0, 360.0);
    }
    
    // Convert to DMS
    int degrees = static_cast<int>(bearingDeg);
    double minFloat = (bearingDeg - degrees) * 60.0;
    int minutes = static_cast<int>(minFloat);
    double seconds = (minFloat - minutes) * 60.0;
    
    QString bearingStr = QString("%1° %2' %3\"")
        .arg(degrees)
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 5, 'f', 2, QChar('0'));
    
    m_resultLabel->setText(QString("Bearing: %1\nDistance: %2 m\n\nΔE: %3\nΔN: %4")
                           .arg(bearingStr)
                           .arg(distance, 0, 'f', 3)
                           .arg(dE, 0, 'f', 3)
                           .arg(dN, 0, 'f', 3));
}

void JoinDialog::pickPointA()
{
    // TODO: Implement pick from canvas
}

void JoinDialog::pickPointB()
{
    // TODO: Implement pick from canvas
}
