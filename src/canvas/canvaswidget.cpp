#include "canvas/canvaswidget.h"
#include "dxf/dxfreader.h"
#include "gdal/gdalreader.h"
#include "gdal/geosbridge.h"
#include <QPainter>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainterPath>
#include <QtMath>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <ogr_spatialref.h>

CanvasWidget::CanvasWidget(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(400, 300);
    updateTransform();
    
    // Initialize snapper
    m_snapper = new Snapper();
}

CanvasWidget::~CanvasWidget()
{
    delete m_snapper;
}

void CanvasWidget::loadDxfData(const DxfData& data)
{
    clearAll();
    
    // Load layers
    for (const auto& layer : data.layers) {
        CanvasLayer cl;
        cl.name = layer.name;
        cl.color = layer.color;
        cl.visible = layer.visible;
        m_layers.append(cl);
        if (!layer.visible) {
            m_hiddenLayers.insert(layer.name);
        }
    }
    
    // Load lines
    for (const auto& line : data.lines) {
        m_lines.append({line.start, line.end, line.layer, line.color});
    }
    
    // Load circles
    for (const auto& circle : data.circles) {
        m_circles.append({circle.center, circle.radius, circle.layer, circle.color});
    }
    
    // Load arcs
    for (const auto& arc : data.arcs) {
        m_arcs.append({arc.center, arc.radius, arc.startAngle, arc.endAngle, arc.layer, arc.color});
    }
    
    // Load ellipses
    for (const auto& ellipse : data.ellipses) {
        m_ellipses.append({ellipse.center, ellipse.majorAxis, ellipse.ratio, 
                          ellipse.startAngle, ellipse.endAngle, ellipse.layer, ellipse.color});
    }
    
    // Load splines - approximate as polylines
    for (const auto& spline : data.splines) {
        CanvasSpline cs;
        cs.layer = spline.layer;
        cs.color = spline.color;
        
        // Use fit points if available, otherwise control points
        QVector<QPointF> pts = spline.fitPoints.isEmpty() ? spline.controlPoints : spline.fitPoints;
        
        if (pts.size() >= 2) {
            cs.points = interpolateSpline(pts, spline.degree, 50);
        }
        
        if (!cs.points.isEmpty()) {
            m_splines.append(cs);
        }
    }
    
    // Load polylines
    for (const auto& poly : data.polylines) {
        m_polylines.append({poly.points, poly.closed, poly.layer, poly.color});
    }
    
    // Load hatches
    for (const auto& hatch : data.hatches) {
        CanvasHatch ch;
        ch.solid = hatch.solid;
        ch.layer = hatch.layer;
        ch.color = hatch.color;
        for (const auto& loop : hatch.loops) {
            ch.loops.append(loop.points);
        }
        if (!ch.loops.isEmpty()) {
            m_hatches.append(ch);
        }
    }
    
    // Load text
    for (const auto& text : data.texts) {
        m_texts.append({text.text, text.position, text.height, text.angle, text.layer, text.color});
    }
    
    // Expand block inserts
    for (const auto& insert : data.inserts) {
        if (!data.blocks.contains(insert.blockName)) continue;
        const auto& block = data.blocks[insert.blockName];
        
        double cosA = qCos(qDegreesToRadians(insert.rotation));
        double sinA = qSin(qDegreesToRadians(insert.rotation));
        
        auto transformPoint = [&](const QPointF& pt) -> QPointF {
            // Translate by base point, scale, rotate, then translate to insert point
            double x = (pt.x() - block.basePoint.x()) * insert.scaleX;
            double y = (pt.y() - block.basePoint.y()) * insert.scaleY;
            double rx = x * cosA - y * sinA;
            double ry = x * sinA + y * cosA;
            return QPointF(rx + insert.insertPoint.x(), ry + insert.insertPoint.y());
        };
        
        // Transform and add block entities
        for (const auto& line : block.lines) {
            m_lines.append({transformPoint(line.start), transformPoint(line.end), insert.layer, line.color});
        }
        for (const auto& circle : block.circles) {
            double avgScale = (qAbs(insert.scaleX) + qAbs(insert.scaleY)) / 2.0;
            m_circles.append({transformPoint(circle.center), circle.radius * avgScale, insert.layer, circle.color});
        }
        for (const auto& arc : block.arcs) {
            double avgScale = (qAbs(insert.scaleX) + qAbs(insert.scaleY)) / 2.0;
            m_arcs.append({transformPoint(arc.center), arc.radius * avgScale, 
                          arc.startAngle + insert.rotation, arc.endAngle + insert.rotation, 
                          insert.layer, arc.color});
        }
        for (const auto& ellipse : block.ellipses) {
            CanvasEllipse ce;
            ce.center = transformPoint(ellipse.center);
            // Transform major axis vector
            double mx = ellipse.majorAxis.x() * insert.scaleX;
            double my = ellipse.majorAxis.y() * insert.scaleY;
            ce.majorAxis = QPointF(mx * cosA - my * sinA, mx * sinA + my * cosA);
            ce.ratio = ellipse.ratio;
            ce.startAngle = ellipse.startAngle;
            ce.endAngle = ellipse.endAngle;
            ce.layer = insert.layer;
            ce.color = ellipse.color;
            m_ellipses.append(ce);
        }
        for (const auto& poly : block.polylines) {
            CanvasPolyline cp;
            cp.closed = poly.closed;
            cp.layer = insert.layer;
            cp.color = poly.color;
            for (const auto& pt : poly.points) {
                cp.points.append(transformPoint(pt));
            }
            m_polylines.append(cp);
        }
        for (const auto& text : block.texts) {
            m_texts.append({text.text, transformPoint(text.position), 
                           text.height * qAbs(insert.scaleY), text.angle + insert.rotation, 
                           insert.layer, text.color});
        }
    }
    
    emit layersChanged();
    fitToWindow();
    update();
}

void CanvasWidget::clearAll()
{
    m_points.clear();
    m_lines.clear();
    m_circles.clear();
    m_arcs.clear();
    m_ellipses.clear();
    m_splines.clear();
    m_polylines.clear();
    m_polygons.clear();
    m_hatches.clear();
    m_texts.clear();
    m_rasters.clear();
    m_layers.clear();
    m_hiddenLayers.clear();
    update();
}

void CanvasWidget::setLayerVisible(const QString& name, bool visible)
{
    if (visible) {
        m_hiddenLayers.remove(name);
    } else {
        m_hiddenLayers.insert(name);
    }
    
    for (auto& layer : m_layers) {
        if (layer.name == name) {
            layer.visible = visible;
            break;
        }
    }
    
    update();
}

bool CanvasWidget::isLayerVisible(const QString& name) const
{
    return !m_hiddenLayers.contains(name);
}

void CanvasWidget::addLayer(const QString& name, QColor color)
{
    // Check if layer already exists
    for (const auto& layer : m_layers) {
        if (layer.name == name) return;
    }
    
    CanvasLayer newLayer;
    newLayer.name = name;
    newLayer.color = color;
    newLayer.visible = true;
    newLayer.locked = false;
    newLayer.order = m_layers.size();
    m_layers.append(newLayer);
    emit layersChanged();
}

void CanvasWidget::removeLayer(const QString& name)
{
    for (int i = 0; i < m_layers.size(); ++i) {
        if (m_layers[i].name == name) {
            m_layers.removeAt(i);
            m_hiddenLayers.remove(name);
            emit layersChanged();
            update();
            return;
        }
    }
}

void CanvasWidget::renameLayer(const QString& oldName, const QString& newName)
{
    for (auto& layer : m_layers) {
        if (layer.name == oldName) {
            layer.name = newName;
            if (m_hiddenLayers.contains(oldName)) {
                m_hiddenLayers.remove(oldName);
                m_hiddenLayers.insert(newName);
            }
            emit layersChanged();
            return;
        }
    }
}

void CanvasWidget::setLayerColor(const QString& name, QColor color)
{
    for (auto& layer : m_layers) {
        if (layer.name == name) {
            layer.color = color;
            emit layersChanged();
            update();
            return;
        }
    }
}

void CanvasWidget::setLayerLocked(const QString& name, bool locked)
{
    for (auto& layer : m_layers) {
        if (layer.name == name) {
            layer.locked = locked;
            emit layersChanged();
            return;
        }
    }
}

bool CanvasWidget::isLayerLocked(const QString& name) const
{
    for (const auto& layer : m_layers) {
        if (layer.name == name) {
            return layer.locked;
        }
    }
    return false;
}

CanvasLayer* CanvasWidget::getLayer(const QString& name)
{
    for (auto& layer : m_layers) {
        if (layer.name == name) {
            return &layer;
        }
    }
    return nullptr;
}

void CanvasWidget::createDefaultSurveyLayers()
{
    addLayer("Boundary", QColor(255, 255, 255));   // White
    addLayer("Offset", QColor(255, 165, 0));       // Orange
    addLayer("Partition", QColor(0, 255, 255));    // Cyan
    addLayer("Pegs", QColor(255, 0, 0));           // Red
    addLayer("Station", QColor(0, 255, 0));        // Green
}

void CanvasWidget::setShowGrid(bool show)
{
    m_showGrid = show;
    update();
}

void CanvasWidget::setCRS(const QString& epsgCode)
{
    m_crs = epsgCode;
    emit statusMessage(QString("CRS set to: %1").arg(epsgCode));
}

void CanvasWidget::setTargetCRS(const QString& epsgCode)
{
    m_targetCRS = epsgCode;
}

void CanvasWidget::setScaleFactor(double factor)
{
    m_scaleFactor = factor;
}

