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
    // Load lines as 2-point polylines to allow editing/merging
    for (const auto& line : data.lines) {
        CanvasPolyline poly;
        poly.points.append(line.start);
        poly.points.append(line.end);
        poly.closed = false;
        poly.layer = line.layer;
        poly.color = line.color;
        m_polylines.append(poly);
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
            CanvasPolyline poly;
            poly.points.append(transformPoint(line.start));
            poly.points.append(transformPoint(line.end));
            poly.closed = false;
            poly.layer = insert.layer;
            poly.color = line.color;
            m_polylines.append(poly);
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
    
    // Clear pegs and station
    m_pegs.clear();
    m_station = CanvasStation();  // Reset to default
    
    // Clear selection state
    m_selectedPolylineIndex = -1;
    m_selectedVertexIndex = -1;
    m_selectedPolylines.clear();
    
    // Clear undo/redo stacks
    m_undoStack.clear();
    m_redoStack.clear();
    emit undoRedoChanged();
    
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
            
            // Remove all entities belonging to this layer
            m_points.erase(std::remove_if(m_points.begin(), m_points.end(),
                [&name](const CanvasPoint& p) { return p.layer == name; }), m_points.end());
            m_lines.erase(std::remove_if(m_lines.begin(), m_lines.end(),
                [&name](const CanvasLine& l) { return l.layer == name; }), m_lines.end());
            m_circles.erase(std::remove_if(m_circles.begin(), m_circles.end(),
                [&name](const CanvasCircle& c) { return c.layer == name; }), m_circles.end());
            m_arcs.erase(std::remove_if(m_arcs.begin(), m_arcs.end(),
                [&name](const CanvasArc& a) { return a.layer == name; }), m_arcs.end());
            m_ellipses.erase(std::remove_if(m_ellipses.begin(), m_ellipses.end(),
                [&name](const CanvasEllipse& e) { return e.layer == name; }), m_ellipses.end());
            m_splines.erase(std::remove_if(m_splines.begin(), m_splines.end(),
                [&name](const CanvasSpline& s) { return s.layer == name; }), m_splines.end());
            m_polylines.erase(std::remove_if(m_polylines.begin(), m_polylines.end(),
                [&name](const CanvasPolyline& p) { return p.layer == name; }), m_polylines.end());
            m_polygons.erase(std::remove_if(m_polygons.begin(), m_polygons.end(),
                [&name](const CanvasPolygon& p) { return p.layer == name; }), m_polygons.end());
            m_hatches.erase(std::remove_if(m_hatches.begin(), m_hatches.end(),
                [&name](const CanvasHatch& h) { return h.layer == name; }), m_hatches.end());
            m_texts.erase(std::remove_if(m_texts.begin(), m_texts.end(),
                [&name](const CanvasText& t) { return t.layer == name; }), m_texts.end());
            m_rasters.erase(std::remove_if(m_rasters.begin(), m_rasters.end(),
                [&name](const CanvasRaster& r) { return r.layer == name; }), m_rasters.end());
            m_pegs.erase(std::remove_if(m_pegs.begin(), m_pegs.end(),
                [&name](const CanvasPeg& p) { return p.layer == name; }), m_pegs.end());
            
            // Clear selection if deleted polyline was selected
            m_selectedPolylineIndex = -1;
            m_selectedPolylines.clear();
            emit selectionChanged(-1);
            
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
            
            // Update all entities that reference this layer
            for (auto& point : m_points) {
                if (point.layer == oldName) point.layer = newName;
            }
            for (auto& line : m_lines) {
                if (line.layer == oldName) line.layer = newName;
            }
            for (auto& circle : m_circles) {
                if (circle.layer == oldName) circle.layer = newName;
            }
            for (auto& arc : m_arcs) {
                if (arc.layer == oldName) arc.layer = newName;
            }
            for (auto& ellipse : m_ellipses) {
                if (ellipse.layer == oldName) ellipse.layer = newName;
            }
            for (auto& spline : m_splines) {
                if (spline.layer == oldName) spline.layer = newName;
            }
            for (auto& poly : m_polylines) {
                if (poly.layer == oldName) poly.layer = newName;
            }
            for (auto& polygon : m_polygons) {
                if (polygon.layer == oldName) polygon.layer = newName;
            }
            for (auto& hatch : m_hatches) {
                if (hatch.layer == oldName) hatch.layer = newName;
            }
            for (auto& text : m_texts) {
                if (text.layer == oldName) text.layer = newName;
            }
            for (auto& raster : m_rasters) {
                if (raster.layer == oldName) raster.layer = newName;
            }
            for (auto& peg : m_pegs) {
                if (peg.layer == oldName) peg.layer = newName;
            }
            
            emit layersChanged();
            update();
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
    
    // Selection box (if dragging)
    if (m_isSelectingBox) {
        QPoint startScreen = worldToScreen(m_selectionBoxStart);
        QPoint endScreen = worldToScreen(m_selectionBoxEnd);
        QRect boxRect = QRect(startScreen, endScreen).normalized();
        
        // Left-to-right: solid blue (window select)
        // Right-to-left: dashed green (crossing select)
        bool isCrossing = m_selectionBoxEnd.x() < m_selectionBoxStart.x();
        
        QColor boxColor = isCrossing ? QColor(0, 180, 0, 80) : QColor(0, 120, 255, 80);
        QColor borderColor = isCrossing ? QColor(0, 200, 0) : QColor(0, 150, 255);
        
        painter.fillRect(boxRect, boxColor);
        
        QPen boxPen(borderColor, 1);
        if (isCrossing) {
            boxPen.setStyle(Qt::DashLine);
        }
        painter.setPen(boxPen);
        painter.drawRect(boxRect);
    }
    
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
    for (int i = 0; i < m_texts.size(); ++i) {
        const auto& text = m_texts[i];
        if (m_hiddenLayers.contains(text.layer)) continue;
        
        QPoint pos = worldToScreen(text.position);
        double fontSize = qMax(8.0, text.height * m_zoom);
        QFont font = painter.font();
        font.setPointSizeF(fontSize);
        painter.setFont(font);
        
        // Check if selected
        bool isTextSelected = m_selectedTexts.contains(i);
        
        painter.save();
        painter.translate(pos);
        painter.rotate(text.angle);
        
        if (isTextSelected) {
            // Draw selection highlight
            QFontMetricsF fm(font);
            QRectF textRect = fm.boundingRect(text.text);
            textRect.adjust(-4, -4, 4, 4);
            
            QPen selPen(Qt::cyan, 2);
            selPen.setStyle(Qt::DashLine);
            selPen.setCosmetic(true);
            painter.setPen(selPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(textRect);
        }
        
        painter.setPen(text.color);
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
    
    // Check if clicking on a vertex grip of selected polyline (AutoCAD-style)
    // Skip this check in measure mode so measure clicks work
    if (m_selectedPolylineIndex >= 0 && m_selectedPolylineIndex < m_polylines.size() &&
        m_toolState != ToolState::MeasureMode && m_toolState != ToolState::MeasureMode2) {
        const CanvasPolyline& poly = m_polylines[m_selectedPolylineIndex];
        double tolerance = m_snapTolerance / m_zoom;
        
        // Find closest vertex
        int closestVertex = -1;
        double minDist = tolerance;
        
        for (int i = 0; i < poly.points.size(); ++i) {
            double dist = QLineF(worldPos, poly.points[i]).length();
            if (dist < minDist) {
                minDist = dist;
                closestVertex = i;
            }
        }
        
        // Left-click on vertex: select it
        if (event->button() == Qt::LeftButton && closestVertex >= 0) {
            if (m_selectedVertexIndex == closestVertex) {
                // Click same vertex again: deselect
                m_selectedVertexIndex = -1;
                emit statusMessage("Vertex deselected");
            } else {
                m_selectedVertexIndex = closestVertex;
                emit statusMessage(QString("Vertex %1 selected - Press Delete to remove").arg(closestVertex + 1));
            }
            update();
            return;
        }
    }
    
    // Click elsewhere: deselect vertex
    if (event->button() == Qt::LeftButton && m_selectedVertexIndex >= 0) {
        m_selectedVertexIndex = -1;
        update();
    }
    
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
    
    // Handle measure mode: click two points to measure distance
    if (m_toolState == ToolState::MeasureMode && event->button() == Qt::LeftButton) {
        m_measureStartPoint = worldPos;
        m_toolState = ToolState::MeasureMode2;
        emit statusMessage("MEASURE: Click second point");
        update();
        return;
    }
    
    if (m_toolState == ToolState::MeasureMode2 && event->button() == Qt::LeftButton) {
        // Calculate distance and angle
        double dx = worldPos.x() - m_measureStartPoint.x();
        double dy = worldPos.y() - m_measureStartPoint.y();
        double distance = std::sqrt(dx * dx + dy * dy);
        
        // Calculate bearing (azimuth from north, clockwise)
        double bearing = std::atan2(dx, dy) * 180.0 / M_PI;
        if (bearing < 0) bearing += 360.0;
        
        // Format bearing as DMS
        int degrees = static_cast<int>(bearing);
        double minFloat = (bearing - degrees) * 60.0;
        int minutes = static_cast<int>(minFloat);
        double seconds = (minFloat - minutes) * 60.0;
        
        QString bearingStr = QString("%1°%2'%3\"")
            .arg(degrees)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 5, 'f', 2, QChar('0'));
        
        emit statusMessage(QString("Distance: %1 | Bearing: %2 | ΔX: %3 | ΔY: %4")
            .arg(distance, 0, 'f', 3)
            .arg(bearingStr)
            .arg(dx, 0, 'f', 3)
            .arg(dy, 0, 'f', 3));
        
        // Stay in measure mode for more measurements
        m_toolState = ToolState::MeasureMode;
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
    
    // Left click: select polyline or start selection box (in Idle/None mode)
    if (event->button() == Qt::LeftButton && 
        (m_toolState == ToolState::Idle || m_toolState == ToolState::None)) {
        double tolerance = m_snapTolerance / m_zoom;
        int hitIndex = hitTestPolyline(worldPos, tolerance);
        int hitTextIndex = hitTestText(worldPos, tolerance);
        
        if (hitIndex >= 0) {
            // Hit a polyline - single or multi-select
            m_selectedTexts.clear();  // Clear text selection when selecting polylines
            if (event->modifiers() & Qt::ShiftModifier) {
                // Shift+click: add to / toggle in selection
                if (isSelected(hitIndex)) {
                    removeFromSelection(hitIndex);
                } else {
                    addToSelection(hitIndex);
                }
            } else {
                // Regular click: replace selection with this polyline
                m_selectedPolylines.clear();
                m_selectedPolylineIndex = hitIndex;
                m_selectedPolylines.insert(hitIndex);
                emit selectionChanged(m_selectedPolylineIndex);
                update();
            }
        } else if (hitTextIndex >= 0) {
            // Hit a text object
            m_selectedPolylines.clear();
            m_selectedPolylineIndex = -1;
            
            if (event->modifiers() & Qt::ShiftModifier) {
                // Shift+click: toggle text in selection
                if (m_selectedTexts.contains(hitTextIndex)) {
                    m_selectedTexts.remove(hitTextIndex);
                } else {
                    m_selectedTexts.insert(hitTextIndex);
                }
            } else {
                // Regular click: replace selection with this text
                m_selectedTexts.clear();
                m_selectedTexts.insert(hitTextIndex);
            }
            emit statusMessage(QString("Selected text: %1").arg(m_texts[hitTextIndex].text));
            update();
        } else {
            // No hit - start selection box drag
            m_isSelectingBox = true;
            m_selectionBoxStart = worldPos;
            m_selectionBoxEnd = worldPos;
            
            // If not holding shift, clear current selection
            if (!(event->modifiers() & Qt::ShiftModifier)) {
                m_selectedPolylines.clear();
                m_selectedPolylineIndex = -1;
                emit selectionChanged(m_selectedPolylineIndex);
            }
            
            setCursor(Qt::CrossCursor);
            update();
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
    
    // Update selection box while dragging
    if (m_isSelectingBox) {
        m_selectionBoxEnd = worldPos;
        update();  // Redraw selection rectangle
        return;
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
    // Complete selection box
    if (m_isSelectingBox && event->button() == Qt::LeftButton) {
        m_isSelectingBox = false;
        setCursor(Qt::ArrowCursor);
        
        // Create selection rectangle (normalized)
        QRectF selectionRect = QRectF(m_selectionBoxStart, m_selectionBoxEnd).normalized();
        
        // Determine selection mode: left-to-right = window (fully inside), right-to-left = crossing (intersects)
        bool isCrossingSelect = m_selectionBoxEnd.x() < m_selectionBoxStart.x();
        
        // Select polylines based on box
        for (int i = 0; i < m_polylines.size(); ++i) {
            const auto& poly = m_polylines[i];
            if (poly.points.isEmpty()) continue;
            
            // Get polyline bounding box
            QRectF polyBounds;
            for (const QPointF& pt : poly.points) {
                if (polyBounds.isNull()) {
                    polyBounds = QRectF(pt, QSizeF(0.001, 0.001));
                } else {
                    polyBounds = polyBounds.united(QRectF(pt, QSizeF(0.001, 0.001)));
                }
            }
            
            bool selected = false;
            if (isCrossingSelect) {
                // Crossing select: any intersection with box
                selected = selectionRect.intersects(polyBounds);
            } else {
                // Window select: fully contained in box
                selected = selectionRect.contains(polyBounds);
            }
            
            if (selected && !isSelected(i)) {
                addToSelection(i);
            }
        }
        
        update();
        return;
    }
    
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
    UndoCommand redoCmd;
    redoCmd.type = cmd.type;
    redoCmd.index = cmd.index;
    redoCmd.indices = cmd.indices;
    redoCmd.layerName = cmd.layerName;
    
    switch (cmd.type) {
        case UndoType::AddPolyline:
            // Remove the added polyline
            if (cmd.index >= 0 && cmd.index < m_polylines.size()) {
                redoCmd.polyline = m_polylines[cmd.index];
                m_polylines.remove(cmd.index);
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
                redoCmd.polyline = cmd.polyline;
            }
            break;
            
        case UndoType::ModifyPolyline:
            // Restore old polyline state
            if (cmd.index >= 0 && cmd.index < m_polylines.size()) {
                redoCmd.polyline = m_polylines[cmd.index];
                redoCmd.oldPolyline = cmd.oldPolyline;
                m_polylines[cmd.index] = cmd.oldPolyline;
            }
            break;
            
        case UndoType::AddMultiple:
            // Remove all added polylines (in reverse order)
            for (int i = cmd.indices.size() - 1; i >= 0; --i) {
                int idx = cmd.indices[i];
                if (idx >= 0 && idx < m_polylines.size()) {
                    redoCmd.polylines.prepend(m_polylines[idx]);
                    m_polylines.remove(idx);
                }
            }
            redoCmd.indices = cmd.indices;
            m_selectedPolylineIndex = -1;
            m_selectedPolylines.clear();
            emit selectionChanged(-1);
            break;
            
        case UndoType::DeleteMultiple:
            // Re-insert all deleted polylines
            for (int i = 0; i < cmd.polylines.size(); ++i) {
                int idx = (i < cmd.indices.size()) ? cmd.indices[i] : m_polylines.size();
                m_polylines.insert(idx, cmd.polylines[i]);
            }
            redoCmd.polylines = cmd.polylines;
            redoCmd.indices = cmd.indices;
            break;
            
        case UndoType::DeleteLayer:
            // Layer undo is complex - for now just message
            emit statusMessage("Layer deletion cannot be undone");
            break;
    }
    
    m_redoStack.append(redoCmd);
    emit undoRedoChanged();
    emit statusMessage("Undo");
    update();
}

void CanvasWidget::redo()
{
    if (m_redoStack.isEmpty()) return;
    
    UndoCommand cmd = m_redoStack.takeLast();
    UndoCommand undoCmd;
    undoCmd.type = cmd.type;
    undoCmd.index = cmd.index;
    undoCmd.indices = cmd.indices;
    undoCmd.layerName = cmd.layerName;
    
    switch (cmd.type) {
        case UndoType::AddPolyline:
            // Re-add the polyline
            if (cmd.index >= 0 && cmd.index <= m_polylines.size()) {
                m_polylines.insert(cmd.index, cmd.polyline);
                undoCmd.polyline = cmd.polyline;
            }
            break;
            
        case UndoType::DeletePolyline:
            // Remove the polyline again
            if (cmd.index >= 0 && cmd.index < m_polylines.size()) {
                undoCmd.polyline = m_polylines[cmd.index];
                m_polylines.remove(cmd.index);
                if (m_selectedPolylineIndex >= cmd.index) {
                    m_selectedPolylineIndex = qMax(-1, m_selectedPolylineIndex - 1);
                    emit selectionChanged(m_selectedPolylineIndex);
                }
            }
            break;
            
        case UndoType::ModifyPolyline:
            // Apply the modification again
            if (cmd.index >= 0 && cmd.index < m_polylines.size()) {
                undoCmd.oldPolyline = m_polylines[cmd.index];
                undoCmd.polyline = cmd.polyline;
                m_polylines[cmd.index] = cmd.polyline;
            }
            break;
            
        case UndoType::AddMultiple:
            // Re-add all the polylines
            for (int i = 0; i < cmd.polylines.size(); ++i) {
                int idx = (i < cmd.indices.size()) ? cmd.indices[i] : m_polylines.size();
                m_polylines.insert(idx, cmd.polylines[i]);
            }
            undoCmd.polylines = cmd.polylines;
            undoCmd.indices = cmd.indices;
            break;
            
        case UndoType::DeleteMultiple:
            // Remove all polylines again (in reverse order)
            for (int i = cmd.indices.size() - 1; i >= 0; --i) {
                int idx = cmd.indices[i];
                if (idx >= 0 && idx < m_polylines.size()) {
                    undoCmd.polylines.prepend(m_polylines[idx]);
                    m_polylines.remove(idx);
                }
            }
            undoCmd.indices = cmd.indices;
            m_selectedPolylineIndex = -1;
            m_selectedPolylines.clear();
            emit selectionChanged(-1);
            break;
            
        case UndoType::DeleteLayer:
            emit statusMessage("Layer redo not supported");
            break;
    }
    
    m_undoStack.append(undoCmd);
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

int CanvasWidget::hitTestText(const QPointF& worldPos, double tolerance)
{
    // Check each text for proximity to click point
    for (int i = 0; i < m_texts.size(); ++i) {
        const auto& text = m_texts[i];
        
        // Check if layer is visible
        if (m_hiddenLayers.contains(text.layer)) continue;
        
        // Create a rough bounding box for the text
        double textWidth = text.text.length() * text.height * 0.6;  // Approximate
        double textHeight = text.height;
        
        QRectF textBounds(text.position.x(), text.position.y() - textHeight, 
                          textWidth, textHeight * 1.5);
        
        // Expand by tolerance
        textBounds.adjust(-tolerance, -tolerance, tolerance, tolerance);
        
        if (textBounds.contains(worldPos)) {
            return i;  // Hit!
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
        } else if (m_selectedVertexIndex >= 0) {
            m_selectedVertexIndex = -1;
            emit statusMessage("Vertex deselected");
            update();
        } else if (hasSelection()) {
            clearSelection();
        }
        return;
    }
    
    // Delete key: remove selected vertex
    if (event->key() == Qt::Key_Delete && m_selectedVertexIndex >= 0 &&
        m_selectedPolylineIndex >= 0 && m_selectedPolylineIndex < m_polylines.size()) {
        
        CanvasPolyline& poly = m_polylines[m_selectedPolylineIndex];
        if (poly.points.size() > 2) {
            int vertexToRemove = m_selectedVertexIndex;
            poly.points.remove(vertexToRemove);
            m_selectedVertexIndex = -1;
            emit statusMessage(QString("Removed vertex %1 (%2 points remaining)")
                .arg(vertexToRemove + 1).arg(poly.points.size()));
            update();
        } else {
            emit statusMessage("Cannot remove: polyline needs at least 2 points");
        }
        return;
    }
    
    // Delete key: delete selected polylines (no vertex selected)
    if (event->key() == Qt::Key_Delete && m_selectedVertexIndex < 0) {
        QVector<int> selectedIndices = getSelectedIndices();
        
        if (!selectedIndices.isEmpty()) {
            // Sort indices in descending order to delete from back to front
            std::sort(selectedIndices.begin(), selectedIndices.end(), std::greater<int>());
            
            // Store for undo
            UndoCommand cmd;
            cmd.type = UndoType::DeleteMultiple;
            for (int idx : selectedIndices) {
                if (idx >= 0 && idx < m_polylines.size()) {
                    cmd.polylines.prepend(m_polylines[idx]);
                    cmd.indices.prepend(idx);
                }
            }
            m_undoStack.append(cmd);
            m_redoStack.clear();
            emit undoRedoChanged();
            
            // Remove polylines
            for (int idx : selectedIndices) {
                if (idx >= 0 && idx < m_polylines.size()) {
                    m_polylines.remove(idx);
                }
            }
            
            int deletedCount = selectedIndices.size();
            m_selectedPolylineIndex = -1;
            m_selectedPolylines.clear();
            emit selectionChanged(-1);
            emit statusMessage(QString("Deleted %1 polyline(s) (Ctrl+Z to undo)").arg(deletedCount));
            update();
            return;
        }
        
        // Delete selected text objects
        if (!m_selectedTexts.isEmpty()) {
            QVector<int> textIndices = m_selectedTexts.values().toVector();
            std::sort(textIndices.begin(), textIndices.end(), std::greater<int>());
            
            for (int idx : textIndices) {
                if (idx >= 0 && idx < m_texts.size()) {
                    m_texts.remove(idx);
                }
            }
            
            int deletedCount = textIndices.size();
            m_selectedTexts.clear();
            emit statusMessage(QString("Deleted %1 text(s)").arg(deletedCount));
            update();
            return;
        }
    }
    
    QWidget::keyPressEvent(event);
}

void CanvasWidget::drawSelection(QPainter& painter)
{
    // Draw all selected polylines
    if (m_selectedPolylines.isEmpty() && m_selectedPolylineIndex < 0) {
        return;
    }
    
    // Build set of indices to draw
    QSet<int> indicesToDraw = m_selectedPolylines;
    if (m_selectedPolylineIndex >= 0 && m_selectedPolylineIndex < m_polylines.size()) {
        indicesToDraw.insert(m_selectedPolylineIndex);
    }
    
    // Draw selected polyline with dashed cyan line
    QPen selectionPen(Qt::cyan, 2);
    selectionPen.setCosmetic(true);
    selectionPen.setStyle(Qt::DashLine);
    
    QPen handlePen(Qt::cyan, 1);
    handlePen.setCosmetic(true);
    
    const int handleSize = 6;
    
    for (int idx : indicesToDraw) {
        if (idx < 0 || idx >= m_polylines.size()) continue;
        
        const auto& poly = m_polylines[idx];
        if (poly.points.size() < 2) continue;
        
        // Draw polyline highlight
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
        painter.setPen(handlePen);
        painter.setBrush(QBrush(QColor(0, 255, 255, 100)));
        
        for (int i = 0; i < poly.points.size(); ++i) {
            const auto& pt = poly.points[i];
            QPoint sp = worldToScreen(pt);
            
            // Highlight selected vertex in red
            if (idx == m_selectedPolylineIndex && i == m_selectedVertexIndex) {
                painter.setPen(QPen(Qt::red, 2));
                painter.setBrush(QBrush(QColor(255, 0, 0, 150)));
                painter.drawRect(sp.x() - handleSize/2 - 1, sp.y() - handleSize/2 - 1, handleSize + 2, handleSize + 2);
                painter.setPen(handlePen);
                painter.setBrush(QBrush(QColor(0, 255, 255, 100)));
            } else {
                painter.drawRect(sp.x() - handleSize/2, sp.y() - handleSize/2, handleSize, handleSize);
            }
        }
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
    QVector<int> selectedIndices = getSelectedIndices();
    
    if (selectedIndices.isEmpty()) {
        emit statusMessage("Select polylines to explode");
        return;
    }
    
    int explodedCount = 0;
    QVector<CanvasPolyline> newSegments;
    
    // Process each selected polyline
    for (int idx : selectedIndices) {
        const CanvasPolyline& poly = m_polylines[idx];
        if (poly.points.size() < 2) continue;
        
        // Create individual line segments
        for (int i = 0; i < poly.points.size() - 1; ++i) {
            CanvasPolyline segment;
            segment.points.append(poly.points[i]);
            segment.points.append(poly.points[i + 1]);
            segment.closed = false;
            segment.layer = poly.layer;
            segment.color = poly.color;
            newSegments.append(segment);
        }
        
        // If closed, add segment from last to first
        if (poly.closed && poly.points.size() > 2) {
            CanvasPolyline segment;
            segment.points.append(poly.points.last());
            segment.points.append(poly.points.first());
            segment.closed = false;
            segment.layer = poly.layer;
            segment.color = poly.color;
            newSegments.append(segment);
        }
        explodedCount++;
    }
    
    // Remove original polylines (in reverse order)
    std::sort(selectedIndices.begin(), selectedIndices.end(), std::greater<int>());
    for (int idx : selectedIndices) {
        m_polylines.remove(idx);
    }
    
    // Add new segments
    m_polylines.append(newSegments);
    
    // Select the new segments
    m_selectedPolylines.clear();
    int startIdx = m_polylines.size() - newSegments.size();
    for (int i = 0; i < newSegments.size(); ++i) {
        m_selectedPolylines.insert(startIdx + i);
    }
    m_selectedPolylineIndex = -1; // No single primary selection
    if (!m_selectedPolylines.isEmpty()) {
        m_selectedPolylineIndex = *m_selectedPolylines.begin();
    }
    
    emit selectionChanged(m_selectedPolylineIndex);
    emit statusMessage(QString("Exploded %1 polylines into %2 segments").arg(explodedCount).arg(newSegments.size()));
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
    QVector<int> selectedIndices = getSelectedIndices();
    
    if (selectedIndices.size() < 2) {
        emit statusMessage("Select at least 2 polylines to merge (Shift+click or drag box)");
        return;
    }
    
    // Get copies of selected polylines
    QList<CanvasPolyline> workList;
    for (int idx : selectedIndices) {
        workList.append(m_polylines[idx]);
    }
    
    // Remove original polylines from canvas (in reverse order)
    std::sort(selectedIndices.begin(), selectedIndices.end(), std::greater<int>());
    for (int idx : selectedIndices) {
        m_polylines.remove(idx);
    }
    
    // Iteratively merge
    bool mergedSomething = true;
    while (mergedSomething && workList.size() > 1) {
        mergedSomething = false;
        
        for (int i = 0; i < workList.size(); ++i) {
            for (int j = i + 1; j < workList.size(); ++j) {
                CanvasPolyline& p1 = workList[i];
                CanvasPolyline& p2 = workList[j];
                
                // Try to merge p2 into p1
                bool didMerge = false;
                double tol = m_snapTolerance / m_zoom; // Tolerance in world units
                
                // 1. Check for Collinear Overlap (for straight lines)
                if (p1.points.size() == 2 && p2.points.size() == 2) {
                    QLineF l1(p1.points[0], p1.points[1]);
                    QLineF l2(p2.points[0], p2.points[1]);
                    
                    // Check angle
                    double angle = qAbs(l1.angle() - l2.angle());
                    while (angle > 180) angle -= 360;
                    angle = qAbs(angle);
                    if (angle < 1.0 || qAbs(angle - 180.0) < 1.0) {
                        // Parallel/Collinear direction. Check if they actually overlap/touch.
                        // Project all points onto l1
                        // If distance of p2 points to infinite line l1 is small
                        // AND intervals overlap
                        
                        // Simple check: distance of p2 endpoints to line p1
                        /*
                        double d1 = l1.isInArea(p2.points[0], tol) ? 0 : 1000; // Qt doesn't have distToLine easily for QLineF
                        */
                        // Custom distance check
                        // ... (omitted for brevity, relying on endpoint check for now or simple overlap)
                        
                        // Let's rely on a robust endpoint check + simplification for now.
                        // If they overlap significantly, standard endpoint chaining won't work.
                        // We need to handle the "Overlap" case the user mentioned.
                        
                        // Project p2 points onto line defined by p1
                        QPointF v = p1.points[1] - p1.points[0];
                        double len2 = v.x()*v.x() + v.y()*v.y();
                        
                        auto getT = [&](QPointF p) {
                            QPointF vp = p - p1.points[0];
                            return (vp.x()*v.x() + vp.y()*v.y()) / len2;
                        };
                        
                        // Check distance to line
                        auto distToLine = [&](QPointF p) {
                            double t = getT(p);
                            QPointF proj = p1.points[0] + v * t;
                            return QLineF(p, proj).length();
                        };
                        
                        if (distToLine(p2.points[0]) < tol && distToLine(p2.points[1]) < tol) {
                            // Collinear. Merge extents.
                            double t1_0 = 0.0;
                            double t1_1 = 1.0;
                            double t2_0 = getT(p2.points[0]);
                            double t2_1 = getT(p2.points[1]);
                            
                            // Check for interval overlap or touching
                            double min1 = qMin(t1_0, t1_1);
                            double max1 = qMax(t1_0, t1_1);
                            double min2 = qMin(t2_0, t2_1);
                            double max2 = qMax(t2_0, t2_1);
                            
                            if (qMax(min1, min2) <= qMin(max1, max2) + 0.01) { // Overlap or touch
                                // Merge into min/max
                                double finalMin = qMin(min1, min2);
                                double finalMax = qMax(max1, max2);
                                
                                p1.points[0] = p1.points[0] + v * finalMin;
                                p1.points[1] = p1.points[0] + v * (finalMax - finalMin); // Re-base from new start
                                // Actually safer:
                                // p1.points[0] corresponds to t=0. We want t=finalMin.
                                // But v is based on old p1.
                                // Let's just pick the 4 points, sort by T, take first and last.
                                QList<QPair<double, QPointF>> points;
                                points.append({0.0, p1.points[0]});
                                points.append({1.0, p1.points[1]});
                                points.append({t2_0, p2.points[0]});
                                points.append({t2_1, p2.points[1]});
                                std::sort(points.begin(), points.end(), [](const auto& a, const auto& b){ return a.first < b.first; });
                                
                                p1.points[0] = points.first().second;
                                p1.points[1] = points.last().second;
                                didMerge = true;
                            }
                        }
                    }
                }
                
                if (!didMerge) {
                    // 2. Check Endpoint Connectivity
                    QPointF s1 = p1.points.first();
                    QPointF e1 = p1.points.last();
                    QPointF s2 = p2.points.first();
                    QPointF e2 = p2.points.last();
                    
                    if (QLineF(e1, s2).length() < tol) {
                        // Append p2 to p1
                        p1.points.append(p2.points);
                        didMerge = true;
                    } else if (QLineF(e1, e2).length() < tol) {
                        // Reverse p2, append to p1
                        QVector<QPointF> rev = p2.points;
                        std::reverse(rev.begin(), rev.end());
                        p1.points.append(rev);
                        didMerge = true;
                    } else if (QLineF(s1, s2).length() < tol) {
                        // Reverse p1, append p2
                        std::reverse(p1.points.begin(), p1.points.end());
                        p1.points.append(p2.points);
                        didMerge = true;
                    } else if (QLineF(s1, e2).length() < tol) {
                        // Prepend p2 to p1 (or append p1 to p2)
                        // Let's append p1 to p2, then copy back to p1
                        p2.points.append(p1.points);
                        p1.points = p2.points;
                        didMerge = true;
                    }
                }
                
                if (didMerge) {
                    workList.removeAt(j);
                    mergedSomething = true;
                    break; // Restart loop or continue? Break inner to restart outer is safer
                }
            }
            if (mergedSomething) break;
        }
    }
    
    // Simplify and add back
    for (CanvasPolyline& poly : workList) {
        // Simplify: remove duplicates and collinear nodes
        if (poly.points.size() > 2) {
            QVector<QPointF> simple;
            simple.append(poly.points[0]);
            for (int i = 1; i < poly.points.size(); ++i) {
                QPointF prev = simple.last();
                QPointF curr = poly.points[i];
                
                // Skip duplicate
                if (QLineF(prev, curr).length() < 0.001) continue;
                
                // Check collinearity with next
                if (i < poly.points.size() - 1) {
                    QPointF next = poly.points[i+1];
                    
                    QLineF l1(prev, curr);
                    QLineF l2(curr, next);
                    
                    double angle = qAbs(l1.angle() - l2.angle());
                    while (angle > 180) angle -= 360;
                    angle = qAbs(angle);
                    
                    // Tolerance: 1 degree
                    if (angle < 1.0) { 
                         // Collinear: Skip curr (don't add it)
                         continue;
                    }
                }
                simple.append(curr);
            }
            poly.points = simple;
        }
        
        m_polylines.append(poly);
    }
    
    // Select the last one (usually the merged result)
    m_selectedPolylines.clear();
    int newIndex = m_polylines.size() - 1;
    m_selectedPolylines.insert(newIndex);
    m_selectedPolylineIndex = newIndex;
    
    emit selectionChanged(m_selectedPolylineIndex);
    emit statusMessage(QString("Merged into %1 polyline(s)").arg(workList.size()));
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

// ============================================================================
// Additional Modify Tools (AutoCAD-style)
// ============================================================================

void CanvasWidget::copySelectedPolyline()
{
    QVector<int> selectedIndices = getSelectedIndices();
    
    if (selectedIndices.isEmpty()) {
        emit statusMessage("Select polylines to copy");
        return;
    }
    
    // Copy each selected polyline with a small offset
    int copiedCount = 0;
    for (int idx : selectedIndices) {
        CanvasPolyline copy = m_polylines[idx];
        
        // Offset by a small amount so the copy is visible
        double offsetX = 1.0;
        double offsetY = -1.0;
        for (QPointF& pt : copy.points) {
            pt.setX(pt.x() + offsetX);
            pt.setY(pt.y() + offsetY);
        }
        
        // Store for undo
        UndoCommand cmd;
        cmd.type = UndoType::AddPolyline;
        cmd.polyline = copy;
        cmd.index = m_polylines.size();
        m_undoStack.append(cmd);
        
        m_polylines.append(copy);
        copiedCount++;
    }
    
    m_redoStack.clear();
    emit undoRedoChanged();
    emit statusMessage(QString("Copied %1 polyline(s)").arg(copiedCount));
    update();
}

void CanvasWidget::startMoveMode()
{
    if (m_selectedPolylineIndex < 0) {
        emit statusMessage("Select a polyline first");
        return;
    }
    
    m_toolState = ToolState::MoveMode;
    emit statusMessage("MOVE: Click base point, then destination point");
    update();
}

void CanvasWidget::startMirrorMode()
{
    if (m_selectedPolylineIndex < 0) {
        emit statusMessage("Select a polyline first");
        return;
    }
    
    m_toolState = ToolState::MirrorMode;
    emit statusMessage("MIRROR: Click first point of mirror axis");
    update();
}

void CanvasWidget::mirrorSelectedPolyline(const QPointF& p1, const QPointF& p2)
{
    if (m_selectedPolylineIndex < 0 || m_selectedPolylineIndex >= m_polylines.size()) {
        emit statusMessage("No polyline selected");
        return;
    }
    
    // Calculate mirror axis
    double dx = p2.x() - p1.x();
    double dy = p2.y() - p1.y();
    double lenSq = dx * dx + dy * dy;
    
    if (lenSq < 1e-12) {
        emit statusMessage("Mirror axis too short");
        return;
    }
    
    // Create mirrored copy
    CanvasPolyline mirrored = m_polylines[m_selectedPolylineIndex];
    
    for (QPointF& pt : mirrored.points) {
        // Vector from p1 to point
        double vx = pt.x() - p1.x();
        double vy = pt.y() - p1.y();
        
        // Project onto axis
        double t = (vx * dx + vy * dy) / lenSq;
        double projX = p1.x() + t * dx;
        double projY = p1.y() + t * dy;
        
        // Mirror: point' = 2 * proj - point
        pt.setX(2 * projX - pt.x());
        pt.setY(2 * projY - pt.y());
    }
    
    // Reverse point order to maintain correct direction
    std::reverse(mirrored.points.begin(), mirrored.points.end());
    
    // Store for undo
    UndoCommand cmd;
    cmd.type = UndoType::AddPolyline;
    cmd.polyline = mirrored;
    cmd.index = m_polylines.size();
    m_undoStack.append(cmd);
    m_redoStack.clear();
    
    m_polylines.append(mirrored);
    m_toolState = ToolState::None;
    
    emit undoRedoChanged();
    emit statusMessage("Polyline mirrored");
    update();
}

void CanvasWidget::startTrimMode()
{
    emit statusMessage("TRIM: Select cutting edge, then click objects to trim");
    m_toolState = ToolState::TrimMode;
    update();
}

void CanvasWidget::startExtendMode()
{
    emit statusMessage("EXTEND: Select boundary edge, then click objects to extend");
    m_toolState = ToolState::ExtendMode;
    update();
}

void CanvasWidget::startFilletMode(double radius)
{
    m_pendingFilletRadius = radius;
    m_toolState = ToolState::FilletMode;
    emit statusMessage(QString("FILLET (R=%1): Click corner to round").arg(radius, 0, 'f', 2));
    update();
}

void CanvasWidget::startMeasureMode()
{
    m_toolState = ToolState::MeasureMode;
    setCursor(Qt::CrossCursor);
    emit statusMessage("MEASURE: Click first point");
    update();
}