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
#include <QPropertyAnimation>
#include "tools/snapper.h"

class QPropertyAnimation;
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
    SetCheckPoint,      // Click to verify against known check point
    StakeoutMode,       // Live stakeout sighting from station
    SplitMode,          // Click to split polyline at point
    CopyMode,           // Click to place copied polyline
    MoveMode,           // Click to place moved polyline
    MirrorMode,         // First click: start of mirror axis
    MirrorMode2,        // Second click: end of mirror axis
    TrimMode,           // Click to trim polyline
    ExtendMode,         // Click to extend polyline
    FilletMode,         // Click corner to fillet
    MeasureMode,        // First click: start point for measurement
    MeasureMode2,       // Second click: end point for measurement
    
    // Drawing tools
    DrawLineMode,       // First click: start point
    DrawLineMode2,      // Second click: end point
    DrawPolylineMode,   // Continuous polyline drawing (double-click or Enter to finish)
    DrawRectMode,       // First click: first corner
    DrawRectMode2,      // Second click: opposite corner
    DrawCircleMode,     // First click: center point
    DrawCircleMode2,    // Second click: radius point
    DrawArcMode,        // First click: start point
    DrawArcMode2,       // Second click: mid point
    DrawArcMode3,       // Third click: end point
    DrawTextMode,       // Click to place text
    
    // Transform tools
    ScaleMode,          // First click: base point
    ScaleMode2,         // Second click: scale reference
    RotateMode,         // First click: base point
    RotateMode2,        // Second click: rotation angle
    
    // Peg creation
    AddPegMode          // Click to add a peg at location
};

// Undo command types
enum class UndoType {
    AddPolyline,        // Single polyline added
    DeletePolyline,     // Single polyline deleted
    ModifyPolyline,     // Polyline modified (points changed)
    AddMultiple,        // Multiple polylines added (explode, copy)
    DeleteMultiple,     // Multiple polylines deleted (join)
    DeleteLayer,        // Layer and all its contents deleted
    AddPeg,             // Single peg added
    DeletePeg,          // Single peg deleted
    ModifyPeg           // Peg modified (position/name changed)
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
    CanvasPolyline polyline;              // For single add/delete/modify
    CanvasPolyline oldPolyline;           // Previous state for modify
    QVector<CanvasPolyline> polylines;    // For batch add/delete
    QVector<int> indices;                 // Indices for batch operations
    int index{-1};                        // Index for single operations
    QString layerName;                    // For layer operations
    
    // Peg data (for AddPeg, DeletePeg, ModifyPeg)
    QPointF pegPosition;
    QPointF oldPegPosition;
    QString pegName;
    QString oldPegName;
    QColor pegColor{Qt::red};
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
    double z{0.0};          // Elevation/height coordinate
    QString name;           // Peg name (e.g., "A", "P1", "NE")
    QString layer;
    QColor color{Qt::red};
    double markerSize{0.5}; // World units
};

// Station setup for theodolite/total station
struct CanvasStation {
    QPointF stationPos;     // Instrument setup location (0,0 reference)
    double stationZ{0.0};   // Station elevation
    QPointF backsightPos;   // Backsight/orientation reference
    double backsightZ{0.0}; // Backsight elevation
    QString stationName{"STN"};
    QString backsightName{"BS"};
    bool hasStation{false};
    bool hasBacksight{false};
};

// Triangulated Irregular Network (TIN) for DTM visualization
struct CanvasTIN {
    struct Point3D {
        double x, y, z;
        Point3D(double x_ = 0, double y_ = 0, double z_ = 0) : x(x_), y(y_), z(z_) {}
    };
    
    QVector<Point3D> points;           // 3D vertices
    QVector<QVector<int>> triangles;   // Triangle indices (3 indices per triangle)
    double minZ{0.0}, maxZ{0.0};       // Z range for coloring
    double designLevel{0.0};           // Reference level for cut/fill coloring
    bool visible{false};               // Whether to draw TIN
    bool colorByElevation{true};       // True = color by Z, False = color by cut/fill
    QString layer{"TIN"};
};


struct CanvasLayer {
    QString name;
    QColor color{Qt::white};
    bool visible{true};
    bool locked{false};     // Prevent editing when locked
    int order{0};           // Layer stacking order
};

struct DxfData;

class CanvasWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(double animatedZoom READ animatedZoom WRITE setAnimatedZoom)

