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
#include <QPair>
#include <QPolygonF>
#include <QPainterPath>
#include <QLineF>
#include <QMessageBox>
#include <QGuiApplication>
#include <algorithm>
#include <QtMath>
#include "layermanager.h"
#include "surveycalculator.h"

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

// Parse a string like "12.3<45" or "<90" or "25" into distance and/or angle
bool CanvasWidget::parseDistanceAngleInput(const QString& text,
                                 bool& hasDist, double& dist,
                                 bool& hasAngle, double& angleDeg) const
{
    hasDist = false; hasAngle = false; dist = 0.0; angleDeg = 0.0;
    QString t = text.trimmed();
    if (t.isEmpty()) return false;
    // Remove spaces
    t.remove(' ');
    int lt = t.indexOf('<');
    if (lt >= 0) {
        QString left = t.left(lt);
        QString right = t.mid(lt+1);
        bool okA=false, okD=false;
        if (!left.isEmpty()) { double d = left.toDouble(&okD); if (okD) { hasDist = true; dist = qMax(0.0, d); } }
        if (!right.isEmpty()) { double a = right.toDouble(&okA); if (okA) { hasAngle = true; angleDeg = SurveyCalculator::normalizeAngle(a); } }
        return hasDist || hasAngle;
    } else {
        bool okD=false; double d = t.toDouble(&okD);
        if (okD) { hasDist = true; dist = qMax(0.0, d); return true; }
    }
    return false;
}

void CanvasWidget::showAllLayers()
{
    if (!m_layerManager) return;
    for (const Layer& L : m_layerManager->layers()) {
        m_layerManager->setLayerVisible(L.name, true);
    }
}

// Utility: segment intersection in world space
static inline bool segmentsIntersectWorld(const QPointF& a1, const QPointF& a2,
                                          const QPointF& b1, const QPointF& b2)
{
    auto cross = [](const QPointF& u, const QPointF& v){ return u.x()*v.y() - u.y()*v.x(); };
    QPointF r = a2 - a1;
    QPointF s = b2 - b1;
    double rxs = cross(r, s);
    QPointF qp = b1 - a1;
    if (qFuzzyIsNull(rxs)) return false; // parallel or collinear
    double t = (qp.x()*s.y() - qp.y()*s.x()) / rxs;
    double u = (qp.x()*r.y() - qp.y()*r.x()) / rxs;
    return (t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0);
}

bool CanvasWidget::hitTestPoint(const QPoint& screen, int& outPointIndex) const
{
    const int tol = 6; // pixels
    int bestIdx = -1;
    int bestD2 = tol*tol + 1;
    for (int i = 0; i < m_points.size(); ++i) {
        const auto& dp = m_points[i];
        if (m_layerManager) {
            Layer L = m_layerManager->getLayer(dp.layer);
            if (!L.name.isEmpty() && !L.visible) continue;
        }
        QPoint s = worldToScreen(toDisplay(dp.point.toQPointF()));
        int dx = s.x() - screen.x();
        int dy = s.y() - screen.y();
        int d2 = dx*dx + dy*dy;
        if (d2 < bestD2) { bestD2 = d2; bestIdx = i; }
    }
    if (bestIdx != -1 && bestD2 <= tol*tol) { outPointIndex = bestIdx; return true; }
    return false;
}

void CanvasWidget::setExclusiveSelectionLine(int idx)
{
    // Skip locked
    if (idx >= 0 && idx < m_lines.size() && m_layerManager) {
        Layer L = m_layerManager->getLayer(m_lines[idx].layer);
        if (!L.name.isEmpty() && L.locked) return;
    }
    m_selectedLineIndices.clear();
    m_selectedPointIndices.clear();
    if (idx >= 0 && idx < m_lines.size()) m_selectedLineIndices.insert(idx);
    int lc = m_selectedLineIndices.size() + ((m_selectedLineIndex>=0 && !m_selectedLineIndices.contains(m_selectedLineIndex))?1:0);
    emit selectionChanged(m_selectedPointIndices.size(), lc);
}

void CanvasWidget::toggleSelectionLine(int idx)
{
    if (idx < 0 || idx >= m_lines.size()) return;
    if (m_layerManager) { Layer L = m_layerManager->getLayer(m_lines[idx].layer); if (!L.name.isEmpty() && L.locked) return; }
    if (m_selectedLineIndices.contains(idx)) m_selectedLineIndices.remove(idx);
    else m_selectedLineIndices.insert(idx);
    int lc = m_selectedLineIndices.size() + ((m_selectedLineIndex>=0 && !m_selectedLineIndices.contains(m_selectedLineIndex))?1:0);
    emit selectionChanged(m_selectedPointIndices.size(), lc);
}

void CanvasWidget::setExclusiveSelectionPoint(int idx)
{
    if (idx >= 0 && idx < m_points.size() && m_layerManager) {
        Layer L = m_layerManager->getLayer(m_points[idx].layer);
        if (!L.name.isEmpty() && L.locked) return;
    }
    m_selectedLineIndices.clear();
    m_selectedPointIndices.clear();
    if (idx >= 0 && idx < m_points.size()) m_selectedPointIndices.insert(idx);
    int lc = m_selectedLineIndices.size() + ((m_selectedLineIndex>=0 && !m_selectedLineIndices.contains(m_selectedLineIndex))?1:0);
    emit selectionChanged(m_selectedPointIndices.size(), lc);
}

void CanvasWidget::toggleSelectionPoint(int idx)
{
    if (idx < 0 || idx >= m_points.size()) return;
    if (m_layerManager) { Layer L = m_layerManager->getLayer(m_points[idx].layer); if (!L.name.isEmpty() && L.locked) return; }
    if (m_selectedPointIndices.contains(idx)) m_selectedPointIndices.remove(idx);
    else m_selectedPointIndices.insert(idx);
    int lc = m_selectedLineIndices.size() + ((m_selectedLineIndex>=0 && !m_selectedLineIndices.contains(m_selectedLineIndex))?1:0);
    emit selectionChanged(m_selectedPointIndices.size(), lc);
}

void CanvasWidget::clearSelection()
{
    m_selectedPointIndices.clear();
    m_selectedLineIndices.clear();
    m_selectedLineIndex = -1;
}

bool CanvasWidget::hasSelection() const
{
    return !m_selectedPointIndices.isEmpty() || !m_selectedLineIndices.isEmpty() || m_selectedLineIndex >= 0;
}

// Undo command for removing current selection
class RemoveSelectionCommand : public QUndoCommand {
public:
    explicit RemoveSelectionCommand(CanvasWidget* w)
        : m_w(w) {
        // Capture selection snapshot (indices and items)
        if (!m_w) return;
        // Lines
        QSet<int> lines = m_w->m_selectedLineIndices;
        if (m_w->m_selectedLineIndex >= 0) lines.insert(m_w->m_selectedLineIndex);
        QList<int> lineIdxs = lines.values();
        std::sort(lineIdxs.begin(), lineIdxs.end());
        for (int idx : lineIdxs) {
            if (idx >= 0 && idx < m_w->m_lines.size()) {
                // Respect layer lock
                if (m_w->m_layerManager) {
                    Layer L = m_w->m_layerManager->getLayer(m_w->m_lines[idx].layer);
                    if (!L.name.isEmpty() && L.locked) continue;
                }
                m_lines.append(qMakePair(idx, m_w->m_lines[idx]));
            }
        }
        // Points
        QList<int> pointIdxs = m_w->m_selectedPointIndices.values();
        std::sort(pointIdxs.begin(), pointIdxs.end());
        for (int idx : pointIdxs) {
            if (idx >= 0 && idx < m_w->m_points.size()) {
                // Respect layer lock
                if (m_w->m_layerManager) {
                    Layer L = m_w->m_layerManager->getLayer(m_w->m_points[idx].layer);
                    if (!L.name.isEmpty() && L.locked) continue;
                }
                m_points.append(qMakePair(idx, m_w->m_points[idx]));
            }
        }
        setText("Delete Selected");
    }
    void undo() override {
        if (m_w) m_w->applyRestoreSelection(m_points, m_lines);
    }
    void redo() override {
        if (m_w) m_w->applyDeleteSelection();
    }
private:
    CanvasWidget* m_w{nullptr};
    QVector<QPair<int, CanvasWidget::DrawnPoint>> m_points;
    QVector<QPair<int, CanvasWidget::DrawnLine>> m_lines;
};

void CanvasWidget::applyDeleteSelection()
{
    // Remove lines (descending indices)
    QList<int> lineIdxs = m_selectedLineIndices.values();
    if (m_selectedLineIndex >= 0) lineIdxs.append(m_selectedLineIndex);
    std::sort(lineIdxs.begin(), lineIdxs.end(), std::greater<int>());
    for (int idx : lineIdxs) {
        if (idx >= 0 && idx < m_lines.size()) {
            // Respect layer lock
            if (m_layerManager) { Layer L = m_layerManager->getLayer(m_lines[idx].layer); if (!L.name.isEmpty() && L.locked) continue; }
            m_lines.remove(idx);
        }
    }
    // Remove points (descending indices)
    QList<int> pointIdxs = m_selectedPointIndices.values();
    std::sort(pointIdxs.begin(), pointIdxs.end(), std::greater<int>());
    for (int idx : pointIdxs) {
        if (idx >= 0 && idx < m_points.size()) {
            if (m_layerManager) { Layer L = m_layerManager->getLayer(m_points[idx].layer); if (!L.name.isEmpty() && L.locked) continue; }
            m_points.remove(idx);
        }
    }
    clearSelection();
    update();
}

void CanvasWidget::applyRestoreSelection(const QVector<QPair<int, DrawnPoint>>& points,
                               const QVector<QPair<int, DrawnLine>>& lines)
{
    // Restore points at original indices
    for (const auto& pr : points) {
        int idx = pr.first;
        if (idx < 0) continue;
        if (idx >= m_points.size()) m_points.append(pr.second);
        else m_points.insert(idx, pr.second);
    }
    // Restore lines at original indices
    for (const auto& pr : lines) {
        int idx = pr.first;
        if (idx < 0) continue;
        if (idx >= m_lines.size()) m_lines.append(pr.second);
        else m_lines.insert(idx, pr.second);
    }
    update();
}

