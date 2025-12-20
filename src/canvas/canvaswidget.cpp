#include "canvas/canvaswidget.h"
#include "dxf/dxfreader.h"
#include "gdal/gdalreader.h"
#include "tools/check_geometry_dialog.h"
#include "tools/check_point_dialog.h"
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
#include <QSettings>
#include <QMessageBox>
#include <limits>
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
    addLayer("Station", QColor(0, 255, 0));        update();
}

void CanvasWidget::setActiveLayer(const QString& name)
{
    // Verify layer exists
    bool found = false;
    for (const auto& layer : m_layers) {
        if (layer.name == name) {
            found = true;
            break;
        }
    }
    
    if (found) {
        m_activeLayer = name;
        emit layersChanged(); // Notify UI
    }
}

void CanvasWidget::setShowGrid(bool show)
{
    m_showGrid = show;
    update();
}

void CanvasWidget::setSouthAzimuth(bool enabled)
{
    m_southAzimuth = enabled;
    emit statusMessage(QString("South Bearing (Zero South): %1").arg(enabled ? "Enabled" : "Disabled"));
    update();
}


void CanvasWidget::setSwapXY(bool enabled)
{
    m_swapXY = enabled;
    emit statusMessage(QString("Swap X/Y (Y-X Convention): %1").arg(enabled ? "Enabled" : "Disabled"));
    update();
}

void CanvasWidget::setCRS(const QString& epsgCode)
{
    m_crs = epsgCode;
    emit statusMessage(QString("CRS set to: %1").arg(epsgCode));
}

void CanvasWidget::setTargetCRS(const QString& epsgCode)
{
    m_targetCrs = epsgCode;
}

void CanvasWidget::setScaleFactor(double factor)
{
    m_scaleFactor = factor;
}

