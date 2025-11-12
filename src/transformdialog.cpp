#include "transformdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QTextEdit>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QAbstractItemView>
#include <QStringList>
#include <QtMath>
#include <cmath>
#include <algorithm>
#include "pointmanager.h"
#include "canvaswidget.h"

static double sqr(double v){ return v*v; }

TransformDialog::TransformDialog(PointManager* pm, CanvasWidget* canvas, QWidget* parent)
    : QDialog(parent), m_pm(pm), m_canvas(canvas)
{
    setWindowTitle("Transformations");
    resize(720, 520);

    QVBoxLayout* root = new QVBoxLayout(this);

    QFormLayout* top = new QFormLayout();
    m_mode = new QComboBox(this);
    m_mode->addItem("Helmert 2D (Similarity)");
    m_mode->addItem("Affine 2D");
    top->addRow("Mode:", m_mode);

    m_weighting = new QComboBox(this);
    m_weighting->addItem("Equal weights");
    m_weighting->addItem("Inverse distance");
    m_weighting->addItem("Inverse distance squared");
    top->addRow("Weighting:", m_weighting);

    m_eps = new QDoubleSpinBox(this);
    m_eps->setRange(1e-6, 1e6);
    m_eps->setDecimals(6);
    m_eps->setSingleStep(0.1);
    m_eps->setValue(1.0);
    top->addRow("Distance epsilon:", m_eps);

    root->addLayout(top);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    QStringList headers;
    headers << "Source point" << "Target point" << "Weight";
    m_table->setHorizontalHeaderLabels(headers);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    root->addWidget(m_table, 1);

    QHBoxLayout* tableActions = new QHBoxLayout();
    QPushButton* addPairBtn = new QPushButton("Add Pair", this);
    QPushButton* removePairBtn = new QPushButton("Remove Selected", this);
    tableActions->addWidget(addPairBtn);
    tableActions->addWidget(removePairBtn);
    tableActions->addStretch();
    root->addLayout(tableActions);

    QHBoxLayout* actions = new QHBoxLayout();
    QPushButton* computeBtn = new QPushButton("Compute", this);
    QPushButton* applyPtsBtn = new QPushButton("Apply to Points", this);
    QPushButton* applyDrawBtn = new QPushButton("Apply to Drawing", this);
    actions->addWidget(computeBtn);
    actions->addStretch();
    actions->addWidget(applyPtsBtn);
    actions->addWidget(applyDrawBtn);
    root->addLayout(actions);

    m_report = new QTextEdit(this);
    m_report->setReadOnly(true);
    root->addWidget(m_report);

    connect(addPairBtn, &QPushButton::clicked, this, &TransformDialog::addRow);
    connect(removePairBtn, &QPushButton::clicked, this, &TransformDialog::removeSelected);
    connect(computeBtn, &QPushButton::clicked, this, &TransformDialog::computeTransform);
    connect(applyPtsBtn, &QPushButton::clicked, this, &TransformDialog::applyToPoints);
    connect(applyDrawBtn, &QPushButton::clicked, this, &TransformDialog::applyToDrawing);

    addRow();
    reload();
}

void TransformDialog::reload()
{
    if (!m_pm || !m_table) {
        return;
    }
    if (m_table->rowCount() == 0) {
        addRow();
    }
    for (int row = 0; row < m_table->rowCount(); ++row) {
        fillPointCombosRow(row);
    }
}

