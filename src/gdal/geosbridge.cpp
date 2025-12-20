#include "gdal/geosbridge.h"
#include "dxf/dxfreader.h"

#ifdef HAVE_GEOS
#include <geos_c.h>
#endif

#include <cpl_error.h>
#include <cpl_conv.h>
#include <QDebug>
#include <QtMath>
#include <QLineF>

// Static GEOS context
#ifdef HAVE_GEOS
static GEOSContextHandle_t s_geosContext = nullptr;
#endif
static QString s_lastError;

// Error handlers
#ifdef HAVE_GEOS
static void geosErrorHandler(const char* message, void* /*userdata*/) {
    s_lastError = QString::fromUtf8(message);
    qDebug() << "GEOS Bridge Error:" << message;
}
#endif


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

// Helper class for RAII CPL Error Handling scope
class ScopedCPLHandler {
public:
    ScopedCPLHandler() {
        initialize();
        CPLPushErrorHandler(CPLQuietErrorHandler);
    }
    ~ScopedCPLHandler() {
        CPLPopErrorHandler();
    }
};

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

// Helper: Create GEOS coordinate sequence from QPointF vector
static GEOSCoordSequence* createCoordSeq(const QVector<QPointF>& points, bool closeRing)
{
    if (!s_geosContext || points.size() < 2) return nullptr;
    
    int size = points.size();
    bool needsClosing = false;
    if (closeRing) {
        if (points.isEmpty()) {
            needsClosing = false;
        } else {
            // Check distance to avoid epsilon issues
            double dist = QLineF(points.first(), points.last()).length();
            needsClosing = (dist > 1e-9); // If > epsilon, we need to add a closing point
        }
    }
    
    if (needsClosing) {
        size += 1;  // Add closing point
    }
    
    GEOSCoordSequence* seq = GEOSCoordSeq_create_r(s_geosContext, size, 2);
    if (!seq) return nullptr;
    
    for (int i = 0; i < points.size(); ++i) {
        double x = points[i].x();
        double y = points[i].y();
        
        // If this is the last point and we want to close the ring,
        // and it is coincident (epsilon) with the first point,
        // force it to be EXACTLY the first point to satisfy OGR/GEOS.
        if (closeRing && i == points.size() - 1) {
             if (QLineF(points[i], points.first()).length() < 1e-9) {
                 x = points.first().x();
                 y = points.first().y();
             }
        }
        
        GEOSCoordSeq_setXY_r(s_geosContext, seq, i, x, y);
    }
    
    // Close the ring if needed (Append point)
    if (needsClosing) {
        GEOSCoordSeq_setXY_r(s_geosContext, seq, size - 1, points.first().x(), points.first().y());
    }
    
    return seq;
}

// Helper: Create GEOS polygon from points
static GEOSGeometry* createPolygon(const QVector<QPointF>& points)
{
    if (!s_geosContext || points.size() < 3) return nullptr;
    
    GEOSCoordSequence* seq = createCoordSeq(points, true);
    if (!seq) return nullptr;
    
    GEOSGeometry* ring = GEOSGeom_createLinearRing_r(s_geosContext, seq);
    if (!ring) {
        GEOSCoordSeq_destroy_r(s_geosContext, seq);
        return nullptr;
    }
    
    GEOSGeometry* polygon = GEOSGeom_createPolygon_r(s_geosContext, ring, nullptr, 0);
    if (!polygon) {
        GEOSGeom_destroy_r(s_geosContext, ring);
    }
    
    return polygon;
}

// Helper: Create GEOS linestring from points
static GEOSGeometry* createLineString(const QVector<QPointF>& points)
{
    if (!s_geosContext || points.size() < 2) return nullptr;
    
    GEOSCoordSequence* seq = createCoordSeq(points, false);
    if (!seq) return nullptr;
    
    GEOSGeometry* line = GEOSGeom_createLineString_r(s_geosContext, seq);
    if (!line) {
        GEOSCoordSeq_destroy_r(s_geosContext, seq);
    }
    
    return line;
}

