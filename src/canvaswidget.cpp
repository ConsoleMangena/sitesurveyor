#include "canvaswidget.h"
#include <QPainter>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QToolTip>
#include <QUndoCommand>
#include <QUndoStack>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QtMath>
#include "layermanager.h"

CanvasWidget::CanvasWidget(QWidget *parent) : QWidget(parent),
    m_zoom(1.0),
    m_offset(0, 0),
    m_isPanning(false),
    m_showGrid(true),
    m_showLabels(true),
    m_pointColor(Qt::yellow),
    m_lineColor(Qt::cyan),
    m_gridColor(QColor(60, 60, 60)),
    m_backgroundColor(QColor(33, 33, 33)),
    m_gridSize(20.0)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(400, 300);
    updateTransform();
}

void CanvasWidget::removePointByName(const QString& name)
{
    // Remove from drawn points only; geometry lines unaffected
    for (int i = m_points.size() - 1; i >= 0; --i) {
        if (m_points[i].point.name.compare(name, Qt::CaseInsensitive) == 0) {
            m_points.remove(i);
        }
    }
    update();
}

QString CanvasWidget::pointLayer(const QString& name) const
{
    for (const auto& dp : m_points) {
        if (dp.point.name.compare(name, Qt::CaseInsensitive) == 0) {
            return dp.layer.isEmpty() ? QStringLiteral("0") : dp.layer;
        }
    }
    return QStringLiteral("0");
}

bool CanvasWidget::setPointLayer(const QString& name, const QString& layer)
{
    if (m_layerManager) {
        if (!m_layerManager->hasLayer(layer)) return false;
    }
    bool changed = false;
    for (auto& dp : m_points) {
        if (dp.point.name.compare(name, Qt::CaseInsensitive) == 0) {
            if (dp.layer != layer) { dp.layer = layer; changed = true; }
        }
    }
    if (changed) update();
    return changed;
}

void CanvasWidget::centerOnPoint(const QPointF& world, double targetZoom)
{
    if (targetZoom > 0.0) {
        m_zoom = qBound(1e-4, targetZoom, 1e6);
    }
    m_offset = -world;
    updateTransform();
    update();
    emit zoomChanged(m_zoom);
}

void CanvasWidget::addPoint(const Point& point)
{
    QString layer = m_layerManager ? m_layerManager->currentLayer() : QStringLiteral("0");
    m_points.append({point, m_pointColor, layer});
    update();
}

void CanvasWidget::addLine(const QPointF& start, const QPointF& end)
{
    QString layer = m_layerManager ? m_layerManager->currentLayer() : QStringLiteral("0");
    m_lines.append({start, end, m_lineColor, layer});
    update();
}

void CanvasWidget::clearAll()
{
    m_points.clear();
    m_lines.clear();
    update();
}

void CanvasWidget::setShowGrid(bool show)
{
    m_showGrid = show;
    update();
}

void CanvasWidget::setShowLabels(bool show)
{
    m_showLabels = show;
    update();
}

void CanvasWidget::setPointColor(const QColor& color)
{
    m_pointColor = color;
    update();
}

void CanvasWidget::setLineColor(const QColor& color)
{
    m_lineColor = color;
    update();
}

void CanvasWidget::setGridSize(double size)
{
    if (size <= 0) {
        return;
    }
    m_gridSize = size;
    update();
}

QPointF CanvasWidget::screenToWorld(const QPoint& screenPos) const
{
    return m_screenToWorld.map(QPointF(screenPos));
}

