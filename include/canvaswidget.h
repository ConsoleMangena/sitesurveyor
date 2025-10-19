#ifndef CANVASWIDGET_H
#define CANVASWIDGET_H

#include <QWidget>
#include <QPointF>
#include <QVector>
#include <QSet>
#include <QRect>
#include <QPair>
#include "point.h"

class LayerManager;
class QUndoStack;

class CanvasWidget : public QWidget {
    Q_OBJECT

public:
    explicit CanvasWidget(QWidget *parent = nullptr);
    
    enum class ToolMode { Select, Pan, ZoomWindow, DrawLine, DrawPolygon, DrawCircle, DrawArc, DrawRectangle, LassoSelect, Lengthen };
    
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
        auto isDraw = [](ToolMode m){
            return m == ToolMode::DrawLine || m == ToolMode::DrawPolygon || m == ToolMode::DrawCircle || m == ToolMode::DrawArc || m == ToolMode::DrawRectangle;
        };
        bool wasDraw = isDraw(m_toolMode);
        bool willDraw = isDraw(mode);
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
    // Dynamic input toggle
    void setDynInputEnabled(bool on) { m_dynInputEnabled = on; update(); }
    bool dynInputEnabled() const { return m_dynInputEnabled; }
    // Polar tracking and object snap tracking
    void setPolarMode(bool on) { m_polarMode = on; update(); }
    bool polarMode() const { return m_polarMode; }
    void setPolarIncrement(double deg) { m_polarIncrementDeg = (deg > 0.0 ? deg : 15.0); update(); }
    double polarIncrement() const { return m_polarIncrementDeg; }
    void setOtrackMode(bool on) { m_otrackMode = on; update(); }
    bool otrackMode() const { return m_otrackMode; }
    
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
    void clearSelection();
    bool hasSelection() const;
    void deleteSelected();
    void selectAllVisible();
    void invertSelectionVisible();
    void selectByCurrentLayer();
    void isolateSelectionLayers();
    void hideSelectedLayers();
    void lockSelectedLayers();
    void setSelectedLayer(const QString& layer);
    void showAllLayers();
    
signals:
    void mouseWorldPosition(const QPointF& pos);
    void canvasClicked(const QPointF& worldPos);
    void zoomChanged(double zoom);
    void drawingDistanceChanged(double distanceMeters);
    void selectedLineChanged(int lineIndex);
    void selectionChanged(int pointCount, int lineCount);
    
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
    void addPolyline(const QVector<QPointF>& pts, bool closed = false);
    // Editing helpers
    bool setLineVertex(int lineIndex, int vertexIndex, const QPointF& world);
    QPointF getLineVertex(int lineIndex, int vertexIndex) const;
    bool hitTestGrip(const QPoint& screen, int& outLineIndex, int& outVertexIndex) const;
    bool hitTestLine(const QPoint& screen, int& outLineIndex) const;
    bool hitTestPoint(const QPoint& screen, int& outPointIndex) const;
    void setExclusiveSelectionLine(int idx);
    void toggleSelectionLine(int idx);
    void setExclusiveSelectionPoint(int idx);
    void toggleSelectionPoint(int idx);

    // Define internal item types before using them in signatures
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

    void applyDeleteSelection();
    void applyRestoreSelection(const QVector<QPair<int, DrawnPoint>>& points,
                               const QVector<QPair<int, DrawnLine>>& lines);
    
    QVector<DrawnPoint> m_points;
    QVector<DrawnLine> m_lines;
    QSet<int> m_selectedPointIndices;
    QSet<int> m_selectedLineIndices;
    
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
    bool m_polarMode{false};
    double m_polarIncrementDeg{15.0};
    bool m_otrackMode{false};
    QPointF m_orthoAnchor{0.0, 0.0};

    // Selection rectangle state (screen space)
    bool m_selectRectActive{false};
    QRect m_selectRect;
    QPoint m_selectRectStart; // to determine window vs crossing

    // Lasso selection (screen space polyline)
    bool m_lassoActive{false};
    QVector<QPoint> m_lassoPoints; // screen points
    QPoint m_lassoHover;
    bool m_lassoMulti{false};

    // Drawing state
    bool m_isDrawing{false};
    bool m_isPolygon{false};
    QVector<QPointF> m_drawVertices; // world coordinates
    QPointF m_currentHoverWorld{0.0, 0.0};
    // Click-drag shape interaction
    bool m_shapeDragActive{false};
    QPoint m_shapePressScreen;

    // Dynamic input (AutoCAD-style) for Draw Line tool
    bool m_dynInputEnabled{true};
    bool m_dynInputActive{false};
    QString m_dynBuffer;           // e.g. "12.5<45" or "<90" or "25"
    bool m_hasPendingAngle{false}; // angle provided without distance yet
    double m_pendingAngleDeg{0.0};
    // Base-point specifications
    bool m_fromActive{false};
    bool m_fromAwaitingBase{false};
    QPointF m_fromBaseWorld{0.0, 0.0};
    bool m_tkActive{false};
    bool m_tkAwaitingBase{false};
    QPointF m_tkBaseWorld{0.0, 0.0};

    // Snap indicator
    bool m_hasSnapIndicator{false};
    QPoint m_snapIndicatorScreen;
    QPointF m_snapIndicatorWorld;
    // Snap tracking (guides from snap indicator)
    bool m_hasTrackOrigin{false};
    QPointF m_trackOriginWorld{0.0, 0.0};

    // Edit (grips) state
    bool m_draggingVertex{false};
    bool m_draggingSelection{false};
    QPoint m_dragLastScreen;
    QPoint m_dragStartScreen;
    bool m_dragCopy{false};
    // Pre-move snapshots for undo
    QVector<QPair<int, QPointF>> m_preMovePointPos; // index -> world pos
    struct LinePos { int idx; QPointF a; QPointF b; };
    QVector<LinePos> m_preMoveLinePos;
    int m_dragLineIndex{-1};
    int m_dragVertexIndex{-1}; // 0=start, 1=end
    QPointF m_dragOldPos;
    int m_hoverLineIndex{-1};
    int m_hoverVertexIndex{-1};
    QUndoStack* m_undoStack{nullptr};
    int m_selectedLineIndex{-1};

    // Allow an undo command to access internals for delete/restore
    friend class RemoveSelectionCommand;
    friend class MoveSelectionCommand;

    // Parsing helper
    bool parseDistanceAngleInput(const QString& text,
                                 bool& hasDist, double& dist,
                                 bool& hasAngle, double& angleDeg) const;
};

#endif // CANVASWIDGET_H