void TransformDialog::fillPointCombosRow(int row)
{
    if (!m_pm || !m_table) {
        return;
    }
    const QStringList names = m_pm->getPointNames();
    auto ensureCombo = [&](int column) {
        auto* combo = qobject_cast<QComboBox*>(m_table->cellWidget(row, column));
        QString previous = combo ? combo->currentText() : QString();
        if (!combo) {
            combo = new QComboBox(m_table);
            m_table->setCellWidget(row, column, combo);
        }
        combo->blockSignals(true);
        combo->clear();
        combo->addItems(names);
        if (!previous.isEmpty()) {
            int idx = combo->findText(previous);
            if (idx >= 0) {
                combo->setCurrentIndex(idx);
            }
        }
        combo->blockSignals(false);
    };

    ensureCombo(0);
    ensureCombo(1);

    auto* weight = qobject_cast<QDoubleSpinBox*>(m_table->cellWidget(row, 2));
    if (!weight) {
        weight = new QDoubleSpinBox(m_table);
        weight->setRange(0.0, 1e9);
        weight->setDecimals(6);
        weight->setValue(1.0);
        m_table->setCellWidget(row, 2, weight);
    }
}

void TransformDialog::addRow()
{
    if (!m_table) {
        return;
    }
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    fillPointCombosRow(row);
}

void TransformDialog::removeSelected()
{
    if (!m_table) {
        return;
    }
    const auto selected = m_table->selectionModel() ? m_table->selectionModel()->selectedRows() : QModelIndexList();
    for (int i = selected.size() - 1; i >= 0; --i) {
        m_table->removeRow(selected.at(i).row());
    }
}

QVector<TransformDialog::Pair> TransformDialog::collectPairs() const
{
    QVector<Pair> pairs;
    if (!m_table) {
        return pairs;
    }
    for (int r = 0; r < m_table->rowCount(); ++r) {
        auto* cbS = qobject_cast<QComboBox*>(m_table->cellWidget(r, 0));
        auto* cbD = qobject_cast<QComboBox*>(m_table->cellWidget(r, 1));
        auto* weight = qobject_cast<QDoubleSpinBox*>(m_table->cellWidget(r, 2));
        if (!cbS || !cbD) {
            continue;
        }
        const QString s = cbS->currentText();
        const QString d = cbD->currentText();
        if (s.isEmpty() || d.isEmpty()) {
            continue;
        }
        Pair pair;
        pair.src = s;
        pair.dst = d;
        pair.weight = weight ? weight->value() : 1.0;
        pairs.append(pair);
    }
    return pairs;
}

static bool solveSymmetricNormal(QVector<double>& N, QVector<double>& b, int n)
{
    for (int k=0;k<n;++k) {
        int kk = k*n + k;
        double pivot = N[kk];
        if (qFuzzyIsNull(pivot)) return false;
        for (int j=k;j<n;++j) N[k*n + j] /= pivot;
        b[k] /= pivot;
        for (int i=0;i<n;++i) if (i!=k) {
            double f = N[i*n + k];
            if (qFuzzyIsNull(f)) continue;
            for (int j=k;j<n;++j) N[i*n + j] -= f * N[k*n + j];
            b[i] -= f * b[k];
        }
    }
    return true;
}

bool TransformDialog::solveHelmert(const QVector<QPointF>& src, const QVector<QPointF>& dst,
                                   const QVector<double>& w, double& a, double& b, double& tx, double& ty,
                                   double& rms, QString& report) const
{
    const int n = src.size();
    if (n < 2) return false;
    QVector<double> N(16, 0.0);
    QVector<double> B(4, 0.0);
    double swr = 0.0;
    for (int i=0;i<n;++i) {
        const double wi = w.isEmpty()? 1.0 : w[i];
        const double xi = src[i].x();
        const double yi = src[i].y();
        const double Xi = dst[i].x();
        const double Yi = dst[i].y();
        double Arow1[4] = { xi, -yi, 1.0, 0.0 };
        double Arow2[4] = { yi,  xi, 0.0, 1.0 };
        for (int r=0;r<4;++r) for (int c=r;c<4;++c) { N[r*4+c] += wi * Arow1[r]*Arow1[c]; }
        for (int r=0;r<4;++r) for (int c=r;c<4;++c) { N[r*4+c] += wi * Arow2[r]*Arow2[c]; }
        for (int r=0;r<4;++r) N[r*4+r] += 0.0;
        B[0] += wi * (Arow1[0]*Xi + Arow2[0]*Yi);
        B[1] += wi * (Arow1[1]*Xi + Arow2[1]*Yi);
        B[2] += wi * (Arow1[2]*Xi + Arow2[2]*Yi);
        B[3] += wi * (Arow1[3]*Xi + Arow2[3]*Yi);
        swr += wi;
    }
    for (int r=0;r<4;++r) for (int c=0;c<r;++c) N[r*4+c] = N[c*4+r];
    if (!solveSymmetricNormal(N, B, 4)) return false;
    a = B[0]; b = B[1]; tx = B[2]; ty = B[3];
    double ss = 0.0; int dof = std::max(1, 2*n - 4);
    for (int i=0;i<n;++i) {
        const double xi = src[i].x();
        const double yi = src[i].y();
        QPointF pred(a*xi - b*yi + tx, b*xi + a*yi + ty);
        QPointF res = dst[i] - pred;
        const double wi = w.isEmpty()? 1.0 : w[i];
        ss += wi * (sqr(res.x()) + sqr(res.y()));
    }
    rms = qSqrt(ss / double(dof));
    report += QString("Helmert 2D: a=%1 b=%2 tx=%3 ty=%4 rms=%5\n").arg(a,0,'f',9).arg(b,0,'f',9).arg(tx,0,'f',4).arg(ty,0,'f',4).arg(rms,0,'f',4);
    return true;
}

