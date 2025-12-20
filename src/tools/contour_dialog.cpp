#include "tools/contour_dialog.h"
#include "canvas/canvaswidget.h"
#include "gdal/geosbridge.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QMessageBox>

ContourDialog::ContourDialog(CanvasWidget* canvas, QWidget *parent)
    : QDialog(parent), m_canvas(canvas)
{
    setupUi();
}

void ContourDialog::setupUi()
{
    setWindowTitle("Contour Generator");
    resize(400, 350);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Instructions
    QLabel* infoLabel = new QLabel("Generate contour lines from survey points using Delaunay triangulation.");
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);
    
    // Settings Group
    QGroupBox* settingsGroup = new QGroupBox("Contour Settings");
    QFormLayout* formLayout = new QFormLayout(settingsGroup);
    
    m_intervalSpin = new QDoubleSpinBox();
    m_intervalSpin->setRange(0.1, 100.0);
    m_intervalSpin->setValue(1.0);
    m_intervalSpin->setSuffix(" m");
    m_intervalSpin->setDecimals(2);
    formLayout->addRow("Contour Interval:", m_intervalSpin);
    
    m_majorFactorSpin = new QSpinBox();
    m_majorFactorSpin->setRange(1, 20);
    m_majorFactorSpin->setValue(5);
    formLayout->addRow("Major Interval Factor:", m_majorFactorSpin);
    
    m_autoRangeCheck = new QCheckBox("Auto-detect elevation range");
    m_autoRangeCheck->setChecked(true);
    formLayout->addRow("", m_autoRangeCheck);
    
    m_minElevSpin = new QDoubleSpinBox();
    m_minElevSpin->setRange(-10000, 10000);
    m_minElevSpin->setValue(0);
    m_minElevSpin->setSuffix(" m");
    m_minElevSpin->setEnabled(false);
    formLayout->addRow("Min Elevation:", m_minElevSpin);
    
    m_maxElevSpin = new QDoubleSpinBox();
    m_maxElevSpin->setRange(-10000, 10000);
    m_maxElevSpin->setValue(100);
    m_maxElevSpin->setSuffix(" m");
    m_maxElevSpin->setEnabled(false);
    formLayout->addRow("Max Elevation:", m_maxElevSpin);
    
    connect(m_autoRangeCheck, &QCheckBox::toggled, [this](bool checked) {
        m_minElevSpin->setEnabled(!checked);
        m_maxElevSpin->setEnabled(!checked);
    });
    
    mainLayout->addWidget(settingsGroup);
    
    // Status
    m_statusLabel = new QLabel("Ready. Click 'Generate' to create contours.");
    m_statusLabel->setStyleSheet("color: gray;");
    mainLayout->addWidget(m_statusLabel);
    
    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    
    m_generateBtn = new QPushButton("Generate");
    connect(m_generateBtn, &QPushButton::clicked, this, &ContourDialog::generate);
    btnLayout->addWidget(m_generateBtn);
    
    m_applyBtn = new QPushButton("Apply to Canvas");
    m_applyBtn->setEnabled(false);
    connect(m_applyBtn, &QPushButton::clicked, this, &ContourDialog::applyToCanvas);
    btnLayout->addWidget(m_applyBtn);
    
    QPushButton* clearBtn = new QPushButton("Clear");
    connect(clearBtn, &QPushButton::clicked, this, &ContourDialog::clearContours);
    btnLayout->addWidget(clearBtn);
    
    mainLayout->addLayout(btnLayout);
    
    // Dialog buttons
    QHBoxLayout* dialogBtns = new QHBoxLayout();
    dialogBtns->addStretch();
    QPushButton* closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    dialogBtns->addWidget(closeBtn);
    mainLayout->addLayout(dialogBtns);
}