QPoint CanvasWidget::worldToScreen(const QPointF& worldPos) const
{
    return m_worldToScreen.map(worldPos).toPoint();
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

void CanvasWidget::fitToWindow()
{
    if (m_points.isEmpty()) return;
    
    QRectF bounds;
    for (const auto& dp : m_points) {
        if (bounds.isNull()) {
            bounds = QRectF(dp.point.toQPointF(), QSizeF(0, 0));
        } else {
            bounds = bounds.united(QRectF(dp.point.toQPointF(), QSizeF(0, 0)));
        }
    }
    
    if (!bounds.isNull() && bounds.width() > 0 && bounds.height() > 0) {
        double scaleX = width() / (bounds.width() + 100);
        double scaleY = height() / (bounds.height() + 100);
        m_zoom = qMin(scaleX, scaleY);
        m_offset = -bounds.center();
        updateTransform();
        update();
        emit zoomChanged(m_zoom);
    }
}

void CanvasWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Fill background
    painter.fillRect(rect(), m_backgroundColor);
    
    // Apply transformation
    painter.setTransform(m_worldToScreen);
    
    // Draw grid
    if (m_showGrid) {
        drawGrid(painter);
    }
    
    // Draw axes
    drawAxes(painter);
    
    // Draw lines
    drawLines(painter);
    
    // Draw points
    drawPoints(painter);

    // Draw in-progress drawing (preview) in world space
    if (m_isDrawing && !m_drawVertices.isEmpty()) {
        painter.save();
        QPen prevPen(QColor(0, 200, 255));
        prevPen.setStyle(Qt::DashLine);
        prevPen.setWidthF(1.5 / m_zoom);
        painter.setPen(prevPen);
        // Draw existing segments
        for (int i = 1; i < m_drawVertices.size(); ++i) {
            painter.drawLine(toDisplay(m_drawVertices[i-1]), toDisplay(m_drawVertices[i]));
        }
        // Draw current segment to hover
        painter.drawLine(toDisplay(m_drawVertices.last()), toDisplay(m_currentHoverWorld));
        painter.restore();
    }

    // UI overlays (screen space)
    painter.resetTransform();
    // Draw zoom window rectangle if active
    if (m_drawZoomRect) {
        QPen rectPen(QColor(200, 200, 255, 220));
        rectPen.setStyle(Qt::DashLine);
        rectPen.setWidth(1);
        painter.setPen(rectPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(m_zoomRect.normalized());
    }
    // Object snap indicator
    if (m_hasSnapIndicator) {
        painter.save();
        QPen sp(Qt::yellow);
        sp.setWidth(2);
        painter.setPen(sp);
        painter.setBrush(Qt::NoBrush);
        const int r = 6;
        painter.drawEllipse(m_snapIndicatorScreen, r, r);
        painter.drawLine(m_snapIndicatorScreen + QPoint(-r-2, 0), m_snapIndicatorScreen + QPoint(r+2, 0));
        painter.drawLine(m_snapIndicatorScreen + QPoint(0, -r-2), m_snapIndicatorScreen + QPoint(0, r+2));
        painter.restore();
    }
    // Crosshair
    if (m_showCrosshair) {
        QPen crossPen(QColor(150, 150, 150, 180));
        crossPen.setWidth(1);
        painter.setPen(crossPen);
        painter.drawLine(QPoint(0, m_currentMousePos.y()), QPoint(width(), m_currentMousePos.y()));
        painter.drawLine(QPoint(m_currentMousePos.x(), 0), QPoint(m_currentMousePos.x(), height()));
    }

    // Draw grips (screen space) when hovering or dragging a line vertex
    if (m_hoverLineIndex >= 0 || m_draggingVertex) {
        painter.save();
        painter.setTransform(QTransform());
        auto drawGrip = [&](const QPoint& s){
            const int sz = 6;
            QRect r(s.x()-sz, s.y()-sz, sz*2, sz*2);
            painter.setPen(QPen(Qt::white));
            painter.setBrush(QBrush(QColor(0, 122, 204)));
            painter.drawRect(r);
        };
        int li = m_draggingVertex ? m_dragLineIndex : m_hoverLineIndex;
        if (li >= 0 && li < m_lines.size()) {
            QPoint sa = worldToScreen(toDisplay(m_lines[li].start));
            QPoint sb = worldToScreen(toDisplay(m_lines[li].end));
            drawGrip(sa);
            drawGrip(sb);
        }
        painter.restore();
    }
}

void CanvasWidget::wheelEvent(QWheelEvent *event)
{
    // Zoom anchored at cursor position
    const double zoomFactor = 1.15;
    QPointF cursorWorldBefore = screenToWorld(event->position().toPoint());
    if (event->angleDelta().y() > 0) m_zoom *= zoomFactor; else m_zoom /= zoomFactor;
    // Clamp zoom to reasonable range
    if (m_zoom < 1e-4) m_zoom = 1e-4;
    if (m_zoom > 1e6) m_zoom = 1e6;
    // Update transform for new zoom and adjust offset so the world point under the cursor stays fixed
    updateTransform();
    QPointF cursorWorldAfter = screenToWorld(event->position().toPoint());
    QPointF delta = cursorWorldBefore - cursorWorldAfter;
    m_offset += delta;
    updateTransform();
    update();
    emit zoomChanged(m_zoom);
}

void CanvasWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        // Middle mouse pan (like CAD)
        m_isPanning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (event->button() == Qt::LeftButton) {
        if (m_toolMode == ToolMode::Pan || m_spacePanActive) {
            m_isPanning = true;
            m_lastMousePos = event->pos();
            setCursor(Qt::ClosedHandCursor);
            return;
        }
        if (m_toolMode == ToolMode::ZoomWindow) {
            m_drawZoomRect = true;
            m_zoomRect.setTopLeft(event->pos());
            m_zoomRect.setBottomRight(event->pos());
            setCursor(Qt::CrossCursor);
            return;
        }
        // Drawing tools
        if (m_toolMode == ToolMode::DrawLine || m_toolMode == ToolMode::DrawPolygon) {
            // Enforce locked layer (prevent draw on locked current layer)
            if (m_layerManager) {
                Layer L = m_layerManager->getLayer(m_layerManager->currentLayer());
                if (!L.name.isEmpty() && L.locked) {
                    QToolTip::showText(event->globalPosition().toPoint(), QString("Layer '%1' is locked").arg(L.name), this);
                    return;
                }
            }
            QPointF wp = adjustedWorldFromScreen(event->pos());
            if (!m_isDrawing) {
                m_isDrawing = true;
                m_isPolygon = (m_toolMode == ToolMode::DrawPolygon);
                m_drawVertices.clear();
                m_drawVertices.append(wp);
                m_orthoAnchor = wp;
            } else {
                // Add segment from last to wp
                QPointF last = m_drawVertices.last();
                if (!qFuzzyCompare(last.x(), wp.x()) || !qFuzzyCompare(last.y(), wp.y())) {
                    m_drawVertices.append(wp);
                    // Add line segment to canvas
                    addLine(last, wp);
                    m_orthoAnchor = wp;
                }
            }
            update();
            return;
        }
        // Select mode: try grips first
        if (m_toolMode == ToolMode::Select) {
            int li=-1, vi=-1;
            if (hitTestGrip(event->pos(), li, vi)) {
                // Enforce lock of the line's layer
                if (m_layerManager) {
                    Layer L = m_layerManager->getLayer(m_lines[li].layer);
                    if (!L.name.isEmpty() && L.locked) {
                        QToolTip::showText(event->globalPosition().toPoint(), QString("Layer '%1' is locked").arg(L.name), this);
                        return;
                    }
                }
                m_draggingVertex = true;
                m_dragLineIndex = li;
                m_dragVertexIndex = vi;
                m_dragOldPos = getLineVertex(li, vi);
                m_orthoAnchor = m_dragOldPos;
                setCursor(Qt::SizeAllCursor);
                update();
                return;
            }
            // If not on a grip, try selecting a line by proximity
            int lineHit = -1;
            if (hitTestLine(event->pos(), lineHit)) {
                setSelectedLine(lineHit);
                update();
                return;
            } else if (m_selectedLineIndex != -1) {
                // Clear selection if clicked empty area
                setSelectedLine(-1);
                update();
                // fall through to allow click coordinates processing if needed
            }
        }
        // Otherwise: set ortho anchor and emit click at adjusted world
        m_orthoAnchor = screenToWorld(event->pos());
        QPointF worldPos = adjustedWorldFromScreen(event->pos());
        emit canvasClicked(worldPos);
        return;
    }
    if (event->button() == Qt::RightButton) {
        // Finish drawing on right-click if in drawing mode
        if (m_isDrawing) {
            if (m_isPolygon && m_drawVertices.size() >= 3) {
                // Close polygon by adding last->first if not same
                QPointF first = m_drawVertices.first();
                QPointF last = m_drawVertices.last();
                if (!qFuzzyCompare(first.x(), last.x()) || !qFuzzyCompare(first.y(), last.y())) {
                    addLine(last, first);
                }
            }
            m_isDrawing = false; m_isPolygon = false; m_drawVertices.clear();
            emit drawingDistanceChanged(0.0);
            update();
            return;
        }
        // Also emit click on right button for selection use-cases
        QPointF worldPos = adjustedWorldFromScreen(event->pos());
        emit canvasClicked(worldPos);
    }
}

void CanvasWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_currentMousePos = event->pos();
    // Compute osnap indicator
    bool snapFound = false;
    QPointF snapWorld = objectSnapFromScreen(event->pos(), snapFound);
    if (m_osnapMode && snapFound) {
        m_hasSnapIndicator = true;
        m_snapIndicatorWorld = snapWorld;
        m_snapIndicatorScreen = worldToScreen(toDisplay(m_snapIndicatorWorld));
    } else {
        m_hasSnapIndicator = false;
    }
    QPointF worldPos = adjustedWorldFromScreen(event->pos());
    emit mouseWorldPosition(worldPos);
    m_currentHoverWorld = worldPos;
    // Live measurement while drawing/dragging
    if (m_isDrawing && !m_drawVertices.isEmpty()) {
        QPointF last = m_drawVertices.last();
        double dx = worldPos.x() - last.x();
        double dy = worldPos.y() - last.y();
        emit drawingDistanceChanged(qSqrt(dx*dx + dy*dy));
    } else if (m_draggingVertex) {
        double dx = worldPos.x() - m_dragOldPos.x();
        double dy = worldPos.y() - m_dragOldPos.y();
        emit drawingDistanceChanged(qSqrt(dx*dx + dy*dy));
    } else {
        emit drawingDistanceChanged(0.0);
    }
    
    if (m_isPanning) {
        QPointF delta = event->pos() - m_lastMousePos;
        m_offset += delta / m_zoom;
        m_lastMousePos = event->pos();
        updateTransform();
        update();
        return;
    }
    if (m_drawZoomRect) {
        m_zoomRect.setBottomRight(event->pos());
        update();
        return;
    }

    // Hover grips in Select mode when not dragging
    if (!m_draggingVertex && m_toolMode == ToolMode::Select) {
        int li=-1, vi=-1;
        if (hitTestGrip(event->pos(), li, vi)) {
            m_hoverLineIndex = li; m_hoverVertexIndex = vi;
            setCursor(Qt::SizeAllCursor);
        } else {
            if (m_hoverLineIndex != -1 || m_hoverVertexIndex != -1) update();
            m_hoverLineIndex = m_hoverVertexIndex = -1;
            setCursor(Qt::ArrowCursor);
        }
        update();
    }

    // Dragging a vertex
    if (m_draggingVertex) {
        QPointF wp = adjustedWorldFromScreen(event->pos());
        setLineVertex(m_dragLineIndex, m_dragVertexIndex, wp);
        update();
        return;
    }
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
        if (m_isPanning) {
            m_isPanning = false;
            setCursor(Qt::ArrowCursor);
        }
        if (event->button() == Qt::LeftButton && m_draggingVertex) {
            // Finalize move: push undo command if available
            QPointF newPos = getLineVertex(m_dragLineIndex, m_dragVertexIndex);
            if (m_undoStack && (newPos != m_dragOldPos)) {
                class MoveVertexCommand : public QUndoCommand {
                public:
                    MoveVertexCommand(CanvasWidget* w, int li, int vi, const QPointF& from, const QPointF& to)
                        : m_w(w), m_li(li), m_vi(vi), m_from(from), m_to(to) {}
                    void undo() override { if (m_w) m_w->setLineVertex(m_li, m_vi, m_from); }
                    void redo() override { if (m_w) m_w->setLineVertex(m_li, m_vi, m_to); }
                private:
                    CanvasWidget* m_w; int m_li; int m_vi; QPointF m_from; QPointF m_to;
                };
                m_undoStack->push(new MoveVertexCommand(this, m_dragLineIndex, m_dragVertexIndex, m_dragOldPos, newPos));
            }
            m_draggingVertex = false; m_dragLineIndex = -1; m_dragVertexIndex = -1;
            setCursor(Qt::ArrowCursor);
            update();
        }
        if (m_drawZoomRect && m_toolMode == ToolMode::ZoomWindow) {
            QRect r = m_zoomRect.normalized();
            m_drawZoomRect = false;
            setCursor(Qt::ArrowCursor);
            if (r.width() > 5 && r.height() > 5) {
                QPointF wTL = screenToWorld(r.topLeft());
                QPointF wBR = screenToWorld(r.bottomRight());
                QRectF wRect = QRectF(wTL, wBR).normalized();
                if (wRect.width() > 0 && wRect.height() > 0) {
                    double scaleX = width() / wRect.width();
                    double scaleY = height() / wRect.height();
                    m_zoom = qMin(scaleX, scaleY) * 0.95; // margin
                    m_offset = -wRect.center();
                    updateTransform();
                    update();
                    emit zoomChanged(m_zoom);
                }
            } else {
                update();
            }
        }
    }
}

void CanvasWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isDrawing) {
        // Finalize current drawing
        if (m_isPolygon && m_drawVertices.size() >= 3) {
            QPointF first = m_drawVertices.first();
            QPointF last = m_drawVertices.last();
            if (!qFuzzyCompare(first.x(), last.x()) || !qFuzzyCompare(first.y(), last.y())) {
                addLine(last, first);
            }
        }
        m_isDrawing = false; m_isPolygon = false; m_drawVertices.clear();
        update();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void CanvasWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateTransform();
}

void CanvasWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !m_spacePanActive) {
        m_spacePanActive = true;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        if (m_drawZoomRect) {
            m_drawZoomRect = false;
            setCursor(Qt::ArrowCursor);
            update();
        }
        if (m_isDrawing) {
            m_isDrawing = false; m_isPolygon = false; m_drawVertices.clear();
            emit drawingDistanceChanged(0.0);
            update();
        }
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void CanvasWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space) {
        m_spacePanActive = false;
        if (!m_isPanning) setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void CanvasWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QActionGroup *group = new QActionGroup(&menu);
    group->setExclusive(true);
    QAction *selectAct = menu.addAction("Select"); selectAct->setCheckable(true); selectAct->setChecked(m_toolMode == ToolMode::Select); selectAct->setActionGroup(group);
    QAction *panAct = menu.addAction("Pan"); panAct->setCheckable(true); panAct->setChecked(m_toolMode == ToolMode::Pan); panAct->setActionGroup(group);
    QAction *zoomWinAct = menu.addAction("Zoom Window"); zoomWinAct->setCheckable(true); zoomWinAct->setChecked(m_toolMode == ToolMode::ZoomWindow); zoomWinAct->setActionGroup(group);
    QAction *drawLineAct = menu.addAction("Draw Line"); drawLineAct->setCheckable(true); drawLineAct->setChecked(m_toolMode == ToolMode::DrawLine); drawLineAct->setActionGroup(group);
    QAction *drawPolyAct = menu.addAction("Draw Polygon"); drawPolyAct->setCheckable(true); drawPolyAct->setChecked(m_toolMode == ToolMode::DrawPolygon); drawPolyAct->setActionGroup(group);
    menu.addSeparator();
    QAction *zoomInAct = menu.addAction("Zoom In");
    QAction *zoomOutAct = menu.addAction("Zoom Out");
    QAction *fitAct = menu.addAction("Fit to Window");
    QAction *resetAct = menu.addAction("Reset View");
    menu.addSeparator();
    QAction *crosshairAct = menu.addAction("Show Crosshair"); crosshairAct->setCheckable(true); crosshairAct->setChecked(m_showCrosshair);
    QAction *orthoAct = menu.addAction("ORTHO"); orthoAct->setCheckable(true); orthoAct->setChecked(m_orthoMode);
    QAction *snapAct = menu.addAction("SNAP"); snapAct->setCheckable(true); snapAct->setChecked(m_snapMode);
    QAction *osnapAct = menu.addAction("OSNAP"); osnapAct->setCheckable(true); osnapAct->setChecked(m_osnapMode);

    QAction *chosen = menu.exec(event->globalPos());
    if (!chosen) return;
    if (chosen == selectAct) setToolMode(ToolMode::Select);
    else if (chosen == panAct) setToolMode(ToolMode::Pan);
    else if (chosen == zoomWinAct) setToolMode(ToolMode::ZoomWindow);
    else if (chosen == drawLineAct) setToolMode(ToolMode::DrawLine);
    else if (chosen == drawPolyAct) setToolMode(ToolMode::DrawPolygon);
    else if (chosen == zoomInAct) zoomIn();
    else if (chosen == zoomOutAct) zoomOut();
    else if (chosen == fitAct) fitToWindow();
    else if (chosen == resetAct) resetView();
    else if (chosen == crosshairAct) setShowCrosshair(!m_showCrosshair);
    else if (chosen == orthoAct) setOrthoMode(!m_orthoMode);
    else if (chosen == snapAct) setSnapMode(!m_snapMode);
    else if (chosen == osnapAct) setOsnapMode(!m_osnapMode);
}