bool TransformDialog::solveAffine(const QVector<QPointF>& src, const QVector<QPointF>& dst,
                                  const QVector<double>& w, double& a1, double& a2, double& a3,
                                  double& b1, double& b2, double& b3,
                                  double& rms, QString& report) const
{
    const int n = src.size();
    if (n < 3) return false;
    QVector<double> N(36, 0.0);
    QVector<double> B(6, 0.0);
    for (int i=0;i<n;++i) {
        const double wi = w.isEmpty()? 1.0 : w[i];
        const double x = src[i].x();
        const double y = src[i].y();
        const double X = dst[i].x();
        const double Y = dst[i].y();
        double Ax[6] = { x, y, 1.0, 0.0, 0.0, 0.0 };
        double Ay[6] = { 0.0, 0.0, 0.0, x, y, 1.0 };
        for (int r=0;r<6;++r) for (int c=r;c<6;++c) { N[r*6+c] += wi * Ax[r]*Ax[c]; }
        for (int r=0;r<6;++r) for (int c=r;c<6;++c) { N[r*6+c] += wi * Ay[r]*Ay[c]; }
        B[0] += wi * (Ax[0]*X + Ay[0]*Y);
        B[1] += wi * (Ax[1]*X + Ay[1]*Y);
        B[2] += wi * (Ax[2]*X + Ay[2]*Y);
        B[3] += wi * (Ax[3]*X + Ay[3]*Y);
        B[4] += wi * (Ax[4]*X + Ay[4]*Y);
        B[5] += wi * (Ax[5]*X + Ay[5]*Y);
    }
    for (int r=0;r<6;++r) for (int c=0;c<r;++c) N[r*6+c] = N[c*6+r];
    if (!solveSymmetricNormal(N, B, 6)) return false;
    a1=B[0]; a2=B[1]; a3=B[2]; b1=B[3]; b2=B[4]; b3=B[5];
    double ss = 0.0; int dof = std::max(1, 2*n - 6);
    for (int i=0;i<n;++i) {
        const double x = src[i].x();
        const double y = src[i].y();
        QPointF pred(a1*x + a2*y + a3, b1*x + b2*y + b3);
        QPointF res = dst[i] - pred;
        const double wi = w.isEmpty()? 1.0 : w[i];
        ss += wi * (sqr(res.x()) + sqr(res.y()));
    }
    rms = qSqrt(ss / double(dof));
    report += QString("Affine 2D: a1=%1 a2=%2 a3=%3 b1=%4 b2=%5 b3=%6 rms=%7\n")
              .arg(a1,0,'f',9).arg(a2,0,'f',9).arg(a3,0,'f',4)
              .arg(b1,0,'f',9).arg(b2,0,'f',9).arg(b3,0,'f',4)
              .arg(rms,0,'f',4);
    return true;
}