QPointF CanvasWidget::transformCoordinate(const QPointF& point) const
{
    // If no transformation needed, return original
    if (m_crs == "LOCAL" || m_targetCRS == "NONE" || m_crs == m_targetCRS) {
        return applyScaleFactor(point);
    }
    
    // Use GDAL/OGR for transformation
    OGRSpatialReference srcSRS, dstSRS;
    
    QString srcEpsg = m_crs;
    QString dstEpsg = m_targetCRS;
    
    // Extract EPSG number from "EPSG:XXXX" format
    if (srcEpsg.startsWith("EPSG:")) {
        srcEpsg = srcEpsg.mid(5);
    }
    if (dstEpsg.startsWith("EPSG:")) {
        dstEpsg = dstEpsg.mid(5);
    }
    
    if (srcSRS.importFromEPSG(srcEpsg.toInt()) != OGRERR_NONE) {
        return applyScaleFactor(point);  // Fallback
    }
    if (dstSRS.importFromEPSG(dstEpsg.toInt()) != OGRERR_NONE) {
        return applyScaleFactor(point);  // Fallback
    }
    
    OGRCoordinateTransformation* transform = 
        OGRCreateCoordinateTransformation(&srcSRS, &dstSRS);
    
    if (!transform) {
        return applyScaleFactor(point);  // Fallback
    }
    
    double x = point.x();
    double y = point.y();
    
    if (transform->Transform(1, &x, &y)) {
        OGRCoordinateTransformation::DestroyCT(transform);
        return applyScaleFactor(QPointF(x, y));
    }
    
    OGRCoordinateTransformation::DestroyCT(transform);
    return applyScaleFactor(point);  // Fallback
}

QPointF CanvasWidget::applyScaleFactor(const QPointF& point) const
{
    if (qFuzzyCompare(m_scaleFactor, 1.0)) {
        return point;
    }
    return QPointF(point.x() * m_scaleFactor, point.y() * m_scaleFactor);
}

void CanvasWidget::updateTransform()
{
    m_worldToScreen = QTransform();
    m_worldToScreen.translate(width() / 2.0, height() / 2.0);
    m_worldToScreen.scale(m_zoom, -m_zoom);
    m_worldToScreen.translate(m_offset.x(), m_offset.y());
    m_screenToWorld = m_worldToScreen.inverted();
}

QPointF CanvasWidget::screenToWorld(const QPoint& screen) const
{
    return m_screenToWorld.map(QPointF(screen));
}

QPoint CanvasWidget::worldToScreen(const QPointF& world) const
{
    return m_worldToScreen.map(world).toPoint();
}

void CanvasWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    // Background
    painter.fillRect(rect(), m_backgroundColor);
    
    // Grid
    if (m_showGrid) {
        drawGrid(painter);
    }
    
    // DXF entities
    drawEntities(painter);
    
    // Selection highlight (on top of entities)
    drawSelection(painter);
    
    // Peg markers
    drawPegs(painter);
    
    // Station and backsight markers
    drawStation(painter);
    
    // Stakeout sighting line (if in stakeout mode)
    drawStakeoutLine(painter);
    
    // Snap marker (on top of everything)
    drawSnapMarker(painter);
    
    // Origin crosshair
    painter.setPen(QPen(Qt::gray, 1));
    QPoint origin = worldToScreen(QPointF(0, 0));
    painter.drawLine(origin.x() - 20, origin.y(), origin.x() + 20, origin.y());
    painter.drawLine(origin.x(), origin.y() - 20, origin.x(), origin.y() + 20);
}

void CanvasWidget::drawGrid(QPainter& painter)
{
    painter.setPen(QPen(m_gridColor, 1));
    
    QPointF topLeft = screenToWorld(QPoint(0, 0));
    QPointF bottomRight = screenToWorld(QPoint(width(), height()));
    
    double minX = qMin(topLeft.x(), bottomRight.x());
    double maxX = qMax(topLeft.x(), bottomRight.x());
    double minY = qMin(topLeft.y(), bottomRight.y());
    double maxY = qMax(topLeft.y(), bottomRight.y());
    
    double gridStep = m_gridSize;
    while (gridStep * m_zoom < 10.0) gridStep *= 2.0;
    while (gridStep * m_zoom > 100.0) gridStep /= 2.0;
    
    double startX = qFloor(minX / gridStep) * gridStep;
    double startY = qFloor(minY / gridStep) * gridStep;
    
    for (double x = startX; x <= maxX; x += gridStep) {
        painter.drawLine(worldToScreen(QPointF(x, minY)), worldToScreen(QPointF(x, maxY)));
    }
    
    for (double y = startY; y <= maxY; y += gridStep) {
        painter.drawLine(worldToScreen(QPointF(minX, y)), worldToScreen(QPointF(maxX, y)));
    }
}

void CanvasWidget::drawEntities(QPainter& painter)
{
    // Draw rasters first (background)
    for (const auto& raster : m_rasters) {
        if (m_hiddenLayers.contains(raster.layer)) continue;
        drawRaster(painter, raster);
    }
    
    // Draw polygons (filled)
    for (const auto& polygon : m_polygons) {
        if (m_hiddenLayers.contains(polygon.layer)) continue;
        drawPolygon(painter, polygon);
    }
    
    // Draw hatches (as fill)
    for (const auto& hatch : m_hatches) {
        if (m_hiddenLayers.contains(hatch.layer)) continue;
        drawHatch(painter, hatch);
    }
    
    // Draw lines
    for (const auto& line : m_lines) {
        if (m_hiddenLayers.contains(line.layer)) continue;
        QPen pen(line.color, 1);
        pen.setCosmetic(true);
        painter.setPen(pen);
        painter.drawLine(worldToScreen(line.start), worldToScreen(line.end));
    }
    
    // Draw circles
    for (const auto& circle : m_circles) {
        if (m_hiddenLayers.contains(circle.layer)) continue;
        QPen pen(circle.color, 1);
        pen.setCosmetic(true);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        QPoint center = worldToScreen(circle.center);
        double screenRadius = circle.radius * m_zoom;
        painter.drawEllipse(QPointF(center), screenRadius, screenRadius);
    }
    
    // Draw arcs
    for (const auto& arc : m_arcs) {
        if (m_hiddenLayers.contains(arc.layer)) continue;
        QPen pen(arc.color, 1);
        pen.setCosmetic(true);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        QPoint center = worldToScreen(arc.center);
        double screenRadius = arc.radius * m_zoom;
        QRectF rect(center.x() - screenRadius, center.y() - screenRadius,
                    screenRadius * 2, screenRadius * 2);
        // Negate angles because Y-axis is flipped (Qt angles are counter-clockwise in screen coords)
        double startAngle = -arc.endAngle * 16;
        double spanAngle = (arc.endAngle - arc.startAngle) * 16;
        if (spanAngle < 0) spanAngle += 360 * 16;
        painter.drawArc(rect, static_cast<int>(startAngle), static_cast<int>(spanAngle));
    }
    
    // Draw ellipses
    for (const auto& ellipse : m_ellipses) {
        if (m_hiddenLayers.contains(ellipse.layer)) continue;
        drawEllipse(painter, ellipse);
    }
    
    // Draw splines
    for (const auto& spline : m_splines) {
        if (m_hiddenLayers.contains(spline.layer)) continue;
        drawSpline(painter, spline);
    }
    
    // Draw polylines
    for (const auto& poly : m_polylines) {
        if (m_hiddenLayers.contains(poly.layer)) continue;
        if (poly.points.size() < 2) continue;
        QPen pen(poly.color, 1);
        pen.setCosmetic(true);
        painter.setPen(pen);
        for (int i = 1; i < poly.points.size(); ++i) {
            painter.drawLine(worldToScreen(poly.points[i-1]), worldToScreen(poly.points[i]));
        }
        if (poly.closed && poly.points.size() > 2) {
            painter.drawLine(worldToScreen(poly.points.last()), worldToScreen(poly.points.first()));
        }
    }
    
    // Draw points
    for (const auto& point : m_points) {
        if (m_hiddenLayers.contains(point.layer)) continue;
        QPen pen(point.color, 5);
        pen.setCosmetic(true);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.drawPoint(worldToScreen(point.position));
    }
    
    // Draw text
    for (const auto& text : m_texts) {
        if (m_hiddenLayers.contains(text.layer)) continue;
        painter.setPen(text.color);
        QPoint pos = worldToScreen(text.position);
        double fontSize = qMax(8.0, text.height * m_zoom);
        QFont font = painter.font();
        font.setPointSizeF(fontSize);
        painter.setFont(font);
        painter.save();
        painter.translate(pos);
        painter.rotate(text.angle);  // Positive rotation in screen coords
        // Draw text at baseline (no Y flip needed - just offset up by font height)
        painter.drawText(0, 0, text.text);
        painter.restore();
    }
}

void CanvasWidget::drawEllipse(QPainter& painter, const CanvasEllipse& ellipse)
{
    QPen pen(ellipse.color, 1);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    
    // Calculate ellipse parameters
    double majorLen = qSqrt(ellipse.majorAxis.x() * ellipse.majorAxis.x() + 
                           ellipse.majorAxis.y() * ellipse.majorAxis.y());
    double minorLen = majorLen * ellipse.ratio;
    double rotation = qAtan2(ellipse.majorAxis.y(), ellipse.majorAxis.x());
    
    // Approximate ellipse as polyline
    int segments = 64;
    double startAngle = ellipse.startAngle;
    double endAngle = ellipse.endAngle;
    if (endAngle <= startAngle) endAngle += 2 * M_PI;
    
    QVector<QPoint> screenPoints;
    for (int i = 0; i <= segments; ++i) {
        double t = startAngle + (endAngle - startAngle) * i / segments;
        double x = majorLen * qCos(t);
        double y = minorLen * qSin(t);
        // Rotate
        double rx = x * qCos(rotation) - y * qSin(rotation);
        double ry = x * qSin(rotation) + y * qCos(rotation);
        screenPoints.append(worldToScreen(QPointF(ellipse.center.x() + rx, ellipse.center.y() + ry)));
    }
    
    for (int i = 1; i < screenPoints.size(); ++i) {
        painter.drawLine(screenPoints[i-1], screenPoints[i]);
    }
}

