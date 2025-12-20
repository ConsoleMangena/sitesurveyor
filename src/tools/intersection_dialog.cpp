#include "tools/intersection_dialog.h"
#include "canvas/canvaswidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QRadioButton>
#include <QGroupBox>
#include <QPushButton>
#include <QMessageBox>
#include <QSettings>
#include <cmath>

IntersectionDialog::IntersectionDialog(CanvasWidget* canvas, QWidget* parent)
    : QDialog(parent), m_canvas(canvas)
{
    setWindowTitle("Intersection Calculation");
    resize(400, 450);
    setupUi();
}

void IntersectionDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Method Selection
    QGroupBox* methodGroup = new QGroupBox("Intersection Method");
    QVBoxLayout* methodLayout = new QVBoxLayout(methodGroup);
    m_modeDistDist = new QRadioButton("Distance - Distance");
    m_modeBearingBearing = new QRadioButton("Bearing - Bearing");
    m_modeDistDist->setChecked(true);
    
    connect(m_modeDistDist, &QRadioButton::toggled, this, &IntersectionDialog::updateInputMode);
    
    methodLayout->addWidget(m_modeDistDist);
    methodLayout->addWidget(m_modeBearingBearing);
    mainLayout->addWidget(methodGroup);
    
    // Check if Swap X/Y is enabled
    QSettings settings;
    bool swapXY = settings.value("coordinates/swapXY", false).toBool();
    QString firstLabel = swapXY ? "Y:" : "X:";
    QString secondLabel = swapXY ? "X:" : "Y:";
    
    // Point A Input
    QGroupBox* groupA = new QGroupBox("Point A (Known)");
    QGridLayout* layoutA = new QGridLayout(groupA);
    
    m_eastA = new QDoubleSpinBox();
    m_eastA->setRange(-9999999.999, 9999999.999);
    m_eastA->setDecimals(3);
    m_northA = new QDoubleSpinBox();
    m_northA->setRange(-9999999.999, 9999999.999);
    m_northA->setDecimals(3);
    
    QPushButton* pickABtn = new QPushButton("Pick");
    connect(pickABtn, &QPushButton::clicked, this, &IntersectionDialog::pickPointA);
    
    layoutA->addWidget(new QLabel(firstLabel), 0, 0);
    layoutA->addWidget(swapXY ? m_northA : m_eastA, 0, 1);
    layoutA->addWidget(new QLabel(secondLabel), 1, 0);
    layoutA->addWidget(swapXY ? m_eastA : m_northA, 1, 1);
    layoutA->addWidget(pickABtn, 0, 2, 2, 1);
    
    mainLayout->addWidget(groupA);
    
    // Point B Input
    QGroupBox* groupB = new QGroupBox("Point B (Known)");
    QGridLayout* layoutB = new QGridLayout(groupB);
    
    m_eastB = new QDoubleSpinBox();
    m_eastB->setRange(-9999999.999, 9999999.999);
    m_eastB->setDecimals(3);
    m_northB = new QDoubleSpinBox();
    m_northB->setRange(-9999999.999, 9999999.999);
    m_northB->setDecimals(3);
    
    QPushButton* pickBBtn = new QPushButton("Pick");
    connect(pickBBtn, &QPushButton::clicked, this, &IntersectionDialog::pickPointB);
    
    layoutB->addWidget(new QLabel(firstLabel), 0, 0);
    layoutB->addWidget(swapXY ? m_northB : m_eastB, 0, 1);
    layoutB->addWidget(new QLabel(secondLabel), 1, 0);
    layoutB->addWidget(swapXY ? m_eastB : m_northB, 1, 1);
    layoutB->addWidget(pickBBtn, 0, 2, 2, 1);
    
    mainLayout->addWidget(groupB);
    
    // Observations
    QGroupBox* obsGroup = new QGroupBox("Observations");
    QGridLayout* obsLayout = new QGridLayout(obsGroup);
    
    m_obsA = new QDoubleSpinBox();
    m_obsA->setRange(0.0, 9999999.999);
    m_obsA->setDecimals(4);
    
    m_obsB = new QDoubleSpinBox();
    m_obsB->setRange(0.0, 9999999.999);
    m_obsB->setDecimals(4);
    
    obsLayout->addWidget(new QLabel("From A:"), 0, 0);
    obsLayout->addWidget(m_obsA, 0, 1);
    obsLayout->addWidget(new QLabel("From B:"), 1, 0);
    obsLayout->addWidget(m_obsB, 1, 1);
    
    mainLayout->addWidget(obsGroup);
    
    // Result
    QGroupBox* resultGroup = new QGroupBox("Result");
    QVBoxLayout* resultLayout = new QVBoxLayout(resultGroup);
    m_resultLabel = new QLabel("Coordinates: Not Calculated");
    resultLayout->addWidget(m_resultLabel);
    mainLayout->addWidget(resultGroup);
    
    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* calcBtn = new QPushButton("Calculate");
    QPushButton* closeBtn = new QPushButton("Close");
    
    connect(calcBtn, &QPushButton::clicked, this, &IntersectionDialog::calculate);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    
    btnLayout->addStretch();
    btnLayout->addWidget(calcBtn);
    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);
    
    updateInputMode();
}

void IntersectionDialog::updateInputMode()
{
    if (m_modeDistDist->isChecked()) {
        m_obsA->setSuffix(" m");
        m_obsB->setSuffix(" m");
    } else {
        m_obsA->setSuffix(" deg");
        m_obsB->setSuffix(" deg");
    }
}

void IntersectionDialog::pickPointA()
{
    QMessageBox::information(this, "Pick Point", "Click on canvas to pick Point A (Feature coming soon).");
}

void IntersectionDialog::pickPointB()
{
    QMessageBox::information(this, "Pick Point", "Click on canvas to pick Point B (Feature coming soon).");
}

void IntersectionDialog::calculate()
{
    double ea = m_eastA->value();
    double na = m_northA->value();
    double eb = m_eastB->value();
    double nb = m_northB->value();
    
    double obsA = m_obsA->value();
    double obsB = m_obsB->value();
    
    double resultE = 0.0;
    double resultN = 0.0;
    
    // TODO: Implement actual intersection calculation algorithms
    // Real implementation would use trigonometry based on selected mode
    
    if (m_modeDistDist->isChecked()) {
        // Circle-Circle intersection logic would go here
        // For now using midpoint as placeholder, obsA and obsB are radii
        (void)obsA; (void)obsB;  // Will be used when algorithm is implemented
        resultE = (ea + eb) / 2.0; // Midpoint for demo
        resultN = (na + nb) / 2.0;
    } else {
        // Bearing-Bearing intersection logic would go here
        // obsA and obsB are bearings in degrees
        (void)obsA; (void)obsB;  // Will be used when algorithm is implemented
        resultE = (ea + eb) / 2.0; 
        resultN = (na + nb) / 2.0;
    }
    
    // Check if Swap X/Y is enabled
    QSettings settings;
    bool swapXY = settings.value("coordinates/swapXY", false).toBool();
    
    if (swapXY) {
        m_resultLabel->setText(QString("Coordinates:\nY: %1\nX: %2")
                               .arg(resultN, 0, 'f', 3)
                               .arg(resultE, 0, 'f', 3));
    } else {
        m_resultLabel->setText(QString("Coordinates:\nX: %1\nY: %2")
                               .arg(resultE, 0, 'f', 3)
                               .arg(resultN, 0, 'f', 3));
    }
}
