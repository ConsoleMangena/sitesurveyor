#ifndef SNAPPER_H
#define SNAPPER_H

#include <QPointF>
#include <QString>
#include <QVector>
#include <QColor>

// Forward declarations
struct DxfPolyline;
struct DxfLine;
struct DxfCircle;
struct DxfArc;

/**
 * @brief SnapType - Types of snap points in priority order
 */
enum class SnapType {
    None = 0,       // No snap
    Endpoint,       // Start or end of a line/polyline
    Intersection,   // Where two lines cross
    Midpoint,       // Center of a segment
    Edge            // Anywhere on a line (nearest point)
};

/**
 * @brief SnapResult - Result of a snap operation
 */
struct SnapResult {
    SnapType type{SnapType::None};
    QPointF worldPos;           // Snapped world coordinate
    QPointF originalPos;        // Original mouse world position
    double distance{0.0};       // Distance from mouse to snap point (world units)
    QString layer;              // Layer of the snapped entity
    
    bool isValid() const { return type != SnapType::None; }
    
    // Comparison for priority sorting (lower type value = higher priority)
    bool operator<(const SnapResult& other) const {
        if (type != other.type) {
            return static_cast<int>(type) < static_cast<int>(other.type);
        }
        return distance < other.distance;
    }
};

/**
 * @brief Snapper - Lightweight snapping logic class
 * 
 * Performs spatial queries to find snap points near the mouse cursor.
 * Uses priority-based snap selection: Endpoint > Intersection > Midpoint > Edge
 */
class Snapper {
public:
    Snapper();
    ~Snapper();
    
    /**
     * @brief Find the best snap point near the given position
     * @param mouseWorld Mouse position in world coordinates
     * @param toleranceWorld Search radius in world units
     * @param polylines Vector of polylines to search
     * @return SnapResult with the best snap point, or invalid result if none found
     */
    SnapResult findSnap(const QPointF& mouseWorld, double toleranceWorld,
                       const QVector<struct CanvasPolyline>& polylines);
    
    /**
     * @brief Enable/disable specific snap types
     */
    void setSnapEnabled(SnapType type, bool enabled);
    bool isSnapEnabled(SnapType type) const;
    
    /**
     * @brief Enable/disable all snapping
     */
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

private:
    // Find endpoint snaps
    void findEndpointSnaps(const QPointF& mouseWorld, double tolerance,
                          const QVector<struct CanvasPolyline>& polylines,
                          QVector<SnapResult>& results);
    
    // Find midpoint snaps
    void findMidpointSnaps(const QPointF& mouseWorld, double tolerance,
                          const QVector<struct CanvasPolyline>& polylines,
                          QVector<SnapResult>& results);
    
    // Find edge snaps (nearest point on line)
    void findEdgeSnaps(const QPointF& mouseWorld, double tolerance,
                      const QVector<struct CanvasPolyline>& polylines,
                      QVector<SnapResult>& results);
    
    // Find intersection snaps
    void findIntersectionSnaps(const QPointF& mouseWorld, double tolerance,
                              const QVector<struct CanvasPolyline>& polylines,
                              QVector<SnapResult>& results);
    
    // Geometry helpers
    static double distanceToPoint(const QPointF& p1, const QPointF& p2);
    static double distanceToSegment(const QPointF& point, const QPointF& segStart, const QPointF& segEnd);
    static QPointF nearestPointOnSegment(const QPointF& point, const QPointF& segStart, const QPointF& segEnd);
    static bool lineIntersection(const QPointF& p1, const QPointF& p2,
                                const QPointF& p3, const QPointF& p4,
                                QPointF& intersection);
    
    bool m_enabled{false};              // Disabled by default for performance
    bool m_snapEndpoint{true};
    bool m_snapMidpoint{false};          // Disabled by default
    bool m_snapEdge{false};              // Disabled by default
    bool m_snapIntersection{false};      // Disabled by default (O(n²) - SLOW!)
};

#endif // SNAPPER_H