void CanvasWidget::drawSpline(QPainter& painter, const CanvasSpline& spline)
{
    if (spline.points.size() < 2) return;
    
    QPen pen(spline.color, 1);
    pen.setCosmetic(true);
    painter.setPen(pen);
    
    for (int i = 1; i < spline.points.size(); ++i) {
        painter.drawLine(worldToScreen(spline.points[i-1]), worldToScreen(spline.points[i]));
    }
}

void CanvasWidget::drawHatch(QPainter& painter, const CanvasHatch& hatch)
{
    if (hatch.loops.isEmpty()) return;
    
    QColor fillColor = hatch.color;
    fillColor.setAlpha(hatch.solid ? 100 : 50);
    
    QPainterPath path;
    for (const auto& loop : hatch.loops) {
        if (loop.size() < 2) continue;
        path.moveTo(m_worldToScreen.map(loop.first()));
        for (int i = 1; i < loop.size(); ++i) {
            path.lineTo(m_worldToScreen.map(loop[i]));
        }
        path.closeSubpath();
    }
    
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);
    painter.drawPath(path);
}

QVector<QPointF> CanvasWidget::interpolateSpline(const QVector<QPointF>& controlPoints, int degree, int segments)
{
    Q_UNUSED(degree);
    
    if (controlPoints.size() < 2) return controlPoints;
    
    // Simple Catmull-Rom spline interpolation
    QVector<QPointF> result;
    
    for (int i = 0; i < controlPoints.size() - 1; ++i) {
        QPointF p0 = (i > 0) ? controlPoints[i-1] : controlPoints[i];
        QPointF p1 = controlPoints[i];
        QPointF p2 = controlPoints[i+1];
        QPointF p3 = (i < controlPoints.size() - 2) ? controlPoints[i+2] : controlPoints[i+1];
        
        int segs = segments / (controlPoints.size() - 1);
        for (int j = 0; j <= segs; ++j) {
            double t = static_cast<double>(j) / segs;
            double t2 = t * t;
            double t3 = t2 * t;
            
            // Catmull-Rom interpolation
            double x = 0.5 * ((2 * p1.x()) + 
                             (-p0.x() + p2.x()) * t + 
                             (2*p0.x() - 5*p1.x() + 4*p2.x() - p3.x()) * t2 + 
                             (-p0.x() + 3*p1.x() - 3*p2.x() + p3.x()) * t3);
            double y = 0.5 * ((2 * p1.y()) + 
                             (-p0.y() + p2.y()) * t + 
                             (2*p0.y() - 5*p1.y() + 4*p2.y() - p3.y()) * t2 + 
                             (-p0.y() + 3*p1.y() - 3*p2.y() + p3.y()) * t3);
            
            if (result.isEmpty() || result.last() != QPointF(x, y)) {
                result.append(QPointF(x, y));
            }
        }
    }
    
    return result;
}

void CanvasWidget::wheelEvent(QWheelEvent *event)
{
    const double zoomFactor = 1.15;
    QPointF cursorWorldBefore = screenToWorld(event->position().toPoint());
    
    if (event->angleDelta().y() > 0) {
        m_zoom *= zoomFactor;
    } else {
        m_zoom /= zoomFactor;
    }
    
    m_zoom = qBound(1e-4, m_zoom, 1e6);
    updateTransform();
    
    QPointF cursorWorldAfter = screenToWorld(event->position().toPoint());
    m_offset += cursorWorldBefore - cursorWorldAfter;
    updateTransform();
    update();
    emit zoomChanged(m_zoom);
}

void CanvasWidget::mousePressEvent(QMouseEvent *event)
{
    QPointF worldPos = screenToWorld(event->pos());
    
    // Handle offset tool state: waiting for side click
    if (m_toolState == ToolState::OffsetWaitForSide && event->button() == Qt::LeftButton) {
        executeOffset(worldPos);
        return;
    }
    
    // Handle split mode: click to split polyline at point
    if (m_toolState == ToolState::SplitMode && event->button() == Qt::LeftButton) {
        splitPolylineAtPoint(worldPos);
        return;
    }
    
    // Handle station setup modes
    if (m_toolState == ToolState::SetStation && event->button() == Qt::LeftButton) {
        double tolerance = m_snapTolerance / m_zoom;
        int pegIndex = hitTestPeg(worldPos, tolerance);
        
        if (pegIndex >= 0) {
            // Snap to exact peg position
            const CanvasPeg& peg = m_pegs[pegIndex];
            setStationPoint(peg.position, peg.name);
            m_toolState = ToolState::Idle;
            setCursor(Qt::ArrowCursor);
        } else {
            emit statusMessage("Station must be placed on a peg. Click on a peg marker.");
        }
        return;
    }
    
    if (m_toolState == ToolState::SetBacksight && event->button() == Qt::LeftButton) {
        double tolerance = m_snapTolerance / m_zoom;
        int pegIndex = hitTestPeg(worldPos, tolerance);
        
        if (pegIndex >= 0) {
            // Snap to exact peg position
            const CanvasPeg& peg = m_pegs[pegIndex];
            setBacksightPoint(peg.position, peg.name);
            m_toolState = ToolState::Idle;
            setCursor(Qt::ArrowCursor);
        } else {
            emit statusMessage("Backsight must be placed on a peg. Click on a peg marker.");
        }
        return;
    }
    
    // Pan mode: left-click starts panning
    if (m_toolState == ToolState::PanMode && event->button() == Qt::LeftButton) {
        m_isPanning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    
    // Middle button or Ctrl+Left for panning (always works)
    if (event->button() == Qt::MiddleButton || 
        (event->button() == Qt::LeftButton && event->modifiers() & Qt::ControlModifier)) {
        m_isPanning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    
    // Left click: select polyline (only in Idle/None mode)
    if (event->button() == Qt::LeftButton && 
        (m_toolState == ToolState::Idle || m_toolState == ToolState::None)) {
        double tolerance = m_snapTolerance / m_zoom;
        int hitIndex = hitTestPolyline(worldPos, tolerance);
        
        if (event->modifiers() & Qt::ShiftModifier) {
            // Shift+click: add to / toggle in selection
            if (hitIndex >= 0) {
                if (isSelected(hitIndex)) {
                    removeFromSelection(hitIndex);
                } else {
                    addToSelection(hitIndex);
                }
            }
        } else {
            // Regular click: replace selection
            if (hitIndex != m_selectedPolylineIndex) {
                m_selectedPolylines.clear();
                m_selectedPolylineIndex = hitIndex;
                if (hitIndex >= 0) {
                    m_selectedPolylines.insert(hitIndex);
                }
                emit selectionChanged(m_selectedPolylineIndex);
                update();
            }
        }
    }
}

void CanvasWidget::mouseMoveEvent(QMouseEvent *event)
{
    QPointF worldPos = screenToWorld(event->pos());
    emit mouseWorldPosition(worldPos);
    
    // Update snap detection
    if (m_snapper && m_snapper->isEnabled()) {
        // Convert pixel tolerance to world units
        double toleranceWorld = m_snapTolerance / m_zoom;
        SnapResult newSnap = m_snapper->findSnap(worldPos, toleranceWorld, m_polylines);
        
        if (newSnap.type != m_currentSnap.type || 
            newSnap.worldPos != m_currentSnap.worldPos) {
            m_currentSnap = newSnap;
            emit snapChanged(m_currentSnap);
            update();  // Redraw snap marker
        }
    }
    
    // Track cursor position for stakeout mode
    if (m_toolState == ToolState::StakeoutMode) {
        m_stakeoutCursorPos = screenToWorld(event->pos());
        update();  // Redraw stakeout line
    }
    
    if (m_isPanning) {
        QPointF delta = screenToWorld(event->pos()) - screenToWorld(m_lastMousePos);
        m_offset += delta;
        m_lastMousePos = event->pos();
        updateTransform();
        update();
    }
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton) {
        if (m_isPanning) {
            m_isPanning = false;
            // Restore appropriate cursor based on mode
            if (m_toolState == ToolState::PanMode) {
                setCursor(Qt::OpenHandCursor);
            } else {
                setCursor(Qt::ArrowCursor);
            }
        }
    }
}

void CanvasWidget::setPanMode(bool enabled)
{
    if (enabled) {
        m_toolState = ToolState::PanMode;
        setCursor(Qt::OpenHandCursor);
        emit statusMessage("Pan mode - drag to pan, press Escape to exit");
    } else {
        m_toolState = ToolState::Idle;
        setCursor(Qt::ArrowCursor);
        emit statusMessage("Selection mode");
    }
}

void CanvasWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    updateTransform();
}

