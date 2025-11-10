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
#include "appsettings.h"
#include <QVariantAnimation>

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
    m_osnapGlyphColor(AppSettings::osnapGlyphColor()),
    m_gridSize(20.0)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(400, 300);
    updateTransform();
}

bool CanvasWidget::setPointXYZ(const QString& name, double x, double y, double z)
{
    QVector<int> idxs; QVector<QPointF> oldXY; QVector<double> oldZ;
    for (int i=0;i<m_points.size();++i) {
        auto& dp = m_points[i];
        if (dp.point.name.compare(name, Qt::CaseInsensitive) == 0) {
            idxs.append(i);
            oldXY.append(dp.point.toQPointF());
            oldZ.append(dp.point.z);
        }
    }
    if (idxs.isEmpty()) return false;
    if (m_undoStack) {
        class SetPointXYZCommand : public QUndoCommand {
        public:
            SetPointXYZCommand(CanvasWidget* w, const QString& name, const QVector<int>& idxs,
                               const QVector<QPointF>& oldXY, const QVector<double>& oldZ,
                               double nx, double ny, double nz)
                : m_w(w), m_name(name), m_idxs(idxs), m_oldXY(oldXY), m_oldZ(oldZ), m_newX(nx), m_newY(ny), m_newZ(nz)
            { setText("Edit Point Coordinates"); }
            void undo() override {
                if (!m_w) return;
                for (int k=0;k<m_idxs.size();++k) {
                    int i = m_idxs[k]; if (i<0 || i>=m_w->m_points.size()) continue;
                    m_w->m_points[i].point.x = m_oldXY[k].x();
                    m_w->m_points[i].point.y = m_oldXY[k].y();
                    m_w->m_points[i].point.z = m_oldZ[k];
                }
                m_w->update();
            }
            void redo() override {
                if (!m_w) return;
                for (int i : m_idxs) {
                    if (i<0 || i>=m_w->m_points.size()) continue;
                    m_w->m_points[i].point.x = m_newX;
                    m_w->m_points[i].point.y = m_newY;
                    m_w->m_points[i].point.z = m_newZ;
                }
                m_w->update();
            }
        private:
            CanvasWidget* m_w{nullptr}; QString m_name; QVector<int> m_idxs; QVector<QPointF> m_oldXY; QVector<double> m_oldZ; double m_newX, m_newY, m_newZ;
        };
        m_undoStack->push(new SetPointXYZCommand(this, name, idxs, oldXY, oldZ, x, y, z));
    } else {
        for (int i : idxs) {
            if (i<0 || i>=m_points.size()) continue;
            m_points[i].point.x = x;
            m_points[i].point.y = y;
            m_points[i].point.z = z;
        }
        update();
    }
    return true;
}

bool CanvasWidget::renamePoint(const QString& oldName, const QString& newName)
{
    if (oldName.compare(newName, Qt::CaseInsensitive) == 0) return true;
    QVector<int> idxs;
    for (int i=0;i<m_points.size();++i) {
        if (m_points[i].point.name.compare(oldName, Qt::CaseInsensitive) == 0) idxs.append(i);
    }
    if (idxs.isEmpty()) return false;
    if (m_undoStack) {
        class RenamePointCommand : public QUndoCommand {
        public:
            RenamePointCommand(CanvasWidget* w, const QVector<int>& idxs, const QString& oldN, const QString& newN)
                : m_w(w), m_idxs(idxs), m_old(oldN), m_new(newN) { setText("Rename Point"); }
            void undo() override { if (!m_w) return; for (int i: m_idxs){ if (i>=0 && i<m_w->m_points.size()) m_w->m_points[i].point.name = m_old; } m_w->update(); }
            void redo() override { if (!m_w) return; for (int i: m_idxs){ if (i>=0 && i<m_w->m_points.size()) m_w->m_points[i].point.name = m_new; } m_w->update(); }
        private: CanvasWidget* m_w{nullptr}; QVector<int> m_idxs; QString m_old, m_new; };
        m_undoStack->push(new RenamePointCommand(this, idxs, oldName, newName));
    } else {
        for (int i: idxs) { if (i>=0 && i<m_points.size()) m_points[i].point.name = newName; }
        update();
    }
    return true;
}
int CanvasWidget::polylineIndexForLine(int lineIndex) const
{
    if (lineIndex < 0 || lineIndex >= m_lines.size()) return -1;
    for (int i = 0; i < m_polylines.size(); ++i) {
        const auto& pl = m_polylines[i];
        for (int li : pl.lineIndices) { if (li == lineIndex) return i; }
    }
    return -1;
}

QVector<int> CanvasWidget::currentLineIndicesForPolyline(int polyIndex) const
{
    QVector<int> res;
    if (polyIndex < 0 || polyIndex >= m_polylines.size()) return res;
    const auto& pl = m_polylines[polyIndex];
    for (int li : pl.lineIndices) { if (li >= 0 && li < m_lines.size()) res.append(li); }
    return res;
}

void CanvasWidget::addDimension(const QPointF& a, const QPointF& b, double textHeight, const QString& layer)
{
    QString lay = !layer.isEmpty() ? layer : (m_layerManager ? m_layerManager->currentLayer() : QStringLiteral("0"));
    DrawnDim dd{ a, b, qMax(0.0, textHeight), lay, Qt::white };
    if (m_undoStack) {
        class AddDimCommand : public QUndoCommand {
        public:
            AddDimCommand(CanvasWidget* w, const DrawnDim& dd) : m_w(w), m_dd(dd) { setText("Add Dimension"); }
            void undo() override {
                if (!m_w) return;
                for (int i = m_w->m_dims.size()-1; i >= 0; --i) {
                    const auto& D = m_w->m_dims[i];
                    if (qFuzzyCompare(D.a.x(), m_dd.a.x()) && qFuzzyCompare(D.a.y(), m_dd.a.y()) &&
                        qFuzzyCompare(D.b.x(), m_dd.b.x()) && qFuzzyCompare(D.b.y(), m_dd.b.y()) &&
                        qFuzzyCompare(D.textHeight, m_dd.textHeight) && D.layer == m_dd.layer) {
                        m_w->m_dims.remove(i);
                        break;
                    }
                }
                m_w->update();
            }
            void redo() override { if (m_w) { m_w->m_dims.append(m_dd); m_w->update(); } }
        private: CanvasWidget* m_w{nullptr}; DrawnDim m_dd; };
        m_undoStack->push(new AddDimCommand(this, dd));
    } else {
        m_dims.append(dd);
        update();
    }
}

int CanvasWidget::dimensionCount() const { return m_dims.size(); }

bool CanvasWidget::dimensionData(int index, QPointF& aOut, QPointF& bOut, double& textHeightOut, QString& layerOut) const
{
    if (index < 0 || index >= m_dims.size()) return false;
    const auto& d = m_dims[index];
    aOut = d.a; bOut = d.b; textHeightOut = d.textHeight; layerOut = d.layer; return true;
}

void CanvasWidget::drawTexts(QPainter& painter)
{
    painter.save();
    for (const auto& dt : m_texts) {
        if (m_layerManager) {
            Layer L = m_layerManager->getLayer(dt.layer);
            if (!L.name.isEmpty() && !L.visible) continue;
        }
        QPoint screen = worldToScreen(toDisplay(dt.pos));
        painter.resetTransform();
        painter.translate(screen);
        painter.rotate(-dt.angleDeg);
        QFont f = painter.font();
        int px = qMax(1, (int)qRound(dt.height * m_zoom));
        f.setPixelSize(px);
        painter.setFont(f);
        QColor c = dt.color;
        if (m_layerManager) {
            Layer L = m_layerManager->getLayer(dt.layer);
            if (!L.name.isEmpty()) c = L.color;
        }
        painter.setPen(QPen(c));
        painter.drawText(QPoint(0, 0), dt.text);
        painter.setTransform(m_worldToScreen);
    }
    painter.restore();
}