void CanvasWidget::drawGrid(QPainter& painter)
{
    painter.save();
    painter.resetTransform();
    
    QPen gridPen(m_gridColor);
    gridPen.setWidth(1);
    painter.setPen(gridPen);
    
    // Calculate grid bounds in world coordinates
    QPointF topLeft = screenToWorld(QPoint(0, 0));
    QPointF bottomRight = screenToWorld(QPoint(width(), height()));
    
    double gridStep = m_gridSize;
    while (gridStep * m_zoom < 10) gridStep *= 2;
    while (gridStep * m_zoom > 100) gridStep /= 2;
    
    // Vertical lines
    double x = qFloor(topLeft.x() / gridStep) * gridStep;
    while (x <= bottomRight.x()) {
        QPoint screenX = worldToScreen(QPointF(x, 0));
        painter.drawLine(screenX.x(), 0, screenX.x(), height());
        x += gridStep;
    }
    
    // Horizontal lines
    double y = qFloor(bottomRight.y() / gridStep) * gridStep;
    while (y <= topLeft.y()) {
        QPoint screenY = worldToScreen(QPointF(0, y));
        painter.drawLine(0, screenY.y(), width(), screenY.y());
        y += gridStep;
    }
    
    painter.restore();
}

void CanvasWidget::drawAxes(QPainter& painter)
{
    painter.save();
    
    QPen axisPen(QColor(100, 100, 100));
    axisPen.setWidth(2);
    painter.setPen(axisPen);
    
    // X-axis
    painter.drawLine(QPointF(-10000, 0), QPointF(10000, 0));
    
    // Y-axis
    painter.drawLine(QPointF(0, -10000), QPointF(0, 10000));
    
    painter.restore();
}

