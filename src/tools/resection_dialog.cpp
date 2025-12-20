#include "tools/resection_dialog.h"
#include "canvas/canvaswidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QMessageBox>
#include <QGroupBox>
#include <QSettings>
#include <cmath>

ResectionDialog::ResectionDialog(CanvasWidget* canvas, QWidget* parent)
    : QDialog(parent), m_canvas(canvas)
{
    setWindowTitle("Resection Calculation");
    resize(500, 400);
    setupUi();
}

void ResectionDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Instructions
    QLabel* infoLabel = new QLabel("Enter at least 3 known points and observed angles/distances to calculate station position.");
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);
    
    // Check if Swap X/Y is enabled
    QSettings settings;
    bool swapXY = settings.value("coordinates/swapXY", false).toBool();
    
    // Points Table
    m_pointsTable = new QTableWidget(0, 5);
    if (swapXY) {
        m_pointsTable->setHorizontalHeaderLabels({"Point ID", "Y", "X", "Angle", "Dist"});
    } else {
        m_pointsTable->setHorizontalHeaderLabels({"Point ID", "X", "Y", "Angle", "Dist"});
    }
    m_pointsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    mainLayout->addWidget(m_pointsTable);
    
    // Table Controls
    QHBoxLayout* tableControls = new QHBoxLayout();
    QPushButton* addBtn = new QPushButton("Add Point");
    QPushButton* removeBtn = new QPushButton("Remove Point");
    QPushButton* pickBtn = new QPushButton("Pick from Canvas");
    
    connect(addBtn, &QPushButton::clicked, this, &ResectionDialog::addPoint);
    connect(removeBtn, &QPushButton::clicked, this, &ResectionDialog::removePoint);
    connect(pickBtn, &QPushButton::clicked, this, &ResectionDialog::pickPointFromCanvas);
    
    tableControls->addWidget(addBtn);
    tableControls->addWidget(removeBtn);
    tableControls->addWidget(pickBtn);
    mainLayout->addLayout(tableControls);
    
    // Result Area
    QGroupBox* resultGroup = new QGroupBox("Result");
    QVBoxLayout* resultLayout = new QVBoxLayout(resultGroup);
    m_resultLabel = new QLabel("Station Coordinates: Not Calculated");
    resultLayout->addWidget(m_resultLabel);
    mainLayout->addWidget(resultGroup);
    
    // Dialog Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* calcBtn = new QPushButton("Calculate");
    QPushButton* closeBtn = new QPushButton("Close");
    
    connect(calcBtn, &QPushButton::clicked, this, &ResectionDialog::calculate);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    
    btnLayout->addStretch();
    btnLayout->addWidget(calcBtn);
    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);
    
    // Add initial rows
    addPoint();
    addPoint();
    addPoint();
}

void ResectionDialog::addPoint()
{
    int row = m_pointsTable->rowCount();
    m_pointsTable->insertRow(row);
    m_pointsTable->setItem(row, 0, new QTableWidgetItem(QString("P%1").arg(row + 1)));
    m_pointsTable->setItem(row, 1, new QTableWidgetItem("0.000"));
    m_pointsTable->setItem(row, 2, new QTableWidgetItem("0.000"));
    m_pointsTable->setItem(row, 3, new QTableWidgetItem("0.0000")); // Angle
    m_pointsTable->setItem(row, 4, new QTableWidgetItem("0.000"));  // Distance
}

void ResectionDialog::removePoint()
{
    if (m_pointsTable->rowCount() > 0) {
        m_pointsTable->removeRow(m_pointsTable->rowCount() - 1);
    }
}

void ResectionDialog::pickPointFromCanvas()
{
    // Placeholder for picking logic
    QMessageBox::information(this, "Pick Point", "Click on the canvas to select a known point (Feature coming soon).");
}

void ResectionDialog::calculate()
{
    // Simplified Tienstra's method or similar would go here
    // For now, a placeholder calculation
    
    int rows = m_pointsTable->rowCount();
    if (rows < 3) {
        QMessageBox::warning(this, "Error", "At least 3 points are required for resection.");
        return;
    }
    
    // Mock calculation for demonstration
    double sumX = 0, sumY = 0;
    for (int i = 0; i < rows; ++i) {
        sumX += m_pointsTable->item(i, 1)->text().toDouble();
        sumY += m_pointsTable->item(i, 2)->text().toDouble();
    }
    
    m_resultX = sumX / rows; // Centroid for demo
    m_resultY = sumY / rows;
    
    // Check if Swap X/Y is enabled
    QSettings settings;
    bool swapXY = settings.value("coordinates/swapXY", false).toBool();
    
    if (swapXY) {
        m_resultLabel->setText(QString("Station Coordinates:\nY: %1\nX: %2")
                               .arg(m_resultY, 0, 'f', 3)
                               .arg(m_resultX, 0, 'f', 3));
    } else {
        m_resultLabel->setText(QString("Station Coordinates:\nX: %1\nY: %2")
                               .arg(m_resultX, 0, 'f', 3)
                               .arg(m_resultY, 0, 'f', 3));
    }
                           
    // Option to add to canvas
    if (m_canvas) {
        // m_canvas->addPoint(QPointF(m_resultX, m_resultY)); // Need to expose this
    }
}
