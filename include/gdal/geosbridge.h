#ifndef GEOSBRIDGE_H
#define GEOSBRIDGE_H

#include <QString>
#include <QVector>
#include <QPointF>

// Forward declarations
struct DxfPolyline;
struct CanvasPolyline;

/**
 * @brief GeosBridge - GEOS utility functions for CAD operations
 * 
 * Provides high-level geometry operations using the GEOS C API.
 */
namespace GeosBridge {

/**
 * @brief Initialize GEOS context (call once at startup)
 */
void initialize();

/**
 * @brief Cleanup GEOS context (call at shutdown)
 */
void cleanup();

/**
 * @brief Get last error message
 */
QString lastError();

/**
 * @brief Create an offset (parallel) line from a polyline
 * 
 * Positive distance = left side; Negative distance = right side
 */
DxfPolyline createOffset(const DxfPolyline& input, double distance);

/**
 * @brief Calculate the area of a closed polygon
 * @param points Polygon vertices (will be auto-closed)
 * @return Area in square units (0 if not a valid polygon)
 */
double calculateArea(const QVector<QPointF>& points);

/**
 * @brief Calculate the perimeter/length of a polyline
 * @param points Polyline vertices
 * @param closed True if polygon (auto-close), false if open polyline
 * @return Length/perimeter in linear units
 */
double calculatePerimeter(const QVector<QPointF>& points, bool closed);

/**
 * @brief Check if a polygon is geometrically valid (no self-intersections)
 * @param points Polygon vertices
 * @return True if valid, false if invalid or error
 */
bool isValid(const QVector<QPointF>& points);

/**
 * @brief Check if two polygons overlap/intersect
 * @param poly1 First polygon vertices
 * @param poly2 Second polygon vertices
 * @return True if they overlap, false if disjoint
 */
bool polygonsOverlap(const QVector<QPointF>& poly1, const QVector<QPointF>& poly2);

/**
 * @brief Create a buffer zone around a polyline/polygon
 * @param points Input geometry vertices
 * @param distance Buffer distance (positive = expand, negative = shrink)
 * @param closed True if input is a polygon
 * @return Buffer polygon points (may be empty on error)
 */
QVector<QPointF> createBuffer(const QVector<QPointF>& points, double distance, bool closed);

/**
 * @brief Calculate the centroid (center of mass) of a polygon
 * @param points Polygon vertices
 * @return Centroid point
 */
QPointF calculateCentroid(const QVector<QPointF>& points);

/**
 * @brief Attempt to repair an invalid geometry using GEOS MakeValid
 * @param points Input polygon vertices
 * @return Repaired polygon points (empty if repair failed)
 */
QVector<QPointF> makeValid(const QVector<QPointF>& points);

/**
 * @brief Check if a point is inside a polygon
 * @param point Point to test
 * @param polygon Polygon vertices
 * @return True if point is inside polygon
 */
bool pointInPolygon(const QPointF& point, const QVector<QPointF>& polygon);

/**
 * @brief Create Delaunay triangulation from points
 * @param points 2D points to triangulate
 * @return Vector of triangles (each triangle is 3 point indices into input)
 */
QVector<QVector<int>> delaunayTriangulate(const QVector<QPointF>& points);

/**
 * @brief 3D point for volume calculations
 */
struct Point3D {
    double x, y, z;
    Point3D(double x_ = 0, double y_ = 0, double z_ = 0) : x(x_), y(y_), z(z_) {}
};

/**
 * @brief Calculate volume between a TIN surface and a design level
 * @param surfacePoints Points with X,Y,Z defining the surface
 * @param designLevel Constant elevation for design surface
 * @param boundary Optional boundary polygon (empty = use convex hull)
 * @return Pair of (cut volume, fill volume) in cubic units
 */
QPair<double, double> calculateVolume(const QVector<Point3D>& surfacePoints,
                                       double designLevel,
                                       const QVector<QPointF>& boundary = QVector<QPointF>());

} // namespace GeosBridge

#endif // GEOSBRIDGE_H