void CanvasWidget::drawPoints(QPainter& painter)
{
    for (const auto& dp : m_points) {
        // Layer visibility
        if (m_layerManager) {
            Layer L = m_layerManager->getLayer(dp.layer);
            if (!L.name.isEmpty() && !L.visible) continue;
        }
        painter.save();
        // Color from layer if available
        QColor c = dp.color;
        if (m_layerManager) {
            Layer L = m_layerManager->getLayer(dp.layer);
            if (!L.name.isEmpty()) c = L.color;
        }
        // Draw point
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(c));
        QPointF pos = toDisplay(dp.point.toQPointF());
        painter.drawEllipse(pos, 4.0/m_zoom, 4.0/m_zoom);
        
        // Draw label
        if (m_showLabels) {
            painter.resetTransform();
            QPoint screenPos = worldToScreen(pos);
            painter.setPen(Qt::white);
            QFont font = painter.font();
            font.setPointSize(10);
            painter.setFont(font);
            painter.drawText(screenPos + QPoint(8, -5), dp.point.name);
            painter.setTransform(m_worldToScreen);
        }
        
        painter.restore();
    }
}

void CanvasWidget::drawLines(QPainter& painter)
{
    int idx = 0;
    for (const auto& dl : m_lines) {
        // Layer visibility
        if (m_layerManager) {
            Layer L = m_layerManager->getLayer(dl.layer);
            if (!L.name.isEmpty() && !L.visible) { idx++; continue; }
        }
        painter.save();
        QColor c = dl.color;
        if (m_layerManager) {
            Layer L = m_layerManager->getLayer(dl.layer);
            if (!L.name.isEmpty()) c = L.color;
        }
        // Highlight selected line
        QPen linePen(idx == m_selectedLineIndex ? QColor(255, 200, 0) : c);
        linePen.setWidthF(idx == m_selectedLineIndex ? qMax(2.0, 2.0 / m_zoom) : 1.0);
        painter.setPen(linePen);
        painter.drawLine(toDisplay(dl.start), toDisplay(dl.end));
        painter.restore();
        idx++;
    }
}

void CanvasWidget::updateTransform()
{
    m_worldToScreen = QTransform();
    m_worldToScreen.translate(width() / 2.0, height() / 2.0);
    m_worldToScreen.scale(m_zoom, -m_zoom);  // Flip Y axis
    m_worldToScreen.translate(m_offset.x(), m_offset.y());
    
    m_screenToWorld = m_worldToScreen.inverted();
}

QPointF CanvasWidget::toDisplay(const QPointF& p) const
{
    // Drawing should not swap X/Y in Gauss/Zimbabwe mode.
    // Gauss mode only affects input/output order and azimuth computations.
    return p;
}

QPointF CanvasWidget::applyOrtho(const QPointF& world) const
{
    if (!m_orthoMode) return world;
    QPointF res = world;
    const double dx = world.x() - m_orthoAnchor.x();
    const double dy = world.y() - m_orthoAnchor.y();
    if (qAbs(dx) >= qAbs(dy)) {
        // Constrain horizontally
        res.setY(m_orthoAnchor.y());
    } else {
        // Constrain vertically
        res.setX(m_orthoAnchor.x());
    }
    return res;
}

QPointF CanvasWidget::applySnap(const QPointF& world) const
{
    if (!m_snapMode) return world;
    const double g = m_gridSize > 0 ? m_gridSize : 1.0;
    QPointF res = world;
    res.setX(qRound(world.x() / g) * g);
    res.setY(qRound(world.y() / g) * g);
    return res;
}

