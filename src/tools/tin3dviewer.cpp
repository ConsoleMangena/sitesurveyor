#include "tools/tin3dviewer.h"
#include "canvas/canvaswidget.h"
#include "gdal/geosbridge.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

// ==================== TIN3DSoftwareWidget ====================

TIN3DSoftwareWidget::TIN3DSoftwareWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(400, 300);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(25, 25, 35));
    setPalette(pal);
}

void TIN3DSoftwareWidget::setTINData(const QVector<QVector3D>& vertices, 
                                      const QVector<QVector<int>>& triangles,
                                      double minZ, double maxZ)
{
    m_vertices = vertices;
    m_triangles = triangles;
    m_minZ = minZ;
    m_maxZ = maxZ;
    
    // Calculate center and scale
    if (!vertices.isEmpty()) {
        QVector3D minPt(1e10, 1e10, 1e10);
        QVector3D maxPt(-1e10, -1e10, -1e10);
        
        for (const auto& v : vertices) {
            minPt.setX(qMin(minPt.x(), v.x()));
            minPt.setY(qMin(minPt.y(), v.y()));
            minPt.setZ(qMin(minPt.z(), v.z()));
            maxPt.setX(qMax(maxPt.x(), v.x()));
            maxPt.setY(qMax(maxPt.y(), v.y()));
            maxPt.setZ(qMax(maxPt.z(), v.z()));
        }
        
        m_center = (minPt + maxPt) / 2.0f;
        float rangeX = maxPt.x() - minPt.x();
        float rangeY = maxPt.y() - minPt.y();
        float rangeZ = maxPt.z() - minPt.z();
        m_scale = qMax(rangeX, qMax(rangeY, rangeZ));
        if (m_scale < 0.001f) m_scale = 1.0f;
    }
    
    update();
}

void TIN3DSoftwareWidget::resetView()
{
    m_rotationX = 30.0f;
    m_rotationZ = 45.0f;
    m_zoom = 1.0f;
    m_panX = 0.0f;
    m_panY = 0.0f;
    update();
}

QPointF TIN3DSoftwareWidget::project3Dto2D(const QVector3D& point3D)
{
    // Center the point
    float x = point3D.x() - m_center.x();
    float y = point3D.y() - m_center.y();
    float z = (point3D.z() - m_center.z()) * 3.0f; // Exaggerate Z
    
    // Scale
    float scale = 2.0f / m_scale;
    x *= scale;
    y *= scale;
    z *= scale;
    
    // Rotate around Z axis
    float cosZ = cos(m_rotationZ * M_PI / 180.0f);
    float sinZ = sin(m_rotationZ * M_PI / 180.0f);
    float x1 = x * cosZ - y * sinZ;
    float y1 = x * sinZ + y * cosZ;
    
    // Rotate around X axis
    float cosX = cos(m_rotationX * M_PI / 180.0f);
    float sinX = sin(m_rotationX * M_PI / 180.0f);
    float y2 = y1 * cosX - z * sinX;
    float z2 = y1 * sinX + z * cosX;
    
    // Isometric projection to screen
    float screenX = x1 * m_zoom * 150.0f + width() / 2.0f + m_panX * 100.0f;
    float screenY = -y2 * m_zoom * 150.0f + height() / 2.0f + m_panY * 100.0f;
    
    return QPointF(screenX, screenY);
}

QColor TIN3DSoftwareWidget::getColorForElevation(double z, double minZ, double maxZ)
{
    double range = maxZ - minZ;
    if (range < 0.001) range = 1.0;
    double t = (z - minZ) / range;
    t = qBound(0.0, t, 1.0);
    
    // Blue -> Cyan -> Green -> Yellow -> Red gradient
    int r, g, b;
    if (t < 0.25) {
        float s = t / 0.25f;
        r = 0; g = static_cast<int>(s * 255); b = 255;
    } else if (t < 0.5) {
        float s = (t - 0.25f) / 0.25f;
        r = 0; g = 255; b = static_cast<int>((1.0f - s) * 255);
    } else if (t < 0.75) {
        float s = (t - 0.5f) / 0.25f;
        r = static_cast<int>(s * 255); g = 255; b = 0;
    } else {
        float s = (t - 0.75f) / 0.25f;
        r = 255; g = static_cast<int>((1.0f - s) * 255); b = 0;
    }
    
    return QColor(r, g, b);
}