QPointF TransformDialog::applyHelmertTo(const QPointF& p, double a, double b, double tx, double ty) const
{
    return QPointF(a*p.x() - b*p.y() + tx, b*p.x() + a*p.y() + ty);
}

QPointF TransformDialog::applyAffineTo(const QPointF& p, double a1, double a2, double a3, double b1, double b2, double b3) const
{
    return QPointF(a1*p.x() + a2*p.y() + a3, b1*p.x() + b2*p.y() + b3);
}

void TransformDialog::computeTransform()
{
    m_hasSolution = false;
    if (!m_pm || !m_table) {
        m_report->setText("No points available");
        return;
    }

    const QVector<Pair> pairs = collectPairs();
    if (pairs.isEmpty()) {
        m_report->setText("Add control pairs to compute a transform");
        return;
    }

    QVector<QPointF> src;
    QVector<QPointF> dst;
    QVector<double> weights;
    src.reserve(pairs.size());
    dst.reserve(pairs.size());
    weights.reserve(pairs.size());

    double cx = 0.0;
    double cy = 0.0;
    int counted = 0;

    for (const auto& pair : pairs) {
        if (!m_pm->hasPoint(pair.src) || !m_pm->hasPoint(pair.dst)) {
            continue;
        }
        const Point srcPt = m_pm->getPoint(pair.src);
        const Point dstPt = m_pm->getPoint(pair.dst);
        src.append(QPointF(srcPt.x, srcPt.y));
        dst.append(QPointF(dstPt.x, dstPt.y));
        weights.append(pair.weight);
        cx += srcPt.x;
        cy += srcPt.y;
        ++counted;
    }

    const int requiredPairs = (!m_mode || m_mode->currentIndex() == 0) ? 2 : 3;
    if (src.size() < requiredPairs) {
        m_report->setText("Not enough valid control pairs for the selected mode");
        return;
    }

    cx /= std::max(1, counted);
    cy /= std::max(1, counted);

    if (m_weighting && m_weighting->currentIndex() > 0) {
        const double eps = m_eps ? m_eps->value() : 1.0;
        for (int i = 0; i < src.size(); ++i) {
            const double d = std::hypot(src[i].x() - cx, src[i].y() - cy);
            double mult = 1.0 / std::max(eps, d);
            if (m_weighting->currentIndex() == 2) {
                mult *= mult;
            }
            weights[i] *= mult;
        }
    }

    QString report;
    double rms = 0.0;
    if (!m_mode || m_mode->currentIndex() == 0) {
        double a = 0.0;
        double b = 0.0;
        double tx = 0.0;
        double ty = 0.0;
        if (!solveHelmert(src, dst, weights, a, b, tx, ty, rms, report)) {
            m_report->setText("Failed to compute Helmert transform");
            return;
        }
        m_lastMode = Mode::Helmert2D;
        m_h_a = a;
        m_h_b = b;
        m_h_tx = tx;
        m_h_ty = ty;
    } else {
        double a1 = 0.0, a2 = 0.0, a3 = 0.0;
        double b1 = 0.0, b2 = 0.0, b3 = 0.0;
        if (!solveAffine(src, dst, weights, a1, a2, a3, b1, b2, b3, rms, report)) {
            m_report->setText("Failed to compute affine transform");
            return;
        }
        m_lastMode = Mode::Affine2D;
        m_af_a1 = a1;
        m_af_a2 = a2;
        m_af_a3 = a3;
        m_af_b1 = b1;
        m_af_b2 = b2;
        m_af_b3 = b3;
    }

    m_hasSolution = true;
    m_report->setText(report);
}