void CanvasWidget::drawDimensions(QPainter& painter)
{
    painter.save();
    for (const auto& d : m_dims) {
        if (m_layerManager) {
            Layer L = m_layerManager->getLayer(d.layer);
            if (!L.name.isEmpty() && !L.visible) continue;
        }
        QColor c = d.color;
        if (m_layerManager) {
            Layer L = m_layerManager->getLayer(d.layer);
            if (!L.name.isEmpty()) c = L.color;
        }
        // Geometry
        QPointF a = d.a, b = d.b;
        double len = SurveyCalculator::distance(a, b);
        if (len <= 1e-9) continue;
        QPointF dir((b.x()-a.x())/len, (b.y()-a.y())/len);
        QPointF n(-dir.y(), dir.x());
        double off = qMax(0.0, d.textHeight);
        QPointF aOff(a.x() + n.x()*off, a.y() + n.y()*off);
        QPointF bOff(b.x() + n.x()*off, b.y() + n.y()*off);
        // Pen
        QPen pen(c);
        painter.setPen(pen);
        // Extension lines (from endpoints up to offset line)
        painter.drawLine(toDisplay(a), toDisplay(aOff));
        painter.drawLine(toDisplay(b), toDisplay(bOff));
        // Dimension line at offset
        painter.drawLine(toDisplay(aOff), toDisplay(bOff));
        // Arrowheads
        auto drawArrow = [&](const QPointF& tip, const QPointF& inwardDir){
            double L = d.textHeight * 0.8;
            QPointF wing = QPointF(-inwardDir.y(), inwardDir.x());
            QPointF p1(tip.x() - inwardDir.x()*L + wing.x()*(L*0.4), tip.y() - inwardDir.y()*L + wing.y()*(L*0.4));
            QPointF p2(tip.x() - inwardDir.x()*L - wing.x()*(L*0.4), tip.y() - inwardDir.y()*L - wing.y()*(L*0.4));
            painter.drawLine(toDisplay(tip), toDisplay(p1));
            painter.drawLine(toDisplay(tip), toDisplay(p2));
        };
        drawArrow(aOff, dir);   // arrow pointing towards inside (+dir)
        drawArrow(bOff, QPointF(-dir.x(), -dir.y())); // arrow pointing towards inside (-dir)
        // Text: length, rotated along dimension, positioned at offset midpoint with slight normal bump
        QPointF mid((aOff.x()+bOff.x())*0.5, (aOff.y()+bOff.y())*0.5);
        QString label = QString::number(len, 'f', 3);
        QPoint screen = worldToScreen(toDisplay(mid));
        painter.resetTransform();
        QFont f = painter.font();
        int px = qMax(1, (int)qRound(d.textHeight * m_zoom));
        f.setPixelSize(px);
        painter.setFont(f);
        painter.setPen(QPen(c));
        double angDeg = qRadiansToDegrees(qAtan2(dir.y(), dir.x()));
        painter.translate(screen);
        painter.rotate(-angDeg);
        painter.drawText(QPoint(0, -px/2), label);
        painter.setTransform(m_worldToScreen);
    }
    painter.restore();
}

void CanvasWidget::addText(const QString& text, const QPointF& pos, double height, double angle, const QString& layer)
{
    QString lay = !layer.isEmpty() ? layer : (m_layerManager ? m_layerManager->currentLayer() : QStringLiteral("0"));
    DrawnText dt{ text, pos, qMax(0.0, height), angle, lay, Qt::white };
    if (m_undoStack) {
        class AddTextCommand : public QUndoCommand {
        public:
            AddTextCommand(CanvasWidget* w, const DrawnText& dt)
                : m_w(w), m_dt(dt) { setText("Add Text"); }
            void undo() override {
                if (!m_w) return;
                for (int i = m_w->m_texts.size()-1; i >= 0; --i) {
                    const auto& T = m_w->m_texts[i];
                    if (T.text == m_dt.text && qFuzzyCompare(T.pos.x(), m_dt.pos.x()) && qFuzzyCompare(T.pos.y(), m_dt.pos.y()) &&
                        qFuzzyCompare(T.height, m_dt.height) && qFuzzyCompare(T.angleDeg, m_dt.angleDeg) && T.layer == m_dt.layer) {
                        m_w->m_texts.remove(i);
                        break;
                    }
                }
                m_w->update();
            }
            void redo() override {
                if (!m_w) return; m_w->m_texts.append(m_dt); m_w->update();
            }
        private:
            CanvasWidget* m_w{nullptr}; DrawnText m_dt;
        };
        m_undoStack->push(new AddTextCommand(this, dt));
    } else {
        m_texts.append(dt);
        update();
    }
}

int CanvasWidget::textCount() const { return m_texts.size(); }

bool CanvasWidget::textData(int index, QString& textOut, QPointF& posOut, double& heightOut, double& angleOut, QString& layerOut) const
{
    if (index < 0 || index >= m_texts.size()) return false;
    const auto& dt = m_texts[index];
    textOut = dt.text; posOut = dt.pos; heightOut = dt.height; angleOut = dt.angleDeg; layerOut = dt.layer; return true;
}

QString CanvasWidget::textLayer(int index) const
{
    if (index < 0 || index >= m_texts.size()) return QString();
    return m_texts[index].layer;
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
    if (idx >= 0 && idx < m_lines.size()) {
        int pli = polylineIndexForLine(idx);
        if (pli >= 0) {
            QVector<int> segs = currentLineIndicesForPolyline(pli);
            for (int li : segs) m_selectedLineIndices.insert(li);
        } else {
            m_selectedLineIndices.insert(idx);
        }
    }
    int lc = m_selectedLineIndices.size() + ((m_selectedLineIndex>=0 && !m_selectedLineIndices.contains(m_selectedLineIndex))?1:0);
    emit selectionChanged(m_selectedPointIndices.size(), lc);
}

void CanvasWidget::toggleSelectionLine(int idx)
{
    if (idx < 0 || idx >= m_lines.size()) return;
    if (m_layerManager) { Layer L = m_layerManager->getLayer(m_lines[idx].layer); if (!L.name.isEmpty() && L.locked) return; }
    int pli = polylineIndexForLine(idx);
    if (pli >= 0) {
        QVector<int> segs = currentLineIndicesForPolyline(pli);
        bool allPresent = true;
        for (int li : segs) { if (!m_selectedLineIndices.contains(li)) { allPresent = false; break; } }
        if (allPresent) {
            for (int li : segs) m_selectedLineIndices.remove(li);
        } else {
            for (int li : segs) m_selectedLineIndices.insert(li);
        }
    } else {
        if (m_selectedLineIndices.contains(idx)) m_selectedLineIndices.remove(idx);
        else m_selectedLineIndices.insert(idx);
    }
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
        // Lines (expand to full polyline entities)
        QSet<int> toCapture;
        {
            QSet<int> sel = m_w->m_selectedLineIndices;
            if (m_w->m_selectedLineIndex >= 0) sel.insert(m_w->m_selectedLineIndex);
            for (int idx : sel) {
                if (idx < 0 || idx >= m_w->m_lines.size()) continue;
                int pli = m_w->polylineIndexForLine(idx);
                if (pli >= 0) {
                    const auto segs = m_w->currentLineIndicesForPolyline(pli);
                    for (int li : segs) toCapture.insert(li);
                } else {
                    toCapture.insert(idx);
                }
            }
        }
        // Cache affected polylines to restore on undo
        {
            QSet<int> affectedPl;
            for (int pi = 0; pi < m_w->m_polylines.size(); ++pi) {
                const auto& pl = m_w->m_polylines[pi];
                bool hit = false;
                for (int li : pl.lineIndices) { if (toCapture.contains(li)) { hit = true; break; } }
                if (hit) affectedPl.insert(pi);
            }
            QList<int> plIdxs = affectedPl.values();
            std::sort(plIdxs.begin(), plIdxs.end());
            for (int pi : plIdxs) m_polylines.append(m_w->m_polylines[pi]);
        }
        QList<int> lineIdxs = toCapture.values();
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
        if (m_w) {
            m_w->applyRestoreSelection(m_points, m_lines);
            // Restore polyline entities
            for (const auto& pl : m_polylines) m_w->m_polylines.append(pl);
            m_w->update();
        }
    }
    void redo() override {
        if (m_w) m_w->applyDeleteSelection();
    }
private:
    CanvasWidget* m_w{nullptr};
    QVector<QPair<int, CanvasWidget::DrawnPoint>> m_points;
    QVector<QPair<int, CanvasWidget::DrawnLine>> m_lines;
    QVector<CanvasWidget::DrawnPolyline> m_polylines;
};

