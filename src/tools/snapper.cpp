#include "tools/snapper.h"
#include "canvas/canvaswidget.h"

#include <QtMath>
#include <algorithm>

Snapper::Snapper() = default;
Snapper::~Snapper() = default;

SnapResult Snapper::findSnap(const QPointF& mouseWorld, double toleranceWorld,
                             const QVector<CanvasPolyline>& polylines)
{
    if (!m_enabled || polylines.isEmpty()) {
        return SnapResult{};
    }
    
    QVector<SnapResult> candidates;
    
    // Find all snap candidates within tolerance
    if (m_snapEndpoint) {
        findEndpointSnaps(mouseWorld, toleranceWorld, polylines, candidates);
    }
    if (m_snapIntersection) {
        findIntersectionSnaps(mouseWorld, toleranceWorld, polylines, candidates);
    }
    if (m_snapMidpoint) {
        findMidpointSnaps(mouseWorld, toleranceWorld, polylines, candidates);
    }
    if (m_snapEdge) {
        findEdgeSnaps(mouseWorld, toleranceWorld, polylines, candidates);
    }
    
    if (candidates.isEmpty()) {
        return SnapResult{};
    }
    
    // Sort by priority (type) then distance
    std::sort(candidates.begin(), candidates.end());
    
    // Return the best candidate (highest priority, smallest distance)
    SnapResult best = candidates.first();
    best.originalPos = mouseWorld;
    return best;
}

void Snapper::setSnapEnabled(SnapType type, bool enabled)
{
    switch (type) {
        case SnapType::Endpoint: m_snapEndpoint = enabled; break;
        case SnapType::Midpoint: m_snapMidpoint = enabled; break;
        case SnapType::Edge: m_snapEdge = enabled; break;
        case SnapType::Intersection: m_snapIntersection = enabled; break;
        default: break;
    }
}

bool Snapper::isSnapEnabled(SnapType type) const
{
    switch (type) {
        case SnapType::Endpoint: return m_snapEndpoint;
        case SnapType::Midpoint: return m_snapMidpoint;
        case SnapType::Edge: return m_snapEdge;
        case SnapType::Intersection: return m_snapIntersection;
        default: return false;
    }
}

void Snapper::findEndpointSnaps(const QPointF& mouseWorld, double tolerance,
                                const QVector<CanvasPolyline>& polylines,
                                QVector<SnapResult>& results)
{
    for (const auto& poly : polylines) {
        if (poly.points.size() < 2) continue;
        
        // Check first point
        double dist = distanceToPoint(mouseWorld, poly.points.first());
        if (dist <= tolerance) {
            SnapResult result;
            result.type = SnapType::Endpoint;
            result.worldPos = poly.points.first();
            result.distance = dist;
            result.layer = poly.layer;
            results.append(result);
        }
        
        // Check last point (if not closed or different from first)
        if (!poly.closed || poly.points.size() > 2) {
            dist = distanceToPoint(mouseWorld, poly.points.last());
            if (dist <= tolerance) {
                SnapResult result;
                result.type = SnapType::Endpoint;
                result.worldPos = poly.points.last();
                result.distance = dist;
                result.layer = poly.layer;
                results.append(result);
            }
        }
        
        // Also check intermediate vertices as endpoints
        for (int i = 1; i < poly.points.size() - 1; ++i) {
            dist = distanceToPoint(mouseWorld, poly.points[i]);
            if (dist <= tolerance) {
                SnapResult result;
                result.type = SnapType::Endpoint;
                result.worldPos = poly.points[i];
                result.distance = dist;
                result.layer = poly.layer;
                results.append(result);
            }
        }
    }
}

void Snapper::findMidpointSnaps(const QPointF& mouseWorld, double tolerance,
                                const QVector<CanvasPolyline>& polylines,
                                QVector<SnapResult>& results)
{
    for (const auto& poly : polylines) {
        if (poly.points.size() < 2) continue;
        
        int segmentCount = poly.closed ? poly.points.size() : poly.points.size() - 1;
        
        for (int i = 0; i < segmentCount; ++i) {
            const QPointF& p1 = poly.points[i];
            const QPointF& p2 = poly.points[(i + 1) % poly.points.size()];
            
            QPointF midpoint((p1.x() + p2.x()) / 2.0, (p1.y() + p2.y()) / 2.0);
            double dist = distanceToPoint(mouseWorld, midpoint);
            
            if (dist <= tolerance) {
                SnapResult result;
                result.type = SnapType::Midpoint;
                result.worldPos = midpoint;
                result.distance = dist;
                result.layer = poly.layer;
                results.append(result);
            }
        }
    }
}

