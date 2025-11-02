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
#include <QLineEdit>
#include <QtMath>
#include <algorithm>
#include "pointmanager.h"
#include "canvaswidget.h"
#include "appsettings.h"

// GDAL/OSR for CRS transformations
#include <ogr_spatialref.h>
#include <ogr_api.h>

static double sqr(double v){ return v*v; }

TransformDialog::TransformDialog(PointManager* pm, CanvasWidget* canvas, QWidget* parent)
    : QDialog(parent), m_pm(pm), m_canvas(canvas)
{
    setWindowTitle("Transformations");
    resize(640, 480);
    QVBoxLayout* root = new QVBoxLayout(this);

    QFormLayout* top = new QFormLayout();
    m_mode = new QComboBox(this);
    m_mode->addItem("CRS Reproject (GDAL)");
    top->addRow("Mode:", m_mode);
    root->addLayout(top);
    // GDAL CRS controls
    m_srcCrsEdit = new QLineEdit(this);
    m_dstCrsEdit = new QLineEdit(this);
    m_srcCrsEdit->setPlaceholderText("e.g., EPSG:4326");
    m_dstCrsEdit->setPlaceholderText("e.g., EPSG:3857");
    m_srcCrsEdit->setText(AppSettings::crs());
    m_dstCrsEdit->setText(AppSettings::crs());
    top->addRow(new QLabel("Source CRS (GDAL):", this), m_srcCrsEdit);
    top->addRow(new QLabel("Target CRS (GDAL):", this), m_dstCrsEdit);

    root->addLayout(top);

    // GDAL-only mode: no control pairs table
    m_table = nullptr;

    // GDAL-only mode: remove control pair row management

    QHBoxLayout* actions = new QHBoxLayout();
    QPushButton* computeBtn = new QPushButton("Compute", this);
    QPushButton* applyPtsBtn = new QPushButton("Apply to Points", this);
    QPushButton* applyDrawBtn = new QPushButton("Apply to Drawing", this);
    actions->addWidget(computeBtn);
    actions->addStretch();
    actions->addWidget(applyPtsBtn);
    actions->addWidget(applyDrawBtn);
    root->addLayout(actions);

    m_report = new QTextEdit(this); m_report->setReadOnly(true);
    root->addWidget(m_report);

    connect(computeBtn, &QPushButton::clicked, this, &TransformDialog::computeTransform);
    connect(applyPtsBtn, &QPushButton::clicked, this, &TransformDialog::applyToPoints);
    connect(applyDrawBtn, &QPushButton::clicked, this, &TransformDialog::applyToDrawing);

    // Default CRS from settings
    if (m_srcCrsEdit && m_srcCrsEdit->text().isEmpty()) m_srcCrsEdit->setText(AppSettings::crs());
    if (m_dstCrsEdit && m_dstCrsEdit->text().isEmpty()) m_dstCrsEdit->setText(AppSettings::crs());
}

void TransformDialog::reload()
{
    // Nothing to reload for GDAL-only mode beyond ensuring defaults
    if (m_srcCrsEdit && m_srcCrsEdit->text().isEmpty()) m_srcCrsEdit->setText(AppSettings::crs());
    if (m_dstCrsEdit && m_dstCrsEdit->text().isEmpty()) m_dstCrsEdit->setText(AppSettings::crs());
}

void TransformDialog::fillPointCombosRow(int row)
{
    if (!m_pm) return;
    const QStringList names = m_pm->getPointNames();
    if (!m_table->cellWidget(row,0)) { auto* cb = new QComboBox(m_table); cb->addItems(names); m_table->setCellWidget(row,0,cb); }
    if (!m_table->cellWidget(row,1)) { auto* cb2 = new QComboBox(m_table); cb2->addItems(names); m_table->setCellWidget(row,1,cb2); }
    if (!m_table->cellWidget(row,2)) { auto* w = new QDoubleSpinBox(m_table); w->setRange(0.0, 1e9); w->setDecimals(6); w->setValue(1.0); m_table->setCellWidget(row,2,w); }
}

void TransformDialog::addRow()
{
    int r = m_table->rowCount(); m_table->insertRow(r); fillPointCombosRow(r);
}

void TransformDialog::removeSelected()
{
    auto sel = m_table->selectionModel()->selectedRows();
    for (int i=sel.size()-1;i>=0;--i) m_table->removeRow(sel.at(i).row());
}