void CanvasWidget::applyDeleteSelection()
{
    // Aggregate full-entity (polyline) line indices to remove
    QSet<int> toRemove;
    {
        QList<int> base = m_selectedLineIndices.values();
        if (m_selectedLineIndex >= 0) base.append(m_selectedLineIndex);
        for (int idx : base) {
            if (idx < 0 || idx >= m_lines.size()) continue;
            int pli = polylineIndexForLine(idx);
            if (pli >= 0) {
                QVector<int> segs = currentLineIndicesForPolyline(pli);
                for (int li : segs) toRemove.insert(li);
            } else {
                toRemove.insert(idx);
            }
        }
    }
    // Respect layer lock per line
    QList<int> lineIdxs = toRemove.values();
    std::sort(lineIdxs.begin(), lineIdxs.end(), std::greater<int>());
    // Remove corresponding polylines fully if any of their segments are removed
    if (!lineIdxs.isEmpty()) {
        QVector<int> removePl;
        for (int pi = 0; pi < m_polylines.size(); ++pi) {
            const auto& pl = m_polylines[pi];
            bool hit = false;
            for (int li : pl.lineIndices) { if (toRemove.contains(li)) { hit = true; break; } }
            if (hit) removePl.append(pi);
        }
        std::sort(removePl.begin(), removePl.end(), std::greater<int>());
        for (int pi : removePl) { if (pi>=0 && pi < m_polylines.size()) m_polylines.remove(pi); }
    }
    for (int idx : lineIdxs) {
        if (idx >= 0 && idx < m_lines.size()) {
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
    // Remove from drawn points only; geometry lines unaffected (undoable)
    QVector<QPair<int, DrawnPoint>> removed;
    for (int i = 0; i < m_points.size(); ++i) {
        if (m_points[i].point.name.compare(name, Qt::CaseInsensitive) == 0) {
            removed.append(qMakePair(i, m_points[i]));
        }
    }
    if (removed.isEmpty()) return;
    if (m_undoStack) {
        class RemovePointsByNameCommand : public QUndoCommand {
        public:
            RemovePointsByNameCommand(CanvasWidget* w, const QVector<QPair<int, DrawnPoint>>& rem)
                : m_w(w), m_removed(rem) { setText("Delete Point(s)"); }
            void undo() override {
                if (!m_w) return;
                QVector<QPair<int, DrawnPoint>> sorted = m_removed;
                std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b){ return a.first < b.first; });
                for (const auto& pr : sorted) {
                    int idx = pr.first;
                    if (idx < 0 || idx > m_w->m_points.size()) m_w->m_points.append(pr.second);
                    else m_w->m_points.insert(idx, pr.second);
                }
                m_w->update();
            }
            void redo() override {
                if (!m_w) return;
                QVector<QPair<int, DrawnPoint>> sorted = m_removed;
                std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b){ return a.first > b.first; });
                for (const auto& pr : sorted) {
                    if (pr.first >= 0 && pr.first < m_w->m_points.size()) m_w->m_points.remove(pr.first);
                }
                m_w->update();
            }
        private:
            CanvasWidget* m_w{nullptr};
            QVector<QPair<int, DrawnPoint>> m_removed;
        };
        m_undoStack->push(new RemovePointsByNameCommand(this, removed));
    } else {
        for (int i = m_points.size() - 1; i >= 0; --i) {
            if (m_points[i].point.name.compare(name, Qt::CaseInsensitive) == 0) {
                m_points.remove(i);
            }
        }
        update();
    }
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
    QVector<int> idxs; QVector<QString> oldLayers;
    for (int i=0;i<m_points.size();++i) {
        auto& dp = m_points[i];
        if (dp.point.name.compare(name, Qt::CaseInsensitive) == 0 && dp.layer != layer) {
            idxs.append(i); oldLayers.append(dp.layer);
        }
    }
    if (idxs.isEmpty()) return false;
    if (m_undoStack) {
        class SetPointLayerCommand : public QUndoCommand {
        public:
            SetPointLayerCommand(CanvasWidget* w, const QVector<int>& idxs, const QVector<QString>& oldL, const QString& newL)
                : m_w(w), m_idxs(idxs), m_old(oldL), m_new(newL) { setText("Set Point Layer"); }
            void undo() override { for (int k=0;k<m_idxs.size();++k){ int i=m_idxs[k]; if (i>=0 && i<m_w->m_points.size()) m_w->m_points[i].layer = m_old[k]; } m_w->update(); }
            void redo() override { for (int k=0;k<m_idxs.size();++k){ int i=m_idxs[k]; if (i>=0 && i<m_w->m_points.size()) m_w->m_points[i].layer = m_new; } m_w->update(); }
        private:
            CanvasWidget* m_w; QVector<int> m_idxs; QVector<QString> m_old; QString m_new;
        };
        m_undoStack->push(new SetPointLayerCommand(this, idxs, oldLayers, layer));
    } else {
        for (int i : idxs) m_points[i].layer = layer; update();
    }
    return true;
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
    DrawnPoint dp{point, m_pointColor, layer};
    if (m_undoStack) {
        class AddPointCommand : public QUndoCommand {
        public:
            AddPointCommand(CanvasWidget* w, const DrawnPoint& dp)
                : m_w(w), m_dp(dp) { setText("Add Point"); }
            void undo() override {
                if (!m_w) return;
                // remove last matching name
                for (int i=m_w->m_points.size()-1;i>=0;--i){ if (m_w->m_points[i].point.name == m_dp.point.name) { m_w->m_points.remove(i); break; } }
                m_w->update();
            }
            void redo() override {
                if (!m_w) return; m_w->m_points.append(m_dp); m_w->update(); }
        private:
            CanvasWidget* m_w{nullptr}; DrawnPoint m_dp;
        };
        m_undoStack->push(new AddPointCommand(this, dp));
    } else {
        m_points.append(dp);
        update();
    }
}

void CanvasWidget::addLine(const QPointF& start, const QPointF& end)
{
    QString layer = m_layerManager ? m_layerManager->currentLayer() : QStringLiteral("0");
    DrawnLine dl{start, end, m_lineColor, layer, m_currentPenWidth, m_currentPenStyle};
    if (m_undoStack) {
        class AddLineCommand : public QUndoCommand {
        public:
            explicit AddLineCommand(CanvasWidget* w, const DrawnLine& dl)
                : m_w(w), m_dl(dl) { setText("Add Line"); }
            void undo() override {
                if (!m_w) return;
                // remove the last matching line
                for (int i = m_w->m_lines.size()-1; i >= 0; --i) {
                    const auto& L = m_w->m_lines[i];
                    if (qFuzzyCompare(L.start.x(), m_dl.start.x()) && qFuzzyCompare(L.start.y(), m_dl.start.y()) &&
                        qFuzzyCompare(L.end.x(), m_dl.end.x()) && qFuzzyCompare(L.end.y(), m_dl.end.y()) &&
                        L.layer == m_dl.layer) {
                        m_w->m_lines.remove(i);
                        break;
                    }
                }
                m_w->update();
            }
            void redo() override {
                if (!m_w) return;
                m_w->m_lines.append(m_dl);
                m_w->update();
            }
        private:
            CanvasWidget* m_w{nullptr};
            DrawnLine m_dl;
        };
        m_undoStack->push(new AddLineCommand(this, dl));
    } else {
        m_lines.append(dl);
        update();
    }
}

void CanvasWidget::addPolyline(const QVector<QPointF>& pts, bool closed)
{
    if (pts.size() < 2) return;
    QString layer = m_layerManager ? m_layerManager->currentLayer() : QStringLiteral("0");
    QVector<DrawnLine> segs;
for (int i = 1; i < pts.size(); ++i) segs.append(DrawnLine{pts[i-1], pts[i], m_lineColor, layer, m_currentPenWidth, m_currentPenStyle});
    if (closed) segs.append(DrawnLine{pts.back(), pts.front(), m_lineColor, layer, m_currentPenWidth, m_currentPenStyle});
    if (m_undoStack) {
        class AddPolylineCommand : public QUndoCommand {
        public:
            AddPolylineCommand(CanvasWidget* w, const QVector<DrawnLine>& segs) : m_w(w), m_segs(segs) { setText("Add Polyline"); }
            void undo() override {
                if (!m_w) return;
                // remove matching segments from the back
                int toRemove = m_segs.size();
                for (int i = m_w->m_lines.size()-1; i >= 0 && toRemove > 0; --i) {
                    const auto& L = m_w->m_lines[i];
                    // try match any remaining seg (order not guaranteed if interleaved). Remove if equal and decrement
                    for (int k=0;k<m_segs.size();++k){ const auto& S=m_segs[k]; if (qFuzzyCompare(L.start.x(), S.start.x()) && qFuzzyCompare(L.start.y(), S.start.y()) && qFuzzyCompare(L.end.x(), S.end.x()) && qFuzzyCompare(L.end.y(), S.end.y()) && L.layer==S.layer){ m_w->m_lines.remove(i); --toRemove; break; } }
                }
                m_w->update();
            }
            void redo() override {
                if (!m_w) return;
                for (const auto& s : m_segs) m_w->m_lines.append(s);
                m_w->update();
            }
        private:
            CanvasWidget* m_w{nullptr};
            QVector<DrawnLine> m_segs;
        };
        m_undoStack->push(new AddPolylineCommand(this, segs));
    } else {
        for (const auto& s : segs) m_lines.append(s);
        update();
    }
}

void CanvasWidget::addPolylineEntity(const QVector<QPointF>& pts, bool closed, const QString& layer)
{
    if (pts.size() < 2) return;
    QString lay = !layer.isEmpty() ? layer : (m_layerManager ? m_layerManager->currentLayer() : QStringLiteral("0"));
    DrawnPolyline pl; pl.pts = pts; pl.closed = closed; pl.layer = lay; pl.color = m_lineColor;
    // Build segments and record indices
    QVector<DrawnLine> segs;
for (int i=1;i<pts.size();++i) segs.append(DrawnLine{pts[i-1], pts[i], m_lineColor, lay, m_currentPenWidth, m_currentPenStyle});
    if (closed) segs.append(DrawnLine{pts.back(), pts.front(), m_lineColor, lay, m_currentPenWidth, m_currentPenStyle});
    if (m_undoStack) {
        class AddPolylineEntityCommand : public QUndoCommand {
        public:
            AddPolylineEntityCommand(CanvasWidget* w, const DrawnPolyline& pl, const QVector<DrawnLine>& segs)
                : m_w(w), m_pl(pl), m_segs(segs) { setText("Add Polyline"); }
            void undo() override {
                if (!m_w) return;
                // remove lines we added
                for (int i=m_w->m_lines.size()-1;i>=0;--i){ if (m_added.contains(i)) m_w->m_lines.remove(i); }
                // remove the polyline record
                if (!m_plIndexRemoved) {
                    for (int i=m_w->m_polylines.size()-1;i>=0;--i){ if (&(m_w->m_polylines[i]) == m_plPtr){ m_w->m_polylines.remove(i); break; } }
                }
                m_w->update();
            }
            void redo() override {
                if (!m_w) return;
                // append polyline and remember address to find later on undo
                m_w->m_polylines.append(m_pl);
                m_plPtr = &m_w->m_polylines.back();
                // append segments and record indices
                m_added.clear(); m_plPtr->lineIndices.clear();
                int base = m_w->m_lines.size();
                for (const auto& s : m_segs) { m_w->m_lines.append(s); m_added.insert(base); m_plPtr->lineIndices.append(base); ++base; }
                m_w->update();
            }
        private:
            CanvasWidget* m_w{nullptr};
            DrawnPolyline m_pl;
            QVector<DrawnLine> m_segs;
            QSet<int> m_added; // indices
            DrawnPolyline* m_plPtr{nullptr};
            bool m_plIndexRemoved{false};
        };
        m_undoStack->push(new AddPolylineEntityCommand(this, pl, segs));
    } else {
        m_polylines.append(pl);
        for (const auto& s : segs) { m_polylines.back().lineIndices.append(m_lines.size()); m_lines.append(s); }
        update();
    }
}