void ContourDialog::generate()
{
    if (!m_canvas) return;
    
    const auto& pegs = m_canvas->pegs();
    if (pegs.size() < 3) {
        QMessageBox::warning(this, "Contour Generator", "Need at least 3 pegs with Z coordinates.");
        return;
    }
    
    // Collect 3D points
    QVector<GeosBridge::Point3D> points3D;
    for (const auto& peg : pegs) {
        points3D.append(GeosBridge::Point3D(peg.position.x(), peg.position.y(), peg.z));
    }
    
    // Get elevation range
    double minZ = points3D[0].z, maxZ = points3D[0].z;
    for (const auto& p : points3D) {
        minZ = qMin(minZ, p.z);
        maxZ = qMax(maxZ, p.z);
    }
    
    if (m_autoRangeCheck->isChecked()) {
        m_minElevSpin->setValue(minZ);
        m_maxElevSpin->setValue(maxZ);
    } else {
        minZ = m_minElevSpin->value();
        maxZ = m_maxElevSpin->value();
    }
    
    double interval = m_intervalSpin->value();
    double majorInterval = interval * m_majorFactorSpin->value();
    
    // Generate triangulation
    QVector<QPointF> points2D;
    for (const auto& p : points3D) {
        points2D.append(QPointF(p.x, p.y));
    }
    
    auto triangles = GeosBridge::delaunayTriangulate(points2D);
    if (triangles.isEmpty()) {
        QMessageBox::warning(this, "Contour Generator", "Failed to create triangulation.");
        return;
    }
    
    m_contours.clear();
    
    // Round min/max to interval
    double startElev = qFloor(minZ / interval) * interval;
    double endElev = qCeil(maxZ / interval) * interval;
    
    // For each contour level
    for (double elev = startElev; elev <= endElev; elev += interval) {
        CanvasWidget::ContourLine contour;
        contour.elevation = elev;
        contour.isMajor = (qAbs(fmod(elev, majorInterval)) < 0.001);
        
        // Process each triangle
        for (const auto& tri : triangles) {
            if (tri.size() != 3) continue;
            
            const auto& p0 = points3D[tri[0]];
            const auto& p1 = points3D[tri[1]];
            const auto& p2 = points3D[tri[2]];
            
            // Find intersections with triangle edges
            QVector<QPointF> intersections;
            
            // Edge 0-1
            if ((p0.z <= elev && p1.z >= elev) || (p0.z >= elev && p1.z <= elev)) {
                if (qAbs(p1.z - p0.z) > 0.0001) {
                    double t = (elev - p0.z) / (p1.z - p0.z);
                    if (t >= 0 && t <= 1) {
                        intersections.append(QPointF(
                            p0.x + t * (p1.x - p0.x),
                            p0.y + t * (p1.y - p0.y)
                        ));
                    }
                }
            }
            
            // Edge 1-2
            if ((p1.z <= elev && p2.z >= elev) || (p1.z >= elev && p2.z <= elev)) {
                if (qAbs(p2.z - p1.z) > 0.0001) {
                    double t = (elev - p1.z) / (p2.z - p1.z);
                    if (t >= 0 && t <= 1) {
                        intersections.append(QPointF(
                            p1.x + t * (p2.x - p1.x),
                            p1.y + t * (p2.y - p1.y)
                        ));
                    }
                }
            }
            
            // Edge 2-0
            if ((p2.z <= elev && p0.z >= elev) || (p2.z >= elev && p0.z <= elev)) {
                if (qAbs(p0.z - p2.z) > 0.0001) {
                    double t = (elev - p2.z) / (p0.z - p2.z);
                    if (t >= 0 && t <= 1) {
                        intersections.append(QPointF(
                            p2.x + t * (p0.x - p2.x),
                            p2.y + t * (p0.y - p2.y)
                        ));
                    }
                }
            }
            
            // If we have exactly 2 intersections, add line segment
            if (intersections.size() == 2) {
                contour.points.append(intersections[0]);
                contour.points.append(intersections[1]);
            }
        }
        
        if (!contour.points.isEmpty()) {
            m_contours.append(contour);
        }
    }
    
    int totalSegments = 0;
    int majorCount = 0;
    for (const auto& c : m_contours) {
        totalSegments += c.points.size() / 2;
        if (c.isMajor) majorCount++;
    }
    
    m_statusLabel->setText(QString("Generated %1 contour levels (%2 major), %3 segments")
        .arg(m_contours.size()).arg(majorCount).arg(totalSegments));
    m_statusLabel->setStyleSheet("color: green;");
    m_applyBtn->setEnabled(!m_contours.isEmpty());
}

void ContourDialog::clearContours()
{
    m_contours.clear();
    m_statusLabel->setText("Contours cleared.");
    m_statusLabel->setStyleSheet("color: gray;");
    m_applyBtn->setEnabled(false);
    
    if (m_canvas) {
        m_canvas->clearContours();
    }
}

void ContourDialog::applyToCanvas()
{
    if (!m_canvas || m_contours.isEmpty()) return;
    
    m_canvas->setContours(m_contours);
    m_statusLabel->setText("Contours applied to canvas.");
}