double calculateArea(const QVector<QPointF>& points)
{
    ScopedCPLHandler handler;
    s_lastError.clear();
    
    if (points.size() < 3) {
        s_lastError = "Need at least 3 points for area calculation";
        return 0.0;
    }
    
    // Use Shoelace formula (simple and accurate for planar coordinates)
    double area = 0.0;
    int n = points.size();
    
    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        area += points[i].x() * points[j].y();
        area -= points[j].x() * points[i].y();
    }
    
    return qAbs(area) / 2.0;
}

double calculatePerimeter(const QVector<QPointF>& points, bool closed)
{
    s_lastError.clear();
    
    if (points.size() < 2) {
        s_lastError = "Need at least 2 points for perimeter calculation";
        return 0.0;
    }
    
    double perimeter = 0.0;
    int n = points.size();
    int end = closed ? n : (n - 1);
    
    for (int i = 0; i < end; ++i) {
        int j = (i + 1) % n;
        double dx = points[j].x() - points[i].x();
        double dy = points[j].y() - points[i].y();
        perimeter += qSqrt(dx * dx + dy * dy);
    }
    
    return perimeter;
}

bool isValid(const QVector<QPointF>& points)
{
    ScopedCPLHandler handler;
    s_lastError.clear();
    
    if (points.size() < 3) {
        s_lastError = "Need at least 3 points";
        return false;
    }
    
    GEOSGeometry* polygon = createPolygon(points);
    if (!polygon) {
        s_lastError = "Failed to create polygon for validation";
        return false;
    }
    
    char valid = GEOSisValid_r(s_geosContext, polygon);
    
    if (valid == 0) {
        // Get the reason for invalidity
        char* reason = GEOSisValidReason_r(s_geosContext, polygon);
        if (reason) {
            s_lastError = QString::fromUtf8(reason);
            GEOSFree_r(s_geosContext, reason);
        }
    }
    
    GEOSGeom_destroy_r(s_geosContext, polygon);
    
    return valid == 1;
}

bool polygonsOverlap(const QVector<QPointF>& poly1, const QVector<QPointF>& poly2)
{
    ScopedCPLHandler handler;
    s_lastError.clear();
    
    if (poly1.size() < 3 || poly2.size() < 3) {
        s_lastError = "Both polygons need at least 3 points";
        return false;
    }
    
    GEOSGeometry* geom1 = createPolygon(poly1);
    GEOSGeometry* geom2 = createPolygon(poly2);
    
    if (!geom1 || !geom2) {
        if (geom1) GEOSGeom_destroy_r(s_geosContext, geom1);
        if (geom2) GEOSGeom_destroy_r(s_geosContext, geom2);
        s_lastError = "Failed to create polygons for overlap check";
        return false;
    }
    
    char result = GEOSIntersects_r(s_geosContext, geom1, geom2);
    
    GEOSGeom_destroy_r(s_geosContext, geom1);
    GEOSGeom_destroy_r(s_geosContext, geom2);
    
    return result == 1;
}

QVector<QPointF> createBuffer(const QVector<QPointF>& points, double distance, bool closed)
{
    ScopedCPLHandler handler;
    s_lastError.clear();
    QVector<QPointF> result;
    
    if (points.size() < 2) {
        s_lastError = "Need at least 2 points for buffer";
        return result;
    }
    
    GEOSGeometry* inputGeom = closed ? createPolygon(points) : createLineString(points);
    if (!inputGeom) {
        s_lastError = "Failed to create input geometry for buffer";
        return result;
    }
    
    // Create buffer with 8 quadrant segments for smooth curves
    GEOSGeometry* bufferGeom = GEOSBuffer_r(s_geosContext, inputGeom, distance, 8);
    GEOSGeom_destroy_r(s_geosContext, inputGeom);
    
    if (!bufferGeom) {
        s_lastError = "Buffer operation failed";
        return result;
    }
    
    // Extract exterior ring from buffer polygon
    const GEOSGeometry* extRing = GEOSGetExteriorRing_r(s_geosContext, bufferGeom);
    if (extRing) {
        const GEOSCoordSequence* seq = GEOSGeom_getCoordSeq_r(s_geosContext, extRing);
        if (seq) {
            unsigned int size;
            GEOSCoordSeq_getSize_r(s_geosContext, seq, &size);
            
            for (unsigned int i = 0; i < size; ++i) {
                double x, y;
                GEOSCoordSeq_getXY_r(s_geosContext, seq, i, &x, &y);
                result.append(QPointF(x, y));
            }
        }
    }
    
    GEOSGeom_destroy_r(s_geosContext, bufferGeom);
    
    return result;
}