int CanvasWidget::polylineCount() const { return m_polylines.size(); }

bool CanvasWidget::polylineData(int index, QVector<QPointF>& ptsOut, bool& closedOut, QString& layerOut) const
{
    if (index < 0 || index >= m_polylines.size()) return false;
    const auto& pl = m_polylines[index]; ptsOut = pl.pts; closedOut = pl.closed; layerOut = pl.layer; return true;
}

void CanvasWidget::linesUsedByPolylines(QSet<int>& out) const
{
    for (const auto& pl : m_polylines) { for (int idx : pl.lineIndices) out.insert(idx); }
}

void CanvasWidget::startRegularPolygonByEdge(int sides)
{
    if (sides < 3) sides = 3;
    m_regPolySides = sides;
    m_regPolyEdgeActive = true;
    m_regPolyHasFirst = false;
    m_toolMode = ToolMode::DrawRegularPolygonEdge;
    update();
}

void CanvasWidget::startTrim() { m_toolMode = ToolMode::Trim; update(); }
void CanvasWidget::startExtend() { m_toolMode = ToolMode::Extend; update(); }
void CanvasWidget::startOffset(double distance) { m_toolMode = ToolMode::OffsetLine; m_offsetActive = true; m_offsetDistance = qAbs(distance); update(); }
void CanvasWidget::startFilletZero() { m_toolMode = ToolMode::FilletZero; m_modHasFirst = false; update(); }
void CanvasWidget::startChamfer(double distance) { m_toolMode = ToolMode::Chamfer; m_modHasFirst = false; m_chamferDistance = qAbs(distance); update(); }

QVector<QPointF> CanvasWidget::makeRegularPolygonFromEdge(const QPointF& a, const QPointF& b, int sides) const
{
    QVector<QPointF> out;
    if (sides < 3) return out;
    QPointF ab = b - a;
    double s = SurveyCalculator::distance(a, b);
    if (s <= 0.0) return out;
    QPointF mid((a.x()+b.x())*0.5, (a.y()+b.y())*0.5);
    // Unit left normal of AB
    double nx = -ab.y();
    double ny = ab.x();
    double nlen = qSqrt(nx*nx + ny*ny);
    if (nlen <= 1e-9) return out;
    nx /= nlen; ny /= nlen;
    // Distance from midpoint to center
    double pi = M_PI;
    double h = s / (2.0 * qTan(pi / sides));
    QPointF center(mid.x() + nx*h, mid.y() + ny*h);
    // Radius and start angle (consistent with earlier sin/cos usage)
    double r = s / (2.0 * qSin(pi / sides));
    auto angleDeg = [&](const QPointF& ctr, const QPointF& p){
        double th = qAtan2(p.x()-ctr.x(), p.y()-ctr.y());
        return SurveyCalculator::normalizeAngle(SurveyCalculator::radiansToDegrees(th));
    };
    double a0 = SurveyCalculator::degreesToRadians(angleDeg(center, a));
    double step = 2.0 * pi / sides;
    out.reserve(sides);
    for (int k=0; k<sides; ++k) {
        double t = a0 + k * step;
        QPointF v(center.x() + r*qSin(t), center.y() + r*qCos(t));
        out.append(v);
    }
    return out;
}

void CanvasWidget::clearAll()
{
    m_points.clear();
    m_lines.clear();
    m_texts.clear();
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

void CanvasWidget::animateZoomTo(double targetZoom, const QPointF& targetOffset, int durationMs)
{
    if (durationMs <= 0) { m_zoom = targetZoom; m_offset = targetOffset; updateTransform(); update(); emit zoomChanged(m_zoom); return; }
    if (m_zoomAnimation) { m_zoomAnimation->stop(); m_zoomAnimation->deleteLater(); m_zoomAnimation = nullptr; }
    m_animStartZoom = m_zoom;
    m_animEndZoom = targetZoom;
    m_animStartOffset = m_offset;
    m_animEndOffset = targetOffset;
    auto* anim = new QVariantAnimation(this);
    m_zoomAnimation = anim;
    anim->setDuration(durationMs);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v){
        double t = v.toDouble();
        m_zoom = m_animStartZoom + (m_animEndZoom - m_animStartZoom) * t;
        m_offset = QPointF(
            m_animStartOffset.x() + (m_animEndOffset.x() - m_animStartOffset.x()) * t,
            m_animStartOffset.y() + (m_animEndOffset.y() - m_animStartOffset.y()) * t
        );
        updateTransform();
        update();
        emit zoomChanged(m_zoom);
    });
    connect(anim, &QVariantAnimation::finished, this, [this, anim](){
        if (m_zoomAnimation == anim) m_zoomAnimation = nullptr;
        anim->deleteLater();
    });
    anim->start();
}

void CanvasWidget::zoomInAnimated()
{
    const double z = m_zoom * 1.2;
    animateZoomTo(z, m_offset, 160);
}

void CanvasWidget::zoomOutAnimated()
{
    const double z = m_zoom / 1.2;
    animateZoomTo(z, m_offset, 160);
}

