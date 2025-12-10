#ifndef CANVASWIDGET_H
#define CANVASWIDGET_H

#include <QWidget>
#include <QPointF>
#include <QTransform>
#include <QVector>
#include <QColor>
#include <QSet>
#include <QMap>
#include <QImage>
#include "tools/snapper.h"

// Forward declarations
struct DxfData;
struct GdalData;
class Snapper;

// Tool state machine
enum class ToolState {
    None,               // Normal selection mode
    Idle,               // Normal mode - select on click
    PanMode,            // Pan tool active - left-click drag pans
    OffsetWaitForSide,  // Waiting for user to click side for offset
    SetStation,         // Click to set station/instrument point
    SetBacksight,       // Click to set backsight point
    StakeoutMode,       // Live stakeout sighting from station
    SplitMode           // Click to split polyline at point
};

// Undo command types
enum class UndoType {
    AddPolyline,
    DeletePolyline
};

// Simple geometry structures for rendering
struct CanvasLine {
    QPointF start;
    QPointF end;
    QString layer;
    QColor color;
};

struct CanvasCircle {
    QPointF center;
    double radius;
    QString layer;
    QColor color;
};

struct CanvasArc {
    QPointF center;
    double radius;
    double startAngle;
    double endAngle;
    QString layer;
    QColor color;
};

struct CanvasEllipse {
    QPointF center;
    QPointF majorAxis;
    double ratio;
    double startAngle;
    double endAngle;
    QString layer;
    QColor color;
};

struct CanvasSpline {
    QVector<QPointF> points;  // Approximated as polyline
    QString layer;
    QColor color;
};

struct CanvasPolyline {
    QVector<QPointF> points;
    bool closed;
    QString layer;
    QColor color;
};

// Undo command data (must be after CanvasPolyline)
struct UndoCommand {
    UndoType type;
    CanvasPolyline polyline;  // The polyline that was added/deleted
    int index{-1};            // Index in the polylines vector
};

struct CanvasPolygon {
    QVector<QVector<QPointF>> rings;  // First is exterior, rest are holes
    QString layer;
    QColor color;
    QColor fillColor;
};

struct CanvasHatch {
    QVector<QVector<QPointF>> loops;
    bool solid;
    QString layer;
    QColor color;
};

struct CanvasText {
    QString text;
    QPointF position;
    double height;
    double angle;
    QString layer;
    QColor color;
};

struct CanvasRaster {
    QImage image;
    QRectF bounds;  // World coordinates
    QString layer;
};

struct CanvasPoint {
    QPointF position;
    QString layer;
    QColor color;
};

// Survey peg marker with label
struct CanvasPeg {
    QPointF position;
    QString name;           // Peg name (e.g., "A", "P1", "NE")
    QString layer;
    QColor color{Qt::red};
    double markerSize{0.5}; // World units
};

// Station setup for theodolite/total station
struct CanvasStation {
    QPointF stationPos;     // Instrument setup location (0,0 reference)
    QPointF backsightPos;   // Backsight/orientation reference
    QString stationName{"STN"};
    QString backsightName{"BS"};
    bool hasStation{false};
    bool hasBacksight{false};
};

struct CanvasLayer {
    QString name;
    QColor color{Qt::white};
    bool visible{true};
    bool locked{false};     // Prevent editing when locked
    int order{0};           // Layer stacking order
};

class CanvasWidget : public QWidget {
    Q_OBJECT

public:
    explicit CanvasWidget(QWidget *parent = nullptr);
    ~CanvasWidget();

    // Load data
    void loadDxfData(const DxfData& data);
    void loadGdalData(const GdalData& data);
    void clearAll();
    
    // Project save/load (JSON format)
    bool saveProject(const QString& filePath) const;
    bool loadProject(const QString& filePath);
    QString projectFilePath() const { return m_projectFilePath; }
    void setProjectFilePath(const QString& path) { m_projectFilePath = path; }
    
    // View controls
    void fitToWindow();
    void zoomIn();
    void zoomOut();
    void resetView();
    
