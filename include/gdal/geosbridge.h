#ifndef GEOSBRIDGE_H
#define GEOSBRIDGE_H

#include <QString>
#include <QVector>
#include <QPointF>

// Forward declarations
struct DxfPolyline;

/**
 * @brief GeosBridge - GEOS utility functions for CAD operations
 * 
 * Provides high-level geometry operations using the GEOS C API.
 */
namespace GeosBridge {

/**
 * @brief Create an offset (parallel) line from a polyline
 * 
 * Uses GEOS single-sided buffer with flat end caps.
 * Positive distance = left side; Negative distance = right side
 * 
 * @param input Source polyline
 * @param distance Offset distance (positive = left, negative = right)
 * @return New polyline offset from the input, or empty polyline on error
 */
DxfPolyline createOffset(const DxfPolyline& input, double distance);

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

} // namespace GeosBridge

#endif // GEOSBRIDGE_H