void CanvasWidget::fitToWindowAnimated()
{
    if (m_points.isEmpty()) return;
    QRectF bounds;
    for (const auto& dp : m_points) {
        bounds = bounds.isNull() ? QRectF(dp.point.toQPointF(), QSizeF(0,0))
                                 : bounds.united(QRectF(dp.point.toQPointF(), QSizeF(0,0)));
    }
    if (!bounds.isNull() && bounds.width() > 0 && bounds.height() > 0) {
        double scaleX = width() / (bounds.width() + 100);
        double scaleY = height() / (bounds.height() + 100);
        double targetZ = qMin(scaleX, scaleY);
        QPointF targetOff = -bounds.center();
        animateZoomTo(targetZ, targetOff, 220);
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

    // Draw texts
    drawTexts(painter);

    // Draw dimensions
    drawDimensions(painter);

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
        } else if (m_toolMode == ToolMode::DrawRectangle && !m_drawVertices.isEmpty()) {
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

    // Regular polygon (by edge) preview: shown after first edge click, even if not using m_isDrawing
    if (m_toolMode == ToolMode::DrawRegularPolygonEdge && m_regPolyHasFirst) {
        painter.save();
        QPen prevPen(QColor(0, 200, 255));
        prevPen.setStyle(Qt::DashLine);
        prevPen.setWidthF(1.5 / m_zoom);
        painter.setPen(prevPen);
        QVector<QPointF> pts = makeRegularPolygonFromEdge(m_regPolyFirst, m_currentHoverWorld, m_regPolySides);
        for (int i = 0; i < pts.size(); ++i) {
            QPointF a = pts[i]; QPointF b = pts[(i+1)%pts.size()];
            painter.drawLine(toDisplay(a), toDisplay(b));
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
        QPen sp(m_osnapGlyphColor);
        sp.setWidth(2);
        painter.setPen(sp);
        painter.setBrush(Qt::NoBrush);
        const int r = qBound(5, int(qRound(6 * devicePixelRatioF())), 12);
        auto cross = [&](){ painter.drawLine(m_snapIndicatorScreen + QPoint(-r, 0), m_snapIndicatorScreen + QPoint(r, 0)); painter.drawLine(m_snapIndicatorScreen + QPoint(0, -r), m_snapIndicatorScreen + QPoint(0, r)); };
        switch (m_snapGlyphType) {
        case SnapGlyph::Center:
            painter.drawEllipse(m_snapIndicatorScreen, r-2, r-2);
            cross();
            break;
        case SnapGlyph::Quadrant:
            painter.drawPolygon(QPolygon() << (m_snapIndicatorScreen + QPoint(0, -r)) << (m_snapIndicatorScreen + QPoint(r, 0)) << (m_snapIndicatorScreen + QPoint(0, r)) << (m_snapIndicatorScreen + QPoint(-r, 0)));
            break;
        case SnapGlyph::Perp:
            painter.drawLine(m_snapIndicatorScreen + QPoint(-r, 0), m_snapIndicatorScreen);
            painter.drawLine(m_snapIndicatorScreen, m_snapIndicatorScreen + QPoint(0, r));
            painter.drawRect(QRect(m_snapIndicatorScreen + QPoint(-3, -3), QSize(3,3)));
            break;
        case SnapGlyph::Tangent:
            painter.drawEllipse(m_snapIndicatorScreen, 2, 2);
            painter.drawLine(m_snapIndicatorScreen + QPoint(-r, -2), m_snapIndicatorScreen + QPoint(r, -2));
            break;
        default:
            painter.drawEllipse(m_snapIndicatorScreen, r-2, r-2);
            cross();
            break;
        }
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
    if (m_dynInputEnabled && (m_isDrawing || m_draggingVertex || m_draggingSelection || m_toolMode == ToolMode::Lengthen || (m_toolMode == ToolMode::DrawRegularPolygonEdge && m_regPolyHasFirst))) {
        QString tip;
        bool hasDist=false, hasAngle=false; double dist=0.0, ang=0.0;
        if (!m_dynBuffer.isEmpty()) parseDistanceAngleInput(m_dynBuffer, hasDist, dist, hasAngle, ang);
        if (m_toolMode == ToolMode::Lengthen) {
            tip = m_dynBuffer.trimmed();
        } else if (m_isDrawing) {
            if ((m_toolMode == ToolMode::DrawLine || m_toolMode == ToolMode::DrawPolygon) && !m_drawVertices.isEmpty()) {
                // Always show current segment length and angle
                QPointF last = m_drawVertices.last();
                double useAng;
                if (hasAngle) useAng = ang; else if (m_hasPendingAngle) useAng = m_pendingAngleDeg; else useAng = SurveyCalculator::azimuth(last, m_currentHoverWorld);
                if (!hasAngle && !m_hasPendingAngle && m_polarMode && !m_orthoMode) {
                    double inc = (m_polarIncrementDeg > 0.0 ? m_polarIncrementDeg : 15.0);
                    useAng = qRound(useAng / inc) * inc;
                }
                double useDist;
                if (hasDist) useDist = dist; else {
                    QPointF v = m_currentHoverWorld - last;
                    if (hasAngle || m_hasPendingAngle) {
                        double azRad = SurveyCalculator::degreesToRadians(useAng);
                        QPointF dir(qSin(azRad), qCos(azRad));
                        useDist = qMax(0.0, v.x()*dir.x() + v.y()*dir.y());
                    } else {
                        useDist = SurveyCalculator::distance(last, m_currentHoverWorld);
                    }
                }
                QString dms = SurveyCalculator::toDMS(useAng);
                tip = QString("L=%1  Brg=%2").arg(useDist, 0, 'f', 3).arg(dms);
            } else if (m_toolMode == ToolMode::DrawCircle && !m_drawVertices.isEmpty()) {
                QPointF c = m_drawVertices.first();
                double r = SurveyCalculator::distance(c, m_currentHoverWorld);
                if (hasDist) r = qMax(0.0, dist);
                tip = QString("R=%1  D=%2").arg(r, 0, 'f', 3).arg(r*2.0, 0, 'f', 3);
            } else if (m_toolMode == ToolMode::DrawRectangle && !m_drawVertices.isEmpty()) {
                QPointF a = m_drawVertices.first();
                QPointF b = m_currentHoverWorld;
                double w = qAbs(b.x() - a.x());
                double h = qAbs(b.y() - a.y());
                double area = w * h;
                tip = QString("W=%1  H=%2  A=%3").arg(w, 0, 'f', 3).arg(h, 0, 'f', 3).arg(area, 0, 'f', 3);
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
                    double arcLen = r * SurveyCalculator::degreesToRadians(qAbs(sweep));
                    tip = QString("R=%1  Ang=%2°  L=%3").arg(r, 0, 'f', 3).arg(sweep, 0, 'f', 2).arg(arcLen, 0, 'f', 3);
                }
            } else if (m_toolMode == ToolMode::DrawRegularPolygonEdge && m_regPolyHasFirst) {
                double side = SurveyCalculator::distance(m_regPolyFirst, m_currentHoverWorld);
                double angb = SurveyCalculator::azimuth(m_regPolyFirst, m_currentHoverWorld);
                QString dms = SurveyCalculator::toDMS(angb);
                tip = QString("Side=%1  Brg=%2").arg(side, 0, 'f', 3).arg(dms);
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
        else if (m_toolMode == ToolMode::Trim) {
            int li=-1; if (!hitTestLine(event->pos(), li)) return; if (li<0 || li>=m_lines.size()) return;
            // Find nearest intersection with any other line
            auto screenDist2 = [&](const QPointF& w){ QPoint s = worldToScreen(toDisplay(w)); int dx=s.x()-event->pos().x(); int dy=s.y()-event->pos().y(); return dx*dx+dy*dy; };
            QPointF bestIp; int bestD2=INT_MAX; bool found=false;
            for (int j=0;j<m_lines.size();++j) { if (j==li) continue; QPointF ip; if (segmentsIntersectWorld(m_lines[li].start, m_lines[li].end, m_lines[j].start, m_lines[j].end)) {
                    // compute ip (duplicate of earlier segIntersect)
                    auto cross=[&](const QPointF& u,const QPointF& v){ return u.x()*v.y()-u.y()*v.x(); };
                    QPointF a1=m_lines[li].start,a2=m_lines[li].end,b1=m_lines[j].start,b2=m_lines[j].end; QPointF r=a2-a1,s=b2-b1; double rxs=cross(r,s); if (qFuzzyIsNull(rxs)) continue; QPointF qp=b1-a1; double t=cross(qp,s)/rxs; if (t<0.0||t>1.0) continue; double u=cross(qp,r)/rxs; if (u<0.0||u>1.0) continue; ip=a1 + t*r;
                    int d2 = screenDist2(ip); if (d2<bestD2){ bestD2=d2; bestIp=ip; found=true; }
                }
            }
            if (!found) return;
            // Move nearer endpoint to intersection
            QPoint sa = worldToScreen(toDisplay(m_lines[li].start)); QPoint sb = worldToScreen(toDisplay(m_lines[li].end));
            int dA = QLineF(sa, event->pos()).length(); int dB = QLineF(sb, event->pos()).length();
int vi = (dA < dB) ? 0 : 1; QPointF oldPos = getLineVertex(li, vi);
            if (m_undoStack) {
                class MoveVertexCommand : public QUndoCommand { public: MoveVertexCommand(CanvasWidget* w,int li,int vi,const QPointF& from,const QPointF& to):m_w(w),m_li(li),m_vi(vi),m_from(from),m_to(to){ setText("Trim"); } void undo() override { if(m_w)m_w->setLineVertex(m_li,m_vi,m_from);} void redo() override { if(m_w)m_w->setLineVertex(m_li,m_vi,m_to);} CanvasWidget* m_w; int m_li; int m_vi; QPointF m_from; QPointF m_to; };
                m_undoStack->push(new MoveVertexCommand(this, li, vi, oldPos, bestIp));
            } else { setLineVertex(li, vi, bestIp); update(); }
            return;
        }
        else if (m_toolMode == ToolMode::Extend) {
            int li=-1; if (!hitTestLine(event->pos(), li)) return; if (li<0||li>=m_lines.size()) return;
            // Infinite line of li intersect with other segments; choose nearest from click
            auto lineLine = [&](const QPointF& a1,const QPointF& a2,const QPointF& b1,const QPointF& b2,QPointF& out)->bool{
                QPointF r=a2-a1,s=b2-b1; double rxs=r.x()*s.y()-r.y()*s.x(); if (qFuzzyIsNull(rxs)) return false; QPointF qp=b1-a1; double t=(qp.x()*s.y()-qp.y()*s.x())/rxs; double u=(qp.x()*r.y()-qp.y()*r.x())/rxs; out=a1 + t*r; return true; };
            auto screenDist2=[&](const QPointF& w){ QPoint s=worldToScreen(toDisplay(w)); int dx=s.x()-event->pos().x(); int dy=s.y()-event->pos().y(); return dx*dx+dy*dy; };
            QPointF bestIp; int bestD2=INT_MAX; bool ok=false; int preferVi; // 0 or 1 near click
            QPoint sa=worldToScreen(toDisplay(m_lines[li].start)); QPoint sb=worldToScreen(toDisplay(m_lines[li].end)); int dA=QLineF(sa,event->pos()).length(); int dB=QLineF(sb,event->pos()).length(); preferVi = (dA<dB)?0:1;
            for (int j=0;j<m_lines.size();++j){ if (j==li) continue; QPointF ip; if (!lineLine(m_lines[li].start,m_lines[li].end,m_lines[j].start,m_lines[j].end,ip)) continue; int d2=screenDist2(ip); if (d2<bestD2){ bestD2=d2; bestIp=ip; ok=true; }}
if (!ok) return; {
                QPointF oldPos = getLineVertex(li, preferVi);
                if (m_undoStack) {
                    class MoveVertexCommand : public QUndoCommand { public: MoveVertexCommand(CanvasWidget* w,int li,int vi,const QPointF& from,const QPointF& to):m_w(w),m_li(li),m_vi(vi),m_from(from),m_to(to){ setText("Extend"); } void undo() override { if(m_w)m_w->setLineVertex(m_li,m_vi,m_from);} void redo() override { if(m_w)m_w->setLineVertex(m_li,m_vi,m_to);} CanvasWidget* m_w; int m_li; int m_vi; QPointF m_from; QPointF m_to; };
                    m_undoStack->push(new MoveVertexCommand(this, li, preferVi, oldPos, bestIp));
                } else { setLineVertex(li, preferVi, bestIp); update(); }
                return; }
        }
else if (m_toolMode == ToolMode::OffsetLine) {
            int li=-1; if (!hitTestLine(event->pos(), li)) return; if (li<0||li>=m_lines.size()) return;
            QPointF a=m_lines[li].start, b=m_lines[li].end; QPointF mid((a.x()+b.x())*0.5,(a.y()+b.y())*0.5);
            // normal
            QPointF n(-(b.y()-a.y()), (b.x()-a.x())); double nlen=qSqrt(n.x()*n.x()+n.y()*n.y()); if (nlen<=1e-9) return; n/=nlen;
            // side based on click
            QPointF wc = adjustedWorldFromScreen(event->pos()); QPointF v = wc - mid; double side = (v.x()*n.x()+v.y()*n.y()) >= 0 ? 1.0 : -1.0;
            QPointF off = QPointF(n.x()*m_offsetDistance*side, n.y()*m_offsetDistance*side);
            addLine(a+off, b+off); update(); return;
        }
        else if (m_toolMode == ToolMode::FilletZero) {
            if (!m_modHasFirst) { int li=-1; if (!hitTestLine(event->pos(), li)) return; m_modHasFirst=true; m_modFirstLine=li; m_modFirstClickScreen=event->pos(); return; }
            int lj=-1; if (!hitTestLine(event->pos(), lj)) { m_modHasFirst=false; return; }
            if (lj==m_modFirstLine) { m_modHasFirst=false; return; }
            auto lineLine=[&](const QPointF& a1,const QPointF& a2,const QPointF& b1,const QPointF& b2,QPointF& out)->bool{ QPointF r=a2-a1,s=b2-b1; double rxs=r.x()*s.y()-r.y()*s.x(); if (qFuzzyIsNull(rxs)) return false; QPointF qp=b1-a1; double t=(qp.x()*s.y()-qp.y()*s.x())/rxs; out=a1 + t*r; return true; };
            QPointF ip; if (!lineLine(m_lines[m_modFirstLine].start,m_lines[m_modFirstLine].end,m_lines[lj].start,m_lines[lj].end,ip)) { m_modHasFirst=false; return; }
            auto nearestIndex=[&](int li, const QPoint& click){ QPoint sa=worldToScreen(toDisplay(m_lines[li].start)); QPoint sb=worldToScreen(toDisplay(m_lines[li].end)); int dA=QLineF(sa,click).length(); int dB=QLineF(sb,click).length(); return (dA<dB)?0:1; };
            int vi1 = nearestIndex(m_modFirstLine, m_modFirstClickScreen);
            int vi2 = nearestIndex(lj, event->pos());
            QPointF old1 = getLineVertex(m_modFirstLine, vi1);
            QPointF old2 = getLineVertex(lj, vi2);
            if (m_undoStack) {
                class TwoMoveCommand : public QUndoCommand { public: TwoMoveCommand(CanvasWidget* w,int l1,int v1,const QPointF& o1,const QPointF& n1,int l2,int v2,const QPointF& o2,const QPointF& n2):m_w(w),L1(l1),V1(v1),O1(o1),N1(n1),L2(l2),V2(v2),O2(o2),N2(n2){ setText("Fillet0"); } void undo() override { if(!m_w)return; m_w->setLineVertex(L1,V1,O1); m_w->setLineVertex(L2,V2,O2);} void redo() override { if(!m_w)return; m_w->setLineVertex(L1,V1,N1); m_w->setLineVertex(L2,V2,N2);} CanvasWidget* m_w; int L1,V1,L2,V2; QPointF O1,N1,O2,N2; };
                m_undoStack->push(new TwoMoveCommand(this, m_modFirstLine, vi1, old1, ip, lj, vi2, old2, ip));
            } else { setLineVertex(m_modFirstLine, vi1, ip); setLineVertex(lj, vi2, ip); update(); }
            m_modHasFirst=false; return;
        }
        else if (m_toolMode == ToolMode::Chamfer) {
            if (!m_modHasFirst) { int li=-1; if (!hitTestLine(event->pos(), li)) return; m_modHasFirst=true; m_modFirstLine=li; m_modFirstClickScreen=event->pos(); return; }
            int lj=-1; if (!hitTestLine(event->pos(), lj)) { m_modHasFirst=false; return; }
            if (lj==m_modFirstLine) { m_modHasFirst=false; return; }
            auto lineLine=[&](const QPointF& a1,const QPointF& a2,const QPointF& b1,const QPointF& b2,QPointF& out)->bool{ QPointF r=a2-a1,s=b2-b1; double rxs=r.x()*s.y()-r.y()*s.x(); if (qFuzzyIsNull(rxs)) return false; QPointF qp=b1-a1; double t=(qp.x()*s.y()-qp.y()*s.x())/rxs; out=a1 + t*r; return true; };
            QPointF ip; if (!lineLine(m_lines[m_modFirstLine].start,m_lines[m_modFirstLine].end,m_lines[lj].start,m_lines[lj].end,ip)) { m_modHasFirst=false; return; }
            auto nearestIndex=[&](int li, const QPoint& click){ QPoint sa=worldToScreen(toDisplay(m_lines[li].start)); QPoint sb=worldToScreen(toDisplay(m_lines[li].end)); int dA=QLineF(sa,click).length(); int dB=QLineF(sb,click).length(); return (dA<dB)?0:1; };
            int vi1 = nearestIndex(m_modFirstLine, m_modFirstClickScreen);
            int vi2 = nearestIndex(lj, event->pos());
            QPointF old1 = getLineVertex(m_modFirstLine, vi1);
            QPointF old2 = getLineVertex(lj, vi2);
            QPointF c1, c2;
            // points at chamfer distance along each line away from intersection, towards clicked endpoints
            auto chamferPoint=[&](int li,const QPoint& click){ QPointF a=m_lines[li].start, b=m_lines[li].end; QPoint sa=worldToScreen(toDisplay(a)); QPoint sb=worldToScreen(toDisplay(b)); bool useA = (QLineF(sa,click).length() < QLineF(sb,click).length()); QPointF dir = (useA ? (a - b) : (b - a)); double len=qSqrt(dir.x()*dir.x()+dir.y()*dir.y()); if (len<=1e-9) return useA? a : b; dir/=len; QPoint si = worldToScreen(toDisplay(ip)); QPointF vdir = worldToScreen(toDisplay(ip + dir)) - si; QPoint sc = click; QPointF vclick(sc.x()-si.x(), sc.y()-si.y()); double sign = ((vclick.x()*vdir.x()+vclick.y()*vdir.y())>=0)?1.0:-1.0; return ip + dir * (m_chamferDistance * sign); };
            c1 = chamferPoint(m_modFirstLine, m_modFirstClickScreen);
            c2 = chamferPoint(lj, event->pos());
            QString layer = m_layerManager ? m_layerManager->currentLayer() : QStringLiteral("0");
            if (m_undoStack) {
                class ChamferCommand : public QUndoCommand { public: ChamferCommand(CanvasWidget* w,int l1,int v1,const QPointF& o1,const QPointF& n1,int l2,int v2,const QPointF& o2,const QPointF& n2,const QPointF& a,const QPointF& b,const QString& layer):m_w(w),L1(l1),V1(v1),O1(o1),N1(n1),L2(l2),V2(v2),O2(o2),N2(n2),A(a),B(b),Layer(layer){ setText("Chamfer"); } void undo() override { if(!m_w) return; for (int i=m_w->m_lines.size()-1;i>=0;--i){ auto& dl=m_w->m_lines[i]; bool eqAB=qFuzzyCompare(dl.start.x(),A.x())&&qFuzzyCompare(dl.start.y(),A.y())&&qFuzzyCompare(dl.end.x(),B.x())&&qFuzzyCompare(dl.end.y(),B.y())&&dl.layer==Layer; bool eqBA=qFuzzyCompare(dl.start.x(),B.x())&&qFuzzyCompare(dl.start.y(),B.y())&&qFuzzyCompare(dl.end.x(),A.x())&&qFuzzyCompare(dl.end.y(),A.y())&&dl.layer==Layer; if (eqAB||eqBA){ m_w->m_lines.remove(i); break; } } m_w->setLineVertex(L1,V1,O1); m_w->setLineVertex(L2,V2,O2); m_w->update(); } void redo() override { if(!m_w) return; m_w->setLineVertex(L1,V1,N1); m_w->setLineVertex(L2,V2,N2); m_w->m_lines.append(CanvasWidget::DrawnLine{A,B,m_w->m_lineColor,Layer}); m_w->update(); } CanvasWidget* m_w; int L1,V1,L2,V2; QPointF O1,N1,O2,N2; QPointF A,B; QString Layer; };
                m_undoStack->push(new ChamferCommand(this, m_modFirstLine, vi1, old1, c1, lj, vi2, old2, c2, c1, c2, layer));
            } else { setLineVertex(m_modFirstLine, vi1, c1); setLineVertex(lj, vi2, c2); addLine(c1, c2); update(); }
            m_modHasFirst=false; return;
        }
        else if (m_toolMode == ToolMode::DrawRegularPolygonEdge) {
            // Regular polygon by edge: pick two points defining an edge
            if (!m_regPolyHasFirst) {
                m_regPolyFirst = adjustedWorldFromScreen(event->pos());
                m_regPolyHasFirst = true;
                m_orthoAnchor = m_regPolyFirst;
                update();
                return;
            } else {
                QPointF second = adjustedWorldFromScreen(event->pos());
                QVector<QPointF> pts = makeRegularPolygonFromEdge(m_regPolyFirst, second, m_regPolySides);
                if (pts.size() >= 3) addPolylineEntity(pts, true);
                // Reset tool state
                m_regPolyHasFirst = false;
                m_regPolyEdgeActive = false;
                m_toolMode = ToolMode::Select;
                update();
                return;
            }
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
                        addPolylineEntity(pts, true);
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
                addPolylineEntity(pts, true);
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
                    addPolylineEntity(pts, false);
                } else {
                    pts = {a, b, c};
                    addPolylineEntity(pts, false);
                }
                m_isDrawing = false; m_drawVertices.clear(); update(); return;
            }
        }
        // Selection interactions
        if (m_toolMode == ToolMode::Select) {
            // Try grips first (vertex edit)
            int gli=-1, gvi=-1;
            if (hitTestGrip(event->pos(), gli, gvi)) {
                // Enforce lock of the line's layer
                if (m_layerManager) {
                    Layer L = m_layerManager->getLayer(m_lines[gli].layer);
                    if (!L.name.isEmpty() && L.locked) return;
                }
                m_draggingVertex = true;
                m_dragLineIndex = gli;
                m_dragVertexIndex = gvi;
                m_dragOldPos = getLineVertex(gli, gvi);
                m_orthoAnchor = m_dragOldPos;
                setCursor(Qt::SizeAllCursor);
                update();
                return;
            }
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
            emit drawingAngleChanged(0.0);
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
        // Heuristic: set glyph by checking enabled modes priority
        m_snapGlyphType = SnapGlyph::Nearest;
        if (m_osnapCenter) m_snapGlyphType = SnapGlyph::Center;
        if (m_osnapQuadrant) m_snapGlyphType = SnapGlyph::Quadrant;
        if (m_osnapPerp) m_snapGlyphType = SnapGlyph::Perp;
        if (m_osnapTangent) m_snapGlyphType = SnapGlyph::Tangent;
        // Emit hint
        QString hint;
        switch (m_snapGlyphType) {
        case SnapGlyph::Center: hint = QStringLiteral("OSNAP: Center"); break;
        case SnapGlyph::Quadrant: hint = QStringLiteral("OSNAP: Quadrant"); break;
        case SnapGlyph::Perp: hint = QStringLiteral("OSNAP: Perpendicular"); break;
        case SnapGlyph::Tangent: hint = QStringLiteral("OSNAP: Tangent"); break;
        default: hint.clear(); break;
        }
        emit osnapHintChanged(hint);
    } else {
        m_hasSnapIndicator = false;
        m_snapGlyphType = SnapGlyph::None;
        if (m_otrackMode) m_hasTrackOrigin = false;
        emit osnapHintChanged(QString());
    }
    QPointF worldPos = adjustedWorldFromScreen(event->pos());
    emit mouseWorldPosition(worldPos);
    m_currentHoverWorld = worldPos;
    // Live measurement while drawing/dragging (use dynamic input preview if applicable)
    if (m_toolMode == ToolMode::DrawRegularPolygonEdge && m_regPolyHasFirst) {
        double side = SurveyCalculator::distance(m_regPolyFirst, adjustedWorldFromScreen(event->pos()));
        emit drawingDistanceChanged(side);
        double ang = SurveyCalculator::azimuth(m_regPolyFirst, adjustedWorldFromScreen(event->pos()));
        emit drawingAngleChanged(ang);
        update();
    } else if (m_isDrawing && !m_drawVertices.isEmpty()) {
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
        double ang = SurveyCalculator::azimuth(last, preview);
        emit drawingAngleChanged(ang);
        // Ensure live preview repaints while moving
        update();
    } else if (m_draggingVertex) {
        double dx = worldPos.x() - m_dragOldPos.x();
        double dy = worldPos.y() - m_dragOldPos.y();
        emit drawingDistanceChanged(qSqrt(dx*dx + dy*dy));
        double ang = SurveyCalculator::normalizeAngle(qRadiansToDegrees(qAtan2(dx, dy)));
        if (AppSettings::gaussMode()) ang = SurveyCalculator::normalizeAngle(ang - 180.0);
        emit drawingAngleChanged(ang);
    } else {
        emit drawingDistanceChanged(0.0);
        emit drawingAngleChanged(0.0);
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
                    addPolylineEntity(pts, true);
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
                addPolylineEntity(pts, true);
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
                    if (take) {
                        int pli = polylineIndexForLine(i);
                        if (pli >= 0) {
                            auto segs = currentLineIndicesForPolyline(pli);
                            for (int li : segs) m_selectedLineIndices.insert(li);
                        } else {
                            m_selectedLineIndices.insert(i);
                        }
                    }
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
                    if (insideA || insideB || polyIntersectSegment(m_lines[i].start, m_lines[i].end)) {
                        int pli = polylineIndexForLine(i);
                        if (pli >= 0) {
                            auto segs = currentLineIndicesForPolyline(pli);
                            for (int li : segs) m_selectedLineIndices.insert(li);
                        } else {
                            m_selectedLineIndices.insert(i);
                        }
                    }
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
            emit drawingAngleChanged(0.0);
            update();
        }
        if (m_toolMode == ToolMode::DrawRegularPolygonEdge) {
            m_regPolyHasFirst = false;
            m_regPolyEdgeActive = false;
            m_toolMode = ToolMode::Select;
            emit drawingDistanceChanged(0.0);
            emit drawingAngleChanged(0.0);
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
        Qt::KeyboardModifiers mods = event->modifiers();
        bool hasShortcutMods = mods.testFlag(Qt::ControlModifier) || mods.testFlag(Qt::MetaModifier) || mods.testFlag(Qt::AltModifier);
        if (!hasShortcutMods && ((k >= Qt::Key_0 && k <= Qt::Key_9) || (k >= Qt::Key_A && k <= Qt::Key_Z) || k == Qt::Key_Period || k == Qt::Key_Minus || k == Qt::Key_Plus || k == Qt::Key_Less || k == Qt::Key_At || k == Qt::Key_Comma)) {
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
        Qt::KeyboardModifiers mods = event->modifiers();
        bool hasShortcutMods = mods.testFlag(Qt::ControlModifier) || mods.testFlag(Qt::MetaModifier) || mods.testFlag(Qt::AltModifier);
        if (!hasShortcutMods && ((k >= Qt::Key_0 && k <= Qt::Key_9) || (k >= Qt::Key_A && k <= Qt::Key_Z) || k == Qt::Key_Period || k == Qt::Key_Minus || k == Qt::Key_Plus || k == Qt::Key_Less)) {
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
        Qt::KeyboardModifiers mods = event->modifiers();
        bool hasShortcutMods = mods.testFlag(Qt::ControlModifier) || mods.testFlag(Qt::MetaModifier) || mods.testFlag(Qt::AltModifier);
        if (!hasShortcutMods && ((k >= Qt::Key_0 && k <= Qt::Key_9) || k == Qt::Key_Period || k == Qt::Key_Plus || k == Qt::Key_Minus)) {
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
        double w = dl.width > 0.0 ? dl.width : 1.0;
        if (sel) w = qMax(w, 2.0);
        linePen.setWidthF(w);
        linePen.setStyle(static_cast<Qt::PenStyle>(dl.style));
        painter.setPen(linePen);
        painter.drawLine(toDisplay(dl.start), toDisplay(dl.end));
        // Length label at midpoint (screen space)
        if (m_showLengthLabels) {
            QPointF mid((dl.start.x()+dl.end.x())*0.5, (dl.start.y()+dl.end.y())*0.5);
            double len = SurveyCalculator::distance(dl.start, dl.end);
            painter.resetTransform();
            QPoint ms = worldToScreen(toDisplay(mid));
            QFont f = painter.font(); f.setPointSize(10);
            painter.setFont(f);
            painter.setPen(QPen(Qt::white));
            painter.drawText(ms + QPoint(6, -6), QString("%1 m").arg(len, 0, 'f', 3));
            painter.setTransform(m_worldToScreen);
        }
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
    // Use the same effective grid step as drawGrid so snapping matches visible grid
    double step = (m_gridSize > 0 ? m_gridSize : 1.0);
    while (step * m_zoom < 10) step *= 2;
    while (step * m_zoom > 100) step /= 2;
    QPointF res = world;
    res.setX(qRound(world.x() / step) * step);
    res.setY(qRound(world.y() / step) * step);
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
// Consider points (visible layers only) as "endpoints" snaps (treat as end)
if (m_osnapEnd) {
    for (const auto& dp : m_points) {
        if (m_layerManager) {
            Layer L = m_layerManager->getLayer(dp.layer);
            if (!L.name.isEmpty() && !L.visible) continue;
        }
        considerWorld(dp.point.toQPointF());
    }
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
    if (m_osnapEnd) { considerWorld(dl.start); considerWorld(dl.end); }
    // Midpoint
    if (m_osnapMid) considerWorld(QPointF((dl.start.x()+dl.end.x())*0.5, (dl.start.y()+dl.end.y())*0.5));
    // Nearest point on segment in screen space
    if (m_osnapNearest) {
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
}
// Consider intersections between visible line segments
if (m_osnapIntersect) {
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
}
// Perpendicular foot from cursor to lines (world projection)
if (m_osnapPerp) {
    QPointF wc = screenToWorld(screen);
    for (const auto& dl : m_lines) {
        if (m_layerManager) { Layer L = m_layerManager->getLayer(dl.layer); if (!L.name.isEmpty() && !L.visible) continue; }
        QPointF v = dl.end - dl.start;
        double denom = v.x()*v.x() + v.y()*v.y();
        if (denom <= 1e-12) continue;
        QPointF wv = wc - dl.start;
        double t = (wv.x()*v.x() + wv.y()*v.y()) / denom;
        if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0;
        QPointF p(dl.start.x() + t*v.x(), dl.start.y() + t*v.y());
        considerWorld(p);
    }
}
// Circle-like polylines: center, quadrant, tangent
if (m_osnapCenter || m_osnapQuadrant || m_osnapTangent) {
    QPointF wc = screenToWorld(screen);
    auto quadPoints = [&](const QPointF& c, double r){
        QVector<QPointF> q; q.reserve(4);
        auto addAng = [&](double deg){ double rad = SurveyCalculator::degreesToRadians(deg); q.append(QPointF(c.x() + r*qSin(rad), c.y() + r*qCos(rad))); };
        addAng(0.0); addAng(90.0); addAng(180.0); addAng(270.0); return q; };
    for (const auto& pl : m_polylines) {
        if (!pl.closed) continue;
        if (m_layerManager) { Layer L = m_layerManager->getLayer(pl.layer); if (!L.name.isEmpty() && !L.visible) continue; }
        QPointF c; double r=0.0;
        if (!pl.isCircleLikeCached) {
            if (pl.pts.size() >= 8) {
                QPointF cc(0,0);
                for (const auto& p : pl.pts) { cc.setX(cc.x()+p.x()); cc.setY(cc.y()+p.y()); }
                cc.setX(cc.x()/pl.pts.size()); cc.setY(cc.y()/pl.pts.size());
                double sumR=0.0; for (const auto& p : pl.pts) { double dx=p.x()-cc.x(); double dy=p.y()-cc.y(); sumR += qSqrt(dx*dx+dy*dy); }
                double meanR = sumR/pl.pts.size();
                double var=0.0; for (const auto& p : pl.pts) { double dx=p.x()-cc.x(); double dy=p.y()-cc.y(); double rr = qSqrt(dx*dx+dy*dy); double d=rr-meanR; var += d*d; }
                double stdev = qSqrt(var/pl.pts.size());
                bool ok = (meanR > 1e-9) && (stdev <= 0.02*meanR);
                pl.isCircleLike = ok; pl.cachedCenter = cc; pl.cachedRadius = meanR; pl.isCircleLikeCached = true;
            } else {
                pl.isCircleLike = false; pl.cachedRadius = 0.0; pl.cachedCenter = QPointF(); pl.isCircleLikeCached = true;
            }
        }
        if (!pl.isCircleLike) continue; c = pl.cachedCenter; r = pl.cachedRadius;
        if (m_osnapCenter) considerWorld(c);
        if (m_osnapQuadrant) { for (const auto& qp : quadPoints(c, r)) considerWorld(qp); }
        if (m_osnapTangent) {
            // Only if cursor is outside circle (tangent exists)
            double dx = wc.x()-c.x(); double dy = wc.y()-c.y(); double dist = qSqrt(dx*dx+dy*dy);
            if (dist > r + 1e-6) {
                double bestAbs = 1e18; QPointF bestP;
                for (const auto& p : pl.pts) {
                    QPointF radv = QPointF(p.x()-c.x(), p.y()-c.y());
                    QPointF sp = QPointF(wc.x()-p.x(), wc.y()-p.y());
                    double dperp = qAbs(radv.x()*sp.x() + radv.y()*sp.y()); // dot ~ 0
                    if (dperp < bestAbs) { bestAbs = dperp; bestP = p; }
                }
                considerWorld(bestP);
            }
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
    QString old = dl.layer;
    if (m_undoStack) {
        class SetLineLayerCommand : public QUndoCommand { public: SetLineLayerCommand(CanvasWidget* w,int idx,const QString& oldL,const QString& newL):m_w(w),m_idx(idx),m_old(oldL),m_new(newL){ setText("Set Line Layer"); } void undo() override { if(m_idx>=0 && m_idx<m_w->m_lines.size()){ m_w->m_lines[m_idx].layer = m_old; m_w->update(); } } void redo() override { if(m_idx>=0 && m_idx<m_w->m_lines.size()){ m_w->m_lines[m_idx].layer = m_new; m_w->update(); } } CanvasWidget* m_w; int m_idx; QString m_old; QString m_new; };
        m_undoStack->push(new SetLineLayerCommand(this, lineIndex, old, layer));
    } else { dl.layer = layer; update(); }
    return true;
}

bool CanvasWidget::lineEndpoints(int lineIndex, QPointF& startOut, QPointF& endOut) const
{
    if (lineIndex < 0 || lineIndex >= m_lines.size()) return false;
    startOut = m_lines[lineIndex].start;
    endOut = m_lines[lineIndex].end;
    return true;
}

bool CanvasWidget::hatchSelectedPolyline(double spacing, double angleDeg)
{
    if (spacing <= 0.0) spacing = 1.0;
    // Pick a selected line to infer a polyline
    int seedLine = -1;
    if (!m_selectedLineIndices.isEmpty()) seedLine = *m_selectedLineIndices.begin();
    else if (m_selectedLineIndex >= 0) seedLine = m_selectedLineIndex;
    if (seedLine < 0 || seedLine >= m_lines.size()) return false;
    int pli = polylineIndexForLine(seedLine);
    if (pli < 0 || pli >= m_polylines.size()) return false;
    const auto& pl = m_polylines[pli];
    // Ensure closed ring of points
    QVector<QPointF> pts = pl.pts;
    if (pts.size() < 3) return false;
    bool closed = pl.closed;
    if (!closed) pts.append(pts.front());
    // Direction v and normal n
    double rad = SurveyCalculator::degreesToRadians(angleDeg);
    QPointF v(qCos(rad), qSin(rad));
    QPointF n(-v.y(), v.x());
    auto dot = [](const QPointF& a, const QPointF& b){ return a.x()*b.x() + a.y()*b.y(); };
    // Range along n
    double minC = 1e18, maxC = -1e18;
    for (const auto& p : pts) {
        double c = dot(p, n);
        if (c < minC) minC = c;
        if (c > maxC) maxC = c;
    }
    if (!(maxC > minC)) return false;
    // Align start
    double startC = std::floor(minC / spacing) * spacing;
    QString layer = m_layerManager ? m_layerManager->currentLayer() : QStringLiteral("0");
    // For each stripe, compute intersections with polygon edges
    for (double c = startC; c <= maxC; c += spacing) {
        QVector<QPointF> inters;
        for (int i = 0; i+1 < pts.size(); ++i) {
            QPointF a = pts[i]; QPointF b = pts[i+1];
            double da = dot(a, n) - c;
            double db = dot(b, n) - c;
            double denom = db - da;
            if (qFuzzyIsNull(denom)) continue;
            double u = -da / denom; // since da + u*(db-da) = 0
            if (u >= 0.0 && u <= 1.0) {
                QPointF p(a.x() + (b.x()-a.x())*u, a.y() + (b.y()-a.y())*u);
                inters.append(p);
            }
        }
        if (inters.size() < 2) continue;
        // Sort intersections along v
        std::sort(inters.begin(), inters.end(), [&](const QPointF& p1, const QPointF& p2){ return dot(p1, v) < dot(p2, v); });
        // Pair up 0-1, 2-3, ...
        for (int k = 0; k+1 < inters.size(); k += 2) {
            QPointF p1 = inters[k];
            QPointF p2 = inters[k+1];
            // Add hatch segment as line using current pen settings and color
            DrawnLine dl{p1, p2, m_lineColor, layer, m_currentPenWidth, m_currentPenStyle};
            m_lines.append(dl);
        }
    }
    update();
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