public:
    explicit CanvasWidget(QWidget *parent = nullptr);
    ~CanvasWidget();
    
    // Animated zoom support
    double animatedZoom() const { return m_zoom; }
    void setAnimatedZoom(double zoom);

    // Load data
    void loadDxfData(const DxfData& data);
    void loadGdalData(const GdalData& data);
    void clearAll();
    
    // Project save/load (JSON format)
    bool saveProject(const QString& filePath) const;
    bool loadProject(const QString& filePath);
    QByteArray saveProjectToJson() const;
    bool loadProjectFromJson(const QByteArray& jsonData);
    QString projectFilePath() const { return m_projectFilePath; }
    void setProjectFilePath(const QString& path) { m_projectFilePath = path; }
    
    // View controls
    void fitToWindow();
    void zoomIn();
    void zoomOut();
    void zoomToPoint(const QPointF& worldPos);  // Zoom and center on point
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
    
    // Active Layer
    void setActiveLayer(const QString& name);
    QString activeLayer() const { return m_activeLayer; }
    
    bool showGrid() const { return m_showGrid; }
    void setShowGrid(bool show);
    
    // Coordinate Reference System
    void setCRS(const QString& epsgCode);
    QString crs() const { return m_crs; }
    void setTargetCRS(const QString& epsgCode);
    QString targetCRS() const { return m_targetCrs; }
    void setScaleFactor(double factor);
    double scaleFactor() const { return m_scaleFactor; }
    void setSouthAzimuth(bool enabled);
    void setSwapXY(bool enabled);
    QPointF transformCoordinate(const QPointF& point) const;
    QPointF applyScaleFactor(const QPointF& point) const;
    
    // Grid
    void setGridEnabled(bool enabled);
    void setGridSpacing(double spacing);
    
    // Markers
    void setTemporaryMarker(const QPointF& pos);
    void clearTemporaryMarker();
    SnapResult currentSnap() const { return m_currentSnap; }
    
    // Snapping
    void setSnappingEnabled(bool enabled);
    bool isSnappingEnabled() const;
    
    // Selection (single and multi)
    int selectedPolylineIndex() const { return m_selectedPolylineIndex; }
    
    // Station Access
    void setStation(const CanvasStation& station);
    const CanvasStation& station() const { return m_station; }
    
    bool hasSelection() const { return m_selectedPolylineIndex >= 0 || !m_selectedPolylines.isEmpty(); }
    void clearSelection();
    const CanvasPolyline* selectedPolyline() const;
    void addToSelection(int index);          // Add to multi-selection
    void removeFromSelection(int index);     // Remove from multi-selection
    bool isSelected(int index) const;        // Check if polyline is selected
    QVector<int> getSelectedIndices() const; // Get all selected indices
    bool replaceSelectedPolylinePoints(const QVector<QPointF>& newPoints); // Replace points of selected polyline
    bool replacePolylinePoints(int index, const QVector<QPointF>& newPoints); // Replace points of arbitrary polyline
    
    // Access to polylines for snapping and tools
    const QVector<CanvasPolyline>& polylines() const { return m_polylines; }
    
    // Add a new polyline (for offset tool result)
    void addPolyline(const CanvasPolyline& polyline);
    
    // Peg markers
    void addPeg(const CanvasPeg& peg);
    void addPegsFromPolyline(const CanvasPolyline& polyline, const QString& prefix = "P");
    void addPegAtPosition(const QPointF& pos, const QString& name, double z = 0.0);  // Add peg at specific position with Z

    void startAddPegMode(const QString& pegName, double z = 0.0);  // Start mode to add peg by clicking

    const QVector<CanvasPeg>& pegs() const { return m_pegs; }
    void clearPegs() { m_pegs.clear(); m_selectedPegIndex = -1; update(); }
    
    // Peg selection
    int selectedPegIndex() const { return m_selectedPegIndex; }
    void selectPeg(int index);
    void deselectPeg();
    void deleteSelectedPeg();
    void updatePeg(int index, const QString& name, double x, double y, double z);
    int pegAtPosition(const QPointF& worldPos, double tolerance = 10.0) const;


    // TIN/DTM visualization
    void setTIN(const CanvasTIN& tin);
    void clearTIN();
    bool hasTIN() const { return m_tin.visible && !m_tin.triangles.isEmpty(); }
    void setTINVisible(bool visible);
    void generateTINFromPegs(double designLevel = 0.0);
    
    // Contour lines
    struct ContourLine {
        double elevation;
        QVector<QPointF> points;
        bool isMajor{false};
    };
    void setContours(const QVector<ContourLine>& contours);
    void clearContours();
    bool hasContours() const { return !m_contours.isEmpty(); }

    

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
    void setCheckPoint(const QPointF& pos, const QString& name);  // Verify station setup
    void clearStation();
    void startSetStationMode();
    void startSetBacksightMode();
    void startSetCheckPointMode();  // Mode 3: verify against known point
    
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
    
    // Additional modify tools (AutoCAD-style)
    void copySelectedPolyline();                      // COPY - duplicate selected objects
    void startMoveMode();                             // MOVE - start moving selected objects
    void startMirrorMode();                           // MIRROR - start mirror operation
    void mirrorSelectedPolyline(const QPointF& p1, const QPointF& p2);  // Execute mirror
    void startTrimMode();                             // TRIM - start trim tool
    void startExtendMode();                           // EXTEND - start extend tool  
    void startFilletMode(double radius);              // FILLET - start fillet tool
    
    // Measurement tools
    void startMeasureMode();                          // MEASURE - start distance/angle measurement
    
    // Drawing tools
    void startDrawLineMode();                         // LINE - 2-point line drawing
    void startDrawPolylineMode();                     // POLYLINE - multi-point polyline
    void finishPolyline(bool close = false);          // Complete current polyline (close=true closes it)
    void startDrawRectMode();                         // RECTANGLE - 2-corner rectangle
    void startDrawCircleMode();                       // CIRCLE - center + radius
    void startDrawArcMode();                          // ARC - 3-point arc
    void startDrawTextMode(const QString& text, double height = 2.5);  // TEXT - place text
    
    // Transform tools
    void startScaleMode(double factor);               // SCALE - scale by factor
    void startRotateMode(double angle);               // ROTATE - rotate by angle (degrees)
    
    // Coordinate input (type X,Y to place point)
    void inputCoordinate(double x, double y);         // Absolute coordinate
    void inputRelativeCoordinate(double dx, double dy); // Relative to last point
    void inputPolar(double distance, double angle);   // Distance@angle input