void TIN3DSoftwareWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    if (m_triangles.isEmpty() || m_vertices.isEmpty()) {
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter, "No terrain data");
        return;
    }
    
    // Sort triangles by depth (painter's algorithm)
    struct TriangleDepth {
        int index;
        float depth;
        QVector3D centroid;
    };
    
    QVector<TriangleDepth> sortedTriangles;
    for (int i = 0; i < m_triangles.size(); ++i) {
        const auto& tri = m_triangles[i];
        if (tri.size() != 3) continue;
        
        int i0 = tri[0], i1 = tri[1], i2 = tri[2];
        if (i0 >= m_vertices.size() || i1 >= m_vertices.size() || i2 >= m_vertices.size()) {
            continue;
        }
        
        const QVector3D& v0 = m_vertices[i0];
        const QVector3D& v1 = m_vertices[i1];
        const QVector3D& v2 = m_vertices[i2];
        
        QVector3D centroid = (v0 + v1 + v2) / 3.0f;
        
        // Calculate depth after rotation
        float y = centroid.y() - m_center.y();
        float z = centroid.z() - m_center.z();
        float cosX = cos(m_rotationX * M_PI / 180.0f);
        float sinX = sin(m_rotationX * M_PI / 180.0f);
        float depth = y * sinX + z * cosX;
        
        sortedTriangles.append({i, depth, centroid});
    }
    
    // Sort back to front
    std::sort(sortedTriangles.begin(), sortedTriangles.end(), 
              [](const TriangleDepth& a, const TriangleDepth& b) {
                  return a.depth < b.depth;
              });
    
    // Draw triangles
    for (const auto& td : sortedTriangles) {
        const auto& tri = m_triangles[td.index];
        
        const QVector3D& v0 = m_vertices[tri[0]];
        const QVector3D& v1 = m_vertices[tri[1]];
        const QVector3D& v2 = m_vertices[tri[2]];
        
        QPointF p0 = project3Dto2D(v0);
        QPointF p1 = project3Dto2D(v1);
        QPointF p2 = project3Dto2D(v2);
        
        // Calculate average Z for coloring
        double avgZ = (v0.z() + v1.z() + v2.z()) / 3.0;
        QColor fillColor = getColorForElevation(avgZ, m_minZ, m_maxZ);
        
        // Add shading based on normal
        QVector3D edge1 = v1 - v0;
        QVector3D edge2 = v2 - v0;
        QVector3D normal = QVector3D::crossProduct(edge1, edge2).normalized();
        float shade = qAbs(normal.z()) * 0.5f + 0.5f;
        fillColor = QColor(
            static_cast<int>(fillColor.red() * shade),
            static_cast<int>(fillColor.green() * shade),
            static_cast<int>(fillColor.blue() * shade)
        );
        
        QPolygonF triangle;
        triangle << p0 << p1 << p2;
        
        painter.setBrush(fillColor);
        painter.setPen(QPen(QColor(50, 50, 60), 0.5));
        painter.drawPolygon(triangle);
    }
    
    // Draw legend
    painter.setPen(Qt::white);
    painter.drawText(10, 20, QString("Z Range: %1 - %2").arg(m_minZ, 0, 'f', 1).arg(m_maxZ, 0, 'f', 1));
    painter.drawText(10, 40, QString("Triangles: %1").arg(m_triangles.size()));
}

void TIN3DSoftwareWidget::mousePressEvent(QMouseEvent *event)
{
    m_lastMousePos = event->pos();
}

void TIN3DSoftwareWidget::mouseMoveEvent(QMouseEvent *event)
{
    int dx = event->pos().x() - m_lastMousePos.x();
    int dy = event->pos().y() - m_lastMousePos.y();
    
    if (event->buttons() & Qt::LeftButton) {
        // Rotate
        m_rotationZ += dx * 0.5f;
        m_rotationX += dy * 0.5f;
        m_rotationX = qBound(-90.0f, m_rotationX, 90.0f);
    } else if (event->buttons() & Qt::RightButton) {
        // Pan
        m_panX += dx * 0.01f;
        m_panY -= dy * 0.01f;
    }
    
    m_lastMousePos = event->pos();
    update();
}

void TIN3DSoftwareWidget::wheelEvent(QWheelEvent *event)
{
    float delta = event->angleDelta().y() / 120.0f;
    m_zoom *= (1.0f + delta * 0.1f);
    m_zoom = qBound(0.1f, m_zoom, 10.0f);
    update();
}

// ==================== TIN3DViewerDialog ====================

TIN3DViewerDialog::TIN3DViewerDialog(CanvasWidget* canvas, QWidget *parent)
    : QDialog(parent), m_canvas(canvas), m_viewer(nullptr)
{
    setWindowTitle("3D DTM Viewer (Software Rendered)");
    setMinimumSize(600, 500);
    resize(800, 600);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Title
    QLabel* titleLabel = new QLabel("3D Terrain Model Viewer");
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    mainLayout->addWidget(titleLabel);
    
    // Instructions
    QLabel* helpLabel = new QLabel(
        "🖱️ Left-drag: Rotate | Right-drag: Pan | Scroll: Zoom");
    helpLabel->setStyleSheet("color: gray;");
    mainLayout->addWidget(helpLabel);
    
    // 3D viewer (software rendered - no OpenGL needed)
    m_viewer = new TIN3DSoftwareWidget();
    m_viewer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(m_viewer, 1);
    
    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    
    QPushButton* resetBtn = new QPushButton("Reset View");
    connect(resetBtn, &QPushButton::clicked, m_viewer, &TIN3DSoftwareWidget::resetView);
    btnLayout->addWidget(resetBtn);
    
    btnLayout->addStretch();
    
    QPushButton* closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(closeBtn);
    
    mainLayout->addLayout(btnLayout);
    
    // Load TIN data
    loadTINFromCanvas();
}

void TIN3DViewerDialog::loadTINFromCanvas()
{
    if (!m_canvas || !m_viewer) return;
    
    const auto& pegs = m_canvas->pegs();
    if (pegs.size() < 3) return;
    
    // Collect 3D points
    QVector<QVector3D> vertices;
    QVector<QPointF> points2D;
    double minZ = 1e10, maxZ = -1e10;
    
    for (const auto& peg : pegs) {
        vertices.append(QVector3D(peg.position.x(), peg.position.y(), peg.z));
        points2D.append(peg.position);
        minZ = qMin(minZ, peg.z);
        maxZ = qMax(maxZ, peg.z);
    }
    
    // Triangulate using GEOS
    auto triangles = GeosBridge::delaunayTriangulate(points2D);
    
    m_viewer->setTINData(vertices, triangles, minZ, maxZ);
}
