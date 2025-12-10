#ifndef DXFREADER_H
#define DXFREADER_H

#include <QVector>
#include <QPointF>
#include <QColor>
#include <QString>
#include <QMap>

// ============================================================================
// DXF Data Structures
// These structures are used to store parsed DXF data for rendering.
// They are populated by the GdalGeosLoader class.
// ============================================================================

struct DxfLine {
    QPointF start;
    QPointF end;
    QString layer;
    QColor color;
};

struct DxfCircle {
    QPointF center;
    double radius;
    QString layer;
    QColor color;
};

struct DxfArc {
    QPointF center;
    double radius;
    double startAngle;  // degrees
    double endAngle;    // degrees
    QString layer;
    QColor color;
};

struct DxfEllipse {
    QPointF center;
    QPointF majorAxis;  // Vector from center to major axis endpoint
    double ratio;       // Minor/major axis ratio
    double startAngle;  // radians, 0 for full ellipse
    double endAngle;    // radians, 2*PI for full ellipse
    QString layer;
    QColor color;
};

struct DxfSpline {
    QVector<QPointF> controlPoints;
    QVector<QPointF> fitPoints;
    int degree;
    bool closed;
    QString layer;
    QColor color;
};

struct DxfPolyline {
    QVector<QPointF> points;
    bool closed;
    QString layer;
    QColor color;
};

struct DxfText {
    QString text;
    QPointF position;
    double height;
    double angle;
    QString layer;
    QColor color;
};

struct DxfHatchLoop {
    QVector<QPointF> points;
    bool closed;
};

struct DxfHatch {
    QVector<DxfHatchLoop> loops;
    QString pattern;
    bool solid;
    QString layer;
    QColor color;
};

struct DxfLayer {
    QString name;
    QColor color;
    bool visible;
    bool locked;
};

// Block definition (kept for compatibility, but GDAL inlines blocks)
struct DxfBlockDef {
    QString name;
    QPointF basePoint;
    QVector<DxfLine> lines;
    QVector<DxfCircle> circles;
    QVector<DxfArc> arcs;
    QVector<DxfEllipse> ellipses;
    QVector<DxfPolyline> polylines;
    QVector<DxfText> texts;
};

// Block insert (kept for compatibility, but GDAL inlines blocks)
struct DxfInsert {
    QString blockName;
    QPointF insertPoint;
    double scaleX;
    double scaleY;
    double rotation;  // degrees
    QString layer;
};

// ============================================================================
// Collected DXF data (output of GdalGeosLoader)
// ============================================================================
struct DxfData {
    QVector<DxfLine> lines;
    QVector<DxfCircle> circles;
    QVector<DxfArc> arcs;
    QVector<DxfEllipse> ellipses;
    QVector<DxfSpline> splines;
    QVector<DxfPolyline> polylines;
    QVector<DxfText> texts;
    QVector<DxfHatch> hatches;
    QVector<DxfLayer> layers;
    QMap<QString, DxfBlockDef> blocks;  // Empty when using GDAL (blocks are inlined)
    QVector<DxfInsert> inserts;         // Empty when using GDAL (blocks are inlined)
    
    void clear() {
        lines.clear();
        circles.clear();
        arcs.clear();
        ellipses.clear();
        splines.clear();
        polylines.clear();
        texts.clear();
        hatches.clear();
        layers.clear();
        blocks.clear();
        inserts.clear();
    }
    
    bool isEmpty() const {
        return lines.isEmpty() && circles.isEmpty() && arcs.isEmpty() &&
               ellipses.isEmpty() && splines.isEmpty() && polylines.isEmpty() &&
               hatches.isEmpty();
    }
    
    int totalEntities() const {
        return lines.size() + circles.size() + arcs.size() +
               ellipses.size() + splines.size() + polylines.size() +
               hatches.size() + texts.size();
    }
};

// ============================================================================
// Utility: Convert AutoCAD Color Index (ACI) to QColor
// ============================================================================
inline QColor aciToColor(int aci)
{
    switch (aci) {
        case 1: return QColor(255, 0, 0);      // Red
        case 2: return QColor(255, 255, 0);    // Yellow
        case 3: return QColor(0, 255, 0);      // Green
        case 4: return QColor(0, 255, 255);    // Cyan
        case 5: return QColor(0, 0, 255);      // Blue
        case 6: return QColor(255, 0, 255);    // Magenta
        case 7: return QColor(255, 255, 255);  // White
        case 8: return QColor(128, 128, 128);  // Gray
        case 9: return QColor(192, 192, 192);  // Light gray
        default:
            if (aci >= 10 && aci <= 249) {
                // Approximate color from ACI palette
                int r = ((aci - 10) % 10) * 25;
                int g = (((aci - 10) / 10) % 5) * 50;
                int b = ((aci - 10) / 50) * 50;
                return QColor(r, g, b);
            }
            return QColor(255, 255, 255);
    }
}

#endif // DXFREADER_H