QPointF calculateCentroid(const QVector<QPointF>& points)
{
    ScopedCPLHandler handler;
    s_lastError.clear();
    
    if (points.size() < 3) {
        // For less than 3 points, return average
        if (points.isEmpty()) return QPointF(0, 0);
        double sumX = 0, sumY = 0;
        for (const auto& p : points) {
            sumX += p.x();
            sumY += p.y();
        }
        return QPointF(sumX / points.size(), sumY / points.size());
    }
    
    GEOSGeometry* polygon = createPolygon(points);
    if (!polygon) {
        s_lastError = "Failed to create polygon for centroid";
        return QPointF(0, 0);
    }
    
    GEOSGeometry* centroid = GEOSGetCentroid_r(s_geosContext, polygon);
    GEOSGeom_destroy_r(s_geosContext, polygon);
    
    if (!centroid) {
        s_lastError = "Centroid calculation failed";
        return QPointF(0, 0);
    }
    
    double x, y;
    GEOSGeomGetX_r(s_geosContext, centroid, &x);
    GEOSGeomGetY_r(s_geosContext, centroid, &y);
    GEOSGeom_destroy_r(s_geosContext, centroid);
    
    return QPointF(x, y);
}

QVector<QPointF> makeValid(const QVector<QPointF>& points)
{
    ScopedCPLHandler handler;
    s_lastError.clear();
    QVector<QPointF> result;
    
    if (points.size() < 3) {
        s_lastError = "Need at least 3 points";
        return result;
    }
    
    GEOSGeometry* polygon = createPolygon(points);
    if (!polygon) {
        s_lastError = "Failed to create polygon for repair";
        return result;
    }
    
    // Check if already valid
    char valid = GEOSisValid_r(s_geosContext, polygon);
    if (valid == 1) {
        // Already valid, return original points
        GEOSGeom_destroy_r(s_geosContext, polygon);
        return points;
    }
    
    // Try to repair
    GEOSGeometry* validGeom = GEOSMakeValid_r(s_geosContext, polygon);
    GEOSGeom_destroy_r(s_geosContext, polygon);
    
    if (!validGeom) {
        s_lastError = "MakeValid failed to repair geometry";
        return result;
    }
    
    // Extract vertices from the repaired geometry
    // MakeValid may return a different geometry type (e.g., MultiPolygon)
    // We'll try to extract the largest polygon
    GEOSGeometry* targetGeom = validGeom;
    int geomType = GEOSGeomTypeId_r(s_geosContext, validGeom);
    
    if (geomType == GEOS_MULTIPOLYGON || geomType == GEOS_GEOMETRYCOLLECTION) {
        // Find the largest polygon by area
        int numGeoms = GEOSGetNumGeometries_r(s_geosContext, validGeom);
        double maxArea = 0;
        const GEOSGeometry* largest = nullptr;
        
        for (int i = 0; i < numGeoms; ++i) {
            const GEOSGeometry* part = GEOSGetGeometryN_r(s_geosContext, validGeom, i);
            if (GEOSGeomTypeId_r(s_geosContext, part) == GEOS_POLYGON) {
                double area = 0;
                GEOSArea_r(s_geosContext, part, &area);
                if (area > maxArea) {
                    maxArea = area;
                    largest = part;
                }
            }
        }
        
        if (largest) {
            targetGeom = const_cast<GEOSGeometry*>(largest);
        }
    }
    
    // Get exterior ring
    if (GEOSGeomTypeId_r(s_geosContext, targetGeom) == GEOS_POLYGON) {
        const GEOSGeometry* ring = GEOSGetExteriorRing_r(s_geosContext, targetGeom);
        if (ring) {
            const GEOSCoordSequence* seq = GEOSGeom_getCoordSeq_r(s_geosContext, ring);
            if (seq) {
                unsigned int size;
                GEOSCoordSeq_getSize_r(s_geosContext, seq, &size);
                
                for (unsigned int i = 0; i < size; ++i) {
                    double x, y;
                    GEOSCoordSeq_getXY_r(s_geosContext, seq, i, &x, &y);
                    result.append(QPointF(x, y));
                }
            }
        }
    }
    
    GEOSGeom_destroy_r(s_geosContext, validGeom);
    
    if (result.isEmpty()) {
        s_lastError = "Could not extract vertices from repaired geometry";
    }
    
    return result;
}

