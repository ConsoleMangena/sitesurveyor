#include "gdal/geosbridge.h"
#include "dxf/dxfreader.h"

#include <geos_c.h>
#include <QDebug>
#include <QtMath>

// Static GEOS context
static GEOSContextHandle_t s_geosContext = nullptr;
static QString s_lastError;

// Error handlers
static void geosErrorHandler(const char* message, void* /*userdata*/) {
    s_lastError = QString::fromUtf8(message);
    qDebug() << "GEOS Bridge Error:" << message;
}

static void geosNoticeHandler(const char* /*message*/, void* /*userdata*/) {
    // Ignore notices
}

namespace GeosBridge {

void initialize()
{
    if (!s_geosContext) {
        s_geosContext = GEOS_init_r();
        if (s_geosContext) {
            GEOSContext_setErrorMessageHandler_r(s_geosContext, geosErrorHandler, nullptr);
            GEOSContext_setNoticeMessageHandler_r(s_geosContext, geosNoticeHandler, nullptr);
        }
    }
}

void cleanup()
{
    if (s_geosContext) {
        GEOS_finish_r(s_geosContext);
        s_geosContext = nullptr;
    }
}

QString lastError()
{
    return s_lastError;
}

// Helper: compute perpendicular offset of a line segment
static void offsetSegment(const QPointF& p1, const QPointF& p2, double distance,
                          QPointF& q1, QPointF& q2)
{
    double dx = p2.x() - p1.x();
    double dy = p2.y() - p1.y();
    double len = qSqrt(dx*dx + dy*dy);
    
    if (len < 1e-12) {
        q1 = p1;
        q2 = p2;
        return;
    }
    
    // Perpendicular unit vector (pointing left when walking from p1 to p2)
    double nx = -dy / len;
    double ny = dx / len;
    
    // Offset points
    q1 = QPointF(p1.x() + nx * distance, p1.y() + ny * distance);
    q2 = QPointF(p2.x() + nx * distance, p2.y() + ny * distance);
}

// Helper: find intersection of two infinite lines defined by points
static bool lineIntersection(const QPointF& a1, const QPointF& a2,
                             const QPointF& b1, const QPointF& b2,
                             QPointF& result)
{
    double d1x = a2.x() - a1.x();
    double d1y = a2.y() - a1.y();
    double d2x = b2.x() - b1.x();
    double d2y = b2.y() - b1.y();
    
    double cross = d1x * d2y - d1y * d2x;
    
    if (qAbs(cross) < 1e-12) {
        // Parallel lines - use midpoint between endpoints
        result = QPointF((a2.x() + b1.x()) / 2.0, (a2.y() + b1.y()) / 2.0);
        return false;
    }
    
    double dx = b1.x() - a1.x();
    double dy = b1.y() - a1.y();
    double t = (dx * d2y - dy * d2x) / cross;
    
    result = QPointF(a1.x() + t * d1x, a1.y() + t * d1y);
    return true;
}

/**
 * Create surveying-style offset (profile/batter board offset)
 * 
 * For a closed polygon:
 * - Each edge is offset perpendicular by the distance (outward)
 * - The offset lines' INTERSECTIONS become the offset peg locations
 * - This creates a larger polygon where lines can be stretched to find corners
 * 
 * For an open polyline:
 * - Each segment is offset, intersections found at vertices
 * - End points are extended perpendicular
 */
DxfPolyline createOffset(const DxfPolyline& input, double distance)
{
    DxfPolyline result;
    result.layer = input.layer;
    result.color = input.color;
    result.closed = input.closed;
    
    s_lastError.clear();
    
    int n = input.points.size();
    if (n < 2) {
        s_lastError = "Need at least 2 points";
        return result;
    }
    
    // For closed polygons, we need at least 3 points
    if (input.closed && n < 3) {
        s_lastError = "Closed polygon needs at least 3 points";
        return result;
    }
    
    // Calculate number of segments
    int numSegments = input.closed ? n : (n - 1);
    
    // Create offset segments
    QVector<QPair<QPointF, QPointF>> offsetSegments;
    for (int i = 0; i < numSegments; ++i) {
        const QPointF& p1 = input.points[i];
        const QPointF& p2 = input.points[(i + 1) % n];
        
        QPointF q1, q2;
        offsetSegment(p1, p2, distance, q1, q2);
        offsetSegments.append(qMakePair(q1, q2));
    }
    
    if (input.closed) {
        // For closed polygon: find intersections of consecutive offset segments
        for (int i = 0; i < numSegments; ++i) {
            const auto& seg1 = offsetSegments[i];
            const auto& seg2 = offsetSegments[(i + 1) % numSegments];
            
            QPointF intersection;
            lineIntersection(seg1.first, seg1.second, seg2.first, seg2.second, intersection);
            result.points.append(intersection);
        }
    } else {
        // For open polyline:
        // First point: start of first offset segment
        result.points.append(offsetSegments.first().first);
        
        // Middle points: intersections of consecutive segments
        for (int i = 0; i < numSegments - 1; ++i) {
            const auto& seg1 = offsetSegments[i];
            const auto& seg2 = offsetSegments[i + 1];
            
            QPointF intersection;
            lineIntersection(seg1.first, seg1.second, seg2.first, seg2.second, intersection);
            result.points.append(intersection);
        }
        
        // Last point: end of last offset segment
        result.points.append(offsetSegments.last().second);
    }
    
    return result;
}

} // namespace GeosBridge