void CanvasWidget::fitToWindow()
{
    double minX = 0, maxX = 0, minY = 0, maxY = 0;
    bool hasData = false;
    
    auto updateBounds = [&](const QPointF& p) {
        if (!hasData) {
            minX = maxX = p.x();
            minY = maxY = p.y();
            hasData = true;
        } else {
            minX = qMin(minX, p.x());
            maxX = qMax(maxX, p.x());
            minY = qMin(minY, p.y());
            maxY = qMax(maxY, p.y());
        }
    };
    
    for (const auto& line : m_lines) {
        updateBounds(line.start);
        updateBounds(line.end);
    }
    for (const auto& circle : m_circles) {
        updateBounds(QPointF(circle.center.x() - circle.radius, circle.center.y() - circle.radius));
        updateBounds(QPointF(circle.center.x() + circle.radius, circle.center.y() + circle.radius));
    }
    for (const auto& arc : m_arcs) {
        updateBounds(QPointF(arc.center.x() - arc.radius, arc.center.y() - arc.radius));
        updateBounds(QPointF(arc.center.x() + arc.radius, arc.center.y() + arc.radius));
    }
    for (const auto& ellipse : m_ellipses) {
        double majorLen = qSqrt(ellipse.majorAxis.x() * ellipse.majorAxis.x() + 
                               ellipse.majorAxis.y() * ellipse.majorAxis.y());
        updateBounds(QPointF(ellipse.center.x() - majorLen, ellipse.center.y() - majorLen));
        updateBounds(QPointF(ellipse.center.x() + majorLen, ellipse.center.y() + majorLen));
    }
    for (const auto& spline : m_splines) {
        for (const auto& p : spline.points) updateBounds(p);
    }
    for (const auto& poly : m_polylines) {
        for (const auto& p : poly.points) updateBounds(p);
    }
    for (const auto& hatch : m_hatches) {
        for (const auto& loop : hatch.loops) {
            for (const auto& p : loop) updateBounds(p);
        }
    }
    for (const auto& text : m_texts) {
        updateBounds(text.position);
    }
    
    if (!hasData) {
        resetView();
        return;
    }
    
    double margin = 50.0;
    double dataWidth = maxX - minX;
    double dataHeight = maxY - minY;
    
    if (dataWidth < 0.001) dataWidth = 1.0;
    if (dataHeight < 0.001) dataHeight = 1.0;
    
    double scaleX = (width() - 2 * margin) / dataWidth;
    double scaleY = (height() - 2 * margin) / dataHeight;
    m_zoom = qMin(scaleX, scaleY);
    m_zoom = qBound(1e-4, m_zoom, 1e6);
    
    double centerX = (minX + maxX) / 2.0;
    double centerY = (minY + maxY) / 2.0;
    m_offset = QPointF(-centerX, -centerY);
    
    updateTransform();
    update();
    emit zoomChanged(m_zoom);
}

void CanvasWidget::zoomIn()
{
    m_zoom *= 1.2;
    updateTransform();
    update();
    emit zoomChanged(m_zoom);
}

void CanvasWidget::zoomOut()
{
    m_zoom /= 1.2;
    updateTransform();
    update();
    emit zoomChanged(m_zoom);
}

void CanvasWidget::resetView()
{
    m_zoom = 1.0;
    m_offset = QPointF(0, 0);
    updateTransform();
    update();
    emit zoomChanged(m_zoom);
}

// GDAL data loading
void CanvasWidget::loadGdalData(const GdalData& data)
{
    clearAll();
    
    // Load layers
    for (const auto& layer : data.layers) {
        CanvasLayer cl;
        cl.name = layer.name;
        cl.color = Qt::white;
        cl.visible = layer.visible;
        m_layers.append(cl);
        if (!layer.visible) {
            m_hiddenLayers.insert(layer.name);
        }
    }
    
    // Load points
    for (const auto& pt : data.points) {
        CanvasPoint cp;
        cp.position = pt.position;
        cp.layer = pt.layer;
        cp.color = pt.color;
        m_points.append(cp);
    }
    
    // Load line strings as polylines
    for (const auto& ls : data.lineStrings) {
        CanvasPolyline cp;
        cp.points = ls.points;
        cp.closed = false;
        cp.layer = ls.layer;
        cp.color = ls.color;
        m_polylines.append(cp);
    }
    
    // Load polygons
    for (const auto& poly : data.polygons) {
        CanvasPolygon cp;
        cp.rings = poly.rings;
        cp.layer = poly.layer;
        cp.color = poly.color;
        cp.fillColor = poly.fillColor;
        m_polygons.append(cp);
    }
    
    // Load text
    for (const auto& text : data.texts) {
        CanvasText ct;
        ct.text = text.text;
        ct.position = text.position;
        ct.height = text.height;
        ct.angle = text.angle;
        ct.layer = text.layer;
        ct.color = text.color;
        m_texts.append(ct);
    }
    
    // Load rasters
    for (const auto& raster : data.rasters) {
        CanvasRaster cr;
        cr.image = raster.image;
        cr.bounds = raster.bounds;
        cr.layer = raster.layer;
        m_rasters.append(cr);
    }
    
    emit layersChanged();
    fitToWindow();
    update();
}

void CanvasWidget::drawPolygon(QPainter& painter, const CanvasPolygon& polygon)
{
    if (polygon.rings.isEmpty()) return;
    
    QPainterPath path;
    
    for (int r = 0; r < polygon.rings.size(); ++r) {
        const auto& ring = polygon.rings[r];
        if (ring.size() < 3) continue;
        
        QPolygonF screenPoly;
        for (const auto& pt : ring) {
            screenPoly.append(m_worldToScreen.map(pt));
        }
        
        if (r == 0) {
            // Exterior ring
            path.addPolygon(screenPoly);
        } else {
            // Interior ring (hole)
            QPainterPath hole;
            hole.addPolygon(screenPoly);
            path = path.subtracted(hole);
        }
    }
    
    // Fill
    painter.setPen(Qt::NoPen);
    painter.setBrush(polygon.fillColor);
    painter.drawPath(path);
    
    // Outline
    QPen pen(polygon.color, 1);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}

void CanvasWidget::drawRaster(QPainter& painter, const CanvasRaster& raster)
{
    if (raster.image.isNull()) return;
    
    // Calculate screen coordinates for the raster bounds
    QPoint topLeft = worldToScreen(QPointF(raster.bounds.left(), raster.bounds.top() + raster.bounds.height()));
    QPoint bottomRight = worldToScreen(QPointF(raster.bounds.right(), raster.bounds.top()));
    
    QRect screenRect(topLeft, bottomRight);
    
    // Draw the raster image scaled to fit the screen rect
    painter.drawImage(screenRect, raster.image);
}

void CanvasWidget::drawSnapMarker(QPainter& painter)
{
    if (!m_currentSnap.isValid()) return;
    
    QPoint screenPos = worldToScreen(m_currentSnap.worldPos);
    const int size = 12;
    
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    // Different shapes for different snap types
    switch (m_currentSnap.type) {
        case SnapType::Endpoint: {
            // Square marker for endpoints
            QPen pen(Qt::cyan, 2);
            pen.setCosmetic(true);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(screenPos.x() - size/2, screenPos.y() - size/2, size, size);
            break;
        }
        case SnapType::Midpoint: {
            // Triangle marker for midpoints
            QPen pen(Qt::yellow, 2);
            pen.setCosmetic(true);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            QPolygon triangle;
            triangle << QPoint(screenPos.x(), screenPos.y() - size/2)
                     << QPoint(screenPos.x() - size/2, screenPos.y() + size/2)
                     << QPoint(screenPos.x() + size/2, screenPos.y() + size/2);
            painter.drawPolygon(triangle);
            break;
        }
        case SnapType::Intersection: {
            // Diamond (rotated square) for intersections
            QPen pen(Qt::magenta, 2);
            pen.setCosmetic(true);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            QPolygon diamond;
            diamond << QPoint(screenPos.x(), screenPos.y() - size/2)
                    << QPoint(screenPos.x() + size/2, screenPos.y())
                    << QPoint(screenPos.x(), screenPos.y() + size/2)
                    << QPoint(screenPos.x() - size/2, screenPos.y());
            painter.drawPolygon(diamond);
            break;
        }
        case SnapType::Edge: {
            // Circle for edge/nearest
            QPen pen(Qt::green, 2);
            pen.setCosmetic(true);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(screenPos, size/2, size/2);
            break;
        }
        default:
            break;
    }
    
    // Draw crosshair at snap point
    QPen crossPen(Qt::white, 1);
    crossPen.setCosmetic(true);
    painter.setPen(crossPen);
    painter.drawLine(screenPos.x() - size, screenPos.y(), screenPos.x() + size, screenPos.y());
    painter.drawLine(screenPos.x(), screenPos.y() - size, screenPos.x(), screenPos.y() + size);
}

void CanvasWidget::setSnappingEnabled(bool enabled)
{
    if (m_snapper) {
        m_snapper->setEnabled(enabled);
        if (!enabled) {
            m_currentSnap = SnapResult{};
            update();
        }
    }
}

bool CanvasWidget::isSnappingEnabled() const
{
    return m_snapper && m_snapper->isEnabled();
}

void CanvasWidget::addPolyline(const CanvasPolyline& polyline)
{
    // Push to undo stack
    UndoCommand cmd;
    cmd.type = UndoType::AddPolyline;
    cmd.polyline = polyline;
    cmd.index = m_polylines.size();
    m_undoStack.append(cmd);
    m_redoStack.clear();  // Clear redo stack on new action
    emit undoRedoChanged();
    
    m_polylines.append(polyline);
    update();
}

void CanvasWidget::undo()
{
    if (m_undoStack.isEmpty()) return;
    
    UndoCommand cmd = m_undoStack.takeLast();
    
    switch (cmd.type) {
        case UndoType::AddPolyline:
            // Remove the added polyline
            if (cmd.index >= 0 && cmd.index < m_polylines.size()) {
                m_polylines.remove(cmd.index);
                // Adjust selection if needed
                if (m_selectedPolylineIndex >= cmd.index) {
                    m_selectedPolylineIndex = qMax(-1, m_selectedPolylineIndex - 1);
                    emit selectionChanged(m_selectedPolylineIndex);
                }
            }
            break;
        case UndoType::DeletePolyline:
            // Re-insert the deleted polyline
            if (cmd.index >= 0 && cmd.index <= m_polylines.size()) {
                m_polylines.insert(cmd.index, cmd.polyline);
            }
            break;
    }
    
    // Push to redo stack (inverted)
    cmd.type = (cmd.type == UndoType::AddPolyline) ? UndoType::DeletePolyline : UndoType::AddPolyline;
    m_redoStack.append(cmd);
    
    emit undoRedoChanged();
    emit statusMessage("Undo");
    update();
}