void CanvasWidget::deleteSelected()
{
    if (!hasSelection()) return;
    // Confirm large deletions
    int total = m_selectedPointIndices.size() + m_selectedLineIndices.size() + (m_selectedLineIndex>=0?1:0);
    if (total >= 50) {
        auto ret = QMessageBox::question(this, "Delete Selected",
                                         QString("Delete %1 selected items?").arg(total),
                                         QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
    }
    if (m_undoStack) m_undoStack->push(new RemoveSelectionCommand(this));
    else applyDeleteSelection();
}

// Selection operations (visible)
void CanvasWidget::selectAllVisible()
{
    clearSelection();
    // Points
    for (int i=0;i<m_points.size();++i) {
        if (m_layerManager) {
            Layer L = m_layerManager->getLayer(m_points[i].layer);
            if (!L.name.isEmpty() && (!L.visible || L.locked)) continue;
        }
        m_selectedPointIndices.insert(i);
    }
    // Lines
    for (int i=0;i<m_lines.size();++i) {
        if (m_layerManager) {
            Layer L = m_layerManager->getLayer(m_lines[i].layer);
            if (!L.name.isEmpty() && (!L.visible || L.locked)) continue;
        }
        m_selectedLineIndices.insert(i);
    }
    int lc = m_selectedLineIndices.size();
    emit selectionChanged(m_selectedPointIndices.size(), lc);
    update();
}

void CanvasWidget::invertSelectionVisible()
{
    QSet<int> newPoints;
    for (int i=0;i<m_points.size();++i) {
        if (m_layerManager) { Layer L = m_layerManager->getLayer(m_points[i].layer); if (!L.name.isEmpty() && (!L.visible || L.locked)) continue; }
        if (!m_selectedPointIndices.contains(i)) newPoints.insert(i);
    }
    QSet<int> newLines;
    for (int i=0;i<m_lines.size();++i) {
        if (m_layerManager) { Layer L = m_layerManager->getLayer(m_lines[i].layer); if (!L.name.isEmpty() && (!L.visible || L.locked)) continue; }
        if (!m_selectedLineIndices.contains(i)) newLines.insert(i);
    }
    m_selectedPointIndices = newPoints;
    m_selectedLineIndices = newLines;
    int lc = m_selectedLineIndices.size();
    emit selectionChanged(m_selectedPointIndices.size(), lc);
    update();
}

void CanvasWidget::selectByCurrentLayer()
{
    if (!m_layerManager) return;
    QString layer = m_layerManager->currentLayer();
    clearSelection();
    for (int i=0;i<m_points.size();++i) { if (m_points[i].layer == layer) m_selectedPointIndices.insert(i); }
    for (int i=0;i<m_lines.size();++i) { if (m_lines[i].layer == layer) m_selectedLineIndices.insert(i); }
    int lc = m_selectedLineIndices.size();
    emit selectionChanged(m_selectedPointIndices.size(), lc);
    update();
}

void CanvasWidget::hideSelectedLayers()
{
    if (!m_layerManager) return;
    QSet<QString> layers;
    for (int i: m_selectedPointIndices) layers.insert(m_points[i].layer);
    for (int i: m_selectedLineIndices) layers.insert(m_lines[i].layer);
    if (m_selectedLineIndex>=0 && m_selectedLineIndex < m_lines.size()) layers.insert(m_lines[m_selectedLineIndex].layer);
    for (const Layer& L : m_layerManager->layers()) {
        if (layers.contains(L.name)) m_layerManager->setLayerVisible(L.name, false);
    }
}

void CanvasWidget::lockSelectedLayers()
{
    if (!m_layerManager) return;
    QSet<QString> layers;
    for (int i: m_selectedPointIndices) layers.insert(m_points[i].layer);
    for (int i: m_selectedLineIndices) layers.insert(m_lines[i].layer);
    if (m_selectedLineIndex>=0 && m_selectedLineIndex < m_lines.size()) layers.insert(m_lines[m_selectedLineIndex].layer);
    for (const Layer& L : m_layerManager->layers()) {
        if (layers.contains(L.name)) m_layerManager->setLayerLocked(L.name, true);
    }
}

void CanvasWidget::isolateSelectionLayers()
{
    if (!m_layerManager) return;
    QSet<QString> selectedLayers;
    for (int i: m_selectedPointIndices) selectedLayers.insert(m_points[i].layer);
    for (int i: m_selectedLineIndices) selectedLayers.insert(m_lines[i].layer);
    if (m_selectedLineIndex>=0 && m_selectedLineIndex < m_lines.size()) selectedLayers.insert(m_lines[m_selectedLineIndex].layer);
    // Hide others, show selected layers
    for (const Layer& L : m_layerManager->layers()) {
        m_layerManager->setLayerVisible(L.name, selectedLayers.contains(L.name));
    }
}

void CanvasWidget::setSelectedLayer(const QString& layer)
{
    if (!m_layerManager || layer.isEmpty()) return;
    if (!m_layerManager->hasLayer(layer)) return;
    // Points
    for (int idx : m_selectedPointIndices) {
        if (idx >= 0 && idx < m_points.size()) {
            // Skip if target layer is locked
            Layer L = m_layerManager->getLayer(layer);
            if (!L.name.isEmpty() && L.locked) continue;
            setPointLayer(m_points[idx].point.name, layer);
        }
    }
    // Lines (including legacy single selection)
    QSet<int> lines = m_selectedLineIndices;
    if (m_selectedLineIndex>=0) lines.insert(m_selectedLineIndex);
    for (int idx : lines) {
        if (idx >= 0 && idx < m_lines.size()) {
            Layer L = m_layerManager->getLayer(layer);
            if (!L.name.isEmpty() && L.locked) continue;
            setLineLayer(idx, layer);
        }
    }
    update();
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

void CanvasWidget::addPolyline(const QVector<QPointF>& pts, bool closed)
{
    if (pts.size() < 2) return;
    for (int i = 1; i < pts.size(); ++i) {
        addLine(pts[i-1], pts[i]);
    }
    if (closed) {
        addLine(pts.back(), pts.front());
    }
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
        // Draw existing segments (for polygon/line)
        for (int i = 1; i < m_drawVertices.size(); ++i) {
            painter.drawLine(toDisplay(m_drawVertices[i-1]), toDisplay(m_drawVertices[i]));
        }
        // Shape-specific preview
        if (m_toolMode == ToolMode::DrawCircle && m_drawVertices.size() >= 1) {
            QPointF c = m_drawVertices.first();
            double r = SurveyCalculator::distance(c, m_currentHoverWorld);
            // Allow typed radius or diameter (D=...)
            if (!m_dynBuffer.isEmpty()) {
                QString t = m_dynBuffer.trimmed();
                if (t.startsWith('D', Qt::CaseInsensitive)) {
                    t.remove(0,1); if (t.startsWith('=')) t.remove(0,1);
                    bool ok=false; double d = t.toDouble(&ok); if (ok) r = qMax(0.0, d/2.0);
                } else {
                    bool ok=false; double val = t.toDouble(&ok); if (ok) r = qMax(0.0, val);
                }
            }
            if (r > 0.0) painter.drawEllipse(toDisplay(c), r, r);
        } else if (m_toolMode == ToolMode::DrawRectangle && m_drawVertices.size() >= 1) {
            QPointF a = m_drawVertices.first();
            QPointF b = m_currentHoverWorld;
            if (m_orthoMode || (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)) {
                double dx = b.x() - a.x();
                double dy = b.y() - a.y();
                double s = qMax(qAbs(dx), qAbs(dy));
                double sx = (dx >= 0.0 ? 1.0 : -1.0);
                double sy = (dy >= 0.0 ? 1.0 : -1.0);
                b = QPointF(a.x() + sx*s, a.y() + sy*s);
            }
            QPointF p1(a.x(), a.y());
            QPointF p2(b.x(), a.y());
            QPointF p3(b.x(), b.y());
            QPointF p4(a.x(), b.y());
            painter.drawLine(toDisplay(p1), toDisplay(p2));
            painter.drawLine(toDisplay(p2), toDisplay(p3));
            painter.drawLine(toDisplay(p3), toDisplay(p4));
            painter.drawLine(toDisplay(p4), toDisplay(p1));
        } else if (m_toolMode == ToolMode::DrawArc && m_drawVertices.size() == 2) {
            // Preview three-point arc using hover as third point
            auto angleDeg = [&](const QPointF& ctr, const QPointF& p){
                double th = qAtan2(p.x()-ctr.x(), p.y()-ctr.y());
                return SurveyCalculator::normalizeAngle(SurveyCalculator::radiansToDegrees(th));
            };
            QPointF a = m_drawVertices[0];
            QPointF b = m_drawVertices[1];
            QPointF c = m_currentHoverWorld;
            // Circumcenter
            QPointF center;
            double r = 0.0;
            auto circum = [&](const QPointF& p1, const QPointF& p2, const QPointF& p3, QPointF& o, double& rad)->bool{
                double ax=p1.x(), ay=p1.y(), bx=p2.x(), by=p2.y(), cx=p3.x(), cy=p3.y();
                double d = 2*(ax*(by-cy) + bx*(cy-ay) + cx*(ay-by));
                if (qFuzzyIsNull(d)) return false;
                double ux = ((ax*ax+ay*ay)*(by-cy) + (bx*bx+by*by)*(cy-ay) + (cx*cx+cy*cy)*(ay-by)) / d;
                double uy = ((ax*ax+ay*ay)*(cx-bx) + (bx*bx+by*by)*(ax-cx) + (cx*cx+cy*cy)*(bx-ax)) / d;
                o = QPointF(ux, uy);
                rad = SurveyCalculator::distance(o, p1);
                return true;
            };
            if (circum(a,b,c,center,r) && r > 0.0) {
                double aa = angleDeg(center, a);
                double ab = angleDeg(center, b);
                double ac = angleDeg(center, c);
                double ccw = fmod(ac - aa + 360.0, 360.0);
                double ab_ccw = fmod(ab - aa + 360.0, 360.0);
                bool useCCW = (ab_ccw <= ccw);
                double sweep = useCCW ? ccw : (ccw - 360.0);
                int segs = qMax(8, int(qCeil(qAbs(sweep) / (180.0/16.0))));
                QPointF prev = a;
                for (int i=1;i<=segs;++i) {
                    double t = aa + (sweep * (double(i)/segs));
                    double radt = SurveyCalculator::degreesToRadians(t);
                    QPointF pt(center.x() + r*qSin(radt), center.y() + r*qCos(radt));
                    painter.drawLine(toDisplay(prev), toDisplay(pt));
                    prev = pt;
                }
            } else {
                // Fallback: polyline through a-b-hover
                painter.drawLine(toDisplay(a), toDisplay(b));
                painter.drawLine(toDisplay(b), toDisplay(c));
            }
        } else {
            // Default: line preview from last vertex to hover (with dynamic input for line)
            QPointF preview = m_currentHoverWorld;
            if (m_toolMode == ToolMode::DrawLine) {
                bool hasDist=false, hasAngle=false; double dist=0.0, ang=0.0;
                if (!m_dynBuffer.isEmpty()) {
                    parseDistanceAngleInput(m_dynBuffer, hasDist, dist, hasAngle, ang);
                }
                QPointF last = m_drawVertices.last();
                double useAng = 0.0;
                if (hasAngle) useAng = ang;
                else if (m_hasPendingAngle) useAng = m_pendingAngleDeg;
                else useAng = SurveyCalculator::azimuth(last, m_currentHoverWorld);
                double useDist = 0.0;
                if (hasDist) useDist = dist;
                else {
                    QPointF v = m_currentHoverWorld - last;
                    double curDist = SurveyCalculator::distance(last, m_currentHoverWorld);
                    if (hasAngle || m_hasPendingAngle) {
                        double azRad = SurveyCalculator::degreesToRadians(useAng);
                        QPointF dir(qSin(azRad), qCos(azRad));
                        useDist = qMax(0.0, v.x()*dir.x() + v.y()*dir.y());
                    } else {
                        useDist = curDist;
                    }
                }
                preview = SurveyCalculator::polarToRectangular(last, useDist, useAng);
            }
            painter.drawLine(toDisplay(m_drawVertices.last()), toDisplay(preview));
        }
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

    // Draw OTRACK guides (from track origin): horizontal/vertical and polar increments
    if (m_otrackMode && m_hasTrackOrigin) {
        painter.save();
        // world transform
        painter.setTransform(m_worldToScreen);
        QPen tp(QColor(100, 220, 100, 150)); tp.setStyle(Qt::DashLine); tp.setWidthF(1.0 / m_zoom);
        painter.setPen(tp);
        // Compute far extents
        QPointF tl = screenToWorld(QPoint(0,0));
        QPointF br = screenToWorld(QPoint(width(), height()));
        double R = qMax(qAbs(br.x()-tl.x()), qAbs(br.y()-tl.y())) * 2.0;
        auto drawRay = [&](double ang){
            double rad = SurveyCalculator::degreesToRadians(ang);
            QPointF dir(qSin(rad), qCos(rad));
            QPointF a = m_trackOriginWorld;
            QPointF p1 = QPointF(a.x() - R*dir.x(), a.y() - R*dir.y());
            QPointF p2 = QPointF(a.x() + R*dir.x(), a.y() + R*dir.y());
            painter.drawLine(toDisplay(p1), toDisplay(p2));
        };
        // Orthogonal
        drawRay(0.0); drawRay(90.0);
        // Polar spokes
        if (m_polarMode) {
            double inc = (m_polarIncrementDeg > 0.0 ? m_polarIncrementDeg : 15.0);
            for (double a = 0.0; a < 180.0; a += inc) drawRay(a);
        }
        painter.restore();
    }

    // Dynamic input overlay text (near cursor) and real-time shape measurements
    if (m_dynInputEnabled && (m_isDrawing || m_draggingVertex || m_draggingSelection || m_toolMode == ToolMode::Lengthen)) {
        QString tip;
        bool hasDist=false, hasAngle=false; double dist=0.0, ang=0.0;
        if (!m_dynBuffer.isEmpty()) parseDistanceAngleInput(m_dynBuffer, hasDist, dist, hasAngle, ang);
        if (m_toolMode == ToolMode::Lengthen) {
            tip = m_dynBuffer.trimmed();
        } else if (m_isDrawing) {
            if (m_toolMode == ToolMode::DrawLine) {
                if (hasDist && hasAngle) tip = QString("%1<%2").arg(dist, 0, 'f', 3).arg(ang, 0, 'f', 2);
                else if (hasDist) tip = QString("%1").arg(dist, 0, 'f', 3);
                else if (hasAngle || m_hasPendingAngle) tip = QString("<%1").arg(hasAngle ? ang : m_pendingAngleDeg, 0, 'f', 2);
            } else if (m_toolMode == ToolMode::DrawCircle && !m_drawVertices.isEmpty()) {
                QPointF c = m_drawVertices.first();
                double r = SurveyCalculator::distance(c, m_currentHoverWorld);
                if (hasDist) r = qMax(0.0, dist);
                tip = QString("R=%1").arg(r, 0, 'f', 3);
            } else if (m_toolMode == ToolMode::DrawRectangle && !m_drawVertices.isEmpty()) {
                QPointF a = m_drawVertices.first();
                QPointF b = m_currentHoverWorld;
                double w = qAbs(b.x() - a.x());
                double h = qAbs(b.y() - a.y());
                tip = QString("W=%1  H=%2").arg(w, 0, 'f', 3).arg(h, 0, 'f', 3);
            } else if (m_toolMode == ToolMode::DrawArc && m_drawVertices.size() == 2) {
                QPointF a = m_drawVertices[0];
                QPointF b = m_drawVertices[1];
                QPointF c = m_currentHoverWorld;
                QPointF center; double r=0.0;
                auto circum = [&](const QPointF& p1, const QPointF& p2, const QPointF& p3, QPointF& o, double& rad)->bool{
                    double ax=p1.x(), ay=p1.y(), bx=p2.x(), by=p2.y(), cx=p3.x(), cy=p3.y();
                    double d = 2*(ax*(by-cy) + bx*(cy-ay) + cx*(ay-by));
                    if (qFuzzyIsNull(d)) return false;
                    double ux = ((ax*ax+ay*ay)*(by-cy) + (bx*bx+by*by)*(cy-ay) + (cx*cx+cy*cy)*(ay-by)) / d;
                    double uy = ((ax*ax+ay*ay)*(cx-bx) + (bx*bx+by*by)*(ax-cx) + (cx*cx+cy*cy)*(bx-ax)) / d;
                    o = QPointF(ux, uy);
                    rad = SurveyCalculator::distance(o, p1);
                    return true;
                };
                auto angleDeg = [&](const QPointF& ctr, const QPointF& p){
                    double th = qAtan2(p.x()-ctr.x(), p.y()-ctr.y());
                    return SurveyCalculator::normalizeAngle(SurveyCalculator::radiansToDegrees(th));
                };
                if (circum(a,b,c,center,r) && r > 0.0) {
                    double aa = angleDeg(center, a);
                    double ac = angleDeg(center, c);
                    double sweep = fmod(ac - aa + 360.0, 360.0);
                    tip = QString("R=%1  Ang=%2°").arg(r, 0, 'f', 3).arg(sweep, 0, 'f', 2);
                }
            }
        }
        if (!tip.isEmpty()) {
            painter.save();
            painter.resetTransform();
            QPoint pos = m_currentMousePos + QPoint(12, -12);
            QFont f = painter.font(); f.setPointSize(10);
            painter.setFont(f);
            painter.setPen(QPen(Qt::yellow));
            painter.drawText(pos, tip);
            painter.restore();
        }
    }

    // Selection rectangle (screen space)
    if (m_selectRectActive) {
        painter.save();
        QPen selPen(QColor(100, 255, 100, 220));
        selPen.setStyle(Qt::DashLine);
        selPen.setWidth(1);
        painter.setPen(selPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(m_selectRect.normalized());
        painter.restore();
    }
    // Lasso polyline (screen space)
    if (m_lassoActive && m_lassoPoints.size() >= 2) {
        painter.save();
        QPen lp(QColor(100, 255, 100, 220));
        lp.setStyle(Qt::DashLine);
        painter.setPen(lp);
        painter.setBrush(Qt::NoBrush);
        for (int i=1;i<m_lassoPoints.size();++i) {
            painter.drawLine(m_lassoPoints[i-1], m_lassoPoints[i]);
        }
        // Hover preview closing segment
        painter.drawLine(m_lassoPoints.back(), m_lassoHover);
        painter.restore();
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
        // Lengthen tool: modify nearest endpoint of a line
        if (m_toolMode == ToolMode::Lengthen) {
            int li = -1;
            if (!hitTestLine(event->pos(), li)) return;
            if (li < 0 || li >= m_lines.size()) return;
            // Respect layer lock
            if (m_layerManager) { Layer L = m_layerManager->getLayer(m_lines[li].layer); if (!L.name.isEmpty() && L.locked) return; }
            QPointF a = m_lines[li].start;
            QPointF b = m_lines[li].end;
            // Choose endpoint nearest to cursor
            QPoint sa = worldToScreen(toDisplay(a));
            QPoint sb = worldToScreen(toDisplay(b));
            int dA = QLineF(sa, event->pos()).length();
            int dB = QLineF(sb, event->pos()).length();
            int vi = (dB < dA) ? 1 : 0;
            QPointF anchor = (vi == 0) ? b : a; // keep the far endpoint fixed
            QPointF moving = (vi == 0) ? a : b;
            // Compute new length
            QString t = m_dynBuffer.trimmed();
            bool hasVal = !t.isEmpty();
            bool ok = false;
            double val = t.toDouble(&ok);
            bool isDelta = false;
            if (hasVal && (t.startsWith('+') || t.startsWith('-'))) isDelta = true;
            double Lcur = SurveyCalculator::distance(anchor, moving);
            double Lnew = Lcur;
            if (hasVal && ok) {
                if (isDelta) Lnew = qMax(0.0, Lcur + val);
                else Lnew = qMax(0.0, val);
            } else {
                // Use projection of click point onto line direction from anchor
                QPointF w = adjustedWorldFromScreen(event->pos());
                QPointF v = moving - anchor; double denom = v.x()*v.x()+v.y()*v.y();
                if (denom > 1e-12) {
                    double tproj = ((w.x()-anchor.x())*v.x() + (w.y()-anchor.y())*v.y()) / qSqrt(denom);
                    Lnew = qMax(0.0, tproj);
                }
            }
            // Compute new endpoint position along line direction from anchor
            double az = SurveyCalculator::azimuth(anchor, moving);
            QPointF target = SurveyCalculator::polarToRectangular(anchor, Lnew, az);
            // Push undoable move for the chosen vertex
            if (m_undoStack) {
                class MoveVertexCommand : public QUndoCommand {
                public:
                    MoveVertexCommand(CanvasWidget* w, int li, int vi, const QPointF& from, const QPointF& to)
                        : m_w(w), m_li(li), m_vi(vi), m_from(from), m_to(to) { setText("Lengthen"); }
                    void undo() override { if (m_w) m_w->setLineVertex(m_li, m_vi, m_from); }
                    void redo() override { if (m_w) m_w->setLineVertex(m_li, m_vi, m_to); }
                private:
                    CanvasWidget* m_w; int m_li; int m_vi; QPointF m_from; QPointF m_to;
                };
                m_undoStack->push(new MoveVertexCommand(this, li, vi, moving, target));
            } else {
                setLineVertex(li, vi, target);
            }
            m_dynBuffer.clear(); m_dynInputActive=false; m_hasPendingAngle=false;
            update();
            return;
        }
        else if (m_toolMode == ToolMode::DrawCircle || m_toolMode == ToolMode::DrawArc || m_toolMode == ToolMode::DrawRectangle) {
            // Enforce locked layer for draw shapes
            if (m_layerManager) {
                Layer L = m_layerManager->getLayer(m_layerManager->currentLayer());
                if (!L.name.isEmpty() && L.locked) {
                    QToolTip::showText(event->globalPosition().toPoint(), QString("Layer '%1' is locked").arg(L.name), this);
                    return;
                }
            }
            QPointF wp = adjustedWorldFromScreen(event->pos());
            if (m_toolMode == ToolMode::DrawCircle) {
                if (!m_isDrawing) {
                    // Start circle with center; allow press-drag to set radius
                    m_isDrawing = true; m_drawVertices.clear(); m_drawVertices.append(wp); m_orthoAnchor = wp;
                    m_shapeDragActive = (event->buttons() & Qt::LeftButton);
                    m_shapePressScreen = event->pos();
                    update(); return;
                } else {
                    QPointF center = m_drawVertices.first();
                    // If user typed distance in dyn buffer, prefer it as radius
                    double r = SurveyCalculator::distance(center, wp);
                    bool hasDist=false, hasAngle=false; double dist=0.0, ang=0.0;
                    if (m_dynInputEnabled && !m_dynBuffer.isEmpty()) parseDistanceAngleInput(m_dynBuffer, hasDist, dist, hasAngle, ang);
                    if (hasDist) r = qMax(0.0, dist);
                    if (r > 0.0) {
                        const int segs = 64;
                        QVector<QPointF> pts; pts.reserve(segs);
                        for (int i=0;i<segs;++i) {
                            double t = (360.0 * i) / segs;
                            double rad = SurveyCalculator::degreesToRadians(t);
                            pts.append(QPointF(center.x() + r*qSin(rad), center.y() + r*qCos(rad)));
                        }
                        addPolyline(pts, true);
                    }
                    m_isDrawing = false; m_drawVertices.clear(); m_dynBuffer.clear(); m_dynInputActive=false; m_shapeDragActive=false; update(); return;
                }
            } else if (m_toolMode == ToolMode::DrawRectangle) {
                if (!m_isDrawing) {
                    m_isDrawing = true; m_drawVertices.clear(); m_drawVertices.append(wp); m_orthoAnchor = wp;
                    m_shapeDragActive = (event->buttons() & Qt::LeftButton);
                    m_shapePressScreen = event->pos();
                    update(); return;
                }
                QPointF a = m_drawVertices.first();
                QPointF b = wp;
                if (m_orthoMode || (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)) {
                    double dx = b.x() - a.x();
                    double dy = b.y() - a.y();
                    double s = qMax(qAbs(dx), qAbs(dy));
                    double sx = (dx >= 0.0 ? 1.0 : -1.0);
                    double sy = (dy >= 0.0 ? 1.0 : -1.0);
                    b = QPointF(a.x() + sx*s, a.y() + sy*s);
                }
                QVector<QPointF> pts{ QPointF(a.x(), a.y()), QPointF(b.x(), a.y()), QPointF(b.x(), b.y()), QPointF(a.x(), b.y()) };
                addPolyline(pts, true);
                m_isDrawing = false; m_drawVertices.clear(); m_shapeDragActive=false; update(); return;
            } else if (m_toolMode == ToolMode::DrawArc) {
                if (!m_isDrawing) { m_isDrawing = true; m_drawVertices.clear(); m_drawVertices.append(wp); m_orthoAnchor = wp; update(); return; }
                if (m_drawVertices.size() == 1) { m_drawVertices.append(wp); update(); return; }
                // Third click: build arc through three points
                QPointF a = m_drawVertices[0];
                QPointF b = m_drawVertices[1];
                QPointF c = wp;
                QPointF center; double r=0.0;
                auto circum = [&](const QPointF& p1, const QPointF& p2, const QPointF& p3, QPointF& o, double& rad)->bool{
                    double ax=p1.x(), ay=p1.y(), bx=p2.x(), by=p2.y(), cx=p3.x(), cy=p3.y();
                    double d = 2*(ax*(by-cy) + bx*(cy-ay) + cx*(ay-by));
                    if (qFuzzyIsNull(d)) return false;
                    double ux = ((ax*ax+ay*ay)*(by-cy) + (bx*bx+by*by)*(cy-ay) + (cx*cx+cy*cy)*(ay-by)) / d;
                    double uy = ((ax*ax+ay*ay)*(cx-bx) + (bx*bx+by*by)*(ax-cx) + (cx*cx+cy*cy)*(bx-ax)) / d;
                    o = QPointF(ux, uy);
                    rad = SurveyCalculator::distance(o, p1);
                    return true;
                };
                auto angleDeg = [&](const QPointF& ctr, const QPointF& p){
                    double th = qAtan2(p.x()-ctr.x(), p.y()-ctr.y());
                    return SurveyCalculator::normalizeAngle(SurveyCalculator::radiansToDegrees(th));
                };
                QVector<QPointF> pts;
                if (circum(a,b,c,center,r) && r > 0.0) {
                    double aa = angleDeg(center, a);
                    double ab = angleDeg(center, b);
                    double ac = angleDeg(center, c);
                    double ccw = fmod(ac - aa + 360.0, 360.0);
                    double ab_ccw = fmod(ab - aa + 360.0, 360.0);
                    bool useCCW = (ab_ccw <= ccw);
                    double sweep = useCCW ? ccw : (ccw - 360.0);
                    int segs = qMax(8, int(qCeil(qAbs(sweep) / (180.0/16.0))));
                    pts.reserve(segs+1);
                    for (int i=0;i<=segs;++i) {
                        double t = aa + (sweep * (double(i)/segs));
                        double radt = SurveyCalculator::degreesToRadians(t);
                        pts.append(QPointF(center.x() + r*qSin(radt), center.y() + r*qCos(radt)));
                    }
                    addPolyline(pts, false);
                } else {
                    pts = {a, b, c};
                    addPolyline(pts, false);
                }
                m_isDrawing = false; m_drawVertices.clear(); update(); return;
            }
        }
        // Selection interactions
        if (m_toolMode == ToolMode::Select) {
            // Try point first, then line
            int pi = -1; int li = -1;
            bool pointHit = hitTestPoint(event->pos(), pi);
            bool lineHit = !pointHit && hitTestLine(event->pos(), li);
            const bool multi = event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
            // Start drag if clicking on an already selected item (no multi-toggle)
            if (!multi && ( (pointHit && m_selectedPointIndices.contains(pi)) || (lineHit && m_selectedLineIndices.contains(li)) )) {
                m_draggingSelection = true;
                m_dragLastScreen = event->pos();
                m_dragStartScreen = event->pos();
                m_dragCopy = (event->modifiers() & Qt::ControlModifier);
                // Snapshot current positions
                m_preMovePointPos.clear();
                for (int idx : m_selectedPointIndices) {
                    if (idx >=0 && idx < m_points.size()) m_preMovePointPos.append(qMakePair(idx, m_points[idx].point.toQPointF()));
                }
                m_preMoveLinePos.clear();
                for (int idx : m_selectedLineIndices) {
                    if (idx >=0 && idx < m_lines.size()) m_preMoveLinePos.append(LinePos{idx, m_lines[idx].start, m_lines[idx].end});
                }
                // Also include single-selection legacy m_selectedLineIndex if set
                if (m_selectedLineIndex>=0 && m_selectedLineIndex < m_lines.size() && !m_selectedLineIndices.contains(m_selectedLineIndex)) {
                    m_preMoveLinePos.append(LinePos{m_selectedLineIndex, m_lines[m_selectedLineIndex].start, m_lines[m_selectedLineIndex].end});
                }
                return;
            }
            if (pointHit) {
                if (!multi) clearSelection();
                if (multi) toggleSelectionPoint(pi); else setExclusiveSelectionPoint(pi);
                update();
                return;
            }
            if (lineHit) {
                if (!multi) clearSelection();
                if (multi) toggleSelectionLine(li); else setExclusiveSelectionLine(li);
                update();
                return;
            }
            // Start rectangle selection when clicking empty area
            m_selectRectActive = true;
            m_selectRectStart = event->pos();
            m_selectRect.setTopLeft(event->pos());
            m_selectRect.setBottomRight(event->pos());
            return;
        }
        if (m_toolMode == ToolMode::ZoomWindow) {
            m_drawZoomRect = true;
            m_zoomRect.setTopLeft(event->pos());
            m_zoomRect.setBottomRight(event->pos());
            setCursor(Qt::CrossCursor);
            return;
        }
        // Lasso select mode
        if (m_toolMode == ToolMode::LassoSelect) {
            m_lassoActive = true;
            m_lassoPoints.clear();
            m_lassoPoints.append(event->pos());
            m_lassoHover = event->pos();
            m_lassoMulti = event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
            if (!m_lassoMulti) clearSelection();
            update();
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
            // Handle FROM/TK awaiting base: first left-click selects base snap point (do not commit a segment)
            if (m_toolMode == ToolMode::DrawLine && (m_fromAwaitingBase || m_tkAwaitingBase)) {
                bool snapFound=false; QPointF base = objectSnapFromScreen(event->pos(), snapFound);
                QPointF world = snapFound ? base : adjustedWorldFromScreen(event->pos());
                if (m_fromAwaitingBase) { m_fromBaseWorld = world; m_fromAwaitingBase = false; m_fromActive = true; }
                if (m_tkAwaitingBase) { m_tkBaseWorld = world; m_tkAwaitingBase = false; m_tkActive = true; }
                update();
                return;
            }
            QPointF wp = adjustedWorldFromScreen(event->pos());
            // If drawing a line and dynamic input present or pending angle, convert wp to dynamic preview endpoint
            if (m_toolMode == ToolMode::DrawLine && m_isDrawing && (!m_dynBuffer.isEmpty() || m_hasPendingAngle)) {
                bool hasDist=false, hasAngle=false; double dist=0.0, ang=0.0;
                if (m_dynInputEnabled && !m_dynBuffer.isEmpty()) parseDistanceAngleInput(m_dynBuffer, hasDist, dist, hasAngle, ang);
                QPointF last = m_drawVertices.last();
                QPointF baseAnchor = (m_fromActive ? m_fromBaseWorld : (m_tkActive ? m_tkBaseWorld : last));
                double useAng = hasAngle ? ang : (m_hasPendingAngle ? m_pendingAngleDeg : SurveyCalculator::azimuth(baseAnchor, wp));
                double useDist;
                if (hasDist) useDist = dist; else {
                    // Project current mouse vector onto angle if only angle known; else use full distance
                    QPointF v = wp - baseAnchor;
                    if (hasAngle || m_hasPendingAngle) {
                        double azRad = SurveyCalculator::degreesToRadians(useAng);
                        QPointF dir(qSin(azRad), qCos(azRad));
                        useDist = qMax(0.0, v.x()*dir.x() + v.y()*dir.y());
                    } else {
                        useDist = SurveyCalculator::distance(baseAnchor, wp);
                    }
                }
                QPointF candidate = SurveyCalculator::polarToRectangular(baseAnchor, useDist, useAng);
                wp = candidate;
                // If we used typed distance, clear buffer; keep angle pending if (hasAngle || pending)
                if (hasDist) {
                    m_dynBuffer.clear();
                    m_dynInputActive = false;
                    if (hasAngle || m_hasPendingAngle) { m_hasPendingAngle = true; m_pendingAngleDeg = useAng; } else { m_hasPendingAngle = false; }
                }
            }
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
                    // FROM/TK consume once after commit
                    m_fromActive = false;
                    m_tkActive = false;
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
            // Clear dynamic input state and base-point states
            m_dynBuffer.clear();
            m_hasPendingAngle = false;
            m_dynInputActive = false;
            m_fromActive = m_fromAwaitingBase = false;
            m_tkActive = m_tkAwaitingBase = false;
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
        if (m_otrackMode) {
            m_hasTrackOrigin = true;
            m_trackOriginWorld = m_snapIndicatorWorld;
        }
    } else {
        m_hasSnapIndicator = false;
        if (m_otrackMode) m_hasTrackOrigin = false;
    }
    QPointF worldPos = adjustedWorldFromScreen(event->pos());
    emit mouseWorldPosition(worldPos);
    m_currentHoverWorld = worldPos;
    // Live measurement while drawing/dragging (use dynamic input preview if applicable)
    if (m_isDrawing && !m_drawVertices.isEmpty()) {
        QPointF last = m_drawVertices.last();
        QPointF preview = worldPos;
        if (m_toolMode == ToolMode::DrawLine) {
            bool hasDist=false, hasAngle=false; double dist=0.0, ang=0.0;
            if (m_dynInputEnabled && !m_dynBuffer.isEmpty()) parseDistanceAngleInput(m_dynBuffer, hasDist, dist, hasAngle, ang);
            double useAng;
            if (hasAngle) useAng = ang; else if (m_hasPendingAngle) useAng = m_pendingAngleDeg; else useAng = SurveyCalculator::azimuth(last, worldPos);
            if (!hasAngle && !m_hasPendingAngle && m_polarMode && !m_orthoMode) {
                double inc = (m_polarIncrementDeg > 0.0 ? m_polarIncrementDeg : 15.0);
                useAng = qRound(useAng / inc) * inc;
            }
            double useDist;
            if (hasDist) useDist = dist; else {
                QPointF v = worldPos - last;
                if (hasAngle || m_hasPendingAngle) {
                    double azRad = SurveyCalculator::degreesToRadians(useAng);
                    QPointF dir(qSin(azRad), qCos(azRad));
                    useDist = qMax(0.0, v.x()*dir.x() + v.y()*dir.y());
                } else {
                    useDist = SurveyCalculator::distance(last, worldPos);
                }
            }
            preview = SurveyCalculator::polarToRectangular(last, useDist, useAng);
        }
        double distance = SurveyCalculator::distance(last, preview);
        emit drawingDistanceChanged(distance);
    } else if (m_draggingVertex) {
        double dx = worldPos.x() - m_dragOldPos.x();
        double dy = worldPos.y() - m_dragOldPos.y();
        emit drawingDistanceChanged(qSqrt(dx*dx + dy*dy));
    } else {
        emit drawingDistanceChanged(0.0);
    }
    
    if (m_isPanning) {
        QPointF delta = event->pos() - m_lastMousePos; // screen-space delta
        // Because we flip Y in updateTransform() (scale by -m_zoom),
        // we need to invert the Y contribution here so drag direction matches cursor.
        m_offset += QPointF(delta.x() / m_zoom, -delta.y() / m_zoom);
        m_lastMousePos = event->pos();
        updateTransform();
        update();
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

    // Dragging a selection
    if (m_draggingSelection) {
        QPoint prev = m_dragLastScreen;
        QPoint cur = event->pos();
        if (cur != prev) {
            QPointF dw = screenToWorld(cur) - screenToWorld(prev);
            // Move selected points
            for (int idx : m_selectedPointIndices) {
                if (idx >= 0 && idx < m_points.size()) {
                    if (m_layerManager) { Layer L = m_layerManager->getLayer(m_points[idx].layer); if (!L.name.isEmpty() && L.locked) continue; }
                    m_points[idx].point.x += dw.x();
                    m_points[idx].point.y += dw.y();
                }
            }
            // Move selected lines (including legacy single selection)
            QSet<int> lines = m_selectedLineIndices;
            if (m_selectedLineIndex>=0) lines.insert(m_selectedLineIndex);
            for (int idx : lines) {
                if (idx >= 0 && idx < m_lines.size()) {
                    if (m_layerManager) { Layer L = m_layerManager->getLayer(m_lines[idx].layer); if (!L.name.isEmpty() && L.locked) continue; }
                    m_lines[idx].start += dw;
                    m_lines[idx].end += dw;
                }
            }
            m_dragLastScreen = cur;
            update();
        }
        return;
    }

    // Update rectangle selection
    if (m_selectRectActive) {
        m_selectRect.setBottomRight(event->pos());
        update();
        return;
    }
    // Update lasso selection
    if (m_lassoActive) {
        m_lassoHover = event->pos();
        m_lassoPoints.append(event->pos());
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
        // Finalize click-drag circle/rectangle on release
        if (event->button() == Qt::LeftButton && m_shapeDragActive) {
            QPointF wp = adjustedWorldFromScreen(event->pos());
            if (m_toolMode == ToolMode::DrawCircle && m_isDrawing && !m_drawVertices.isEmpty()) {
                QPointF center = m_drawVertices.first();
                double r = SurveyCalculator::distance(center, wp);
                bool hasDist=false, hasAngle=false; double dist=0.0, ang=0.0;
                if (m_dynInputEnabled && !m_dynBuffer.isEmpty()) {
                    // Check D= for diameter first
                    QString t = m_dynBuffer.trimmed();
                    if (t.startsWith('D', Qt::CaseInsensitive)) {
                        t.remove(0,1); if (t.startsWith('=')) t.remove(0,1);
                        bool ok=false; double d = t.toDouble(&ok); if (ok) r = qMax(0.0, d/2.0);
                    } else {
                        parseDistanceAngleInput(m_dynBuffer, hasDist, dist, hasAngle, ang);
                        if (hasDist) r = qMax(0.0, dist);
                    }
                }
                if (r > 0.0) {
                    const int segs = 64;
                    QVector<QPointF> pts; pts.reserve(segs);
                    for (int i=0;i<segs;++i) {
                        double t = (360.0 * i) / segs;
                        double rad = SurveyCalculator::degreesToRadians(t);
                        pts.append(QPointF(center.x() + r*qSin(rad), center.y() + r*qCos(rad)));
                    }
                    addPolyline(pts, true);
                }
                m_isDrawing = false; m_drawVertices.clear(); m_dynBuffer.clear(); m_dynInputActive=false; m_shapeDragActive=false; update();
                return;
            } else if (m_toolMode == ToolMode::DrawRectangle && m_isDrawing && !m_drawVertices.isEmpty()) {
                QPointF a = m_drawVertices.first();
                QPointF b = wp;
                if (m_orthoMode || (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)) {
                    double dx = b.x() - a.x();
                    double dy = b.y() - a.y();
                    double s = qMax(qAbs(dx), qAbs(dy));
                    double sx = (dx >= 0.0 ? 1.0 : -1.0);
                    double sy = (dy >= 0.0 ? 1.0 : -1.0);
                    b = QPointF(a.x() + sx*s, a.y() + sy*s);
                }
                QVector<QPointF> pts{ QPointF(a.x(), a.y()), QPointF(b.x(), a.y()), QPointF(b.x(), b.y()), QPointF(a.x(), b.y()) };
                addPolyline(pts, true);
                m_isDrawing = false; m_drawVertices.clear(); m_shapeDragActive=false; update();
                return;
            }
            // If we got here, we were dragging in some other mode; stop drag flag
            m_shapeDragActive = false;
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
        // Finalize selection rectangle
        if (event->button() == Qt::LeftButton && m_selectRectActive) {
            QRect r = m_selectRect.normalized();
            m_selectRectActive = false;
            if (r.width() > 3 && r.height() > 3) {
                const bool multi = event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
                if (!multi) clearSelection();
                QPointF wTL = screenToWorld(r.topLeft());
                QPointF wBR = screenToWorld(r.bottomRight());
                QRectF wRect = QRectF(wTL, wBR).normalized();
                // Drag direction determines mode: left->right = window (fully inside), right->left = crossing (intersect)
                bool crossing = (event->pos().x() < m_selectRectStart.x());
                // Select points
                for (int i = 0; i < m_points.size(); ++i) {
                    if (m_layerManager) {
                        Layer L = m_layerManager->getLayer(m_points[i].layer);
                        if (!L.name.isEmpty() && !L.visible) continue;
                        if (!L.name.isEmpty() && L.locked) continue; // skip locked
                    }
                    if (wRect.contains(m_points[i].point.toQPointF())) {
                        m_selectedPointIndices.insert(i);
                    }
                }
                // Select lines (window: both endpoints inside; crossing: endpoint inside or intersects rect)
                QPointF a(wRect.left(), wRect.top());
                QPointF b(wRect.right(), wRect.top());
                QPointF c(wRect.right(), wRect.bottom());
                QPointF d(wRect.left(), wRect.bottom());
                for (int i = 0; i < m_lines.size(); ++i) {
                    if (m_layerManager) {
                        Layer L = m_layerManager->getLayer(m_lines[i].layer);
                        if (!L.name.isEmpty() && !L.visible) continue;
                        if (!L.name.isEmpty() && L.locked) continue; // skip locked
                    }
                    bool insideA = wRect.contains(m_lines[i].start);
                    bool insideB = wRect.contains(m_lines[i].end);
                    bool take = false;
                    if (crossing) {
                        take = insideA || insideB
                            || segmentsIntersectWorld(m_lines[i].start, m_lines[i].end, a, b)
                            || segmentsIntersectWorld(m_lines[i].start, m_lines[i].end, b, c)
                            || segmentsIntersectWorld(m_lines[i].start, m_lines[i].end, c, d)
                            || segmentsIntersectWorld(m_lines[i].start, m_lines[i].end, d, a);
                    } else {
                        take = insideA && insideB;
                    }
                    if (take) m_selectedLineIndices.insert(i);
                }
                // Emit selection count
                int lc = m_selectedLineIndices.size() + ((m_selectedLineIndex>=0 && !m_selectedLineIndices.contains(m_selectedLineIndex))?1:0);
                emit selectionChanged(m_selectedPointIndices.size(), lc);
                update();
            } else {
                update();
            }
        }
        // Finalize selection move
        if (event->button() == Qt::LeftButton && m_draggingSelection) {
            m_draggingSelection = false;
            // Build post state
            QVector<QPair<int, QPointF>> postPts;
            for (const auto& pr : m_preMovePointPos) {
                int idx = pr.first;
                if (idx>=0 && idx < m_points.size()) postPts.append(qMakePair(idx, m_points[idx].point.toQPointF()));
            }
            QVector<LinePos> postLines;
            for (const auto& lr : m_preMoveLinePos) {
                int idx = lr.idx;
                if (idx>=0 && idx < m_lines.size()) postLines.append(LinePos{idx, m_lines[idx].start, m_lines[idx].end});
            }
            // If nothing moved, return
            bool anyMoved = false;
            if (postPts.size() != m_preMovePointPos.size() || postLines.size() != m_preMoveLinePos.size()) anyMoved = true;
            for (int i=0;i<m_preMovePointPos.size() && !anyMoved;++i) if (m_preMovePointPos[i].second != postPts[i].second) anyMoved = true;
            for (int i=0;i<m_preMoveLinePos.size() && !anyMoved;++i) if (!(qFuzzyCompare(m_preMoveLinePos[i].a.x(), postLines[i].a.x()) && qFuzzyCompare(m_preMoveLinePos[i].a.y(), postLines[i].a.y()) && qFuzzyCompare(m_preMoveLinePos[i].b.x(), postLines[i].b.x()) && qFuzzyCompare(m_preMoveLinePos[i].b.y(), postLines[i].b.y()))) anyMoved = true;
            if (!anyMoved) { update(); return; }

            class MoveSelectionCommand : public QUndoCommand {
            public:
                MoveSelectionCommand(CanvasWidget* w,
                                     const QVector<QPair<int,QPointF>>& prePts,
                                     const QVector<LinePos>& preLines,
                                     const QVector<QPair<int,QPointF>>& postPts,
                                     const QVector<LinePos>& postLines)
                    : m_w(w), m_prePts(prePts), m_preLines(preLines), m_postPts(postPts), m_postLines(postLines) { setText("Move Selection"); }
                void apply(const QVector<QPair<int,QPointF>>& pts,
                           const QVector<LinePos>& lines) {
                    if (!m_w) return;
                    for (const auto& pr : pts) {
                        int idx = pr.first; if (idx<0 || idx>=m_w->m_points.size()) continue;
                        m_w->m_points[idx].point.x = pr.second.x();
                        m_w->m_points[idx].point.y = pr.second.y();
                    }
                    for (const auto& lr : lines) {
                        int idx = lr.idx; if (idx<0 || idx>=m_w->m_lines.size()) continue;
                        m_w->m_lines[idx].start = lr.a;
                        m_w->m_lines[idx].end = lr.b;
                    }
                    m_w->update();
                }
                void undo() override { apply(m_prePts, m_preLines); }
                void redo() override { apply(m_postPts, m_postLines); }
            private:
                CanvasWidget* m_w;
                QVector<QPair<int,QPointF>> m_prePts;
                QVector<LinePos> m_preLines;
                QVector<QPair<int,QPointF>> m_postPts;
                QVector<LinePos> m_postLines;
            };

            if (m_dragCopy) {
                // Build clones at post positions
                struct ClonePoint { CanvasWidget::DrawnPoint dp; };
                struct CloneLine { CanvasWidget::DrawnLine dl; };
                QVector<ClonePoint> clonesPts;
                QVector<CloneLine> clonesLines;
                auto postPtMap = QMap<int,QPointF>();
                for (const auto& pr : postPts) postPtMap.insert(pr.first, pr.second);
                auto postLineMap = QMap<int, QPair<QPointF,QPointF>>();
                for (const auto& lr : postLines) postLineMap.insert(lr.idx, qMakePair(lr.a, lr.b));
                // Prepare point clones
                for (int idx : m_selectedPointIndices) {
                    if (postPtMap.contains(idx)) {
                        DrawnPoint base = m_points[idx];
                        QPointF pos = postPtMap.value(idx);
                        base.point.x = pos.x();
                        base.point.y = pos.y();
                        base.point.name = base.point.name + "_copy"; // simple suffix
                        clonesPts.append({base});
                    }
                }
                // Prepare line clones
                QSet<int> lines = m_selectedLineIndices;
                if (m_selectedLineIndex>=0) lines.insert(m_selectedLineIndex);
                for (int idx : lines) {
                    if (postLineMap.contains(idx)) {
                        DrawnLine base = m_lines[idx];
                        auto pr = postLineMap.value(idx);
                        base.start = pr.first;
                        base.end = pr.second;
                        clonesLines.append({base});
                    }
                }
                // Restore originals to pre positions (undo preview move)
                for (const auto& pr : m_preMovePointPos) {
                    int idx = pr.first; if (idx<0 || idx>=m_points.size()) continue;
                    m_points[idx].point.x = pr.second.x();
                    m_points[idx].point.y = pr.second.y();
                }
                for (const auto& lr : m_preMoveLinePos) {
                    int idx = lr.idx; if (idx<0 || idx>=m_lines.size()) continue;
                    m_lines[idx].start = lr.a;
                    m_lines[idx].end = lr.b;
                }
                update();

                class CopySelectionCommand : public QUndoCommand {
                public:
                    CopySelectionCommand(CanvasWidget* w,
                                         const QVector<ClonePoint>& pts,
                                         const QVector<CloneLine>& lines)
                        : m_w(w), m_pts(pts), m_lines(lines) { setText("Copy Selection"); }
                    void undo() override {
                        if (!m_w) return;
                        // Remove appended items
                        for (int i=0;i<m_lines.size();++i) {
                            if (!m_w->m_lines.isEmpty()) m_w->m_lines.removeLast();
                        }
                        for (int i=0;i<m_pts.size();++i) {
                            if (!m_w->m_points.isEmpty()) m_w->m_points.removeLast();
                        }
                        m_w->update();
                    }
                    void redo() override {
                        if (!m_w) return;
                        for (const auto& c : m_lines) m_w->m_lines.append(c.dl);
                        for (const auto& c : m_pts) m_w->m_points.append(c.dp);
                        m_w->update();
                    }
                private:
                    CanvasWidget* m_w;
                    QVector<ClonePoint> m_pts;
                    QVector<CloneLine> m_lines;
                };
                if (m_undoStack) m_undoStack->push(new CopySelectionCommand(this, clonesPts, clonesLines));
            } else {
                if (m_undoStack) {
                    m_undoStack->push(new MoveSelectionCommand(this, m_preMovePointPos, m_preMoveLinePos, postPts, postLines));
                }
            }
            update();
        }
        // Finalize lasso selection
        if (event->button() == Qt::LeftButton && m_lassoActive) {
            m_lassoActive = false;
            // Build polygon in world coordinates
            if (m_lassoPoints.size() >= 3) {
                QPolygonF wpoly;
                for (const QPoint& s : m_lassoPoints) wpoly << screenToWorld(s);
                auto pointInPoly = [&](const QPointF& w){ return wpoly.containsPoint(w, Qt::OddEvenFill); };
                // Select points inside
                for (int i=0;i<m_points.size();++i) {
                    if (m_layerManager) { Layer L = m_layerManager->getLayer(m_points[i].layer); if (!L.name.isEmpty() && (!L.visible || L.locked)) continue; }
                    if (pointInPoly(m_points[i].point.toQPointF())) m_selectedPointIndices.insert(i);
                }
                // Select lines if either endpoint inside or any segment intersects polygon edges
                auto polyIntersectSegment = [&](const QPointF& a, const QPointF& b){
                    for (int i=0;i<wpoly.size();++i) {
                        QPointF p1 = wpoly[i];
                        QPointF p2 = wpoly[(i+1)%wpoly.size()];
                        if (segmentsIntersectWorld(a,b,p1,p2)) return true;
                    }
                    return false;
                };
                for (int i=0;i<m_lines.size();++i) {
                    if (m_layerManager) { Layer L = m_layerManager->getLayer(m_lines[i].layer); if (!L.name.isEmpty() && (!L.visible || L.locked)) continue; }
                    bool insideA = pointInPoly(m_lines[i].start);
                    bool insideB = pointInPoly(m_lines[i].end);
                    if (insideA || insideB || polyIntersectSegment(m_lines[i].start, m_lines[i].end)) m_selectedLineIndices.insert(i);
                }
                int lc = m_selectedLineIndices.size() + ((m_selectedLineIndex>=0 && !m_selectedLineIndices.contains(m_selectedLineIndex))?1:0);
                emit selectionChanged(m_selectedPointIndices.size(), lc);
                update();
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
            // Clear dynamic input state
            m_dynBuffer.clear();
            m_hasPendingAngle = false;
            m_dynInputActive = false;
            emit drawingDistanceChanged(0.0);
            update();
        }
        if (hasSelection()) { clearSelection(); update(); emit selectionChanged(0,0); }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (hasSelection()) {
            deleteSelected();
            event->accept();
            return;
        }
    }
    // Dynamic input while drawing lines (AutoCAD-style): distance<angle or <angle or distance, @x,y, FROM/TK
    if (m_toolMode == ToolMode::DrawLine && m_isDrawing && m_dynInputEnabled) {
        int k = event->key();
        // Accept digits, decimal point, sign, angle marker '<', @, comma, and letters (for FROM/TK)
        if ((k >= Qt::Key_0 && k <= Qt::Key_9) || (k >= Qt::Key_A && k <= Qt::Key_Z) || k == Qt::Key_Period || k == Qt::Key_Minus || k == Qt::Key_Plus || k == Qt::Key_Less || k == Qt::Key_At || k == Qt::Key_Comma) {
            m_dynBuffer += event->text();
            m_dynInputActive = true;
            update();
            event->accept();
            return;
        }
        if (k == Qt::Key_Backspace) {
            if (!m_dynBuffer.isEmpty()) m_dynBuffer.chop(1);
            update();
            event->accept();
            return;
        }
        if (k == Qt::Key_Return || k == Qt::Key_Enter) {
            bool hasDist=false, hasAngle=false; double dist=0.0, ang=0.0;
            parseDistanceAngleInput(m_dynBuffer.trimmed(), hasDist, dist, hasAngle, ang);
            QString t = m_dynBuffer.trimmed();
            QString tl = t.toLower();
            // FROM and TK commands
            if (tl == "from") {
                m_fromAwaitingBase = true; m_fromActive = false; m_dynBuffer.clear(); update(); event->accept(); return;
            }
            if (tl == "tk") {
                m_tkAwaitingBase = true; m_tkActive = false; m_dynBuffer.clear(); update(); event->accept(); return;
            }
            // Coordinate entry: @x,y (relative) or x,y (absolute)
            if (t.contains(',')) {
                bool relative = t.startsWith('@');
                if (relative) t = t.mid(1);
                QStringList parts = t.split(',');
                if (parts.size() == 2) {
                    bool ok1=false, ok2=false; double x = parts[0].toDouble(&ok1); double y = parts[1].toDouble(&ok2);
                    if (ok1 && ok2) {
                        QPointF last = m_drawVertices.last();
                        QPointF next;
                        if (m_fromActive) next = QPointF(m_fromBaseWorld.x()+x, m_fromBaseWorld.y()+y);
                        else if (m_tkActive) next = QPointF(m_tkBaseWorld.x()+x, m_tkBaseWorld.y()+y);
                        else next = relative ? QPointF(last.x()+x, last.y()+y) : QPointF(x, y);
                        addLine(last, next);
                        m_drawVertices.append(next);
                        m_orthoAnchor = next;
                        m_dynBuffer.clear(); m_dynInputActive = false; m_hasPendingAngle = false; m_fromActive = false; m_tkActive = false;
                        update(); event->accept(); return;
                    }
                }
            }
            // If only angle specified, store as pending and wait for distance entries
            if (!hasDist && hasAngle) {
                m_hasPendingAngle = true;
                m_pendingAngleDeg = ang;
                m_dynBuffer.clear();
                update();
                event->accept();
                return;
            }
            // If we have a distance (with or without angle), commit a segment
            if (hasDist) {
                QPointF last = m_drawVertices.last();
                QPointF baseAnchor = (m_fromActive ? m_fromBaseWorld : (m_tkActive ? m_tkBaseWorld : last));
                double useAng = hasAngle ? ang : (m_hasPendingAngle ? m_pendingAngleDeg : SurveyCalculator::azimuth(baseAnchor, m_currentHoverWorld));
                if (!hasAngle && m_polarMode && !m_orthoMode) {
                    double inc = (m_polarIncrementDeg > 0.0 ? m_polarIncrementDeg : 15.0);
                    useAng = qRound(useAng / inc) * inc;
                }
                double useDist = dist;
                QPointF next = SurveyCalculator::polarToRectangular(baseAnchor, useDist, useAng);
                addLine(last, next);
                m_drawVertices.append(next);
                m_orthoAnchor = next;
                // Keep angle pending if angle present or previously pending
                if (hasAngle || m_hasPendingAngle) {
                    m_hasPendingAngle = true;
                    m_pendingAngleDeg = useAng;
                }
                m_dynBuffer.clear();
                m_dynInputActive = false;
                m_fromActive = false; m_tkActive = false;
                update();
                event->accept();
                return;
            }
        }
    }
    // Typed constraints during vertex grip edit
    if (m_draggingVertex && m_dynInputEnabled) {
        int k = event->key();
        if ((k >= Qt::Key_0 && k <= Qt::Key_9) || (k >= Qt::Key_A && k <= Qt::Key_Z) || k == Qt::Key_Period || k == Qt::Key_Minus || k == Qt::Key_Plus || k == Qt::Key_Less) {
            m_dynBuffer += event->text();
            m_dynInputActive = true;
            update();
            event->accept();
            return;
        }
        if (k == Qt::Key_Backspace) {
            if (!m_dynBuffer.isEmpty()) m_dynBuffer.chop(1);
            update();
            event->accept();
            return;
        }
        if (k == Qt::Key_Return || k == Qt::Key_Enter) {
            bool hasDist=false, hasAngle=false; double dist=0.0, ang=0.0;
            parseDistanceAngleInput(m_dynBuffer.trimmed(), hasDist, dist, hasAngle, ang);
            if (!hasDist && !hasAngle) { m_dynBuffer.clear(); m_dynInputActive=false; update(); event->accept(); return; }
            // Compute target position from drag anchor
            QPointF anchor = m_dragOldPos;
            double useAng = hasAngle ? ang : SurveyCalculator::azimuth(anchor, m_currentHoverWorld);
            double useDist = 0.0;
            if (hasDist) useDist = dist; else {
                QPointF v = m_currentHoverWorld - anchor;
                double azRad = SurveyCalculator::degreesToRadians(useAng);
                QPointF dir(qSin(azRad), qCos(azRad));
                useDist = qMax(0.0, v.x()*dir.x() + v.y()*dir.y());
            }
            QPointF target = SurveyCalculator::polarToRectangular(anchor, useDist, useAng);
            // Push undo command and finalize
            if (m_undoStack) {
                class MoveVertexCommand : public QUndoCommand {
                public:
                    MoveVertexCommand(CanvasWidget* w, int li, int vi, const QPointF& from, const QPointF& to)
                        : m_w(w), m_li(li), m_vi(vi), m_from(from), m_to(to) { setText("Move Vertex"); }
                    void undo() override { if (m_w) m_w->setLineVertex(m_li, m_vi, m_from); }
                    void redo() override { if (m_w) m_w->setLineVertex(m_li, m_vi, m_to); }
                private:
                    CanvasWidget* m_w; int m_li; int m_vi; QPointF m_from; QPointF m_to;
                };
                m_undoStack->push(new MoveVertexCommand(this, m_dragLineIndex, m_dragVertexIndex, m_dragOldPos, target));
            } else {
                setLineVertex(m_dragLineIndex, m_dragVertexIndex, target);
            }
            m_dynBuffer.clear(); m_dynInputActive = false; m_hasPendingAngle=false;
            m_draggingVertex = false;
            setCursor(Qt::ArrowCursor);
            update();
            event->accept();
            return;
        }
    }
    // Typed input for Lengthen tool
    if (m_toolMode == ToolMode::Lengthen && m_dynInputEnabled) {
        int k = event->key();
        if ((k >= Qt::Key_0 && k <= Qt::Key_9) || k == Qt::Key_Period || k == Qt::Key_Plus || k == Qt::Key_Minus) {
            m_dynBuffer += event->text();
            m_dynInputActive = true;
            update();
            event->accept();
            return;
        }
        if (k == Qt::Key_Backspace) {
            if (!m_dynBuffer.isEmpty()) m_dynBuffer.chop(1);
            update();
            event->accept();
            return;
        }
        if (k == Qt::Key_Return || k == Qt::Key_Enter) {
            int li=-1; if (!hitTestLine(m_currentMousePos, li)) { m_dynBuffer.clear(); update(); event->accept(); return; }
            if (li < 0 || li >= m_lines.size()) { m_dynBuffer.clear(); update(); event->accept(); return; }
            if (m_layerManager) { Layer L = m_layerManager->getLayer(m_lines[li].layer); if (!L.name.isEmpty() && L.locked) { m_dynBuffer.clear(); update(); event->accept(); return; } }
            QPointF a = m_lines[li].start; QPointF b = m_lines[li].end;
            QPoint sa = worldToScreen(toDisplay(a)); QPoint sb = worldToScreen(toDisplay(b));
            int dA = QLineF(sa, m_currentMousePos).length(); int dB = QLineF(sb, m_currentMousePos).length();
            int vi = (dB < dA) ? 1 : 0; QPointF anchor = (vi==0? b:a); QPointF moving = (vi==0? a:b);
            QString t = m_dynBuffer.trimmed(); bool ok=false; double val=t.toDouble(&ok); bool isDelta=(t.startsWith('+')||t.startsWith('-'));
            double Lcur = SurveyCalculator::distance(anchor, moving);
            double Lnew = Lcur;
            if (ok) { if (isDelta) Lnew = qMax(0.0, Lcur+val); else Lnew = qMax(0.0, val); }
            double az = SurveyCalculator::azimuth(anchor, moving);
            QPointF target = SurveyCalculator::polarToRectangular(anchor, Lnew, az);
            if (m_undoStack) {
                class MoveVertexCommand : public QUndoCommand { public: MoveVertexCommand(CanvasWidget* w,int li,int vi,const QPointF& from,const QPointF& to):m_w(w),m_li(li),m_vi(vi),m_from(from),m_to(to){ setText("Lengthen"); } void undo() override { if(m_w)m_w->setLineVertex(m_li,m_vi,m_from);} void redo() override { if(m_w)m_w->setLineVertex(m_li,m_vi,m_to);} CanvasWidget* m_w; int m_li; int m_vi; QPointF m_from; QPointF m_to; };
                m_undoStack->push(new MoveVertexCommand(this, li, vi, moving, target));
            } else {
                setLineVertex(li, vi, target);
            }
            m_dynBuffer.clear(); m_dynInputActive=false; m_hasPendingAngle=false;
            update(); event->accept(); return;
        }
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
    QAction *lassoAct = menu.addAction("Lasso Select"); lassoAct->setCheckable(true); lassoAct->setChecked(m_toolMode == ToolMode::LassoSelect); lassoAct->setActionGroup(group);
    QAction *lengthenAct = menu.addAction("Lengthen"); lengthenAct->setCheckable(true); lengthenAct->setChecked(m_toolMode == ToolMode::Lengthen); lengthenAct->setActionGroup(group);
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

    menu.addSeparator();
    QAction *delSel = menu.addAction("Delete Selected");
    delSel->setEnabled(hasSelection());
    QAction *clearSel = menu.addAction("Clear Selection");
    QAction *selAll = menu.addAction("Select All");
    QAction *invSel = menu.addAction("Invert Selection");
    menu.addSeparator();
    QAction *selByLayer = menu.addAction("Select by Current Layer");
    QAction *hideSelLayers = menu.addAction("Hide Selected Layers");
    QAction *isoSelLayers = menu.addAction("Isolate Selection Layers");
    QAction *lockSelLayers = menu.addAction("Lock Selected Layers");

    QAction *chosen = menu.exec(event->globalPos());
    if (!chosen) return;
    if (chosen == selectAct) setToolMode(ToolMode::Select);
    else if (chosen == panAct) setToolMode(ToolMode::Pan);
    else if (chosen == zoomWinAct) setToolMode(ToolMode::ZoomWindow);
    else if (chosen == drawLineAct) setToolMode(ToolMode::DrawLine);
    else if (chosen == drawPolyAct) setToolMode(ToolMode::DrawPolygon);
    else if (chosen == lassoAct) setToolMode(ToolMode::LassoSelect);
    else if (chosen == lengthenAct) setToolMode(ToolMode::Lengthen);
    else if (chosen == zoomInAct) zoomIn();
    else if (chosen == zoomOutAct) zoomOut();
    else if (chosen == fitAct) fitToWindow();
    else if (chosen == resetAct) resetView();
    else if (chosen == crosshairAct) setShowCrosshair(!m_showCrosshair);
    else if (chosen == orthoAct) setOrthoMode(!m_orthoMode);
    else if (chosen == snapAct) setSnapMode(!m_snapMode);
    else if (chosen == osnapAct) setOsnapMode(!m_osnapMode);
    else if (chosen == delSel) deleteSelected();
    else if (chosen == clearSel) { clearSelection(); update(); emit selectionChanged(0,0); }
    else if (chosen == selAll) selectAllVisible();
    else if (chosen == invSel) invertSelectionVisible();
    else if (chosen == selByLayer) selectByCurrentLayer();
    else if (chosen == hideSelLayers) hideSelectedLayers();
    else if (chosen == isoSelLayers) isolateSelectionLayers();
    else if (chosen == lockSelLayers) lockSelectedLayers();
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
    int pidx = 0;
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
        // Selected: draw outer ring highlight
        if (m_selectedPointIndices.contains(pidx)) {
            painter.save();
            painter.setPen(QPen(QColor(255, 200, 0)));
            painter.setBrush(QBrush(c));
            painter.drawEllipse(pos, 5.0/m_zoom, 5.0/m_zoom);
            painter.restore();
        } else {
            painter.drawEllipse(pos, 4.0/m_zoom, 4.0/m_zoom);
        }
        
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
        const bool sel = (idx == m_selectedLineIndex) || m_selectedLineIndices.contains(idx);
        // Highlight selected line
        QPen linePen(sel ? QColor(255, 200, 0) : c);
        linePen.setWidthF(sel ? qMax(2.0, 2.0 / m_zoom) : 1.0);
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
// Apply ortho first relative to anchor
w = applyOrtho(w);

// Polar tracking: snap angle to increments from anchor when drawing or dragging (unless ORTHO active)
if (!m_orthoMode && m_polarMode && (m_isDrawing || m_draggingVertex || m_draggingSelection)) {
    QPointF anchor = m_orthoAnchor;
    if (m_isDrawing && !m_drawVertices.isEmpty()) anchor = m_drawVertices.last();
    else if (m_draggingVertex) anchor = m_dragOldPos;
    // Compute snapped azimuth and reconstruct point at same radial distance
    double az = SurveyCalculator::azimuth(anchor, w);
    double inc = (m_polarIncrementDeg > 0.0 ? m_polarIncrementDeg : 15.0);
    double snapped = qRound(az / inc) * inc;
    double R = SurveyCalculator::distance(anchor, w);
    w = SurveyCalculator::polarToRectangular(anchor, R, snapped);
}

// Object snap tracking projection (basic): project to nearest guide through track origin if close
if (m_otrackMode && m_hasTrackOrigin) {
    // Screen tolerance in pixels
    const double tolPx = 8.0;
    // Evaluate candidate guides: horizontal, vertical, and polar increments if enabled
    auto projScreenDist = [&](double angleDeg, QPointF& outWorld) -> double {
        // Build direction unit vector in world
        double rad = SurveyCalculator::degreesToRadians(angleDeg);
        QPointF dir(qSin(rad), qCos(rad));
        QPointF a = m_trackOriginWorld;
        QPointF v = w - a;
        double t = v.x()*dir.x() + v.y()*dir.y();
        QPointF p = QPointF(a.x() + t*dir.x(), a.y() + t*dir.y());
        // Distance from cursor to guide in screen space
        QPoint ps = worldToScreen(p);
        QPoint ws = worldToScreen(w);
        double dpx = QLineF(ps, ws).length();
        outWorld = p;
        return dpx;
    };
    double bestD = 1e9; QPointF bestP = w;
    // Horizontal (0°) and Vertical (90°)
    {
        QPointF cand; double d = projScreenDist(0.0, cand); if (d < bestD) { bestD = d; bestP = cand; }
        d = projScreenDist(90.0, cand); if (d < bestD) { bestD = d; bestP = cand; }
    }
    // Polar spokes
    if (m_polarMode) {
        double inc = (m_polarIncrementDeg > 0.0 ? m_polarIncrementDeg : 15.0);
        for (double a = 0.0; a < 180.0; a += inc) {
            QPointF cand; double d = projScreenDist(a, cand); if (d < bestD) { bestD = d; bestP = cand; }
        }
    }
    if (bestD <= tolPx) {
        w = bestP;
    }
}

// Finally snap to grid
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