bool pointInPolygon(const QPointF& point, const QVector<QPointF>& polygon)
{
    ScopedCPLHandler handler;
    s_lastError.clear();
    
    if (polygon.size() < 3) {
        return false;
    }
    
    GEOSGeometry* polyGeom = createPolygon(polygon);
    if (!polyGeom) {
        return false;
    }
    
    // Create point geometry
    GEOSCoordSequence* seq = GEOSCoordSeq_create_r(s_geosContext, 1, 2);
    GEOSCoordSeq_setXY_r(s_geosContext, seq, 0, point.x(), point.y());
    GEOSGeometry* pointGeom = GEOSGeom_createPoint_r(s_geosContext, seq);
    
    if (!pointGeom) {
        GEOSGeom_destroy_r(s_geosContext, polyGeom);
        return false;
    }
    
    char result = GEOSContains_r(s_geosContext, polyGeom, pointGeom);
    
    GEOSGeom_destroy_r(s_geosContext, pointGeom);
    GEOSGeom_destroy_r(s_geosContext, polyGeom);
    
    return result == 1;
}

QVector<QVector<int>> delaunayTriangulate(const QVector<QPointF>& points)
{
    ScopedCPLHandler handler;
    s_lastError.clear();
    QVector<QVector<int>> triangles;
    
    if (points.size() < 3) {
        s_lastError = "Need at least 3 points for triangulation";
        return triangles;
    }
    
    // Create multipoint geometry from input points
    GEOSCoordSequence* seq = GEOSCoordSeq_create_r(s_geosContext, points.size(), 2);
    for (int i = 0; i < points.size(); ++i) {
        GEOSCoordSeq_setXY_r(s_geosContext, seq, i, points[i].x(), points[i].y());
    }
    
    // Create points as a collection
    QVector<GEOSGeometry*> pointGeoms;
    for (int i = 0; i < points.size(); ++i) {
        GEOSCoordSequence* pSeq = GEOSCoordSeq_create_r(s_geosContext, 1, 2);
        GEOSCoordSeq_setXY_r(s_geosContext, pSeq, 0, points[i].x(), points[i].y());
        GEOSGeometry* pt = GEOSGeom_createPoint_r(s_geosContext, pSeq);
        if (pt) pointGeoms.append(pt);
    }
    
    GEOSGeometry* multiPoint = GEOSGeom_createCollection_r(s_geosContext, GEOS_MULTIPOINT,
                                                           pointGeoms.data(), pointGeoms.size());
    
    if (!multiPoint) {
        for (auto* g : pointGeoms) GEOSGeom_destroy_r(s_geosContext, g);
        s_lastError = "Failed to create multipoint";
        return triangles;
    }
    
    // Create Delaunay triangulation
    GEOSGeometry* delaunay = GEOSDelaunayTriangulation_r(s_geosContext, multiPoint, 0.0, 0);
    GEOSGeom_destroy_r(s_geosContext, multiPoint);
    
    if (!delaunay) {
        s_lastError = "Delaunay triangulation failed";
        return triangles;
    }
    
    // Extract triangles - result is a GeometryCollection of polygons
    int numTriangles = GEOSGetNumGeometries_r(s_geosContext, delaunay);
    
    for (int i = 0; i < numTriangles; ++i) {
        const GEOSGeometry* tri = GEOSGetGeometryN_r(s_geosContext, delaunay, i);
        if (!tri) continue;
        
        const GEOSGeometry* ring = GEOSGetExteriorRing_r(s_geosContext, tri);
        if (!ring) continue;
        
        const GEOSCoordSequence* triSeq = GEOSGeom_getCoordSeq_r(s_geosContext, ring);
        if (!triSeq) continue;
        
        unsigned int size;
        GEOSCoordSeq_getSize_r(s_geosContext, triSeq, &size);
        
        if (size >= 4) { // Triangle + closing point
            QVector<int> triIndices;
            for (unsigned int j = 0; j < 3; ++j) {
                double x, y;
                GEOSCoordSeq_getXY_r(s_geosContext, triSeq, j, &x, &y);
                
                // Find matching input point
                for (int k = 0; k < points.size(); ++k) {
                    if (qAbs(points[k].x() - x) < 1e-9 && qAbs(points[k].y() - y) < 1e-9) {
                        triIndices.append(k);
                        break;
                    }
                }
            }
            
            if (triIndices.size() == 3) {
                triangles.append(triIndices);
            }
        }
    }
    
    GEOSGeom_destroy_r(s_geosContext, delaunay);
    
    return triangles;
}