void CanvasWidget::redo()
{
    if (m_redoStack.isEmpty()) return;
    
    UndoCommand cmd = m_redoStack.takeLast();
    
    switch (cmd.type) {
        case UndoType::AddPolyline:
            // Re-add the polyline (was deleted in undo)
            if (cmd.index >= 0 && cmd.index <= m_polylines.size()) {
                m_polylines.insert(cmd.index, cmd.polyline);
            }
            break;
        case UndoType::DeletePolyline:
            // Remove the polyline again
            if (cmd.index >= 0 && cmd.index < m_polylines.size()) {
                m_polylines.remove(cmd.index);
                if (m_selectedPolylineIndex >= cmd.index) {
                    m_selectedPolylineIndex = qMax(-1, m_selectedPolylineIndex - 1);
                    emit selectionChanged(m_selectedPolylineIndex);
                }
            }
            break;
    }
    
    // Push back to undo stack (inverted again)
    cmd.type = (cmd.type == UndoType::AddPolyline) ? UndoType::DeletePolyline : UndoType::AddPolyline;
    m_undoStack.append(cmd);
    
    emit undoRedoChanged();
    emit statusMessage("Redo");
    update();
}

void CanvasWidget::clearSelection()
{
    if (m_selectedPolylineIndex >= 0 || !m_selectedPolylines.isEmpty()) {
        m_selectedPolylineIndex = -1;
        m_selectedPolylines.clear();
        emit selectionChanged(-1);
        update();
    }
}

const CanvasPolyline* CanvasWidget::selectedPolyline() const
{
    if (m_selectedPolylineIndex >= 0 && m_selectedPolylineIndex < m_polylines.size()) {
        return &m_polylines[m_selectedPolylineIndex];
    }
    return nullptr;
}

void CanvasWidget::addToSelection(int index)
{
    if (index >= 0 && index < m_polylines.size()) {
        m_selectedPolylines.insert(index);
        if (m_selectedPolylineIndex < 0) {
            m_selectedPolylineIndex = index;
        }
        emit selectionChanged(index);
        update();
    }
}

void CanvasWidget::removeFromSelection(int index)
{
    m_selectedPolylines.remove(index);
    if (m_selectedPolylineIndex == index) {
        m_selectedPolylineIndex = m_selectedPolylines.isEmpty() ? -1 : *m_selectedPolylines.begin();
    }
    emit selectionChanged(m_selectedPolylineIndex);
    update();
}

bool CanvasWidget::isSelected(int index) const
{
    return index == m_selectedPolylineIndex || m_selectedPolylines.contains(index);
}

QVector<int> CanvasWidget::getSelectedIndices() const
{
    QVector<int> result;
    if (m_selectedPolylineIndex >= 0) {
        result.append(m_selectedPolylineIndex);
    }
    for (int idx : m_selectedPolylines) {
        if (!result.contains(idx)) {
            result.append(idx);
        }
    }
    return result;
}

int CanvasWidget::hitTestPolyline(const QPointF& worldPos, double tolerance)
{
    // Check each polyline for proximity to click point
    for (int i = 0; i < m_polylines.size(); ++i) {
        const auto& poly = m_polylines[i];
        if (poly.points.size() < 2) continue;
        
        // Check if layer is visible
        if (m_hiddenLayers.contains(poly.layer)) continue;
        
        int segmentCount = poly.closed ? poly.points.size() : poly.points.size() - 1;
        for (int j = 0; j < segmentCount; ++j) {
            const QPointF& p1 = poly.points[j];
            const QPointF& p2 = poly.points[(j + 1) % poly.points.size()];
            
            // Distance from point to segment
            double dx = p2.x() - p1.x();
            double dy = p2.y() - p1.y();
            double lengthSq = dx * dx + dy * dy;
            
            double t = 0;
            if (lengthSq > 1e-12) {
                t = ((worldPos.x() - p1.x()) * dx + (worldPos.y() - p1.y()) * dy) / lengthSq;
                t = qBound(0.0, t, 1.0);
            }
            
            QPointF projection(p1.x() + t * dx, p1.y() + t * dy);
            double distX = worldPos.x() - projection.x();
            double distY = worldPos.y() - projection.y();
            double dist = qSqrt(distX * distX + distY * distY);
            
            if (dist <= tolerance) {
                return i;  // Hit!
            }
        }
    }
    return -1;  // No hit
}

bool CanvasWidget::isLeft(const QPointF& a, const QPointF& b, const QPointF& p)
{
    // Cross product: (B-A) × (P-A)
    // Positive = P is on left of A→B
    double cross = (b.x() - a.x()) * (p.y() - a.y()) 
                 - (b.y() - a.y()) * (p.x() - a.x());
    return cross > 0;
}

void CanvasWidget::startOffsetTool(double distance)
{
    if (!hasSelection()) {
        emit statusMessage("Please select a polyline first");
        return;
    }
    
    m_pendingOffsetDistance = distance;
    m_toolState = ToolState::OffsetWaitForSide;
    setCursor(Qt::CrossCursor);
    emit statusMessage("Click to indicate offset side (left or right of line)");
}

void CanvasWidget::cancelOffsetTool()
{
    if (m_toolState != ToolState::Idle) {
        m_toolState = ToolState::Idle;
        m_pendingOffsetDistance = 0;
        setCursor(Qt::ArrowCursor);
        emit statusMessage("Offset cancelled");
    }
}

void CanvasWidget::executeOffset(const QPointF& sideClickPos)
{
    if (m_toolState != ToolState::OffsetWaitForSide || !hasSelection()) {
        m_toolState = ToolState::Idle;
        setCursor(Qt::ArrowCursor);
        return;
    }
    
    const CanvasPolyline* poly = selectedPolyline();
    if (!poly || poly->points.size() < 2) {
        emit statusMessage("Invalid polyline selected");
        m_toolState = ToolState::Idle;
        setCursor(Qt::ArrowCursor);
        return;
    }
    
    // Determine which side the user clicked using first segment
    const QPointF& a = poly->points[0];
    const QPointF& b = poly->points[1];
    bool clickedLeft = isLeft(a, b, sideClickPos);
    
    // Signed distance: positive = left, negative = right
    double signedDistance = clickedLeft ? m_pendingOffsetDistance : -m_pendingOffsetDistance;
    
    // Convert to DxfPolyline and create offset
    DxfPolyline dxfPoly;
    dxfPoly.points = poly->points;
    dxfPoly.closed = poly->closed;
    dxfPoly.layer = poly->layer;
    dxfPoly.color = poly->color;
    
    GeosBridge::initialize();
    DxfPolyline offsetPoly = GeosBridge::createOffset(dxfPoly, signedDistance);
    
    if (!offsetPoly.points.isEmpty()) {
        // Add the new offset polyline
        CanvasPolyline newPoly;
        newPoly.points = offsetPoly.points;
        newPoly.closed = offsetPoly.closed;
        newPoly.layer = offsetPoly.layer + "_offset";
        newPoly.color = QColor(255, 128, 0);  // Orange for offset lines
        m_polylines.append(newPoly);
        
        // Auto-create peg markers at offset vertices
        addPegsFromPolyline(newPoly, "PEG");
        
        // Select the new polyline (CAD standard behavior)
        m_selectedPolylineIndex = m_polylines.size() - 1;
        emit selectionChanged(m_selectedPolylineIndex);
        emit offsetCompleted(true);
        emit statusMessage(QString("Offset created at distance %1 (%2 side) with %3 pegs")
            .arg(qAbs(m_pendingOffsetDistance), 0, 'f', 3)
            .arg(clickedLeft ? "left" : "right")
            .arg(newPoly.points.size()));
    } else {
        emit offsetCompleted(false);
        emit statusMessage("Offset failed: " + GeosBridge::lastError());
    }
    
    m_toolState = ToolState::Idle;
    m_pendingOffsetDistance = 0;
    setCursor(Qt::ArrowCursor);
    update();
}

void CanvasWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        if (m_toolState != ToolState::Idle) {
            cancelOffsetTool();
        } else if (hasSelection()) {
            clearSelection();
        }
        return;
    }
    QWidget::keyPressEvent(event);
}

void CanvasWidget::drawSelection(QPainter& painter)
{
    if (m_selectedPolylineIndex < 0 || m_selectedPolylineIndex >= m_polylines.size()) {
        return;
    }
    
    const auto& poly = m_polylines[m_selectedPolylineIndex];
    if (poly.points.size() < 2) return;
    
    // Draw selected polyline with dashed cyan line
    QPen selectionPen(Qt::cyan, 2);
    selectionPen.setCosmetic(true);
    selectionPen.setStyle(Qt::DashLine);
    painter.setPen(selectionPen);
    painter.setBrush(Qt::NoBrush);
    
    QPolygonF screenPoly;
    for (const auto& pt : poly.points) {
        screenPoly << QPointF(worldToScreen(pt));
    }
    
    if (poly.closed && !screenPoly.isEmpty()) {
        screenPoly << screenPoly.first();
    }
    
    painter.drawPolyline(screenPoly);
    
    // Draw vertex handles
    QPen handlePen(Qt::cyan, 1);
    handlePen.setCosmetic(true);
    painter.setPen(handlePen);
    painter.setBrush(QBrush(QColor(0, 255, 255, 100)));
    
    const int handleSize = 6;
    for (const auto& pt : poly.points) {
        QPoint sp = worldToScreen(pt);
        painter.drawRect(sp.x() - handleSize/2, sp.y() - handleSize/2, handleSize, handleSize);
    }
}

void CanvasWidget::addPeg(const CanvasPeg& peg)
{
    m_pegs.append(peg);
    update();
}