    // Layer visibility
    QVector<CanvasLayer> layers() const { return m_layers; }
    void setLayerVisible(const QString& name, bool visible);
    bool isLayerVisible(const QString& name) const;
    
    // Layer management
    void addLayer(const QString& name, QColor color = Qt::white);
    void removeLayer(const QString& name);
    void renameLayer(const QString& oldName, const QString& newName);
    void setLayerColor(const QString& name, QColor color);
    void setLayerLocked(const QString& name, bool locked);
    bool isLayerLocked(const QString& name) const;
    CanvasLayer* getLayer(const QString& name);
    void createDefaultSurveyLayers();
    
    bool showGrid() const { return m_showGrid; }
    void setShowGrid(bool show);
    
    // Coordinate Reference System
    void setCRS(const QString& epsgCode);
    QString crs() const { return m_crs; }
    void setTargetCRS(const QString& epsgCode);
    QString targetCRS() const { return m_targetCRS; }
    void setScaleFactor(double factor);
    double scaleFactor() const { return m_scaleFactor; }
    QPointF transformCoordinate(const QPointF& point) const;
    QPointF applyScaleFactor(const QPointF& point) const;
    
    // Snapping
    void setSnappingEnabled(bool enabled);
    bool isSnappingEnabled() const;
    SnapResult currentSnap() const { return m_currentSnap; }
    
    // Selection (single and multi)
    int selectedPolylineIndex() const { return m_selectedPolylineIndex; }
    bool hasSelection() const { return m_selectedPolylineIndex >= 0 || !m_selectedPolylines.isEmpty(); }
    void clearSelection();
    const CanvasPolyline* selectedPolyline() const;
    void addToSelection(int index);          // Add to multi-selection
    void removeFromSelection(int index);     // Remove from multi-selection
    bool isSelected(int index) const;        // Check if polyline is selected
    QVector<int> getSelectedIndices() const; // Get all selected indices
    
    // Access to polylines for snapping and tools
    const QVector<CanvasPolyline>& polylines() const { return m_polylines; }
    
    // Add a new polyline (for offset tool result)
    void addPolyline(const CanvasPolyline& polyline);
    
    // Peg markers
    void addPeg(const CanvasPeg& peg);
    void addPegsFromPolyline(const CanvasPolyline& polyline, const QString& prefix = "P");
    const QVector<CanvasPeg>& pegs() const { return m_pegs; }
    void clearPegs() { m_pegs.clear(); update(); }
    
    // Offset tool workflow
    void startOffsetTool(double distance);
    void cancelOffsetTool();
    ToolState toolState() const { return m_toolState; }
    void setPanMode(bool enabled);
    
    // Helper: determine if point is on left side of line A->B
    static bool isLeft(const QPointF& a, const QPointF& b, const QPointF& p);
    
    // Undo/Redo
    void undo();
    void redo();
    bool canUndo() const { return !m_undoStack.isEmpty(); }
    bool canRedo() const { return !m_redoStack.isEmpty(); }
    
    // Partition projection - extend line to find intersection with offset polyline
    int projectPartitionToOffset(const QString& pegPrefix = "PART");
    
    // Station setup for theodolite/total station
    void setStationPoint(const QPointF& pos, const QString& name = "STN");
    void setBacksightPoint(const QPointF& pos, const QString& name = "BS");
    void clearStation();
    const CanvasStation& station() const { return m_station; }
    void startSetStationMode();
    void startSetBacksightMode();
    
    // Stakeout/bearing calculations
    double calculateBearing(const QPointF& from, const QPointF& to) const;
    double calculateDistance(const QPointF& from, const QPointF& to) const;
    QString bearingToDMS(double bearing) const;
    QString getStakeoutInfo(int pegIndex) const;
    void startStakeoutMode();
    