QPointF CanvasWidget::adjustedWorldFromScreen(const QPoint& screen) const
{
QPointF w = screenToWorld(screen);
// Object snap has priority
if (m_osnapMode) {
    bool found = false;
    QPointF ow = objectSnapFromScreen(screen, found);
    if (found) return ow;
}
// Apply ortho first relative to anchor, then snap to grid
w = applyOrtho(w);
w = applySnap(w);
return w;
}

QPointF CanvasWidget::objectSnapFromScreen(const QPoint& screen, bool& found) const
{
found = false;
// pixel tolerance
const int tol = 10;
int bestDist2 = tol * tol + 1;
QPointF bestWorld;
auto considerWorld = [&](const QPointF& world){
    QPoint s = worldToScreen(toDisplay(world));
    int dx = s.x() - screen.x();
    int dy = s.y() - screen.y();
    int d2 = dx*dx + dy*dy;
    if (d2 < bestDist2) {
        bestDist2 = d2;
        bestWorld = world;
    }
};
// Consider points (visible layers only)
for (const auto& dp : m_points) {
    if (m_layerManager) {
        Layer L = m_layerManager->getLayer(dp.layer);
        if (!L.name.isEmpty() && !L.visible) continue;
    }
    considerWorld(dp.point.toQPointF());
}
// Consider line endpoints, midpoints, nearest points (visible layers only)
const QPointF sP = QPointF(screen);
auto screenPoint = [&](const QPointF& world){ return m_worldToScreen.map(toDisplay(world)); };
for (const auto& dl : m_lines) {
    if (m_layerManager) {
        Layer L = m_layerManager->getLayer(dl.layer);
        if (!L.name.isEmpty() && !L.visible) continue;
    }
    // Endpoints
    considerWorld(dl.start);
    considerWorld(dl.end);
    // Midpoint
    considerWorld(QPointF((dl.start.x()+dl.end.x())*0.5, (dl.start.y()+dl.end.y())*0.5));
    // Nearest point on segment in screen space (more intuitive with pixel tol)
    QPointF sa = screenPoint(dl.start);
    QPointF sb = screenPoint(dl.end);
    QPointF v = sb - sa;
    double denom = v.x()*v.x() + v.y()*v.y();
    if (denom > 1e-9) {
        QPointF w = sP - sa;
        double t = (w.x()*v.x() + w.y()*v.y()) / denom;
        if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0;
        QPointF worldNearest(dl.start.x() + t*(dl.end.x()-dl.start.x()),
                             dl.start.y() + t*(dl.end.y()-dl.start.y()));
        considerWorld(worldNearest);
    }
}
// Consider intersections between visible line segments
auto segIntersect = [](const QPointF& a1, const QPointF& a2, const QPointF& b1, const QPointF& b2, QPointF& out) -> bool {
    auto cross = [](const QPointF& u, const QPointF& v){ return u.x()*v.y() - u.y()*v.x(); };
    QPointF r = a2 - a1;
    QPointF s = b2 - b1;
    double rxs = cross(r, s);
    if (qFuzzyIsNull(rxs)) return false; // parallel
    QPointF qp = b1 - a1;
    double t = cross(qp, s) / rxs;
    double u = cross(qp, r) / rxs;
    if (t < 0.0 || t > 1.0 || u < 0.0 || u > 1.0) return false;
    out = a1 + t * r;
    return true;
};
for (int i = 0; i < m_lines.size(); ++i) {
    const auto& li = m_lines[i];
    if (m_layerManager) { Layer L = m_layerManager->getLayer(li.layer); if (!L.name.isEmpty() && !L.visible) continue; }
    for (int j = i+1; j < m_lines.size(); ++j) {
        const auto& lj = m_lines[j];
        if (m_layerManager) { Layer L2 = m_layerManager->getLayer(lj.layer); if (!L2.name.isEmpty() && !L2.visible) continue; }
        QPointF ip;
        if (segIntersect(li.start, li.end, lj.start, lj.end, ip)) {
            considerWorld(ip);
        }
    }
}
if (bestDist2 <= tol*tol) {
    found = true;
    return bestWorld;
}
return screenToWorld(screen);
}