void CanvasWidget::addPegsFromPolyline(const CanvasPolyline& polyline, const QString& prefix)
{
    // Add a peg at each vertex of the polyline
    for (int i = 0; i < polyline.points.size(); ++i) {
        CanvasPeg peg;
        peg.position = polyline.points[i];
        peg.name = QString("%1%2").arg(prefix).arg(i + 1);
        peg.layer = polyline.layer;
        peg.color = Qt::red;
        m_pegs.append(peg);
    }
    update();
}

void CanvasWidget::drawPegs(QPainter& painter)
{
    if (m_pegs.isEmpty()) return;
    
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    // Calculate marker size based on zoom (keep consistent screen size)
    const int markerRadius = 8;  // pixels
    const int fontSize = 10;
    
    QFont font = painter.font();
    font.setPointSize(fontSize);
    font.setBold(true);
    painter.setFont(font);
    
    for (const auto& peg : m_pegs) {
        // Check if layer is visible
        if (m_hiddenLayers.contains(peg.layer)) continue;
        
        QPoint screenPos = worldToScreen(peg.position);
        
        // Draw peg marker (filled circle with cross)
        QPen markerPen(peg.color, 2);
        markerPen.setCosmetic(true);
        painter.setPen(markerPen);
        painter.setBrush(QBrush(QColor(peg.color.red(), peg.color.green(), peg.color.blue(), 100)));
        
        // Draw triangle (survey peg symbol)
        QPolygon triangle;
        triangle << QPoint(screenPos.x(), screenPos.y() - markerRadius)
                 << QPoint(screenPos.x() - markerRadius, screenPos.y() + markerRadius)
                 << QPoint(screenPos.x() + markerRadius, screenPos.y() + markerRadius);
        painter.drawPolygon(triangle);
        
        // Draw cross inside
        painter.drawLine(screenPos.x() - 4, screenPos.y(), screenPos.x() + 4, screenPos.y());
        painter.drawLine(screenPos.x(), screenPos.y() - 4, screenPos.x(), screenPos.y() + 4);
        
        // Draw label background first
        QString labelText = peg.name;
        QFontMetrics fm(font);
        int textWidth = fm.horizontalAdvance(labelText);
        QRect bgRect(screenPos.x() + markerRadius + 2, screenPos.y() - fontSize/2 - 2, textWidth + 6, fontSize + 6);
        painter.fillRect(bgRect, QColor(0, 0, 0, 180));
        
        // Draw label text
        painter.setPen(Qt::white);
        QRect textRect(screenPos.x() + markerRadius + 4, screenPos.y() - fontSize/2, textWidth + 4, fontSize + 4);
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, labelText);
    }
}

int CanvasWidget::projectPartitionToOffset(const QString& pegPrefix)
{
    // Check if we have a selected polyline (partition wall)
    if (!hasSelection()) {
        emit statusMessage("Please select a partition wall line first");
        return 0;
    }
    
    const CanvasPolyline* partition = selectedPolyline();
    if (!partition || partition->points.size() < 2) {
        emit statusMessage("Selected line must have at least 2 points");
        return 0;
    }
    
    // Find offset polylines (those with "_offset" in layer name)
    QVector<int> offsetPolylineIndices;
    for (int i = 0; i < m_polylines.size(); ++i) {
        if (m_polylines[i].layer.contains("_offset")) {
            offsetPolylineIndices.append(i);
        }
    }
    
    if (offsetPolylineIndices.isEmpty()) {
        emit statusMessage("No offset polylines found. Create main wall offset first.");
        return 0;
    }
    
    int pegsCreated = 0;
    
    // For each segment of the partition, extend as an infinite line
    // and find intersections with offset polylines
    for (int segIdx = 0; segIdx < partition->points.size() - 1; ++segIdx) {
        const QPointF& p1 = partition->points[segIdx];
        const QPointF& p2 = partition->points[segIdx + 1];
        
        // Direction vector
        double dx = p2.x() - p1.x();
        double dy = p2.y() - p1.y();
        
        // Check each offset polyline for intersections
        for (int offsetIdx : offsetPolylineIndices) {
            const CanvasPolyline& offsetPoly = m_polylines[offsetIdx];
            
            int numSegs = offsetPoly.closed ? offsetPoly.points.size() : offsetPoly.points.size() - 1;
            
            for (int i = 0; i < numSegs; ++i) {
                const QPointF& a1 = offsetPoly.points[i];
                const QPointF& a2 = offsetPoly.points[(i + 1) % offsetPoly.points.size()];
                
                // Line-line intersection (treat partition line as infinite)
                double ax = a2.x() - a1.x();
                double ay = a2.y() - a1.y();
                
                double cross = dx * ay - dy * ax;
                if (qAbs(cross) < 1e-12) continue;  // Parallel
                
                double t1 = ((a1.x() - p1.x()) * ay - (a1.y() - p1.y()) * ax) / cross;
                double t2 = ((a1.x() - p1.x()) * dy - (a1.y() - p1.y()) * dx) / cross;
                
                // t2 must be in [0,1] for intersection to be on offset segment
                // t1 can be any value (we're extending the partition line)
                if (t2 >= 0 && t2 <= 1) {
                    QPointF intersection(p1.x() + t1 * dx, p1.y() + t1 * dy);
                    
                    // Create peg at intersection
                    CanvasPeg peg;
                    peg.position = intersection;
                    peg.name = QString("%1%2").arg(pegPrefix).arg(pegsCreated + 1);
                    peg.layer = partition->layer + "_projection";
                    peg.color = Qt::magenta;  // Different color for partition pegs
                    m_pegs.append(peg);
                    pegsCreated++;
                }
            }
        }
    }
    
    if (pegsCreated > 0) {
        emit statusMessage(QString("Created %1 partition projection peg(s)").arg(pegsCreated));
        update();
    } else {
        emit statusMessage("No intersections found with offset polylines");
    }
    
    return pegsCreated;
}

int CanvasWidget::hitTestPeg(const QPointF& worldPos, double tolerance)
{
    for (int i = 0; i < m_pegs.size(); ++i) {
        const auto& peg = m_pegs[i];
        if (m_hiddenLayers.contains(peg.layer)) continue;
        
        double dx = worldPos.x() - peg.position.x();
        double dy = worldPos.y() - peg.position.y();
        double dist = qSqrt(dx * dx + dy * dy);
        
        if (dist <= tolerance) {
            return i;
        }
    }
    return -1;
}

void CanvasWidget::renamePeg(int pegIndex)
{
    if (pegIndex < 0 || pegIndex >= m_pegs.size()) return;
    
    QString currentName = m_pegs[pegIndex].name;
    bool ok;
    QString newName = QInputDialog::getText(this, "Rename Peg",
        "Enter new peg name:", QLineEdit::Normal, currentName, &ok);
    
    if (ok && !newName.isEmpty()) {
        m_pegs[pegIndex].name = newName;
        emit statusMessage(QString("Peg renamed to '%1'").arg(newName));
        update();
    }
}

void CanvasWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF worldPos = screenToWorld(event->pos());
        double tolerance = m_snapTolerance / m_zoom;
        
        int pegIndex = hitTestPeg(worldPos, tolerance);
        if (pegIndex >= 0) {
            renamePeg(pegIndex);
            return;
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void CanvasWidget::setStationPoint(const QPointF& pos, const QString& name)
{
    m_station.stationPos = pos;
    m_station.stationName = name;
    m_station.hasStation = true;
    emit statusMessage(QString("Station '%1' set at (%2, %3)")
        .arg(name).arg(pos.x(), 0, 'f', 3).arg(pos.y(), 0, 'f', 3));
    update();
}

void CanvasWidget::setBacksightPoint(const QPointF& pos, const QString& name)
{
    m_station.backsightPos = pos;
    m_station.backsightName = name;
    m_station.hasBacksight = true;
    emit statusMessage(QString("Backsight '%1' set at (%2, %3)")
        .arg(name).arg(pos.x(), 0, 'f', 3).arg(pos.y(), 0, 'f', 3));
    update();
}

void CanvasWidget::clearStation()
{
    m_station = CanvasStation();
    emit statusMessage("Station cleared");
    update();
}

void CanvasWidget::startSetStationMode()
{
    m_toolState = ToolState::SetStation;
    setCursor(Qt::CrossCursor);
    emit statusMessage("Click to set station point (instrument location)");
}

void CanvasWidget::startSetBacksightMode()
{
    if (!m_station.hasStation) {
        emit statusMessage("Set station point first");
        return;
    }
    m_toolState = ToolState::SetBacksight;
    setCursor(Qt::CrossCursor);
    emit statusMessage("Click to set backsight point (orientation reference)");
}

void CanvasWidget::drawStation(QPainter& painter)
{
    if (!m_station.hasStation && !m_station.hasBacksight) return;
    
    painter.setRenderHint(QPainter::Antialiasing, true);
    const int markerSize = 12;
    const int fontSize = 10;
    
    QFont font = painter.font();
    font.setPointSize(fontSize);
    font.setBold(true);
    painter.setFont(font);
    
    // Draw baseline first (behind markers)
    if (m_station.hasStation && m_station.hasBacksight) {
        QPoint stnScreen = worldToScreen(m_station.stationPos);
        QPoint bsScreen = worldToScreen(m_station.backsightPos);
        
        QPen basePen(Qt::yellow, 2, Qt::DashLine);
        basePen.setCosmetic(true);
        painter.setPen(basePen);
        painter.drawLine(stnScreen, bsScreen);
    }
    
    // Draw station point (tripod symbol)
    if (m_station.hasStation) {
        QPoint stn = worldToScreen(m_station.stationPos);
        
        QPen stnPen(Qt::green, 2);
        stnPen.setCosmetic(true);
        painter.setPen(stnPen);
        painter.setBrush(QBrush(QColor(0, 255, 0, 100)));
        
        // Draw circle with crosshairs (theodolite symbol)
        painter.drawEllipse(stn, markerSize, markerSize);
        painter.drawLine(stn.x() - markerSize - 4, stn.y(), stn.x() + markerSize + 4, stn.y());
        painter.drawLine(stn.x(), stn.y() - markerSize - 4, stn.x(), stn.y() + markerSize + 4);
        
        // Label
        painter.setPen(Qt::white);
        QRect textRect(stn.x() + markerSize + 4, stn.y() - fontSize/2, 100, fontSize + 4);
        painter.fillRect(textRect.adjusted(-2, -2, 2, 2), QColor(0, 100, 0, 200));
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, m_station.stationName);
    }
    
    // Draw backsight point
    if (m_station.hasBacksight) {
        QPoint bs = worldToScreen(m_station.backsightPos);
        
        QPen bsPen(Qt::cyan, 2);
        bsPen.setCosmetic(true);
        painter.setPen(bsPen);
        painter.setBrush(QBrush(QColor(0, 255, 255, 100)));
        
        // Draw diamond (backsight symbol)
        QPolygon diamond;
        diamond << QPoint(bs.x(), bs.y() - markerSize)
                << QPoint(bs.x() + markerSize, bs.y())
                << QPoint(bs.x(), bs.y() + markerSize)
                << QPoint(bs.x() - markerSize, bs.y());
        painter.drawPolygon(diamond);
        
        // Label
        painter.setPen(Qt::white);
        QRect textRect(bs.x() + markerSize + 4, bs.y() - fontSize/2, 100, fontSize + 4);
        painter.fillRect(textRect.adjusted(-2, -2, 2, 2), QColor(0, 100, 100, 200));
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, m_station.backsightName);
    }
}