signals:
    void mouseWorldPosition(const QPointF& pos);
    void zoomChanged(double zoom);
    void layersChanged();
    void snapChanged(const SnapResult& snap);
    void selectionChanged(int polylineIndex);
    void offsetCompleted(bool success);
    void statusMessage(const QString& message);
    void undoRedoChanged();
    void pegDeleted();  // Emitted when a peg is deleted
    void pegAdded();    // Emitted when a peg is added
    void pegsChanged(); // Emitted when pegs are modified



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
    void drawTIN(QPainter& painter);
    void drawContours(QPainter& painter);

    void drawStation(QPainter& painter);
    void drawStakeoutLine(QPainter& painter);
    
    // Hit testing
    int hitTestPolyline(const QPointF& worldPos, double tolerance);
    int hitTestText(const QPointF& worldPos, double tolerance);
    
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
    QPoint m_cursorPos;
    int m_crosshairSize{20}; // Percentage or pixels? Settings uses 1-100.
    bool m_useFixedGrid{false};
    
    // Display settings
    bool m_showGrid{true};
    double m_gridSize{20.0};
    QColor m_gridColor{60, 60, 60};
    QColor m_backgroundColor{33, 33, 33};
    
    // Layer visibility
    QString m_activeLayer{"0"};
    QVector<CanvasLayer> m_layers;
    QSet<QString> m_hiddenLayers;
    
    // Snapping
    Snapper* m_snapper{nullptr};
    SnapResult m_currentSnap;
    double m_snapTolerance{15.0};  // Pixels (Increased from 10.0 for better usability)
    
    // Selection (single and multi)
    int m_selectedPolylineIndex{-1};
    int m_selectedVertexIndex{-1};  // Selected vertex for editing (AutoCAD-style)
    QSet<int> m_selectedPolylines;  // For multi-selection (Shift+click)
    QSet<int> m_selectedTexts;      // For text multi-selection
    
    // Selection box (AutoCAD-style drag selection)
    bool m_isSelectingBox{false};
    QPointF m_selectionBoxStart;
    QPointF m_selectionBoxEnd;
    
    // Temporary marker (for validation feedback)
    bool m_hasTempMarker = false;
    QPointF m_tempMarkerPos;
    void drawTemporaryMarker(QPainter& painter);

    // Tool state
    ToolState m_toolState{ToolState::Idle};
    double m_pendingOffsetDistance{0.0};
    QPointF m_stakeoutCursorPos;  // Current cursor position for stakeout mode
    QPointF m_mirrorAxisStart;    // First point of mirror axis
    QPointF m_moveStartPos;       // Start position for move operation
    double m_pendingFilletRadius{0.0};  // Fillet radius
    QPointF m_measureStartPoint;  // Start point for distance measurement
    
    // Drawing tool state
    QPointF m_drawStartPoint;     // First point for line/rect/circle/arc
    QPointF m_drawMidPoint;       // Middle point for 3-point arc
    QPointF m_drawCurrentPoint;   // Current cursor position for preview
    QString m_pendingText;        // Text to place
    double m_pendingTextHeight{2.5}; // Text height in world units
    double m_pendingScaleFactor{1.0}; // Scale factor
    double m_pendingRotateAngle{0.0}; // Rotation angle in degrees
    QPointF m_lastInputPoint{0, 0}; // Last point for relative coordinate input
    CanvasPolyline m_currentPolyline; // Polyline being actively drawn
    QString m_pendingPegName;     // Name for peg to be added
    double m_pendingPegZ{0.0};    // Z coordinate for peg to be added
    
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
    int m_selectedPegIndex{-1};   // Currently selected peg (-1 = none)
    
    // Station setup
    CanvasStation m_station;
    
    // TIN/DTM surface
    CanvasTIN m_tin;
    
    // Contour lines
    QVector<ContourLine> m_contours;

    
    // Undo/Redo stacks
    QVector<UndoCommand> m_undoStack;
    QVector<UndoCommand> m_redoStack;
    
    // Project file
    QString m_projectFilePath;
    
    // Coordinate Reference System
    QString m_crs{"LOCAL"};
    QString m_targetCrs{"NONE"};
    double m_scaleFactor{1.0};
    bool m_southAzimuth{false};
    bool m_swapXY{false};
};

#endif // CANVASWIDGET_H