QPair<double, double> calculateVolume(const QVector<Point3D>& surfacePoints,
                                       double designLevel,
                                       const QVector<QPointF>& boundary)
{
    ScopedCPLHandler handler;
    s_lastError.clear();
    
    double cutVolume = 0.0;
    double fillVolume = 0.0;
    
    if (surfacePoints.size() < 3) {
        s_lastError = "Need at least 3 points for volume calculation";
        return qMakePair(0.0, 0.0);
    }
    
    // Convert 3D points to 2D for triangulation
    QVector<QPointF> points2D;
    for (const auto& p : surfacePoints) {
        points2D.append(QPointF(p.x, p.y));
    }
    
    // Triangulate
    QVector<QVector<int>> triangles = delaunayTriangulate(points2D);
    
    if (triangles.isEmpty()) {
        s_lastError = "Triangulation produced no triangles";
        return qMakePair(0.0, 0.0);
    }
    
    // Calculate volume for each triangle
    for (const auto& tri : triangles) {
        if (tri.size() != 3) continue;
        
        int i0 = tri[0];
        int i1 = tri[1];
        int i2 = tri[2];
        
        if (i0 >= surfacePoints.size() || i1 >= surfacePoints.size() || i2 >= surfacePoints.size()) {
            continue;
        }
        
        const Point3D& p0 = surfacePoints[i0];
        const Point3D& p1 = surfacePoints[i1];
        const Point3D& p2 = surfacePoints[i2];
        
        // Calculate triangle centroid for boundary check
        QPointF centroid((p0.x + p1.x + p2.x) / 3.0, (p0.y + p1.y + p2.y) / 3.0);
        
        // If boundary specified, check if triangle centroid is inside
        if (!boundary.isEmpty()) {
            if (!pointInPolygon(centroid, boundary)) {
                continue;
            }
        }
        
        // Calculate triangle area using Shoelace formula
        double area = qAbs((p0.x * (p1.y - p2.y) + p1.x * (p2.y - p0.y) + p2.x * (p0.y - p1.y)) / 2.0);
        
        // Average elevation of triangle
        double avgZ = (p0.z + p1.z + p2.z) / 3.0;
        
        // Height difference from design level
        double dz = avgZ - designLevel;
        
        // Volume of triangular prism
        double volume = area * qAbs(dz);
        
        if (dz > 0) {
            // Surface is above design level = CUT needed
            cutVolume += volume;
        } else if (dz < 0) {
            // Surface is below design level = FILL needed
            fillVolume += volume;
        }
    }
    
    return qMakePair(cutVolume, fillVolume);
}

} // namespace GeosBridge