double CanvasWidget::calculateBearing(const QPointF& from, const QPointF& to) const
{
    double dx = to.x() - from.x();
    double dy = to.y() - from.y();
    
    // atan2 gives angle from positive X axis, we need angle from north (positive Y)
    // Bearing is measured clockwise from north
    double angleRad = qAtan2(dx, dy);  // Note: atan2(x,y) for bearing from north
    double angleDeg = qRadiansToDegrees(angleRad);
    
    // Normalize to 0-360
    if (angleDeg < 0) angleDeg += 360.0;
    
    return angleDeg;
}

double CanvasWidget::calculateDistance(const QPointF& from, const QPointF& to) const
{
    double dx = to.x() - from.x();
    double dy = to.y() - from.y();
    return qSqrt(dx * dx + dy * dy);
}

QString CanvasWidget::bearingToDMS(double bearing) const
{
    // Convert decimal degrees to degrees, minutes, seconds
    int degrees = static_cast<int>(bearing);
    double remaining = (bearing - degrees) * 60.0;
    int minutes = static_cast<int>(remaining);
    double seconds = (remaining - minutes) * 60.0;
    
    return QString("%1° %2' %3\"")
        .arg(degrees, 3, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 5, 'f', 2, QChar('0'));
}

QString CanvasWidget::getStakeoutInfo(int pegIndex) const
{
    if (pegIndex < 0 || pegIndex >= m_pegs.size()) return QString();
    if (!m_station.hasStation) return "No station set";
    
    const CanvasPeg& peg = m_pegs[pegIndex];
    double bearing = calculateBearing(m_station.stationPos, peg.position);
    double distance = calculateDistance(m_station.stationPos, peg.position);
    
    return QString("%1: Bearing %2, Distance %3")
        .arg(peg.name)
        .arg(bearingToDMS(bearing))
        .arg(distance, 0, 'f', 3);
}

void CanvasWidget::startStakeoutMode()
{
    if (!m_station.hasStation) {
        emit statusMessage("Set station point first (press 1)");
        return;
    }
    m_toolState = ToolState::StakeoutMode;
    setCursor(Qt::CrossCursor);
    emit statusMessage("Stakeout mode - move cursor to see bearing/distance, click peg to select, Escape to exit");
}

void CanvasWidget::drawStakeoutLine(QPainter& painter)
{
    if (m_toolState != ToolState::StakeoutMode) return;
    if (!m_station.hasStation) return;
    
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    QPoint stationScreen = worldToScreen(m_station.stationPos);
    QPoint cursorScreen = worldToScreen(m_stakeoutCursorPos);
    
    // Draw sighting line from station to cursor
    QPen linePen(Qt::yellow, 2, Qt::DashLine);
    linePen.setCosmetic(true);
    painter.setPen(linePen);
    painter.drawLine(stationScreen, cursorScreen);
    
    // Calculate bearing and distance
    double bearing = calculateBearing(m_station.stationPos, m_stakeoutCursorPos);
    double distance = calculateDistance(m_station.stationPos, m_stakeoutCursorPos);
    
    // Check if cursor is near a peg
    QString pegName;
    double tolerance = m_snapTolerance / m_zoom;
    int pegIndex = hitTestPeg(m_stakeoutCursorPos, tolerance);
    if (pegIndex >= 0) {
        pegName = m_pegs[pegIndex].name;
        // Snap to peg position
        bearing = calculateBearing(m_station.stationPos, m_pegs[pegIndex].position);
        distance = calculateDistance(m_station.stationPos, m_pegs[pegIndex].position);
    }
    
    // Draw info label at midpoint of line
    int midX = (stationScreen.x() + cursorScreen.x()) / 2;
    int midY = (stationScreen.y() + cursorScreen.y()) / 2;
    
    QString infoText;
    if (!pegName.isEmpty()) {
        infoText = QString("→ %1\n%2\n%3m")
            .arg(pegName)
            .arg(bearingToDMS(bearing))
            .arg(distance, 0, 'f', 3);
    } else {
        infoText = QString("%1\n%2m")
            .arg(bearingToDMS(bearing))
            .arg(distance, 0, 'f', 3);
    }
    
    // Draw background box
    QFont font = painter.font();
    font.setPointSize(11);
    font.setBold(true);
    painter.setFont(font);
    
    QFontMetrics fm(font);
    QRect textBounds = fm.boundingRect(QRect(0, 0, 200, 100), Qt::AlignLeft, infoText);
    QRect bgRect(midX + 10, midY - textBounds.height()/2 - 4, textBounds.width() + 12, textBounds.height() + 8);
    
    painter.fillRect(bgRect, QColor(0, 0, 0, 200));
    painter.setPen(Qt::yellow);
    painter.drawRect(bgRect);
    
    painter.setPen(Qt::white);
    painter.drawText(bgRect.adjusted(6, 4, -6, -4), Qt::AlignLeft | Qt::AlignVCenter, infoText);
}

bool CanvasWidget::saveProject(const QString& filePath) const
{
    QJsonObject root;
    root["version"] = "1.0";
    root["format"] = "SiteSurveyor Project";
    
    // Save layers
    QJsonArray layersArray;
    for (const auto& layer : m_layers) {
        QJsonObject layerObj;
        layerObj["name"] = layer.name;
        layerObj["color"] = layer.color.name();
        layerObj["visible"] = layer.visible;
        layerObj["locked"] = layer.locked;
        layerObj["order"] = layer.order;
        layersArray.append(layerObj);
    }
    root["layers"] = layersArray;
    
    // Save polylines
    QJsonArray polylinesArray;
    for (const auto& poly : m_polylines) {
        QJsonObject polyObj;
        polyObj["layer"] = poly.layer;
        polyObj["color"] = poly.color.name();
        polyObj["closed"] = poly.closed;
        
        QJsonArray pointsArray;
        for (const auto& pt : poly.points) {
            QJsonObject ptObj;
            ptObj["x"] = pt.x();
            ptObj["y"] = pt.y();
            pointsArray.append(ptObj);
        }
        polyObj["points"] = pointsArray;
        polylinesArray.append(polyObj);
    }
    root["polylines"] = polylinesArray;
    
    // Save pegs
    QJsonArray pegsArray;
    for (const auto& peg : m_pegs) {
        QJsonObject pegObj;
        pegObj["name"] = peg.name;
        pegObj["x"] = peg.position.x();
        pegObj["y"] = peg.position.y();
        pegObj["layer"] = peg.layer;
        pegObj["color"] = peg.color.name();
        pegsArray.append(pegObj);
    }
    root["pegs"] = pegsArray;
    
    // Save station setup
    QJsonObject stationObj;
    stationObj["hasStation"] = m_station.hasStation;
    stationObj["hasBacksight"] = m_station.hasBacksight;
    stationObj["stationX"] = m_station.stationPos.x();
    stationObj["stationY"] = m_station.stationPos.y();
    stationObj["backsightX"] = m_station.backsightPos.x();
    stationObj["backsightY"] = m_station.backsightPos.y();
    stationObj["stationName"] = m_station.stationName;
    stationObj["backsightName"] = m_station.backsightName;
    root["station"] = stationObj;
    
    // Write to file
    QJsonDocument doc(root);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    return true;
}