void TransformDialog::applyToPoints()
{
    if (!m_hasSolution || !m_pm || !m_canvas) return;
    const QVector<Point> pts = m_pm->getAllPoints();
    for (const auto& p : pts) {
        QPointF q;
        if (m_lastMode == Mode::Helmert2D) {
            q = applyHelmertTo(QPointF(p.x, p.y), m_h_a, m_h_b, m_h_tx, m_h_ty);
        } else {
            q = applyAffineTo(QPointF(p.x, p.y), m_af_a1, m_af_a2, m_af_a3, m_af_b1, m_af_b2, m_af_b3);
        }
        const QString prefix = QStringLiteral("T_");
        const QString name = prefix + p.name;
        Point np(name, q.x(), q.y(), p.z);
        m_pm->addPoint(np);
        m_canvas->addPoint(np);
    }
}

void TransformDialog::applyToDrawing()
{
    if (!m_hasSolution || !m_canvas) return;
    const QString layer = QStringLiteral("Transformed");
    for (int i=0;i<m_canvas->lineCount();++i) {
        QPointF a,b; if (!m_canvas->lineEndpoints(i,a,b)) continue;
        QPointF aa = a, bb = b;
        if (m_lastMode==Mode::Helmert2D) {
            aa = applyHelmertTo(a, m_h_a, m_h_b, m_h_tx, m_h_ty);
            bb = applyHelmertTo(b, m_h_a, m_h_b, m_h_tx, m_h_ty);
        } else {
            aa = applyAffineTo(a, m_af_a1, m_af_a2, m_af_a3, m_af_b1, m_af_b2, m_af_b3);
            bb = applyAffineTo(b, m_af_a1, m_af_a2, m_af_a3, m_af_b1, m_af_b2, m_af_b3);
        }
        int before = m_canvas->lineCount();
        m_canvas->addLine(aa, bb);
        int idx = m_canvas->lineCount()-1;
        if (idx >= before) m_canvas->setLineLayer(idx, layer);
    }
    for (int i=0;i<m_canvas->polylineCount();++i) {
        QVector<QPointF> pts; bool closed=false; QString lay; if (!m_canvas->polylineData(i, pts, closed, lay)) continue;
        QVector<QPointF> out; out.reserve(pts.size());
        for (const auto& p : pts) {
            if (m_lastMode==Mode::Helmert2D) out.append(applyHelmertTo(p, m_h_a, m_h_b, m_h_tx, m_h_ty));
            else out.append(applyAffineTo(p, m_af_a1, m_af_a2, m_af_a3, m_af_b1, m_af_b2, m_af_b3));
        }
        if (!out.isEmpty()) m_canvas->addPolylineEntity(out, closed, layer);
    }
    for (int i=0;i<m_canvas->textCount();++i) {
        QString t; QPointF pos; double h=0.0, ang=0.0; QString lay;
        if (!m_canvas->textData(i, t, pos, h, ang, lay)) continue;
        QPointF pp = pos;
        if (m_lastMode==Mode::Helmert2D) pp = applyHelmertTo(pos, m_h_a, m_h_b, m_h_tx, m_h_ty);
        else pp = applyAffineTo(pos, m_af_a1, m_af_a2, m_af_a3, m_af_b1, m_af_b2, m_af_b3);
        m_canvas->addText(t, pp, h, ang, layer);
    }
    for (int i=0;i<m_canvas->dimensionCount();++i) {
        QPointF a,b; double th=0.0; QString lay;
        if (!m_canvas->dimensionData(i, a, b, th, lay)) continue;
        QPointF aa=a, bb=b;
        if (m_lastMode==Mode::Helmert2D) {
            aa = applyHelmertTo(a, m_h_a, m_h_b, m_h_tx, m_h_ty);
            bb = applyHelmertTo(b, m_h_a, m_h_b, m_h_tx, m_h_ty);
        } else {
            aa = applyAffineTo(a, m_af_a1, m_af_a2, m_af_a3, m_af_b1, m_af_b2, m_af_b3);
            bb = applyAffineTo(b, m_af_a1, m_af_a2, m_af_a3, m_af_b1, m_af_b2, m_af_b3);
        }
        m_canvas->addDimension(aa, bb, th, layer);
    }
}