QVector<TransformDialog::Pair> TransformDialog::collectPairs() const
{
    QVector<Pair> pairs;
    for (int r=0;r<m_table->rowCount();++r) {
        auto* cbS = qobject_cast<QComboBox*>(m_table->cellWidget(r,0));
        auto* cbD = qobject_cast<QComboBox*>(m_table->cellWidget(r,1));
        auto* w = qobject_cast<QDoubleSpinBox*>(m_table->cellWidget(r,2));
        if (!cbS || !cbD) continue;
        const QString s = cbS->currentText();
        const QString d = cbD->currentText();
        if (s.isEmpty() || d.isEmpty()) continue;
        Pair p; p.src = s; p.dst = d; p.weight = w ? w->value() : 1.0; pairs.append(p);
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

bool TransformDialog::gdalCreateTransform(const QString& srcCrs, const QString& dstCrs) const
{
    OGRSpatialReference srcSRS;
    OGRSpatialReference dstSRS;
    if (srcSRS.SetFromUserInput(srcCrs.toUtf8().constData()) != OGRERR_NONE) return false;
    if (dstSRS.SetFromUserInput(dstCrs.toUtf8().constData()) != OGRERR_NONE) return false;
#ifdef OAMS_TRADITIONAL_GIS_ORDER
    srcSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    dstSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
#endif
    OGRCoordinateTransformation* ct = OGRCreateCoordinateTransformation(&srcSRS, &dstSRS);
    if (!ct) return false;
    OCTDestroyCoordinateTransformation(ct);
    return true;
}

bool TransformDialog::gdalTransformXY(double& x, double& y, const QString& srcCrs, const QString& dstCrs) const
{
    OGRSpatialReference srcSRS;
    OGRSpatialReference dstSRS;
    if (srcSRS.SetFromUserInput(srcCrs.toUtf8().constData()) != OGRERR_NONE) return false;
    if (dstSRS.SetFromUserInput(dstCrs.toUtf8().constData()) != OGRERR_NONE) return false;
#ifdef OAMS_TRADITIONAL_GIS_ORDER
    srcSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    dstSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
#endif
    OGRCoordinateTransformation* ct = OGRCreateCoordinateTransformation(&srcSRS, &dstSRS);
    if (!ct) return false;
    double z = 0.0;
    int ok = ct->Transform(1, &x, &y, &z);
    OCTDestroyCoordinateTransformation(ct);
    return ok != 0;
}

void TransformDialog::computeTransform()
{
    m_hasSolution = false;
    if (!m_pm) { m_report->setText("No points"); return; }

    // GDAL CRS mode only
    {
        const QString src = m_srcCrsEdit ? m_srcCrsEdit->text().trimmed() : QString();
        const QString dst = m_dstCrsEdit ? m_dstCrsEdit->text().trimmed() : QString();
        if (src.isEmpty() || dst.isEmpty()) { m_report->setText("Enter Source and Target CRS (e.g., EPSG:4326)"); return; }
        if (!gdalCreateTransform(src, dst)) { m_report->setText("Failed to create GDAL transform. Check CRS codes."); return; }
        m_lastMode = Mode::GdalCRS;
        m_lastSrcCrs = src;
        m_lastDstCrs = dst;
        m_hasSolution = true;
        m_report->setText(QString("GDAL transform ready: %1 -> %2").arg(src, dst));
        return;
    }
    QVector<Pair> pairs = collectPairs(); if (pairs.size() < 2) { m_report->setText("Add at least two control pairs"); return; }
    QVector<QPointF> src, dst; src.reserve(pairs.size()); dst.reserve(pairs.size());
    QVector<double> w; w.reserve(pairs.size());
    double cx=0.0, cy=0.0; int cnt=0;
    for (const auto& p : pairs) {
        if (!m_pm->hasPoint(p.src) || !m_pm->hasPoint(p.dst)) continue;
        const Point ps = m_pm->getPoint(p.src);
        const Point pd = m_pm->getPoint(p.dst);
        src.push_back(QPointF(ps.x, ps.y)); dst.push_back(QPointF(pd.x, pd.y));
        cx += ps.x; cy += ps.y; cnt++;
        w.push_back(p.weight);
    }
    if (src.size() < 2) { m_report->setText("Insufficient valid pairs"); return; }
    cx /= std::max(1, cnt); cy /= std::max(1, cnt);
    if (m_weighting->currentIndex() > 0) {
        for (int i=0;i<src.size();++i) {
            const double d = std::hypot(src[i].x()-cx, src[i].y()-cy);
            const double eps = m_eps->value();
            double mult = 1.0 / std::max(eps, d);
            if (m_weighting->currentIndex() == 2) mult = mult*mult;
            w[i] *= mult;
        }
    }
    QString rep;
    double rms = 0.0;
    if (m_mode->currentIndex() == 0) {
        double a,b,tx,ty; if (!solveHelmert(src,dst,w,a,b,tx,ty,rms,rep)) { m_report->setText("Solve failed"); return; }
        m_lastMode = Mode::Helmert2D; m_h_a=a; m_h_b=b; m_h_tx=tx; m_h_ty=ty; m_hasSolution=true;
    } else {
        double a1,a2,a3,b1,b2,b3; if (!solveAffine(src,dst,w,a1,a2,a3,b1,b2,b3,rms,rep)) { m_report->setText("Solve failed"); return; }
        m_lastMode = Mode::Affine2D; m_af_a1=a1; m_af_a2=a2; m_af_a3=a3; m_af_b1=b1; m_af_b2=b2; m_af_b3=b3; m_hasSolution=true;
    }
    m_report->setText(rep);
}

void TransformDialog::applyToPoints()
{
    if (!m_hasSolution || !m_pm || !m_canvas) return;
    const QVector<Point> pts = m_pm->getAllPoints();
    for (const auto& p : pts) {
        QPointF q;
        if (m_lastMode == Mode::Helmert2D) {
            q = applyHelmertTo(QPointF(p.x, p.y), m_h_a, m_h_b, m_h_tx, m_h_ty);
        } else if (m_lastMode == Mode::Affine2D) {
            q = applyAffineTo(QPointF(p.x, p.y), m_af_a1, m_af_a2, m_af_a3, m_af_b1, m_af_b2, m_af_b3);
        } else /* GDAL */ {
            double x = p.x, y = p.y;
            if (!gdalTransformXY(x, y, m_lastSrcCrs, m_lastDstCrs)) continue;
            q = QPointF(x, y);
        }
        const QString prefix = (m_lastMode == Mode::GdalCRS) ? QStringLiteral("R_") : QStringLiteral("T_");
        const QString name = prefix + p.name;
        Point np(name, q.x(), q.y(), p.z);
        m_pm->addPoint(np);
        m_canvas->addPoint(np);
    }
}

void TransformDialog::applyToDrawing()
{
    if (!m_hasSolution || !m_canvas) return;
    const QString layer = (m_lastMode == Mode::GdalCRS) ? QStringLiteral("Reprojected") : QStringLiteral("Transformed");
    for (int i=0;i<m_canvas->lineCount();++i) {
        QPointF a,b; if (!m_canvas->lineEndpoints(i,a,b)) continue;
        QPointF aa = a, bb = b;
        if (m_lastMode==Mode::Helmert2D) {
            aa = applyHelmertTo(a, m_h_a, m_h_b, m_h_tx, m_h_ty);
            bb = applyHelmertTo(b, m_h_a, m_h_b, m_h_tx, m_h_ty);
        } else if (m_lastMode==Mode::Affine2D) {
            aa = applyAffineTo(a, m_af_a1, m_af_a2, m_af_a3, m_af_b1, m_af_b2, m_af_b3);
            bb = applyAffineTo(b, m_af_a1, m_af_a2, m_af_a3, m_af_b1, m_af_b2, m_af_b3);
        } else {
            double x1=a.x(), y1=a.y(); double x2=b.x(), y2=b.y();
            if (!gdalTransformXY(x1, y1, m_lastSrcCrs, m_lastDstCrs)) continue;
            if (!gdalTransformXY(x2, y2, m_lastSrcCrs, m_lastDstCrs)) continue;
            aa = QPointF(x1, y1); bb = QPointF(x2, y2);
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
            else if (m_lastMode==Mode::Affine2D) out.append(applyAffineTo(p, m_af_a1, m_af_a2, m_af_a3, m_af_b1, m_af_b2, m_af_b3));
            else { double x=p.x(), y=p.y(); if (gdalTransformXY(x,y, m_lastSrcCrs, m_lastDstCrs)) out.append(QPointF(x,y)); }
        }
        if (!out.isEmpty()) m_canvas->addPolylineEntity(out, closed, layer);
    }
    for (int i=0;i<m_canvas->textCount();++i) {
        QString t; QPointF pos; double h=0.0, ang=0.0; QString lay;
        if (!m_canvas->textData(i, t, pos, h, ang, lay)) continue;
        QPointF pp = pos;
        if (m_lastMode==Mode::Helmert2D) pp = applyHelmertTo(pos, m_h_a, m_h_b, m_h_tx, m_h_ty);
        else if (m_lastMode==Mode::Affine2D) pp = applyAffineTo(pos, m_af_a1, m_af_a2, m_af_a3, m_af_b1, m_af_b2, m_af_b3);
        else { double x=pos.x(), y=pos.y(); if (gdalTransformXY(x,y, m_lastSrcCrs, m_lastDstCrs)) pp = QPointF(x,y); else continue; }
        m_canvas->addText(t, pp, h, ang, layer);
    }
    for (int i=0;i<m_canvas->dimensionCount();++i) {
        QPointF a,b; double th=0.0; QString lay;
        if (!m_canvas->dimensionData(i, a, b, th, lay)) continue;
        QPointF aa=a, bb=b;
        if (m_lastMode==Mode::Helmert2D) { aa = applyHelmertTo(a, m_h_a, m_h_b, m_h_tx, m_h_ty); bb = applyHelmertTo(b, m_h_a, m_h_b, m_h_tx, m_h_ty); }
        else if (m_lastMode==Mode::Affine2D) { aa = applyAffineTo(a, m_af_a1, m_af_a2, m_af_a3, m_af_b1, m_af_b2, m_af_b3); bb = applyAffineTo(b, m_af_a1, m_af_a2, m_af_a3, m_af_b1, m_af_b2, m_af_b3); }
        else { double x1=a.x(), y1=a.y(); double x2=b.x(), y2=b.y(); if (!gdalTransformXY(x1,y1, m_lastSrcCrs, m_lastDstCrs)) continue; if (!gdalTransformXY(x2,y2, m_lastSrcCrs, m_lastDstCrs)) continue; aa=QPointF(x1,y1); bb=QPointF(x2,y2); }
        m_canvas->addDimension(aa, bb, th, layer);
    }
}
