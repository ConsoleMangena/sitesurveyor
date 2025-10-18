#ifndef CANVASWIDGET_H
#define CANVASWIDGET_H

#include <QWidget>
#include <QPointF>
#include <QVector>
#include "point.h"

class LayerManager;
class QUndoStack;

class CanvasWidget : public QWidget {
    Q_OBJECT

public:
    explicit CanvasWidget(QWidget *parent = nullptr);
    
    enum class ToolMode { Select, Pan, ZoomWindow, DrawLine, DrawPolygon };
    
    void addPoint(const Point& point);
    void addLine(const QPointF& start, const QPointF& end);
    void clearAll();
    void setShowGrid(bool show);
    void setShowLabels(bool show);
    void setPointColor(const QColor& color);
    void setLineColor(const QColor& color);
    bool showGrid() const { return m_showGrid; }
    bool showLabels() const { return m_showLabels; }
    double gridSize() const { return m_gridSize; }
    void setGridSize(double size);
    void setGaussMode(bool enabled) { m_gaussMode = enabled; update(); }
    bool gaussMode() const { return m_gaussMode; }
    
    void setToolMode(ToolMode mode) {
        bool wasDraw = (m_toolMode == ToolMode::DrawLine || m_toolMode == ToolMode::DrawPolygon);
        bool willDraw = (mode == ToolMode::DrawLine || mode == ToolMode::DrawPolygon);
        if (wasDraw && !willDraw) { m_isDrawing = false; m_isPolygon = false; m_drawVertices.clear(); }
        m_toolMode = mode; update(); }
    ToolMode toolMode() const { return m_toolMode; }
    
    void setShowCrosshair(bool show) { m_showCrosshair = show; update(); }
    bool showCrosshair() const { return m_showCrosshair; }
    
    void setOrthoMode(bool on) { m_orthoMode = on; update(); }
    bool orthoMode() const { return m_orthoMode; }
    void setSnapMode(bool on) { m_snapMode = on; update(); }
    bool snapMode() const { return m_snapMode; }
    void setOsnapMode(bool on) { m_osnapMode = on; update(); }
    bool osnapMode() const { return m_osnapMode; }
    
    QPointF screenToWorld(const QPoint& screenPos) const;
    QPoint worldToScreen(const QPointF& worldPos) const;
    double zoom() const { return m_zoom; }
    void setLayerManager(LayerManager* lm) { m_layerManager = lm; }
    void setUndoStack(QUndoStack* s) { m_undoStack = s; }
    QString pointLayer(const QString& name) const;
    bool setPointLayer(const QString& name, const QString& layer);
    // Line selection and layers
    int selectedLineIndex() const { return m_selectedLineIndex; }
    bool setSelectedLine(int index);
    QString lineLayer(int lineIndex) const;
    bool setLineLayer(int lineIndex, const QString& layer);
    bool lineEndpoints(int lineIndex, QPointF& startOut, QPointF& endOut) const;
    
public slots:
    void zoomIn();
    void zoomOut();
    void resetView();
    void fitToWindow();
    void centerOnPoint(const QPointF& world, double targetZoom = -1.0);
    void removePointByName(const QString& name);
    
signals:
    void mouseWorldPosition(const QPointF& pos);
    void canvasClicked(const QPointF& worldPos);
    void zoomChanged(double zoom);
    void drawingDistanceChanged(double distanceMeters);
    void selectedLineChanged(int lineIndex);
    
protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    
private:
    void drawGrid(QPainter& painter);
    void drawAxes(QPainter& painter);
    void drawPoints(QPainter& painter);
    void drawLines(QPainter& painter);
    void updateTransform();
    QPointF toDisplay(const QPointF& p) const;
    QPointF adjustedWorldFromScreen(const QPoint& screen) const;
    QPointF applyOrtho(const QPointF& world) const;
    QPointF applySnap(const QPointF& world) const;
    QPointF objectSnapFromScreen(const QPoint& screen, bool& found) const;
    // Editing helpers
    bool setLineVertex(int lineIndex, int vertexIndex, const QPointF& world);
    QPointF getLineVertex(int lineIndex, int vertexIndex) const;
    bool hitTestGrip(const QPoint& screen, int& outLineIndex, int& outVertexIndex) const;
    bool hitTestLine(const QPoint& screen, int& outLineIndex) const;
    
    struct DrawnPoint {
        Point point;
        QColor color;
        QString layer;
    };
    
    struct DrawnLine {
        QPointF start;
        QPointF end;
        QColor color;
        QString layer;
    };
    
    QVector<DrawnPoint> m_points;
    QVector<DrawnLine> m_lines;
    
    double m_zoom;
    QPointF m_offset;
    QPointF m_lastMousePos;
    bool m_isPanning;
    
    bool m_showGrid;
    bool m_showLabels;
    QColor m_pointColor;
    QColor m_lineColor;
    QColor m_gridColor;
    QColor m_backgroundColor;
    
    double m_gridSize;
    QTransform m_worldToScreen;
    QTransform m_screenToWorld;
    bool m_gaussMode{false};
    LayerManager* m_layerManager{nullptr};
    
    // Interaction state
    ToolMode m_toolMode{ToolMode::Select};
    bool m_spacePanActive{false};
    bool m_drawZoomRect{false};
    QRect m_zoomRect; // in screen coordinates
    QPoint m_currentMousePos; // screen coordinates
    bool m_showCrosshair{true};
    bool m_orthoMode{false};
    bool m_snapMode{false};
    bool m_osnapMode{true};
    QPointF m_orthoAnchor{0.0, 0.0};

    // Drawing state
    bool m_isDrawing{false};
    bool m_isPolygon{false};
    QVector<QPointF> m_drawVertices; // world coordinates
    QPointF m_currentHoverWorld{0.0, 0.0};

    // Snap indicator
    bool m_hasSnapIndicator{false};
    QPoint m_snapIndicatorScreen;
    QPointF m_snapIndicatorWorld;

    // Edit (grips) state
    bool m_draggingVertex{false};
    int m_dragLineIndex{-1};
    int m_dragVertexIndex{-1}; // 0=start, 1=end
    QPointF m_dragOldPos;
    int m_hoverLineIndex{-1};
    int m_hoverVertexIndex{-1};
    QUndoStack* m_undoStack{nullptr};
    int m_selectedLineIndex{-1};
};

#endif // CANVASWIDGET_H