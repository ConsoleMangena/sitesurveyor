#ifndef TIN3DVIEWER_H
#define TIN3DVIEWER_H

#include <QDialog>
#include <QWidget>
#include <QVector>
#include <QVector3D>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>

class CanvasWidget;

/**
 * @brief Software-rendered 3D TIN viewer using QPainter (no OpenGL required)
 */
class TIN3DSoftwareWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TIN3DSoftwareWidget(QWidget *parent = nullptr);
    
    void setTINData(const QVector<QVector3D>& vertices, 
                    const QVector<QVector<int>>& triangles,
                    double minZ, double maxZ);
    
public slots:
    void resetView();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QColor getColorForElevation(double z, double minZ, double maxZ);
    QPointF project3Dto2D(const QVector3D& point3D);
    
    QVector<QVector3D> m_vertices;
    QVector<QVector<int>> m_triangles;
    double m_minZ{0}, m_maxZ{1};
    
    // View parameters
    float m_rotationX{30.0f};
    float m_rotationZ{45.0f};
    float m_zoom{1.0f};
    float m_panX{0.0f};
    float m_panY{0.0f};
    
    QPoint m_lastMousePos;
    
    // Model center and scale
    QVector3D m_center;
    float m_scale{1.0f};
};

/**
 * @brief Dialog containing software-rendered 3D TIN viewer
 */
class TIN3DViewerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TIN3DViewerDialog(CanvasWidget* canvas, QWidget *parent = nullptr);

private:
    void loadTINFromCanvas();
    
    CanvasWidget* m_canvas;
    TIN3DSoftwareWidget* m_viewer;
};

#endif // TIN3DVIEWER_H