QPointF CanvasWidget::transformCoordinate(const QPointF& point) const
{
    // If no transformation needed, return original
    if (m_crs == "LOCAL" || m_targetCrs == "NONE" || m_crs == m_targetCrs) {
        return point;
    }
    
    // Use GDAL/PROJ to transform
    // This is a placeholder - actual implementation would use OGRCoordinateTransformation
    OGRSpatialReference srcSRS, dstSRS;
    
    QString srcEpsg = m_crs;
    QString dstEpsg = m_targetCrs;
    
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
    
    // Load display settings
    QSettings settings;
    QString bgName = settings.value("display/backgroundColor", "#000000").toString();
    m_backgroundColor = QColor(bgName);
    m_crosshairSize = settings.value("drafting/crosshairSize", 20).toInt();
    
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
    
    // TIN surface (draw before pegs so triangles appear behind peg markers)
    drawTIN(painter);
    
    // Contour lines (after TIN, before pegs)
    drawContours(painter);
    
    // Peg markers
    drawPegs(painter);


    
    // Station and backsight markers
    drawStation(painter);
    
    // Stakeout sighting line (if in stakeout mode)
    drawStakeoutLine(painter);
    
    // Snap marker (on top of everything)
    drawSnapMarker(painter);
    
    // Temporary marker (geometry feedback)
    drawTemporaryMarker(painter);
    
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
    
    // Drawing preview (rubber banding)
    QPen previewPen(QColor(0, 255, 255), 1, Qt::DashLine);  // Cyan dashed
    previewPen.setCosmetic(true);
    painter.setPen(previewPen);
    
    switch (m_toolState) {
        case ToolState::DrawLineMode2: {
            // Line preview: from start to cursor
            QPoint p1 = worldToScreen(m_drawStartPoint);
            QPoint p2 = worldToScreen(m_drawCurrentPoint);
            painter.drawLine(p1, p2);
            
            // Show distance
            double dist = QLineF(m_drawStartPoint, m_drawCurrentPoint).length();
            painter.drawText(p2 + QPoint(10, -10), QString::number(dist, 'f', 3));
            break;
        }
        
        case ToolState::DrawPolylineMode: {
            // Draw existing polyline points
            if (m_currentPolyline.points.size() >= 1) {
                for (int i = 1; i < m_currentPolyline.points.size(); ++i) {
                    QPoint p1 = worldToScreen(m_currentPolyline.points[i-1]);
                    QPoint p2 = worldToScreen(m_currentPolyline.points[i]);
                    painter.drawLine(p1, p2);
                }
                // Rubber band from last point to cursor
                QPoint lastPt = worldToScreen(m_currentPolyline.points.last());
                QPoint cursorPt = worldToScreen(m_drawCurrentPoint);
                painter.drawLine(lastPt, cursorPt);
                
                // Show distance
                double dist = QLineF(m_currentPolyline.points.last(), m_drawCurrentPoint).length();
                painter.drawText(cursorPt + QPoint(10, -10), QString::number(dist, 'f', 3));
            }
            break;
        }
        
        case ToolState::DrawRectMode2: {
            // Rectangle preview
            QPoint p1 = worldToScreen(m_drawStartPoint);
            QPoint p2 = worldToScreen(QPointF(m_drawCurrentPoint.x(), m_drawStartPoint.y()));
            QPoint p3 = worldToScreen(m_drawCurrentPoint);
            QPoint p4 = worldToScreen(QPointF(m_drawStartPoint.x(), m_drawCurrentPoint.y()));
            painter.drawLine(p1, p2);
            painter.drawLine(p2, p3);
            painter.drawLine(p3, p4);
            painter.drawLine(p4, p1);
            
            // Show dimensions
            double width = qAbs(m_drawCurrentPoint.x() - m_drawStartPoint.x());
            double height = qAbs(m_drawCurrentPoint.y() - m_drawStartPoint.y());
            painter.drawText(p3 + QPoint(10, -10), QString("W:%1 H:%2").arg(width, 0, 'f', 3).arg(height, 0, 'f', 3));
            break;
        }
        
        case ToolState::DrawCircleMode2: {
            // Circle preview
            double radius = QLineF(m_drawStartPoint, m_drawCurrentPoint).length();
            QPoint center = worldToScreen(m_drawStartPoint);
            int screenRadius = static_cast<int>(radius * m_zoom);
            painter.drawEllipse(center, screenRadius, screenRadius);
            
            // Show radius
            QPoint cursorPt = worldToScreen(m_drawCurrentPoint);
            painter.drawLine(center, cursorPt);  // Radius line
            painter.drawText(cursorPt + QPoint(10, -10), QString("R:%1").arg(radius, 0, 'f', 3));
            break;
        }
        
        case ToolState::DrawArcMode2: {
            // Show line from start to mid point
            QPoint p1 = worldToScreen(m_drawStartPoint);
            QPoint p2 = worldToScreen(m_drawCurrentPoint);
            painter.drawLine(p1, p2);
            painter.drawText(p2 + QPoint(10, -10), "Mid point");
            break;
        }
        
        case ToolState::DrawArcMode3: {
            // Show arc preview (approximation using lines)
            QPoint p1 = worldToScreen(m_drawStartPoint);
            QPoint p2 = worldToScreen(m_drawMidPoint);
            QPoint p3 = worldToScreen(m_drawCurrentPoint);
            painter.drawLine(p1, p2);
            painter.drawLine(p2, p3);
            painter.drawText(p3 + QPoint(10, -10), "End point");
            break;
        }
        
        default:
            break;
    }
    
    // Origin crosshair
    painter.setPen(QPen(Qt::gray, 1));
    QPoint origin = worldToScreen(QPointF(0, 0));
    painter.drawLine(origin.x() - 20, origin.y(), origin.x() + 20, origin.y());
    painter.drawLine(origin.x(), origin.y() - 20, origin.x(), origin.y() + 20);
    
    // Cursor Crosshair
    if (!m_cursorPos.isNull()) {
        painter.setPen(QPen(Qt::white, 1)); // Or inverse of background?
        int size = m_crosshairSize;
        if (size >= 100) {
            // Full screen crosshair
            painter.drawLine(0, m_cursorPos.y(), width(), m_cursorPos.y());
            painter.drawLine(m_cursorPos.x(), 0, m_cursorPos.x(), height());
        } else {
            // Fixed size (scaled roughly 1-100 to sensible pixels, e.g. 10 to 1000?)
            // If value is 1-100 from slider. 
            // 20 -> 20 pixels? Or 20%? 
            // Let's treat 1-99 as percentage of screen or just pixels * 5?
            // Slider usually 1-100. Let's say pixels = size * 2. 
            int px = size * 2;
            painter.drawLine(m_cursorPos.x() - px, m_cursorPos.y(), m_cursorPos.x() + px, m_cursorPos.y());
            painter.drawLine(m_cursorPos.x(), m_cursorPos.y() - px, m_cursorPos.x(), m_cursorPos.y() + px);
        }
    }
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
    
    QSettings settings;
    double settingsGrid = settings.value("display/gridSpacing", 10.0).toDouble();
    if (settingsGrid > 0.001) m_gridSize = settingsGrid;
    
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

// Animated zoom setter for Q_PROPERTY
void CanvasWidget::setAnimatedZoom(double zoom)
{
    m_zoom = qBound(1e-4, zoom, 1e6);
    updateTransform();
    update();
    emit zoomChanged(m_zoom);
}

void CanvasWidget::wheelEvent(QWheelEvent *event)
{
    const double zoomFactor = 1.15;
    
    // Store cursor position in world coords before zoom
    QPointF cursorWorldBefore = screenToWorld(event->position().toPoint());
    
    // Calculate target zoom
    double targetZoom = m_zoom;
    if (event->angleDelta().y() > 0) {
        targetZoom *= zoomFactor;
    } else {
        targetZoom /= zoomFactor;
    }
    targetZoom = qBound(1e-4, targetZoom, 1e6);
    
    // Apply zoom directly for responsiveness (animation can cause lag on fast scrolling)
    m_zoom = targetZoom;
    updateTransform();
    
    // Adjust offset to zoom centered on cursor
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
    
    // Check if clicking on a peg (only when not in special tool modes)
    if (event->button() == Qt::LeftButton && 
        m_toolState != ToolState::AddPegMode &&
        m_toolState != ToolState::MeasureMode &&
        m_toolState != ToolState::MeasureMode2 &&
        m_toolState != ToolState::DrawLineMode &&
        m_toolState != ToolState::DrawPolylineMode) {

        int pegIdx = pegAtPosition(worldPos);
        if (pegIdx >= 0) {
            selectPeg(pegIdx);
            update();
            return;
        } else if (m_selectedPegIndex >= 0) {
            // Clicked elsewhere, deselect peg
            deselectPeg();
        }
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
        
        // Check if Swap X/Y is enabled
        QSettings settings;
        bool swapXY = settings.value("coordinates/swapXY", false).toBool();
        
        if (swapXY) {
            emit statusMessage(QString("Distance: %1 | Bearing: %2 | ΔY: %3 | ΔX: %4")
                .arg(distance, 0, 'f', 3)
                .arg(bearingStr)
                .arg(dy, 0, 'f', 3)
                .arg(dx, 0, 'f', 3));
        } else {
            emit statusMessage(QString("Distance: %1 | Bearing: %2 | ΔX: %3 | ΔY: %4")
                .arg(distance, 0, 'f', 3)
                .arg(bearingStr)
                .arg(dx, 0, 'f', 3)
                .arg(dy, 0, 'f', 3));
        }
        
        // Stay in measure mode for more measurements
        m_toolState = ToolState::MeasureMode;
        return;
    }
    
    // Stakeout mode: right-click shows detailed peg info popup
    if (m_toolState == ToolState::StakeoutMode && event->button() == Qt::RightButton) {
        double tolerance = m_snapTolerance / m_zoom;
        QPointF targetPos = worldPos;
        QString targetName;
        bool foundTarget = false;
        
        // Check backsight
        if (m_station.hasBacksight) {
            double bsDist = QLineF(worldPos, m_station.backsightPos).length();
            if (bsDist < tolerance) {
                targetPos = m_station.backsightPos;
                targetName = m_station.backsightName + " (Backsight)";
                foundTarget = true;
            }
        }
        
        // Check pegs
        if (!foundTarget) {
            int pegIndex = hitTestPeg(worldPos, tolerance);
            if (pegIndex >= 0) {
                targetPos = m_pegs[pegIndex].position;
                targetName = m_pegs[pegIndex].name;
                foundTarget = true;
            }
        }
        
        if (foundTarget && m_station.hasStation) {
            // Calculate all info
            double bearing = calculateBearing(m_station.stationPos, targetPos);
            double distance = calculateDistance(m_station.stationPos, targetPos);
            double deltaX = targetPos.x() - m_station.stationPos.x();
            double deltaY = targetPos.y() - m_station.stationPos.y();
            
            QString info = QString("<b>Point: %1</b><br><br>"
                                   "<table>"
                                   "<tr><td><b>Coordinates:</b></td><td>X: %2, Y: %3</td></tr>"
                                   "<tr><td><b>Bearing:</b></td><td>%4</td></tr>"
                                   "<tr><td><b>Distance:</b></td><td>%5 m</td></tr>"
                                   "<tr><td><b>ΔX:</b></td><td>%6 m</td></tr>"
                                   "<tr><td><b>ΔY:</b></td><td>%7 m</td></tr>")
                .arg(targetName)
                .arg(targetPos.x(), 0, 'f', 3)
                .arg(targetPos.y(), 0, 'f', 3)
                .arg(bearingToDMS(bearing))
                .arg(distance, 0, 'f', 3)
                .arg(deltaX, 0, 'f', 3)
                .arg(deltaY, 0, 'f', 3);
            
            // Add turn angle if backsight is set
            if (m_station.hasBacksight) {
                double bsBearing = calculateBearing(m_station.stationPos, m_station.backsightPos);
                double turnAngle = bearing - bsBearing;
                if (turnAngle < 0) turnAngle += 360.0;
                if (turnAngle > 360) turnAngle -= 360.0;
                info += QString("<tr><td><b>Turn from BS:</b></td><td>%1</td></tr>")
                    .arg(bearingToDMS(turnAngle));
            }
            info += "</table>";
            
            QMessageBox::information(this, QString("Stakeout: %1").arg(targetName), info);
        } else {
            emit statusMessage("Right-click on a peg or known point for details");
        }
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
        } else {
            // Use click position with generated name
            setStationPoint(worldPos, QString("STN"));
        }
        m_toolState = ToolState::Idle;
        setCursor(Qt::ArrowCursor);
        return;
    }
    
    if (m_toolState == ToolState::SetBacksight && event->button() == Qt::LeftButton) {
        double tolerance = m_snapTolerance / m_zoom;
        int pegIndex = hitTestPeg(worldPos, tolerance);
        
        if (pegIndex >= 0) {
            // Snap to exact peg position
            const CanvasPeg& peg = m_pegs[pegIndex];
            setBacksightPoint(peg.position, peg.name);
        } else {
            // Use click position with generated name
            setBacksightPoint(worldPos, QString("BS"));
        }
        m_toolState = ToolState::Idle;
        setCursor(Qt::ArrowCursor);
        return;
    }
    
    if (m_toolState == ToolState::SetCheckPoint && event->button() == Qt::LeftButton) {
        double tolerance = m_snapTolerance / m_zoom;
        int pegIndex = hitTestPeg(worldPos, tolerance);
        
        if (pegIndex >= 0) {
            // Verify against this known peg
            const CanvasPeg& peg = m_pegs[pegIndex];
            setCheckPoint(peg.position, peg.name);
        } else {
            // Use click position for check
            setCheckPoint(worldPos, QString("CHK"));
        }
        return;
    }
    
    // AddPegMode: click to place a peg
    if (m_toolState == ToolState::AddPegMode && event->button() == Qt::LeftButton) {
        QPointF pegPos = worldPos;
        // Use snap if available
        if (m_currentSnap.type != SnapType::None) {
            pegPos = m_currentSnap.worldPos;
        }
        addPegAtPosition(pegPos, m_pendingPegName, m_pendingPegZ);

        // Stay in AddPegMode for continuous placement, auto-increment name
        bool ok;
        int num = m_pendingPegName.right(m_pendingPegName.length() - 1).toInt(&ok);
        if (ok && m_pendingPegName.length() > 1) {
            // Auto-increment numeric suffix
            QString prefix = m_pendingPegName.left(1);
            while (m_pendingPegName.length() > 1 && !m_pendingPegName[m_pendingPegName.length()-2].isDigit()) {
                prefix = m_pendingPegName.left(m_pendingPegName.length() - QString::number(num).length());
                break;
            }
            m_pendingPegName = prefix + QString::number(num + 1);
        }
        emit statusMessage(QString("Click to place next peg '%1', ESC to finish").arg(m_pendingPegName));
        return;
    }
    
    // Pan mode: left-click starts panning
    if (m_toolState == ToolState::PanMode && event->button() == Qt::LeftButton) {
        m_isPanning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    
    // Handle drawing tool modes
    if (event->button() == Qt::LeftButton) {
        switch (m_toolState) {
            case ToolState::DrawLineMode:
            case ToolState::DrawLineMode2:
            case ToolState::DrawPolylineMode:
            case ToolState::DrawRectMode:
            case ToolState::DrawRectMode2:
            case ToolState::DrawCircleMode:
            case ToolState::DrawCircleMode2:
            case ToolState::DrawArcMode:
            case ToolState::DrawArcMode2:
            case ToolState::DrawArcMode3:
            case ToolState::DrawTextMode:
            case ToolState::ScaleMode:
            case ToolState::RotateMode:
                // Use snap position if available, otherwise use world position
                if (m_currentSnap.type != SnapType::None) {
                    inputCoordinate(m_currentSnap.worldPos.x(), m_currentSnap.worldPos.y());
                } else {
                    inputCoordinate(worldPos.x(), worldPos.y());
                }
                return;
            default:
                break;
        }
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
    m_cursorPos = event->pos();
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
    
    // Track cursor for drawing mode previews (rubber banding)
    switch (m_toolState) {
        case ToolState::DrawLineMode2:
        case ToolState::DrawPolylineMode:
        case ToolState::DrawRectMode2:
        case ToolState::DrawCircleMode2:
        case ToolState::DrawArcMode2:
        case ToolState::DrawArcMode3:
            // Use snap position if available
            m_drawCurrentPoint = (m_currentSnap.type != SnapType::None) ? 
                                  m_currentSnap.worldPos : worldPos;
            update();  // Trigger repaint for preview
            break;
        default:
            break;
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

void CanvasWidget::zoomToPoint(const QPointF& worldPos)
{
    // Set a comfortable zoom level
    m_zoom = 10.0;  // Higher zoom for better visibility
    
    // To center worldPos on screen, we need offset = -worldPos
    // The transform applies: screen = translate(center) * scale(zoom) * translate(offset) * world
    // So for worldPos to map to center: offset = -worldPos
    m_offset = QPointF(-worldPos.x(), -worldPos.y());
    
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
            
        case UndoType::AddPeg:
            // Remove the added peg
            if (cmd.index >= 0 && cmd.index < m_pegs.size()) {
                redoCmd.pegPosition = m_pegs[cmd.index].position;
                redoCmd.pegName = m_pegs[cmd.index].name;
                redoCmd.pegColor = m_pegs[cmd.index].color;
                m_pegs.remove(cmd.index);
            }
            break;
            
        case UndoType::DeletePeg:
            // Re-insert the deleted peg
            if (cmd.index >= 0 && cmd.index <= m_pegs.size()) {
                CanvasPeg peg;
                peg.position = cmd.pegPosition;
                peg.name = cmd.pegName;
                peg.color = cmd.pegColor;
                m_pegs.insert(cmd.index, peg);
                redoCmd.pegPosition = cmd.pegPosition;
                redoCmd.pegName = cmd.pegName;
                redoCmd.pegColor = cmd.pegColor;
            }
            break;
            
        case UndoType::ModifyPeg:
            // Restore old peg state
            if (cmd.index >= 0 && cmd.index < m_pegs.size()) {
                redoCmd.pegPosition = m_pegs[cmd.index].position;
                redoCmd.pegName = m_pegs[cmd.index].name;
                redoCmd.oldPegPosition = cmd.oldPegPosition;
                redoCmd.oldPegName = cmd.oldPegName;
                m_pegs[cmd.index].position = cmd.oldPegPosition;
                m_pegs[cmd.index].name = cmd.oldPegName;
            }
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
            
        case UndoType::AddPeg:
            // Re-add the peg
            if (cmd.index >= 0 && cmd.index <= m_pegs.size()) {
                CanvasPeg peg;
                peg.position = cmd.pegPosition;
                peg.name = cmd.pegName;
                peg.color = cmd.pegColor;
                m_pegs.insert(cmd.index, peg);
                undoCmd.pegPosition = cmd.pegPosition;
                undoCmd.pegName = cmd.pegName;
                undoCmd.pegColor = cmd.pegColor;
            }
            break;
            
        case UndoType::DeletePeg:
            // Remove the peg again
            if (cmd.index >= 0 && cmd.index < m_pegs.size()) {
                undoCmd.pegPosition = m_pegs[cmd.index].position;
                undoCmd.pegName = m_pegs[cmd.index].name;
                undoCmd.pegColor = m_pegs[cmd.index].color;
                m_pegs.remove(cmd.index);
            }
            break;
            
        case UndoType::ModifyPeg:
            // Apply the modification again
            if (cmd.index >= 0 && cmd.index < m_pegs.size()) {
                undoCmd.oldPegPosition = m_pegs[cmd.index].position;
                undoCmd.oldPegName = m_pegs[cmd.index].name;
                undoCmd.pegPosition = cmd.pegPosition;
                undoCmd.pegName = cmd.pegName;
                m_pegs[cmd.index].position = cmd.pegPosition;
                m_pegs[cmd.index].name = cmd.pegName;
            }
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

bool CanvasWidget::replaceSelectedPolylinePoints(const QVector<QPointF>& newPoints)
{
    if (m_selectedPolylineIndex < 0 || m_selectedPolylineIndex >= m_polylines.size()) {
        return false;
    }
    
    if (newPoints.isEmpty()) {
        return false;
    }
    
    // Store undo state (simplified - stores old points)
    m_polylines[m_selectedPolylineIndex].points = newPoints;
    emit undoRedoChanged();
    update();
    return true;
}

bool CanvasWidget::replacePolylinePoints(int index, const QVector<QPointF>& newPoints)
{
    if (index < 0 || index >= m_polylines.size()) {
        return false;
    }
    
    if (newPoints.isEmpty()) {
        return false;
    }
    
    m_polylines[index].points = newPoints;
    emit undoRedoChanged();
    update();
    return true;
}

int CanvasWidget::hitTestPolyline(const QPointF& worldPos, double tolerance)
{
    // Check each polyline for proximity to click point
    // Check each polyline for proximity to click point (Reverse order for Top-Most selection)
    for (int i = m_polylines.size() - 1; i >= 0; --i) {
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
    // Check each text for proximity to click point (Reverse order)
    for (int i = m_texts.size() - 1; i >= 0; --i) {
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
        // Polyline mode: cancel current polyline
        if (m_toolState == ToolState::DrawPolylineMode) {
            m_currentPolyline = CanvasPolyline();
            m_toolState = ToolState::Idle;
            setCursor(Qt::ArrowCursor);
            emit statusMessage("POLYLINE: Cancelled");
            update();
            return;
        }
        
        if (m_toolState != ToolState::Idle) {
            cancelOffsetTool();
        } else if (m_selectedPegIndex >= 0) {
            deselectPeg();
            emit statusMessage("Peg deselected");
        } else if (m_selectedVertexIndex >= 0) {
            m_selectedVertexIndex = -1;
            emit statusMessage("Vertex deselected");
            update();
        } else if (hasSelection()) {
            clearSelection();
        }
        return;
    }
    
    // Enter key: finish polyline
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && 
        m_toolState == ToolState::DrawPolylineMode) {
        finishPolyline(false);
        return;
    }
    
    // C key: close polyline
    if (event->key() == Qt::Key_C && m_toolState == ToolState::DrawPolylineMode) {
        finishPolyline(true);
        return;
    }
    
    // Delete key: delete selected peg
    if (event->key() == Qt::Key_Delete && m_selectedPegIndex >= 0) {
        deleteSelectedPeg();
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
    // Push to undo stack
    UndoCommand cmd;
    cmd.type = UndoType::AddPeg;
    cmd.pegPosition = peg.position;
    cmd.pegName = peg.name;
    cmd.pegColor = peg.color;
    cmd.index = m_pegs.size();
    m_undoStack.append(cmd);
    m_redoStack.clear();
    emit undoRedoChanged();
    
    m_pegs.append(peg);
    emit pegAdded();  // Auto-refresh peg panel
    update();
}


void CanvasWidget::addPegAtPosition(const QPointF& pos, const QString& name, double z)
{
    CanvasPeg peg;
    peg.position = pos;
    peg.z = z;
    peg.name = name;
    peg.layer = m_layers.isEmpty() ? "Default" : m_layers.first().name;
    peg.color = Qt::cyan;
    addPeg(peg);
    
    QSettings settings;
    bool swapXY = settings.value("coordinates/swapXY", false).toBool();
    QString zStr = (qAbs(z) > 0.001) ? QString(", Z: %1").arg(z, 0, 'f', 3) : "";
    if (swapXY) {
        emit statusMessage(QString("Peg '%1' added at (Y: %2, X: %3%4)")
            .arg(name).arg(pos.y(), 0, 'f', 3).arg(pos.x(), 0, 'f', 3).arg(zStr));
    } else {
        emit statusMessage(QString("Peg '%1' added at (X: %2, Y: %3%4)")
            .arg(name).arg(pos.x(), 0, 'f', 3).arg(pos.y(), 0, 'f', 3).arg(zStr));
    }
}


void CanvasWidget::startAddPegMode(const QString& pegName, double z)
{
    m_pendingPegName = pegName;
    m_pendingPegZ = z;
    m_toolState = ToolState::AddPegMode;
    setCursor(Qt::CrossCursor);
    QString zStr = (qAbs(z) > 0.001) ? QString(" at Z=%1").arg(z, 0, 'f', 3) : "";
    emit statusMessage(QString("Click to place peg '%1'%2 (snapping applies)").arg(pegName).arg(zStr));
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

int CanvasWidget::pegAtPosition(const QPointF& worldPos, double tolerance) const
{
    // Convert tolerance from screen to world coordinates
    double worldTolerance = tolerance / m_zoom;
    
    for (int i = 0; i < m_pegs.size(); ++i) {
        double dx = m_pegs[i].position.x() - worldPos.x();
        double dy = m_pegs[i].position.y() - worldPos.y();
        double dist = qSqrt(dx*dx + dy*dy);
        if (dist <= worldTolerance) {
            return i;
        }
    }
    return -1;
}

void CanvasWidget::selectPeg(int index)
{
    if (index >= 0 && index < m_pegs.size()) {
        m_selectedPegIndex = index;
        emit statusMessage(QString("Selected peg '%1' (Press Delete to remove)")
            .arg(m_pegs[index].name));
        update();
    }
}

void CanvasWidget::deselectPeg()
{
    m_selectedPegIndex = -1;
    update();
}

void CanvasWidget::deleteSelectedPeg()
{
    if (m_selectedPegIndex >= 0 && m_selectedPegIndex < m_pegs.size()) {
        QString name = m_pegs[m_selectedPegIndex].name;
        
        // Push to undo stack
        UndoCommand cmd;
        cmd.type = UndoType::DeletePeg;
        cmd.pegPosition = m_pegs[m_selectedPegIndex].position;
        cmd.pegName = name;
        cmd.pegColor = m_pegs[m_selectedPegIndex].color;
        cmd.index = m_selectedPegIndex;
        m_undoStack.append(cmd);
        m_redoStack.clear();
        emit undoRedoChanged();
        
        m_pegs.remove(m_selectedPegIndex);
        m_selectedPegIndex = -1;
        update();
        emit statusMessage(QString("Deleted peg '%1'").arg(name));
        emit pegDeleted();
    }
}

void CanvasWidget::updatePeg(int index, const QString& name, double x, double y, double z)
{
    if (index >= 0 && index < m_pegs.size()) {
        // Store old values for undo
        UndoCommand cmd;
        cmd.type = UndoType::ModifyPeg;
        cmd.pegPosition = m_pegs[index].position;
        cmd.pegName = m_pegs[index].name;
        cmd.pegColor = m_pegs[index].color;
        cmd.index = index;
        m_undoStack.append(cmd);
        m_redoStack.clear();
        emit undoRedoChanged();
        
        // Update peg
        m_pegs[index].name = name;
        m_pegs[index].position = QPointF(x, y);
        m_pegs[index].z = z;
        
        update();
        emit statusMessage(QString("Updated peg '%1' to (%2, %3, %4)")
            .arg(name).arg(x, 0, 'f', 3).arg(y, 0, 'f', 3).arg(z, 0, 'f', 3));
    }
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
    
    for (int i = 0; i < m_pegs.size(); ++i) {
        const auto& peg = m_pegs[i];
        
        // Check if layer is visible
        if (m_hiddenLayers.contains(peg.layer)) continue;
        
        QPoint screenPos = worldToScreen(peg.position);
        
        bool isSelected = (i == m_selectedPegIndex);
        
        // Draw selection highlight if selected
        if (isSelected) {
            QPen selPen(QColor(255, 255, 0), 3);
            selPen.setCosmetic(true);
            painter.setPen(selPen);
            painter.setBrush(QBrush(QColor(255, 255, 0, 60)));
            painter.drawEllipse(screenPos, markerRadius + 4, markerRadius + 4);
        }
        
        // Draw peg marker (filled circle with cross)
        QColor drawColor = isSelected ? QColor(255, 200, 0) : peg.color;
        QPen markerPen(drawColor, 2);
        markerPen.setCosmetic(true);
        painter.setPen(markerPen);
        painter.setBrush(QBrush(QColor(drawColor.red(), drawColor.green(), drawColor.blue(), 100)));
        
        // Draw triangle (survey peg symbol)
        QPolygon triangle;
        triangle << QPoint(screenPos.x(), screenPos.y() - markerRadius)
                 << QPoint(screenPos.x() - markerRadius, screenPos.y() + markerRadius)
                 << QPoint(screenPos.x() + markerRadius, screenPos.y() + markerRadius);
        painter.drawPolygon(triangle);
        
        // Draw cross inside
        painter.drawLine(screenPos.x() - 4, screenPos.y(), screenPos.x() + 4, screenPos.y());
        painter.drawLine(screenPos.x(), screenPos.y() - 4, screenPos.x(), screenPos.y() + 4);
        
        // Build label text - include Z if non-zero
        QString labelText = peg.name;
        if (qAbs(peg.z) > 0.001) {
            labelText += QString(" (Z:%1)").arg(peg.z, 0, 'f', 2);
        }
        
        // Draw label background first
        QFontMetrics fm(font);
        int textWidth = fm.horizontalAdvance(labelText);
        QColor bgColor = isSelected ? QColor(200, 150, 0, 220) : QColor(0, 0, 0, 180);
        QRect bgRect(screenPos.x() + markerRadius + 2, screenPos.y() - fontSize/2 - 2, textWidth + 6, fontSize + 6);
        painter.fillRect(bgRect, bgColor);
        
        // Draw label text
        painter.setPen(Qt::white);
        QRect textRect(screenPos.x() + markerRadius + 4, screenPos.y() - fontSize/2, textWidth + 4, fontSize + 4);
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, labelText);
    }

}


void CanvasWidget::setTIN(const CanvasTIN& tin)
{
    m_tin = tin;
    update();
}

void CanvasWidget::clearTIN()
{
    m_tin = CanvasTIN();
    update();
}

void CanvasWidget::setTINVisible(bool visible)
{
    m_tin.visible = visible;
    update();
}

void CanvasWidget::generateTINFromPegs(double designLevel)
{
    m_tin = CanvasTIN();
    
    if (m_pegs.size() < 3) {
        emit statusMessage("Need at least 3 pegs with Z to generate TIN");
        return;
    }
    
    // Collect 3D points
    QVector<QPointF> points2D;
    double minZ = std::numeric_limits<double>::max();
    double maxZ = std::numeric_limits<double>::lowest();
    
    for (const auto& peg : m_pegs) {
        m_tin.points.append(CanvasTIN::Point3D(peg.position.x(), peg.position.y(), peg.z));
        points2D.append(peg.position);
        minZ = qMin(minZ, peg.z);
        maxZ = qMax(maxZ, peg.z);
    }
    
    m_tin.minZ = minZ;
    m_tin.maxZ = maxZ;
    m_tin.designLevel = designLevel;
    
    // Triangulate
    m_tin.triangles = GeosBridge::delaunayTriangulate(points2D);
    m_tin.visible = true;
    
    emit statusMessage(QString("Generated TIN with %1 triangles from %2 points")
        .arg(m_tin.triangles.size()).arg(m_pegs.size()));
    
    update();
}

void CanvasWidget::drawTIN(QPainter& painter)
{
    if (!m_tin.visible || m_tin.triangles.isEmpty()) return;
    
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    double zRange = m_tin.maxZ - m_tin.minZ;
    if (zRange < 0.001) zRange = 1.0;
    
    for (const auto& tri : m_tin.triangles) {
        if (tri.size() != 3) continue;
        
        int i0 = tri[0], i1 = tri[1], i2 = tri[2];
        if (i0 >= m_tin.points.size() || i1 >= m_tin.points.size() || i2 >= m_tin.points.size()) {
            continue;
        }
        
        const auto& p0 = m_tin.points[i0];
        const auto& p1 = m_tin.points[i1];
        const auto& p2 = m_tin.points[i2];
        
        // Convert to screen coordinates
        QPoint s0 = worldToScreen(QPointF(p0.x, p0.y));
        QPoint s1 = worldToScreen(QPointF(p1.x, p1.y));
        QPoint s2 = worldToScreen(QPointF(p2.x, p2.y));
        
        // Calculate average Z for coloring
        double avgZ = (p0.z + p1.z + p2.z) / 3.0;
        
        QColor fillColor;
        if (m_tin.colorByElevation) {
            // Color by elevation (blue=low, green=mid, red=high)
            double t = (avgZ - m_tin.minZ) / zRange;
            int r = static_cast<int>(t < 0.5 ? 0 : (t - 0.5) * 2 * 255);
            int g = static_cast<int>(t < 0.5 ? t * 2 * 255 : (1.0 - t) * 2 * 255);
            int b = static_cast<int>(t < 0.5 ? (1.0 - t * 2) * 255 : 0);
            fillColor = QColor(r, g, b, 100);
        } else {
            // Color by cut/fill relative to design level
            if (avgZ > m_tin.designLevel) {
                fillColor = QColor(200, 50, 50, 100);  // Red = cut
            } else if (avgZ < m_tin.designLevel) {
                fillColor = QColor(50, 200, 50, 100);  // Green = fill
            } else {
                fillColor = QColor(200, 200, 50, 100); // Yellow = on grade
            }
        }
        
        QPolygon triangle;
        triangle << s0 << s1 << s2;
        
        painter.setBrush(fillColor);
        painter.setPen(QPen(QColor(100, 100, 100, 150), 1));
        painter.drawPolygon(triangle);
    }
}

// ===== Contour Line Methods =====

void CanvasWidget::setContours(const QVector<ContourLine>& contours)
{
    m_contours = contours;
    emit statusMessage(QString("Set %1 contour levels").arg(contours.size()));
    update();
}

void CanvasWidget::clearContours()
{
    m_contours.clear();
    emit statusMessage("Contours cleared");
    update();
}

void CanvasWidget::drawContours(QPainter& painter)
{
    if (m_contours.isEmpty()) return;
    
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    // Track label positions to avoid overlap
    QVector<QPoint> labelPositions;
    const int minLabelDistance = 120;  // Minimum pixels between labels
    
    int contourIndex = 0;
    for (const auto& contour : m_contours) {
        // Set pen based on major/minor
        if (contour.isMajor) {
            painter.setPen(QPen(QColor(139, 69, 19), 2.0));  // Brown, thicker
        } else {
            painter.setPen(QPen(QColor(139, 69, 19, 180), 1.0));  // Brown, thinner
        }
        
        // Draw line segments
        for (int i = 0; i + 1 < contour.points.size(); i += 2) {
            QPoint s0 = worldToScreen(contour.points[i]);
            QPoint s1 = worldToScreen(contour.points[i + 1]);
            painter.drawLine(s0, s1);
        }
        
        // Draw elevation labels on major contours (every other major to reduce clutter)
        if (contour.isMajor && !contour.points.isEmpty() && contour.points.size() >= 4) {
            // Try a few positions along the contour to find best spot
            for (int attempt = 0; attempt < 3; attempt++) {
                int idx = (contour.points.size() / 4) * (attempt + 1);
                if (idx >= contour.points.size()) idx = contour.points.size() / 2;
                
                QPointF labelPos = contour.points[idx];
                QPoint screenPos = worldToScreen(labelPos);
                
                // Check if too close to existing labels
                bool tooClose = false;
                for (const QPoint& existing : labelPositions) {
                    int dx = screenPos.x() - existing.x();
                    int dy = screenPos.y() - existing.y();
                    if (dx*dx + dy*dy < minLabelDistance * minLabelDistance) {
                        tooClose = true;
                        break;
                    }
                }
                
                if (!tooClose) {
                    // Green text for visibility
                    painter.setPen(QPen(QColor(0, 180, 0), 1));  // Bright green
                    QFont font = painter.font();
                    font.setPointSize(10);
                    font.setBold(true);
                    painter.setFont(font);
                    
                    QString text = QString::number(contour.elevation, 'f', 1);
                    painter.drawText(screenPos, text);
                    labelPositions.append(screenPos);
                    break;
                }

            }
        }
        contourIndex++;
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
    
    QSettings settings;
    bool swapXY = settings.value("coordinates/swapXY", false).toBool();
    if (swapXY) {
        emit statusMessage(QString("Station '%1' set at (Y: %2, X: %3)")
            .arg(name).arg(pos.y(), 0, 'f', 3).arg(pos.x(), 0, 'f', 3));
    } else {
        emit statusMessage(QString("Station '%1' set at (X: %2, Y: %3)")
            .arg(name).arg(pos.x(), 0, 'f', 3).arg(pos.y(), 0, 'f', 3));
    }
    update();
}

void CanvasWidget::setBacksightPoint(const QPointF& pos, const QString& name)
{
    m_station.backsightPos = pos;
    m_station.backsightName = name;
    m_station.hasBacksight = true;
    
    QSettings settings;
    bool swapXY = settings.value("coordinates/swapXY", false).toBool();
    if (swapXY) {
        emit statusMessage(QString("Backsight '%1' set at (Y: %2, X: %3)")
            .arg(name).arg(pos.y(), 0, 'f', 3).arg(pos.x(), 0, 'f', 3));
    } else {
        emit statusMessage(QString("Backsight '%1' set at (X: %2, Y: %3)")
            .arg(name).arg(pos.x(), 0, 'f', 3).arg(pos.y(), 0, 'f', 3));
    }
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
        emit statusMessage("Set station point first (press 1)");
        return;
    }
    m_toolState = ToolState::SetBacksight;
    setCursor(Qt::CrossCursor);
    emit statusMessage("Click to set backsight point (orientation reference)");
}

void CanvasWidget::startSetCheckPointMode()
{
    if (!m_station.hasStation) {
        emit statusMessage("Set station point first (press 1)");
        return;
    }
    if (!m_station.hasBacksight) {
        emit statusMessage("Set backsight point first (press 2)");
        return;
    }
    m_toolState = ToolState::SetCheckPoint;
    setCursor(Qt::CrossCursor);
    emit statusMessage("Click on a known peg to verify station setup error");
}

void CanvasWidget::setCheckPoint(const QPointF& pos, const QString& name)
{
    // Verify verification logic inside the dialog
    CheckPointDialog dialog(m_station, pos, name, this);
    dialog.exec();
    
    // Return to idle
    m_toolState = ToolState::Idle;
    setCursor(Qt::ArrowCursor);
    update();
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
    
    // Convert to 0-360 range (North Azimuth: 0 at N, 90 at E)
    if (angleDeg < 0) angleDeg += 360.0;
    
    // If South Azimuth is enabled (0 at S, 90 at W)
    // North Azimuth 0 (N) -> South Azimuth 180
    // North Azimuth 90 (E) -> South Azimuth 270
    // North Azimuth 180 (S) -> South Azimuth 0
    // North Azimuth 270 (W) -> South Azimuth 90
    if (m_southAzimuth) {
        angleDeg += 180.0;
        if (angleDeg >= 360.0) angleDeg -= 360.0;
    }
    
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
    // Format as D°M'S"
    // If South Azimuth, maybe add a suffix or prefix? 
    // Standard practice is just the angle, but for clarity let's keep it standard DMS
    // The value passed here is already adjusted by calculateBearing
    
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
    
    // Determine snap target (peg, backsight, or station)
    QPointF targetPos = m_stakeoutCursorPos;
    QString targetName;
    double tolerance = m_snapTolerance / m_zoom;
    
    // Check if cursor is near the backsight point first
    if (m_station.hasBacksight) {
        double bsDist = QLineF(m_stakeoutCursorPos, m_station.backsightPos).length();
        if (bsDist < tolerance) {
            targetPos = m_station.backsightPos;
            targetName = m_station.backsightName + " (BS)";
        }
    }
    
    // Check if cursor is near the station point
    double stnDist = QLineF(m_stakeoutCursorPos, m_station.stationPos).length();
    if (stnDist < tolerance) {
        targetPos = m_station.stationPos;
        targetName = m_station.stationName + " (STN)";
    }
    
    // Check if cursor is near a peg
    if (targetName.isEmpty()) {
        int pegIndex = hitTestPeg(m_stakeoutCursorPos, tolerance);
        if (pegIndex >= 0) {
            targetPos = m_pegs[pegIndex].position;
            targetName = m_pegs[pegIndex].name;
        }
    }
    
    QPoint stationScreen = worldToScreen(m_station.stationPos);
    QPoint targetScreen = worldToScreen(targetPos);
    
    // Draw sighting line from station to target
    QPen linePen(Qt::yellow, 2, Qt::DashLine);
    linePen.setCosmetic(true);
    painter.setPen(linePen);
    painter.drawLine(stationScreen, targetScreen);
    
    // Calculate bearing and distance
    double bearing = calculateBearing(m_station.stationPos, targetPos);
    double distance = calculateDistance(m_station.stationPos, targetPos);
    
    // Calculate delta X/Y
    double deltaX = targetPos.x() - m_station.stationPos.x();
    double deltaY = targetPos.y() - m_station.stationPos.y();
    
    // Calculate turn angle from backsight if available
    QString turnStr;
    if (m_station.hasBacksight) {
        double bsBearing = calculateBearing(m_station.stationPos, m_station.backsightPos);
        double turnAngle = bearing - bsBearing;
        if (turnAngle < 0) turnAngle += 360.0;
        if (turnAngle > 360) turnAngle -= 360.0;
        turnStr = QString("Turn: %1").arg(bearingToDMS(turnAngle));
    }
    
    // Draw info label at midpoint of line
    int midX = (stationScreen.x() + targetScreen.x()) / 2;
    int midY = (stationScreen.y() + targetScreen.y()) / 2;
    
    QString infoText;
    if (!targetName.isEmpty()) {
        infoText = QString("→ %1\nAz: %2\nDist: %3m\nΔX: %4  ΔY: %5")
            .arg(targetName)
            .arg(bearingToDMS(bearing))
            .arg(distance, 0, 'f', 3)
            .arg(deltaX, 0, 'f', 3)
            .arg(deltaY, 0, 'f', 3);
        if (!turnStr.isEmpty()) {
            infoText += "\n" + turnStr;
        }
    } else {
        infoText = QString("Az: %1\nDist: %2m\nΔX: %3  ΔY: %4")
            .arg(bearingToDMS(bearing))
            .arg(distance, 0, 'f', 3)
            .arg(deltaX, 0, 'f', 3)
            .arg(deltaY, 0, 'f', 3);
        if (!turnStr.isEmpty()) {
            infoText += "\n" + turnStr;
        }
    }
    
    // Draw background box
    QFont font = painter.font();
    font.setPointSize(10);
    font.setBold(true);
    painter.setFont(font);
    
    QFontMetrics fm(font);
    QRect textBounds = fm.boundingRect(QRect(0, 0, 250, 150), Qt::AlignLeft, infoText);
    QRect bgRect(midX + 10, midY - textBounds.height()/2 - 4, textBounds.width() + 12, textBounds.height() + 8);
    
    painter.fillRect(bgRect, QColor(0, 0, 0, 220));
    painter.setPen(Qt::yellow);
    painter.drawRect(bgRect);
    
    // Highlight if snapped
    if (!targetName.isEmpty()) {
        painter.setPen(Qt::green);
    } else {
        painter.setPen(Qt::white);
    }
    painter.drawText(bgRect.adjusted(6, 4, -6, -4), Qt::AlignLeft | Qt::AlignVCenter, infoText);
    
    // Draw snap indicator on target if snapped
    if (!targetName.isEmpty()) {
        QPen snapPen(Qt::green, 3);
        snapPen.setCosmetic(true);
        painter.setPen(snapPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(targetScreen, 12, 12);
    }
}

bool CanvasWidget::saveProject(const QString& filePath) const
{
    QByteArray jsonData = saveProjectToJson();
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(jsonData);
    file.close();
    return true;
}

QByteArray CanvasWidget::saveProjectToJson() const
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
        pegObj["z"] = peg.z;
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
    stationObj["stationZ"] = m_station.stationZ;
    stationObj["backsightX"] = m_station.backsightPos.x();
    stationObj["backsightY"] = m_station.backsightPos.y();
    stationObj["backsightZ"] = m_station.backsightZ;
    stationObj["stationName"] = m_station.stationName;
    stationObj["backsightName"] = m_station.backsightName;
    root["station"] = stationObj;

    QJsonDocument doc(root);
    return doc.toJson(QJsonDocument::Indented);
}

bool CanvasWidget::loadProject(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QByteArray jsonData = file.readAll();
    file.close();
    
    return loadProjectFromJson(jsonData);
}

bool CanvasWidget::loadProjectFromJson(const QByteArray& jsonData)
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    
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
        peg.z = pegObj["z"].toDouble(0.0);
        peg.layer = pegObj["layer"].toString();
        peg.color = QColor(pegObj["color"].toString());
        m_pegs.append(peg);
    }

    
    // Load station setup
    QJsonObject stationObj = root["station"].toObject();
    m_station.hasStation = stationObj["hasStation"].toBool(false);
    m_station.hasBacksight = stationObj["hasBacksight"].toBool(false);
    m_station.stationPos = QPointF(stationObj["stationX"].toDouble(), stationObj["stationY"].toDouble());
    m_station.stationZ = stationObj["stationZ"].toDouble(0.0);
    m_station.backsightPos = QPointF(stationObj["backsightX"].toDouble(), stationObj["backsightY"].toDouble());
    m_station.backsightZ = stationObj["backsightZ"].toDouble(0.0);
    m_station.stationName = stationObj["stationName"].toString("STN");
    m_station.backsightName = stationObj["backsightName"].toString("BS");

    
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

// ============== Drawing Tools ==============

void CanvasWidget::startDrawLineMode()
{
    clearSelection();
    m_toolState = ToolState::DrawLineMode;
    setCursor(Qt::CrossCursor);
    emit statusMessage("LINE: Click first point or type X,Y");
    update();
}

void CanvasWidget::startDrawPolylineMode()
{
    clearSelection();
    m_currentPolyline = CanvasPolyline();  // Reset
    m_currentPolyline.closed = false;
    m_currentPolyline.layer = m_layers.isEmpty() ? "Default" : m_layers.first().name;
    m_currentPolyline.color = m_layers.isEmpty() ? Qt::white : m_layers.first().color;
    m_toolState = ToolState::DrawPolylineMode;
    setCursor(Qt::CrossCursor);
    emit statusMessage("POLYLINE: Click to add vertices, double-click or Enter to finish, C to close");
    update();
}

void CanvasWidget::finishPolyline(bool close)
{
    if (m_currentPolyline.points.size() >= 2) {
        m_currentPolyline.closed = close;
        addPolyline(m_currentPolyline);
        emit statusMessage(QString("POLYLINE: Created with %1 vertices%2")
            .arg(m_currentPolyline.points.size())
            .arg(close ? " (closed)" : ""));
    } else {
        emit statusMessage("POLYLINE: Need at least 2 points");
    }
    m_currentPolyline = CanvasPolyline();  // Reset
    m_toolState = ToolState::Idle;
    setCursor(Qt::ArrowCursor);
    update();
}

void CanvasWidget::startDrawRectMode()
{
    clearSelection();
    m_toolState = ToolState::DrawRectMode;
    setCursor(Qt::CrossCursor);
    emit statusMessage("RECTANGLE: Click first corner or type X,Y");
    update();
}

void CanvasWidget::startDrawCircleMode()
{
    clearSelection();
    m_toolState = ToolState::DrawCircleMode;
    setCursor(Qt::CrossCursor);
    emit statusMessage("CIRCLE: Click center point or type X,Y");
    update();
}

void CanvasWidget::startDrawArcMode()
{
    clearSelection();
    m_toolState = ToolState::DrawArcMode;
    setCursor(Qt::CrossCursor);
    emit statusMessage("ARC: Click start point");
    update();
}

void CanvasWidget::startDrawTextMode(const QString& text, double height)
{
    clearSelection();
    m_pendingText = text;
    m_pendingTextHeight = height;
    m_toolState = ToolState::DrawTextMode;
    setCursor(Qt::CrossCursor);
    emit statusMessage("TEXT: Click to place text");
    update();
}

// ============== Transform Tools ==============

void CanvasWidget::startScaleMode(double factor)
{
    if (!hasSelection()) {
        emit statusMessage("SCALE: Select object(s) first");
        return;
    }
    m_pendingScaleFactor = factor;
    m_toolState = ToolState::ScaleMode;
    setCursor(Qt::SizeAllCursor);
    emit statusMessage(QString("SCALE (%1x): Click base point").arg(factor, 0, 'f', 2));
    update();
}

void CanvasWidget::startRotateMode(double angle)
{
    if (!hasSelection()) {
        emit statusMessage("ROTATE: Select object(s) first");
        return;
    }
    m_pendingRotateAngle = angle;
    m_toolState = ToolState::RotateMode;
    setCursor(Qt::SizeAllCursor);
    emit statusMessage(QString("ROTATE (%1°): Click base point").arg(angle, 0, 'f', 1));
    update();
}

// ============== Coordinate Input ==============

void CanvasWidget::inputCoordinate(double x, double y)
{
    QPointF point(x, y);
    m_lastInputPoint = point;
    
    switch (m_toolState) {
        case ToolState::DrawLineMode:
            m_drawStartPoint = point;
            m_toolState = ToolState::DrawLineMode2;
            emit statusMessage(QString("LINE: First point at (%1, %2). Click end point").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3));
            break;
            
        case ToolState::DrawLineMode2: {
            // Create line as a 2-point polyline
            CanvasPolyline line;
            line.points.append(m_drawStartPoint);
            line.points.append(point);
            line.closed = false;
            line.layer = m_activeLayer;
            CanvasLayer* layerPtr = getLayer(m_activeLayer);
            line.color = layerPtr ? layerPtr->color : Qt::white;
            addPolyline(line);
            m_toolState = ToolState::DrawLineMode;
            emit statusMessage("LINE: Line created. Click next start point or ESC to exit");
            break;
        }
        
        case ToolState::DrawPolylineMode: {
            // Add vertex to current polyline
            m_currentPolyline.points.append(point);
            int count = m_currentPolyline.points.size();
            emit statusMessage(QString("POLYLINE: %1 vertices. Click next point, Enter to finish, C to close").arg(count));
            update();
            break;
        }
        
        case ToolState::DrawRectMode:
            m_drawStartPoint = point;
            m_toolState = ToolState::DrawRectMode2;
            emit statusMessage("RECTANGLE: Click opposite corner");
            break;
            
        case ToolState::DrawRectMode2: {
            // Create rectangle as closed polyline
            CanvasPolyline rect;
            rect.points.append(m_drawStartPoint);
            rect.points.append(QPointF(point.x(), m_drawStartPoint.y()));
            rect.points.append(point);
            rect.points.append(QPointF(m_drawStartPoint.x(), point.y()));
            rect.closed = true;
            rect.layer = m_activeLayer;
            CanvasLayer* layerPtr = getLayer(m_activeLayer);
            rect.color = layerPtr ? layerPtr->color : Qt::white;
            addPolyline(rect);
            m_toolState = ToolState::DrawRectMode;
            emit statusMessage("RECTANGLE: Created. Click next first corner or ESC");
            break;
        }
        
        case ToolState::DrawCircleMode:
            m_drawStartPoint = point;
            m_toolState = ToolState::DrawCircleMode2;
            emit statusMessage("CIRCLE: Click point on circle or type radius");
            break;
            
        case ToolState::DrawCircleMode2: {
            // Create circle as polyline approximation
            double radius = QLineF(m_drawStartPoint, point).length();
            CanvasPolyline circle;
            const int segments = 36;
            for (int i = 0; i < segments; ++i) {
                double angle = 2.0 * M_PI * i / segments;
                circle.points.append(QPointF(
                    m_drawStartPoint.x() + radius * qCos(angle),
                    m_drawStartPoint.y() + radius * qSin(angle)
                ));
            }
            circle.closed = true;
            circle.layer = m_activeLayer;
            CanvasLayer* layerPtr = getLayer(m_activeLayer);
            circle.color = layerPtr ? layerPtr->color : Qt::white;
            addPolyline(circle);
            m_toolState = ToolState::DrawCircleMode;
            emit statusMessage("CIRCLE: Created. Click next center or ESC");
            break;
        }
        
        case ToolState::DrawArcMode:
            m_drawStartPoint = point;
            m_toolState = ToolState::DrawArcMode2;
            emit statusMessage("ARC: Click second point (on arc)");
            break;
            
        case ToolState::DrawArcMode2:
            m_drawMidPoint = point;
            m_toolState = ToolState::DrawArcMode3;
            emit statusMessage("ARC: Click end point");
            break;
            
        case ToolState::DrawArcMode3: {
            // Create 3-point arc as polyline approximation
            QPointF p1 = m_drawStartPoint;
            QPointF p2 = m_drawMidPoint;
            QPointF p3 = point;
            
            // Calculate circle through 3 points
            double ax = p1.x(), ay = p1.y();
            double bx = p2.x(), by = p2.y();
            double cx = p3.x(), cy = p3.y();
            
            double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
            if (qAbs(d) < 0.0001) {
                emit statusMessage("ARC: Points are nearly collinear");
                m_toolState = ToolState::DrawArcMode;
                break;
            }
            
            double ux = ((ax*ax + ay*ay) * (by - cy) + (bx*bx + by*by) * (cy - ay) + (cx*cx + cy*cy) * (ay - by)) / d;
            double uy = ((ax*ax + ay*ay) * (cx - bx) + (bx*bx + by*by) * (ax - cx) + (cx*cx + cy*cy) * (bx - ax)) / d;
            QPointF center(ux, uy);
            double radius = QLineF(center, p1).length();
            
            // Calculate angles
            double startAngle = qAtan2(p1.y() - center.y(), p1.x() - center.x());
            double midAngle = qAtan2(p2.y() - center.y(), p2.x() - center.x());
            double endAngle = qAtan2(p3.y() - center.y(), p3.x() - center.x());
            
            // Determine direction (clockwise or counter-clockwise)
            bool ccw = (midAngle - startAngle > 0 && midAngle - startAngle < M_PI) ||
                       (midAngle - startAngle < -M_PI);
            
            // Generate arc points
            CanvasPolyline arc;
            const int segments = 24;
            double angleSpan = ccw ? (endAngle - startAngle) : (startAngle - endAngle);
            if (ccw && angleSpan < 0) angleSpan += 2 * M_PI;
            if (!ccw && angleSpan < 0) angleSpan += 2 * M_PI;
            
            for (int i = 0; i <= segments; ++i) {
                double t = static_cast<double>(i) / segments;
                double angle = ccw ? (startAngle + t * angleSpan) : (startAngle - t * angleSpan);
                arc.points.append(QPointF(
                    center.x() + radius * qCos(angle),
                    center.y() + radius * qSin(angle)
                ));
            }
            arc.closed = false;
            arc.layer = m_activeLayer;
            CanvasLayer* layerPtr = getLayer(m_activeLayer);
            arc.color = m_layers.isEmpty() ? Qt::white : m_layers.first().color;
            addPolyline(arc);
            m_toolState = ToolState::DrawArcMode;
            emit statusMessage("ARC: Created. Click next start point or ESC");
            break;
        }
        
        case ToolState::DrawTextMode: {
            CanvasText text;
            text.text = m_pendingText;
            text.position = point;
            text.height = m_pendingTextHeight;
            text.angle = 0.0;
            text.layer = m_layers.isEmpty() ? "Default" : m_layers.first().name;
            text.color = m_layers.isEmpty() ? Qt::white : m_layers.first().color;
            m_texts.append(text);
            emit statusMessage("TEXT: Placed. Click next position or ESC");
            break;
        }
        
        case ToolState::ScaleMode:
            m_drawStartPoint = point;  // Base point
            // Apply scale immediately
            if (m_selectedPolylineIndex >= 0 && m_selectedPolylineIndex < m_polylines.size()) {
                CanvasPolyline& poly = m_polylines[m_selectedPolylineIndex];
                for (QPointF& pt : poly.points) {
                    pt = m_drawStartPoint + (pt - m_drawStartPoint) * m_pendingScaleFactor;
                }
            }
            m_toolState = ToolState::Idle;
            emit statusMessage(QString("SCALE: Applied %1x scale").arg(m_pendingScaleFactor, 0, 'f', 2));
            break;
            
        case ToolState::RotateMode:
            m_drawStartPoint = point;  // Base point
            // Apply rotation immediately
            if (m_selectedPolylineIndex >= 0 && m_selectedPolylineIndex < m_polylines.size()) {
                CanvasPolyline& poly = m_polylines[m_selectedPolylineIndex];
                double angleRad = m_pendingRotateAngle * M_PI / 180.0;
                double cosA = qCos(angleRad);
                double sinA = qSin(angleRad);
                for (QPointF& pt : poly.points) {
                    double dx = pt.x() - m_drawStartPoint.x();
                    double dy = pt.y() - m_drawStartPoint.y();
                    pt.setX(m_drawStartPoint.x() + dx * cosA - dy * sinA);
                    pt.setY(m_drawStartPoint.y() + dx * sinA + dy * cosA);
                }
            }
            m_toolState = ToolState::Idle;
            emit statusMessage(QString("ROTATE: Applied %1° rotation").arg(m_pendingRotateAngle, 0, 'f', 1));
            break;
            
        default:
            // For other states, just update last input point
            break;
    }
    update();
}

void CanvasWidget::inputRelativeCoordinate(double dx, double dy)
{
    double x = m_lastInputPoint.x() + dx;
    double y = m_lastInputPoint.y() + dy;
    inputCoordinate(x, y);
}

void CanvasWidget::inputPolar(double distance, double angle)
{
    // Angle is in degrees from North (Y+), clockwise
    double angleRad = angle * M_PI / 180.0;
    double dx = distance * qSin(angleRad);
    double dy = distance * qCos(angleRad);
    inputRelativeCoordinate(dx, dy);
}
void CanvasWidget::setTemporaryMarker(const QPointF& pos)
{
    m_hasTempMarker = true;
    m_tempMarkerPos = pos;
    update();
}

void CanvasWidget::clearTemporaryMarker()
{
    m_hasTempMarker = false;
    update();
}

void CanvasWidget::drawTemporaryMarker(QPainter& painter)
{
    if (!m_hasTempMarker) return;
    
    QPoint center = worldToScreen(m_tempMarkerPos);
    
    QPen pen(Qt::red, 3);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    
    // Draw crosshair
    int size = 15;
    painter.drawLine(center - QPoint(size, size), center + QPoint(size, size));
    painter.drawLine(center - QPoint(size, -size), center + QPoint(size, -size));
    
    // Draw Circle
    painter.drawEllipse(center, size*2, size*2);
}

void CanvasWidget::setStation(const CanvasStation& station)
{
    m_station = station;
    update();
}