void Snapper::findEdgeSnaps(const QPointF& mouseWorld, double tolerance,
                            const QVector<CanvasPolyline>& polylines,
                            QVector<SnapResult>& results)
{
    for (const auto& poly : polylines) {
        if (poly.points.size() < 2) continue;
        
        int segmentCount = poly.closed ? poly.points.size() : poly.points.size() - 1;
        
        for (int i = 0; i < segmentCount; ++i) {
            const QPointF& p1 = poly.points[i];
            const QPointF& p2 = poly.points[(i + 1) % poly.points.size()];
            
            double dist = distanceToSegment(mouseWorld, p1, p2);
            
            if (dist <= tolerance) {
                SnapResult result;
                result.type = SnapType::Edge;
                result.worldPos = nearestPointOnSegment(mouseWorld, p1, p2);
                result.distance = dist;
                result.layer = poly.layer;
                results.append(result);
            }
        }
    }
}

void Snapper::findIntersectionSnaps(const QPointF& mouseWorld, double tolerance,
                                    const QVector<CanvasPolyline>& polylines,
                                    QVector<SnapResult>& results)
{
    // Collect all segments
    struct Segment {
        QPointF p1, p2;
        QString layer;
    };
    QVector<Segment> segments;
    
    for (const auto& poly : polylines) {
        if (poly.points.size() < 2) continue;
        int segmentCount = poly.closed ? poly.points.size() : poly.points.size() - 1;
        for (int i = 0; i < segmentCount; ++i) {
            Segment seg;
            seg.p1 = poly.points[i];
            seg.p2 = poly.points[(i + 1) % poly.points.size()];
            seg.layer = poly.layer;
            segments.append(seg);
        }
    }
    
    // Check all segment pairs for intersections
    for (int i = 0; i < segments.size(); ++i) {
        for (int j = i + 1; j < segments.size(); ++j) {
            QPointF intersection;
            if (lineIntersection(segments[i].p1, segments[i].p2,
                                segments[j].p1, segments[j].p2, intersection)) {
                double dist = distanceToPoint(mouseWorld, intersection);
                if (dist <= tolerance) {
                    SnapResult result;
                    result.type = SnapType::Intersection;
                    result.worldPos = intersection;
                    result.distance = dist;
                    result.layer = segments[i].layer;
                    results.append(result);
                }
            }
        }
    }
}

double Snapper::distanceToPoint(const QPointF& p1, const QPointF& p2)
{
    double dx = p2.x() - p1.x();
    double dy = p2.y() - p1.y();
    return qSqrt(dx * dx + dy * dy);
}

double Snapper::distanceToSegment(const QPointF& point, const QPointF& segStart, const QPointF& segEnd)
{
    double dx = segEnd.x() - segStart.x();
    double dy = segEnd.y() - segStart.y();
    double lengthSq = dx * dx + dy * dy;
    
    if (lengthSq < 1e-12) {
        // Degenerate segment
        return distanceToPoint(point, segStart);
    }
    
    // Project point onto line
    double t = ((point.x() - segStart.x()) * dx + (point.y() - segStart.y()) * dy) / lengthSq;
    t = qBound(0.0, t, 1.0);
    
    QPointF projection(segStart.x() + t * dx, segStart.y() + t * dy);
    return distanceToPoint(point, projection);
}

QPointF Snapper::nearestPointOnSegment(const QPointF& point, const QPointF& segStart, const QPointF& segEnd)
{
    double dx = segEnd.x() - segStart.x();
    double dy = segEnd.y() - segStart.y();
    double lengthSq = dx * dx + dy * dy;
    
    if (lengthSq < 1e-12) {
        return segStart;
    }
    
    double t = ((point.x() - segStart.x()) * dx + (point.y() - segStart.y()) * dy) / lengthSq;
    t = qBound(0.0, t, 1.0);
    
    return QPointF(segStart.x() + t * dx, segStart.y() + t * dy);
}

bool Snapper::lineIntersection(const QPointF& p1, const QPointF& p2,
                               const QPointF& p3, const QPointF& p4,
                               QPointF& intersection)
{
    double d1x = p2.x() - p1.x();
    double d1y = p2.y() - p1.y();
    double d2x = p4.x() - p3.x();
    double d2y = p4.y() - p3.y();
    
    double cross = d1x * d2y - d1y * d2x;
    
    if (qAbs(cross) < 1e-12) {
        // Lines are parallel
        return false;
    }
    
    double dx = p3.x() - p1.x();
    double dy = p3.y() - p1.y();
    
    double t1 = (dx * d2y - dy * d2x) / cross;
    double t2 = (dx * d1y - dy * d1x) / cross;
    
    // Check if intersection is within both segments
    if (t1 >= 0.0 && t1 <= 1.0 && t2 >= 0.0 && t2 <= 1.0) {
        intersection = QPointF(p1.x() + t1 * d1x, p1.y() + t1 * d1y);
        return true;
    }
    
    return false;
}