// --- Editing helpers ---
bool CanvasWidget::hitTestGrip(const QPoint& screen, int& outLineIndex, int& outVertexIndex) const
{
    const int tol = 8; // pixels
    int bestD2 = tol*tol + 1;
    int bi = -1, bv = -1;
    for (int i = 0; i < m_lines.size(); ++i) {
        // Respect layer visibility
        if (m_layerManager) {
            Layer L = m_layerManager->getLayer(m_lines[i].layer);
            if (!L.name.isEmpty() && !L.visible) continue;
        }
        QPoint sa = worldToScreen(toDisplay(m_lines[i].start));
        QPoint sb = worldToScreen(toDisplay(m_lines[i].end));
        auto consider = [&](const QPoint& s, int vi){
            int dx = s.x()-screen.x(); int dy = s.y()-screen.y(); int d2 = dx*dx+dy*dy;
            if (d2 < bestD2) { bestD2 = d2; bi = i; bv = vi; }
        };
        consider(sa, 0);
        consider(sb, 1);
    }
    if (bestD2 <= tol*tol) { outLineIndex = bi; outVertexIndex = bv; return true; }
    return false;
}

QPointF CanvasWidget::getLineVertex(int lineIndex, int vertexIndex) const
{
    if (lineIndex < 0 || lineIndex >= m_lines.size()) return QPointF();
    const auto& dl = m_lines[lineIndex];
    return vertexIndex == 0 ? dl.start : dl.end;
}

bool CanvasWidget::setLineVertex(int lineIndex, int vertexIndex, const QPointF& world)
{
    if (lineIndex < 0 || lineIndex >= m_lines.size()) return false;
    // Respect layer lock
    if (m_layerManager) {
        Layer L = m_layerManager->getLayer(m_lines[lineIndex].layer);
        if (!L.name.isEmpty() && L.locked) return false;
    }
    auto& dl = m_lines[lineIndex];
    if (vertexIndex == 0) dl.start = world; else dl.end = world;
    update();
    return true;
}

// Helper to reset drawing state when switching tools (called via setToolMode)
static inline bool isDrawMode(CanvasWidget::ToolMode m) {
    return m == CanvasWidget::ToolMode::DrawLine || m == CanvasWidget::ToolMode::DrawPolygon;
}

bool CanvasWidget::setSelectedLine(int index)
{
    if (index < -1 || index >= m_lines.size()) return false;
    if (m_selectedLineIndex == index) return true;
    m_selectedLineIndex = index;
    emit selectedLineChanged(index);
    update();
    return true;
}

QString CanvasWidget::lineLayer(int lineIndex) const
{
    if (lineIndex < 0 || lineIndex >= m_lines.size()) return QStringLiteral("0");
    const auto& dl = m_lines[lineIndex];
    return dl.layer.isEmpty() ? QStringLiteral("0") : dl.layer;
}

bool CanvasWidget::setLineLayer(int lineIndex, const QString& layer)
{
    if (lineIndex < 0 || lineIndex >= m_lines.size()) return false;
    if (m_layerManager && !m_layerManager->hasLayer(layer)) return false;
    auto& dl = m_lines[lineIndex];
    if (dl.layer == layer) return true;
    dl.layer = layer;
    update();
    return true;
}

bool CanvasWidget::lineEndpoints(int lineIndex, QPointF& startOut, QPointF& endOut) const
{
    if (lineIndex < 0 || lineIndex >= m_lines.size()) return false;
    startOut = m_lines[lineIndex].start;
    endOut = m_lines[lineIndex].end;
    return true;
}

bool CanvasWidget::hitTestLine(const QPoint& screen, int& outLineIndex) const
{
    const int tol = 8; // pixels
    int bestIdx = -1;
    double bestD2 = (tol*tol) + 1.0;
    for (int i = 0; i < m_lines.size(); ++i) {
        const auto& dl = m_lines[i];
        // Respect layer visibility
        if (m_layerManager) {
            Layer L = m_layerManager->getLayer(dl.layer);
            if (!L.name.isEmpty() && !L.visible) continue;
        }
        QPoint sa = worldToScreen(toDisplay(dl.start));
        QPoint sb = worldToScreen(toDisplay(dl.end));
        QPointF v = sb - sa;
        const double denom = v.x()*v.x() + v.y()*v.y();
        if (denom < 1e-6) continue;
        QPointF w = screen - sa;
        double t = (w.x()*v.x() + w.y()*v.y()) / denom;
        if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0;
        QPointF proj = sa + t * v;
        double dx = proj.x() - screen.x();
        double dy = proj.y() - screen.y();
        double d2 = dx*dx + dy*dy;
        if (d2 < bestD2) { bestD2 = d2; bestIdx = i; }
    }
    if (bestIdx != -1 && bestD2 <= tol*tol) {
        outLineIndex = bestIdx;
        return true;
    }
    return false;
}