    // Polyline editing tools
    void startSelectMode();                           // Enter selection mode
    void explodeSelectedPolyline();                   // Break into individual line segments
    void splitPolylineAtPoint(const QPointF& point);  // Split at nearest point
    void joinPolylines();                             // Join selected with nearest
    void closeSelectedPolyline();                     // Connect last to first
    void reverseSelectedPolyline();                   // Reverse point order
    void deleteSelectedPolyline();                    // Remove selected
    void startSplitMode();                            // Enter split tool mode

signals:
    void mouseWorldPosition(const QPointF& pos);
    void zoomChanged(double zoom);
    void layersChanged();
    void snapChanged(const SnapResult& snap);
    void selectionChanged(int polylineIndex);
    void offsetCompleted(bool success);
    void statusMessage(const QString& message);
    void undoRedoChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    // Hit test for pegs
    int hitTestPeg(const QPointF& worldPos, double tolerance);
    void renamePeg(int pegIndex);
    void updateTransform();
    QPointF screenToWorld(const QPoint& screen) const;
    QPoint worldToScreen(const QPointF& world) const;
    void drawGrid(QPainter& painter);
    void drawEntities(QPainter& painter);
    void drawEllipse(QPainter& painter, const CanvasEllipse& ellipse);
    void drawSpline(QPainter& painter, const CanvasSpline& spline);
    void drawHatch(QPainter& painter, const CanvasHatch& hatch);
    void drawPolygon(QPainter& painter, const CanvasPolygon& polygon);
    void drawRaster(QPainter& painter, const CanvasRaster& raster);
    void drawSnapMarker(QPainter& painter);
    void drawSelection(QPainter& painter);
    void drawPegs(QPainter& painter);
    void drawStation(QPainter& painter);
    void drawStakeoutLine(QPainter& painter);
    
    // Hit testing
    int hitTestPolyline(const QPointF& worldPos, double tolerance);
    
    // Offset execution
    void executeOffset(const QPointF& sideClickPos);
    
    // Spline interpolation helper
    QVector<QPointF> interpolateSpline(const QVector<QPointF>& controlPoints, int degree, int segments);

    // View state
    double m_zoom{1.0};
    QPointF m_offset{0, 0};
    QTransform m_worldToScreen;
    QTransform m_screenToWorld;
    bool m_isPanning{false};
    QPoint m_lastMousePos;
    
    // Display settings
    bool m_showGrid{true};
    double m_gridSize{20.0};
    QColor m_gridColor{60, 60, 60};
    QColor m_backgroundColor{33, 33, 33};
    
    // Layer visibility
    QVector<CanvasLayer> m_layers;
    QSet<QString> m_hiddenLayers;
    
    // Snapping
    Snapper* m_snapper{nullptr};
    SnapResult m_currentSnap;
    double m_snapTolerance{10.0};  // Pixels
    
    // Selection (single and multi)
    int m_selectedPolylineIndex{-1};
    QSet<int> m_selectedPolylines;  // For multi-selection (Shift+click)
    
    // Tool state
    ToolState m_toolState{ToolState::Idle};
    double m_pendingOffsetDistance{0.0};
    QPointF m_stakeoutCursorPos;  // Current cursor position for stakeout mode
    
    // Entities
    QVector<CanvasPoint> m_points;
    QVector<CanvasLine> m_lines;
    QVector<CanvasCircle> m_circles;
    QVector<CanvasArc> m_arcs;
    QVector<CanvasEllipse> m_ellipses;
    QVector<CanvasSpline> m_splines;
    QVector<CanvasPolyline> m_polylines;
    QVector<CanvasPolygon> m_polygons;
    QVector<CanvasHatch> m_hatches;
    QVector<CanvasText> m_texts;
    QVector<CanvasRaster> m_rasters;
    QVector<CanvasPeg> m_pegs;
    
    // Station setup
    CanvasStation m_station;
    
    // Undo/Redo stacks
    QVector<UndoCommand> m_undoStack;
    QVector<UndoCommand> m_redoStack;
    
    // Project file
    QString m_projectFilePath;
    
    // Coordinate Reference System
    QString m_crs{"LOCAL"};
    QString m_targetCRS{"NONE"};
    double m_scaleFactor{1.0};
};

#endif // CANVASWIDGET_H