bool CanvasWidget::loadProject(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (doc.isNull() || !doc.isObject()) {
        return false;
    }
    
    QJsonObject root = doc.object();
    
    // Clear existing data
    clearAll();
    
    // Load layers
    QJsonArray layersArray = root["layers"].toArray();
    for (const auto& layerVal : layersArray) {
        QJsonObject layerObj = layerVal.toObject();
        CanvasLayer layer;
        layer.name = layerObj["name"].toString();
        layer.color = QColor(layerObj["color"].toString());
        layer.visible = layerObj["visible"].toBool(true);
        layer.locked = layerObj["locked"].toBool(false);
        layer.order = layerObj["order"].toInt(0);
        m_layers.append(layer);
        if (!layer.visible) {
            m_hiddenLayers.insert(layer.name);
        }
    }
    
    // Load polylines
    QJsonArray polylinesArray = root["polylines"].toArray();
    for (const auto& polyVal : polylinesArray) {
        QJsonObject polyObj = polyVal.toObject();
        CanvasPolyline poly;
        poly.layer = polyObj["layer"].toString();
        poly.color = QColor(polyObj["color"].toString());
        poly.closed = polyObj["closed"].toBool(false);
        
        QJsonArray pointsArray = polyObj["points"].toArray();
        for (const auto& ptVal : pointsArray) {
            QJsonObject ptObj = ptVal.toObject();
            poly.points.append(QPointF(ptObj["x"].toDouble(), ptObj["y"].toDouble()));
        }
        m_polylines.append(poly);
    }
    
    // Load pegs
    QJsonArray pegsArray = root["pegs"].toArray();
    for (const auto& pegVal : pegsArray) {
        QJsonObject pegObj = pegVal.toObject();
        CanvasPeg peg;
        peg.name = pegObj["name"].toString();
        peg.position = QPointF(pegObj["x"].toDouble(), pegObj["y"].toDouble());
        peg.layer = pegObj["layer"].toString();
        peg.color = QColor(pegObj["color"].toString());
        m_pegs.append(peg);
    }
    
    // Load station setup
    QJsonObject stationObj = root["station"].toObject();
    m_station.hasStation = stationObj["hasStation"].toBool(false);
    m_station.hasBacksight = stationObj["hasBacksight"].toBool(false);
    m_station.stationPos = QPointF(stationObj["stationX"].toDouble(), stationObj["stationY"].toDouble());
    m_station.backsightPos = QPointF(stationObj["backsightX"].toDouble(), stationObj["backsightY"].toDouble());
    m_station.stationName = stationObj["stationName"].toString("STN");
    m_station.backsightName = stationObj["backsightName"].toString("BS");
    
    m_projectFilePath = filePath;
    
    emit layersChanged();
    update();
    fitToWindow();
    
    return true;
}

// ==================== Polyline Editing Tools ====================

void CanvasWidget::startSelectMode()
{
    m_toolState = ToolState::None;
    emit statusMessage("Selection mode: Click on a polyline to select it");
    update();
}

void CanvasWidget::startSplitMode()
{
    if (m_selectedPolylineIndex < 0) {
        emit statusMessage("Select a polyline first (click on it)");
        return;
    }
    m_toolState = ToolState::SplitMode;
    emit statusMessage("Split mode: Click on the polyline where you want to split");
    update();
}

void CanvasWidget::explodeSelectedPolyline()
{
    if (m_selectedPolylineIndex < 0 || m_selectedPolylineIndex >= m_polylines.size()) {
        emit statusMessage("No polyline selected");
        return;
    }
    
    const CanvasPolyline& poly = m_polylines[m_selectedPolylineIndex];
    if (poly.points.size() < 2) {
        emit statusMessage("Polyline too short to explode");
        return;
    }
    
    // Create individual line segments
    QVector<CanvasPolyline> segments;
    for (int i = 0; i < poly.points.size() - 1; ++i) {
        CanvasPolyline segment;
        segment.points.append(poly.points[i]);
        segment.points.append(poly.points[i + 1]);
        segment.closed = false;
        segment.layer = poly.layer;
        segment.color = poly.color;
        segments.append(segment);
    }
    
    // If closed, add segment from last to first
    if (poly.closed && poly.points.size() > 2) {
        CanvasPolyline segment;
        segment.points.append(poly.points.last());
        segment.points.append(poly.points.first());
        segment.closed = false;
        segment.layer = poly.layer;
        segment.color = poly.color;
        segments.append(segment);
    }
    
    // Remove original and add segments
    m_polylines.remove(m_selectedPolylineIndex);
    for (const auto& seg : segments) {
        m_polylines.append(seg);
    }
    
    m_selectedPolylineIndex = -1;
    emit selectionChanged(-1);
    emit statusMessage(QString("Exploded into %1 segments").arg(segments.size()));
    update();
}

void CanvasWidget::splitPolylineAtPoint(const QPointF& point)
{
    if (m_selectedPolylineIndex < 0 || m_selectedPolylineIndex >= m_polylines.size()) {
        emit statusMessage("No polyline selected");
        return;
    }
    
    const CanvasPolyline& poly = m_polylines[m_selectedPolylineIndex];
    if (poly.points.size() < 2) {
        emit statusMessage("Polyline too short to split");
        return;
    }
    
    // Find nearest segment
    int nearestSegment = -1;
    double minDist = std::numeric_limits<double>::max();
    QPointF nearestPoint;
    
    for (int i = 0; i < poly.points.size() - 1; ++i) {
        QPointF p1 = poly.points[i];
        QPointF p2 = poly.points[i + 1];
        
        // Project point onto line segment
        QPointF v = p2 - p1;
        QPointF w = point - p1;
        double c1 = QPointF::dotProduct(w, v);
        double c2 = QPointF::dotProduct(v, v);
        
        QPointF proj;
        if (c1 <= 0) {
            proj = p1;
        } else if (c2 <= c1) {
            proj = p2;
        } else {
            double b = c1 / c2;
            proj = p1 + b * v;
        }
        
        double dist = QLineF(point, proj).length();
        if (dist < minDist) {
            minDist = dist;
            nearestSegment = i;
            nearestPoint = proj;
        }
    }
    
    if (nearestSegment < 0) {
        emit statusMessage("Could not find split point");
        return;
    }
    
    // Create two new polylines
    CanvasPolyline poly1, poly2;
    poly1.layer = poly.layer;
    poly1.color = poly.color;
    poly1.closed = false;
    poly2.layer = poly.layer;
    poly2.color = poly.color;
    poly2.closed = false;
    
    // First part: points 0 to nearestSegment + split point
    for (int i = 0; i <= nearestSegment; ++i) {
        poly1.points.append(poly.points[i]);
    }
    poly1.points.append(nearestPoint);
    
    // Second part: split point + points from nearestSegment+1 to end
    poly2.points.append(nearestPoint);
    for (int i = nearestSegment + 1; i < poly.points.size(); ++i) {
        poly2.points.append(poly.points[i]);
    }
    
    // Remove original and add new polylines
    m_polylines.remove(m_selectedPolylineIndex);
    m_polylines.append(poly1);
    m_polylines.append(poly2);
    
    m_selectedPolylineIndex = -1;
    m_toolState = ToolState::None;
    emit selectionChanged(-1);
    emit statusMessage("Polyline split into two parts");
    update();
}

void CanvasWidget::joinPolylines()
{
    QVector<int> selected = getSelectedIndices();
    
    if (selected.size() < 2) {
        emit statusMessage("Select at least 2 polylines to join (Shift+click)");
        return;
    }
    
    // Sort indices by first point X then Y to get consistent order
    std::sort(selected.begin(), selected.end(), [this](int a, int b) {
        const auto& pa = m_polylines[a].points;
        const auto& pb = m_polylines[b].points;
        if (pa.isEmpty()) return false;
        if (pb.isEmpty()) return true;
        if (qAbs(pa.first().x() - pb.first().x()) > 0.001) {
            return pa.first().x() < pb.first().x();
        }
        return pa.first().y() < pb.first().y();
    });
    
    // Create joined polyline from all selected
    CanvasPolyline joined;
    joined.layer = m_polylines[selected.first()].layer;
    joined.color = m_polylines[selected.first()].color;
    joined.closed = false;
    
    for (int idx : selected) {
        const CanvasPolyline& poly = m_polylines[idx];
        for (const auto& pt : poly.points) {
            joined.points.append(pt);
        }
    }
    
    // Remove selected polylines (in reverse index order to preserve indices)
    std::sort(selected.begin(), selected.end(), std::greater<int>());
    for (int idx : selected) {
        m_polylines.remove(idx);
    }
    
    // Add joined polyline
    m_polylines.append(joined);
    
    // Clear selection and select the new polyline
    m_selectedPolylines.clear();
    m_selectedPolylineIndex = m_polylines.size() - 1;
    emit selectionChanged(m_selectedPolylineIndex);
    emit statusMessage(QString("Joined %1 polylines into one").arg(selected.size()));
    update();
}

void CanvasWidget::closeSelectedPolyline()
{
    if (m_selectedPolylineIndex < 0 || m_selectedPolylineIndex >= m_polylines.size()) {
        emit statusMessage("No polyline selected");
        return;
    }
    
    CanvasPolyline& poly = m_polylines[m_selectedPolylineIndex];
    if (poly.closed) {
        poly.closed = false;
        emit statusMessage("Polyline opened");
    } else {
        poly.closed = true;
        emit statusMessage("Polyline closed");
    }
    update();
}

void CanvasWidget::reverseSelectedPolyline()
{
    if (m_selectedPolylineIndex < 0 || m_selectedPolylineIndex >= m_polylines.size()) {
        emit statusMessage("No polyline selected");
        return;
    }
    
    CanvasPolyline& poly = m_polylines[m_selectedPolylineIndex];
    std::reverse(poly.points.begin(), poly.points.end());
    emit statusMessage("Polyline direction reversed");
    update();
}

void CanvasWidget::deleteSelectedPolyline()
{
    if (m_selectedPolylineIndex < 0 || m_selectedPolylineIndex >= m_polylines.size()) {
        emit statusMessage("No polyline selected");
        return;
    }
    
    // Store for undo
    UndoCommand cmd;
    cmd.type = UndoType::DeletePolyline;
    cmd.polyline = m_polylines[m_selectedPolylineIndex];
    cmd.index = m_selectedPolylineIndex;
    m_undoStack.append(cmd);
    m_redoStack.clear();
    emit undoRedoChanged();
    
    m_polylines.remove(m_selectedPolylineIndex);
    m_selectedPolylineIndex = -1;
    emit selectionChanged(-1);
    emit statusMessage("Polyline deleted (Ctrl+Z to undo)");
    update();
}