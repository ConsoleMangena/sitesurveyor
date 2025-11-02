#include "intersectresectiondialog.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTextEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QLabel>
#include <QDateTime>
#include <QCheckBox>
#include <QSignalBlocker>
#include <QtMath>
#include "pointmanager.h"
#include "canvaswidget.h"
#include "surveycalculator.h"

static inline double deg2rad_(double d){ return SurveyCalculator::degreesToRadians(d); }
static inline double rad2deg_(double r){ return SurveyCalculator::radiansToDegrees(r); }
static inline double wrap180_(double a){ while (a>180.0) a-=360.0; while (a<-180.0) a+=360.0; return a; }

IntersectResectionDialog::IntersectResectionDialog(PointManager* pm, CanvasWidget* canvas, QWidget* parent)
    : QDialog(parent), m_pm(pm), m_canvas(canvas)
{
    setWindowTitle("Intersection / Resection");
    resize(560, 420);
    m_tabs = new QTabWidget(this);

    // Intersection tab
    QWidget* t1 = new QWidget(this);
    QVBoxLayout* v1 = new QVBoxLayout(t1);
    {
        QFormLayout* form = new QFormLayout();
        m_ptA = new QComboBox(this);
        m_ptB = new QComboBox(this);
        form->addRow("Point A:", m_ptA);
        form->addRow("Point B:", m_ptB);
        m_method = new QComboBox(this);
        m_method->addItems(QStringList() << "Distance-Distance" << "Bearing-Bearing" << "Bearing-Distance");
        form->addRow("Method:", m_method);
        m_distA = new QDoubleSpinBox(this); m_distA->setRange(0, 1e9); m_distA->setDecimals(3);
        m_distB = new QDoubleSpinBox(this); m_distB->setRange(0, 1e9); m_distB->setDecimals(3);
        m_bearA = new QDoubleSpinBox(this); m_bearA->setRange(-360,360); m_bearA->setDecimals(4);
        m_bearB = new QDoubleSpinBox(this); m_bearB->setRange(-360,360); m_bearB->setDecimals(4);
        form->addRow("Dist A:", m_distA);
        form->addRow("Dist B:", m_distB);
        form->addRow("Bear A (deg):", m_bearA);
        form->addRow("Bear B (deg):", m_bearB);
        m_side = new QComboBox(this); m_side->addItems(QStringList()<<"Left of AB"<<"Right of AB");
        form->addRow("Side:", m_side);
        m_outName = new QLineEdit(this); m_outName->setPlaceholderText("New point name");
        form->addRow("Out name:", m_outName);
        v1->addLayout(form);
        QHBoxLayout* opts = new QHBoxLayout();
        m_addPointCheck = new QCheckBox("Add point", this); m_addPointCheck->setChecked(true);
        m_drawLinesCheck = new QCheckBox("Draw lines", this); m_drawLinesCheck->setChecked(true);
        opts->addWidget(m_addPointCheck); opts->addWidget(m_drawLinesCheck); opts->addStretch();
        QPushButton* computeBtn = new QPushButton("Compute", this);
        opts->addWidget(computeBtn);
        v1->addLayout(opts);
        m_report = new QTextEdit(this); m_report->setReadOnly(true);
        v1->addWidget(m_report);
        connect(computeBtn, &QPushButton::clicked, this, &IntersectResectionDialog::computeIntersection);
    }

    // Resection tab
    QWidget* t2 = new QWidget(this);
    QVBoxLayout* v2 = new QVBoxLayout(t2);
    {
        m_resTable = new QTableWidget(0, 7, this);
        m_resTable->setHorizontalHeaderLabels(QStringList()
                                              << "Control A"
                                              << "Type"
                                              << "Control B"
                                              << "Angle/Dir (deg)"
                                              << "Distance"
                                              << "σangle (deg)"
                                              << "σdist");
        m_resTable->horizontalHeader()->setStretchLastSection(true);
        v2->addWidget(m_resTable);
        QHBoxLayout* rowBtns = new QHBoxLayout();
        QPushButton* addRow = new QPushButton("Add Row", this);
        QPushButton* delRow = new QPushButton("Remove Selected", this);
        rowBtns->addWidget(addRow); rowBtns->addWidget(delRow); rowBtns->addStretch();
        v2->addLayout(rowBtns);
        QFormLayout* rf = new QFormLayout();
        m_resOutName = new QLineEdit(this); m_resOutName->setPlaceholderText("Station name");
        rf->addRow("Out name:", m_resOutName);
        v2->addLayout(rf);
        QHBoxLayout* act = new QHBoxLayout();
        QPushButton* compR = new QPushButton("Compute Resection", this);
        act->addStretch(); act->addWidget(compR);
        v2->addLayout(act);
        m_resReport = new QTextEdit(this); m_resReport->setReadOnly(true);
        v2->addWidget(m_resReport);
        connect(addRow, &QPushButton::clicked, this, [this](){ m_resTable->insertRow(m_resTable->rowCount()); reload(); });
        connect(delRow, &QPushButton::clicked, this, [this](){
            auto sel = m_resTable->selectionModel()->selectedRows();
            for (int i=sel.size()-1;i>=0;--i) m_resTable->removeRow(sel.at(i).row());
        });
        connect(compR, &QPushButton::clicked, this, &IntersectResectionDialog::computeResection);
    }

    m_tabs->addTab(t1, "Intersection");
    m_tabs->addTab(t2, "Resection (LS)");

    QVBoxLayout* root = new QVBoxLayout(this);
    root->addWidget(m_tabs);
    setLayout(root);
    reload();
}

void IntersectResectionDialog::reload()
{
    if (!m_pm) return;
    const QStringList names = m_pm->getPointNames();
    auto refill = [&](QComboBox* cb){ if (!cb) return; QSignalBlocker b(cb); cb->clear(); cb->addItems(names); };
    refill(m_ptA); refill(m_ptB);
    // Update controls in table
    for (int r=0;r<m_resTable->rowCount();++r) {
        // Control A
        if (!m_resTable->cellWidget(r,0)) {
            QComboBox* cb = new QComboBox(m_resTable); cb->addItems(names); m_resTable->setCellWidget(r,0,cb);
        } else { QComboBox* cb = qobject_cast<QComboBox*>(m_resTable->cellWidget(r,0)); if (cb) { QSignalBlocker b(cb); cb->clear(); cb->addItems(names); } }
        // Type
        if (!m_resTable->cellWidget(r,1)) {
            QComboBox* tp = new QComboBox(m_resTable); tp->addItems(QStringList()<<"Bearing"<<"Distance"<<"Bearing+Distance"<<"Direction"<<"Angle A→B"); m_resTable->setCellWidget(r,1,tp);
        } else { QComboBox* tp = qobject_cast<QComboBox*>(m_resTable->cellWidget(r,1)); if (tp) { QSignalBlocker b(tp); tp->clear(); tp->addItems(QStringList()<<"Bearing"<<"Distance"<<"Bearing+Distance"<<"Direction"<<"Angle A→B"); } }
        // Control B (for Angle)
        if (!m_resTable->cellWidget(r,2)) {
            QComboBox* cb2 = new QComboBox(m_resTable); cb2->addItem(""); cb2->addItems(names); m_resTable->setCellWidget(r,2,cb2);
        } else { QComboBox* cb2 = qobject_cast<QComboBox*>(m_resTable->cellWidget(r,2)); if (cb2) { QSignalBlocker b(cb2); cb2->clear(); cb2->addItem(""); cb2->addItems(names); } }
        // Angle/Dir
        if (!m_resTable->cellWidget(r,3)) {
            QDoubleSpinBox* ang = new QDoubleSpinBox(m_resTable); ang->setRange(-360,360); ang->setDecimals(4); m_resTable->setCellWidget(r,3,ang);
        }
        // Distance
        if (!m_resTable->cellWidget(r,4)) {
            QDoubleSpinBox* dd = new QDoubleSpinBox(m_resTable); dd->setRange(0, 1e12); dd->setDecimals(3); m_resTable->setCellWidget(r,4,dd);
        }
        // σangle
        if (!m_resTable->cellWidget(r,5)) {
            QDoubleSpinBox* sb = new QDoubleSpinBox(m_resTable); sb->setRange(0, 1e3); sb->setDecimals(5); sb->setValue(0.1); m_resTable->setCellWidget(r,5,sb);
        }
        // σdist
        if (!m_resTable->cellWidget(r,6)) {
            QDoubleSpinBox* sd = new QDoubleSpinBox(m_resTable); sd->setRange(0, 1e6); sd->setDecimals(4); sd->setValue(0.01); m_resTable->setCellWidget(r,6,sd);
        }
    }
}

static inline double cross2D(const QPointF& a, const QPointF& b){ return a.x()*b.y() - a.y()*b.x(); }

bool IntersectResectionDialog::circleCircle(const QPointF& A, double rA, const QPointF& B, double rB, bool leftOfAB, QPointF& out) const
{
    QPointF d = B - A; double D = qSqrt(QPointF::dotProduct(d,d)); if (D<1e-12) return false;
    double a = (rA*rA - rB*rB + D*D) / (2*D);
    double h2 = rA*rA - a*a; if (h2 < 0) h2 = 0;
    double h = qSqrt(h2);
    QPointF P0 = A + (a/D) * d;
    QPointF perp(-d.y()/D, d.x()/D);
    QPointF i1 = P0 + h * perp;
    QPointF i2 = P0 - h * perp;
    QPointF vAB = d;
    bool left1 = cross2D(vAB, i1 - A) > 0;
    out = (leftOfAB ? (left1 ? i1 : i2) : (left1 ? i2 : i1));
    return true;
}

bool IntersectResectionDialog::rayRay(const QPointF& A, double azAdeg, const QPointF& B, double azBdeg, QPointF& out) const
{
    double ra = deg2rad_(azAdeg); QPointF da(qSin(ra), qCos(ra));
    double rb = deg2rad_(azBdeg); QPointF db(qSin(rb), qCos(rb));
    QPointF r = B - A; double det = cross2D(da, db); if (qFuzzyIsNull(det)) return false;
    double t = cross2D(r, db) / det; out = A + da * t; return true;
}

bool IntersectResectionDialog::rayCircle(const QPointF& A, double azAdeg, const QPointF& B, double rB, bool leftOfAB, QPointF& out) const
{
    double ra = deg2rad_(azAdeg); QPointF da(qSin(ra), qCos(ra));
    QPointF m = A; QPointF c = B; QPointF mc = m - c;
    double A2 = QPointF::dotProduct(da, da);
    double B2 = 2.0 * QPointF::dotProduct(da, mc);
    double C2 = QPointF::dotProduct(mc, mc) - rB*rB;
    double disc = B2*B2 - 4*A2*C2; if (disc < 0) return false;
    double sqrtDisc = qSqrt(disc);
    double t1 = (-B2 + sqrtDisc) / (2*A2);
    double t2 = (-B2 - sqrtDisc) / (2*A2);
    QPointF i1 = m + da * t1;
    QPointF i2 = m + da * t2;
    QPointF vAB = B - A; bool left1 = cross2D(vAB, i1 - A) > 0;
    out = (leftOfAB ? (left1 ? i1 : i2) : (left1 ? i2 : i1));
    return true;
}

void IntersectResectionDialog::computeIntersection()
{
    if (!m_pm) return;
    QString aName = m_ptA->currentText(); QString bName = m_ptB->currentText();
    if (!m_pm->hasPoint(aName) || !m_pm->hasPoint(bName)) { m_report->setText("Pick valid A and B."); return; }
    Point A = m_pm->getPoint(aName); Point B = m_pm->getPoint(bName);
    QPointF out; bool ok=false; QString method = m_method->currentText(); bool left = (m_side->currentIndex()==0);
    if (method.startsWith("Distance")) {
        ok = circleCircle(A.toQPointF(), m_distA->value(), B.toQPointF(), m_distB->value(), left, out);
    } else if (method.startsWith("Bearing-Bearing")) {
        ok = rayRay(A.toQPointF(), m_bearA->value(), B.toQPointF(), m_bearB->value(), out);
    } else {
        ok = rayCircle(A.toQPointF(), m_bearA->value(), B.toQPointF(), m_distB->value(), left, out);
    }
    if (!ok) { m_report->setText("No valid intersection."); return; }
    QString name = m_outName->text().trimmed(); if (name.isEmpty()) name = QString("INT_%1_%2").arg(aName, bName);
    QString rep;
    rep += QString("A=(%1,%2) B=(%3,%4)\n").arg(A.x,0,'f',3).arg(A.y,0,'f',3).arg(B.x,0,'f',3).arg(B.y,0,'f',3);
    rep += QString("Result: %1 = (%2, %3)\n").arg(name).arg(out.x(),0,'f',3).arg(out.y(),0,'f',3);
    double dA = SurveyCalculator::distance(A.toQPointF(), out);
    double dB = SurveyCalculator::distance(B.toQPointF(), out);
    double azA = SurveyCalculator::azimuth(A.toQPointF(), out);
    double azB = SurveyCalculator::azimuth(B.toQPointF(), out);
    rep += QString("From A: d=%1, az=%2°\n").arg(dA,0,'f',3).arg(azA,0,'f',4);
    rep += QString("From B: d=%1, az=%2°\n").arg(dB,0,'f',3).arg(azB,0,'f',4);
    m_report->setText(rep);
    if (m_addPointCheck->isChecked()) {
        Point P(name, out.x(), out.y(), 0.0);
        m_pm->addPoint(P);
        if (m_canvas) m_canvas->addPoint(P);
    }
    if (m_drawLinesCheck->isChecked() && m_canvas) {
        m_canvas->addLine(A.toQPointF(), out);
        m_canvas->addLine(B.toQPointF(), out);
    }
}

bool IntersectResectionDialog::resectionMixedLS(const QVector<QPointF>& ctrls, const QVector<ResObs>& obs,
                          QPointF& Pxy,
                          double& N11, double& N12, double& N22,
                          double& thetaDegOut,
                          QVector<double>* residualsOut,
                          QStringList* labelsOut) const
{
    if (ctrls.isEmpty() || obs.isEmpty()) return false;
    // Initial guess: centroid and theta=0
    QPointF x(0,0); for (const auto& c: ctrls) x += c; if (!ctrls.isEmpty()) x /= double(ctrls.size());
    double t = 0.0; // deg
    double Nxx=0,Nxy=0,Nyy=0,NxT=0,NyT=0,NTT=0;
    for (int iter=0; iter<25; ++iter) {
        Nxx=Nxy=Nyy=NxT=NyT=NTT=0.0; double bx=0, by=0, bT=0; double maxstep=0;
        for (const auto& o : obs) {
            // Control->P vector
            const QPointF C1 = ctrls[o.ctrlIndex]; QPointF v1 = x - C1; double dx=v1.x(), dy=v1.y(); double r2=dx*dx+dy*dy; double r=qSqrt(qMax(1e-12,r2));
            double dadx1 = (180.0/M_PI)*(-dy/r2); double dady1 = (180.0/M_PI)*(dx/r2);
            if (o.type==ObsBearing || o.type==ObsBearDist) {
                double comp = rad2deg_(qAtan2(dx, dy)); double vi = wrap180_(o.bearingDeg - comp);
                double w = 1.0 / qMax(1e-12, o.sb*o.sb);
                Nxx += w*dadx1*dadx1; Nxy += w*dadx1*dady1; Nyy += w*dady1*dady1; bx += w*dadx1*vi; by += w*dady1*vi;
            }
            if (o.type==ObsDistance || o.type==ObsBearDist) {
                double vi = o.distance - r; double w = 1.0 / qMax(1e-12, o.sd*o.sd);
                double ddx = dx/r; double ddy = dy/r; Nxx += w*ddx*ddx; Nxy += w*ddx*ddy; Nyy += w*ddy*ddy; bx += w*ddx*vi; by += w*ddy*vi;
            }
            if (o.type==ObsDirection) {
                // P->Control vector
                double ux = C1.x() - x.x(), uy = C1.y() - x.y(); double r2p = ux*ux+uy*uy;
                double comp = rad2deg_(qAtan2(ux, uy));
                double vi = wrap180_(o.bearingDeg + t - comp);
                double w = 1.0 / qMax(1e-12, o.sb*o.sb);
                // d(atan2(ux,uy))/dP.x = (uy)/r2, d/dP.y = -(ux)/r2
                double dadxP = (180.0/M_PI) * ( (C1.y()-x.y())/r2p );
                double dadyP = (180.0/M_PI) * ( (x.x()-C1.x())/r2p );
                Nxx += w*dadxP*dadxP; Nxy += w*dadxP*dadyP; Nyy += w*dadyP*dadyP; NxT += w*dadxP; NyT += w*dadyP; NTT += w;
                bx += w*dadxP*vi; by += w*dadyP*vi; bT += w*vi;
            }
            if (o.type==ObsAngle) {
                const QPointF C2 = ctrls[o.ctrlIndex2];
                double u1x = C1.x()-x.x(), u1y = C1.y()-x.y(); double r2a = u1x*u1x+u1y*u1y;
                double u2x = C2.x()-x.x(), u2y = C2.y()-x.y(); double r2b = u2x*u2x+u2y*u2y;
                double a1 = rad2deg_(qAtan2(u1x, u1y)); double a2 = rad2deg_(qAtan2(u2x, u2y));
                double vi = wrap180_(o.bearingDeg - (a2 - a1));
                double w = 1.0 / qMax(1e-12, o.sb*o.sb);
                // d a(P->C)/dP.x = (C.y-P.y)/r2, d/dP.y = -(C.x-P.x)/r2
                double dadxA = (180.0/M_PI) * ( (C1.y()-x.y())/r2a );
                double dadyA = (180.0/M_PI) * ( (x.x()-C1.x())/r2a );
                double dadxB = (180.0/M_PI) * ( (C2.y()-x.y())/r2b );
                double dadyB = (180.0/M_PI) * ( (x.x()-C2.x())/r2b );
                double dax = (dadxB - dadxA), day = (dadyB - dadyA);
                Nxx += w*dax*dax; Nxy += w*dax*day; Nyy += w*day*day; bx += w*dax*vi; by += w*day*vi;
            }
        }
        // Schur complement reduction to XY
        double NTTs = qMax(1e-18, NTT);
        double S11 = Nxx - (NxT*NxT)/NTTs;
        double S12 = Nxy - (NxT*NyT)/NTTs;
        double S22 = Nyy - (NyT*NyT)/NTTs;
        double sb1 = bx  - (NxT*bT)/NTTs;
        double sb2 = by  - (NyT*bT)/NTTs;
        double detS = S11*S22 - S12*S12; if (qAbs(detS) < 1e-18) break;
        double dxs = ( S22*sb1 - S12*sb2)/detS;
        double dys = (-S12*sb1 + S11*sb2)/detS;
        double dts = (bT - NxT*dxs - NyT*dys) / NTTs;
        x.setX(x.x()+dxs); x.setY(x.y()+dys); t += dts;
        N11=S11; N12=S12; N22=S22;
        maxstep = qMax(qAbs(dxs), qMax(qAbs(dys), qAbs(dts)));
        if (maxstep < 1e-8) break;
    }
    Pxy = x; thetaDegOut = t;
    if (residualsOut || labelsOut) {
        if (residualsOut) residualsOut->clear(); if (labelsOut) labelsOut->clear();
        for (const auto& o : obs) {
            const QPointF C1 = ctrls[o.ctrlIndex]; QPointF v1 = x - C1; double dx=v1.x(), dy=v1.y(); double r2=dx*dx+dy*dy; double r=qSqrt(qMax(1e-12,r2));
            if (o.type==ObsBearing || o.type==ObsBearDist) {
                double comp = rad2deg_(qAtan2(dx, dy)); double vi = wrap180_(o.bearingDeg - comp);
                if (residualsOut) residualsOut->append(vi); if (labelsOut) labelsOut->append("B");
            }
            if (o.type==ObsDistance || o.type==ObsBearDist) {
                double comp = r; double vi = o.distance - comp; if (residualsOut) residualsOut->append(vi); if (labelsOut) labelsOut->append("D");
            }
            if (o.type==ObsDirection) {
                double ux = C1.x() - x.x(), uy = C1.y() - x.y(); double comp = rad2deg_(qAtan2(ux, uy)); double vi = wrap180_(o.bearingDeg + t - comp);
                if (residualsOut) residualsOut->append(vi); if (labelsOut) labelsOut->append("Dir");
            }
            if (o.type==ObsAngle) {
                const QPointF C2 = ctrls[o.ctrlIndex2]; double a1 = rad2deg_(qAtan2(C1.x()-x.x(), C1.y()-x.y())); double a2 = rad2deg_(qAtan2(C2.x()-x.x(), C2.y()-x.y()));
                double vi = wrap180_(o.bearingDeg - (a2 - a1)); if (residualsOut) residualsOut->append(vi); if (labelsOut) labelsOut->append("Ang");
            }
        }
    }
    return true;
}

void IntersectResectionDialog::computeResection()
{
    if (!m_pm) return;
    QVector<QPointF> ctrls; QVector<ResObs> obs;
    for (int r=0;r<m_resTable->rowCount();++r) {
        QComboBox* cbA = qobject_cast<QComboBox*>(m_resTable->cellWidget(r,0));
        QComboBox* tp  = qobject_cast<QComboBox*>(m_resTable->cellWidget(r,1));
        QComboBox* cbB = qobject_cast<QComboBox*>(m_resTable->cellWidget(r,2));
        QDoubleSpinBox* ang = qobject_cast<QDoubleSpinBox*>(m_resTable->cellWidget(r,3));
        QDoubleSpinBox* dist = qobject_cast<QDoubleSpinBox*>(m_resTable->cellWidget(r,4));
        QDoubleSpinBox* sb = qobject_cast<QDoubleSpinBox*>(m_resTable->cellWidget(r,5));
        QDoubleSpinBox* sd = qobject_cast<QDoubleSpinBox*>(m_resTable->cellWidget(r,6));
        if (!cbA || !tp) continue; QString aName = cbA->currentText(); if (!m_pm->hasPoint(aName)) continue; QPointF CA = m_pm->getPoint(aName).toQPointF();
        int iA = ctrls.indexOf(CA); if (iA == -1) { ctrls.append(CA); iA = ctrls.size()-1; }
        int tpi = tp->currentIndex();
        if (tpi == 0) { // Bearing
            obs.append(ResObs{iA, iA, ObsBearing, ang?ang->value():0.0, 0.0, sb?sb->value():0.1, 0.0});
        } else if (tpi == 1) { // Distance
            obs.append(ResObs{iA, iA, ObsDistance, 0.0, dist?dist->value():0.0, 0.0, sd?sd->value():0.01});
        } else if (tpi == 2) { // Bearing+Distance
            obs.append(ResObs{iA, iA, ObsBearDist, ang?ang->value():0.0, dist?dist->value():0.0, sb?sb->value():0.1, sd?sd->value():0.01});
        } else if (tpi == 3) { // Direction
            obs.append(ResObs{iA, iA, ObsDirection, ang?ang->value():0.0, 0.0, sb?sb->value():0.1, 0.0});
        } else if (tpi == 4) { // Angle A->B
            int iB = iA;
            if (cbB) { QString bName = cbB->currentText(); if (m_pm->hasPoint(bName)) { QPointF CB = m_pm->getPoint(bName).toQPointF(); int idxB = ctrls.indexOf(CB); if (idxB==-1){ ctrls.append(CB); idxB=ctrls.size()-1; } iB = idxB; }}
            obs.append(ResObs{iA, iB, ObsAngle, ang?ang->value():0.0, 0.0, sb?sb->value():0.1, 0.0});
        }
    }
    if (obs.size() < 2) { m_resReport->setText("Add at least two observations."); return; }
    QPointF P; double N11=0,N12=0,N22=0, thetaDeg=0; QVector<double> resids; QStringList labels;
    if (!resectionMixedLS(ctrls, obs, P, N11, N12, N22, thetaDeg, &resids, &labels)) { m_resReport->setText("Failed to compute."); return; }
    QString name = m_resOutName->text().trimmed(); if (name.isEmpty()) name = QString("RESEC_%1").arg(QDateTime::currentDateTime().toString("HHmmss"));
    // Compute weighted posterior variance sigma0^2 = v^T W v / (m-3)
    int mcnt = 0; double vWv = 0.0;
    for (int i=0;i<obs.size();++i) {
        const auto& o = obs[i]; const QPointF C1 = ctrls[o.ctrlIndex]; QPointF v1p = P - C1; double dx=v1p.x(), dy=v1p.y(); double r2=dx*dx+dy*dy; double r=qSqrt(qMax(1e-12,r2));
        if (o.type==ObsBearing || o.type==ObsBearDist) { double comp = rad2deg_(qAtan2(dx, dy)); double vi = wrap180_(o.bearingDeg - comp); double w=1.0/qMax(1e-12,o.sb*o.sb); vWv += w*vi*vi; ++mcnt; }
        if (o.type==ObsDistance || o.type==ObsBearDist) { double comp = r; double vi = o.distance - comp; double w=1.0/qMax(1e-12,o.sd*o.sd); vWv += w*vi*vi; ++mcnt; }
        if (o.type==ObsDirection) { double ux = C1.x()-P.x(), uy=C1.y()-P.y(); double comp = rad2deg_(qAtan2(ux, uy)); double vi = wrap180_(o.bearingDeg + thetaDeg - comp); double w=1.0/qMax(1e-12,o.sb*o.sb); vWv += w*vi*vi; ++mcnt; }
        if (o.type==ObsAngle) { const QPointF C2 = ctrls[o.ctrlIndex2]; double a1=rad2deg_(qAtan2(C1.x()-P.x(), C1.y()-P.y())); double a2=rad2deg_(qAtan2(C2.x()-P.x(), C2.y()-P.y())); double vi=wrap180_(o.bearingDeg - (a2-a1)); double w=1.0/qMax(1e-12,o.sb*o.sb); vWv += w*vi*vi; ++mcnt; }
    }
    const int u = 3; double s0sq = (mcnt>u) ? (vWv / double(mcnt - u)) : 1.0;
    double det = N11*N22 - N12*N12; double c11=0,c12=0,c22=0;
    if (qAbs(det) > 1e-18) {
        c11 =  N22/det * s0sq; c12 = -N12/det * s0sq; c22 =  N11/det * s0sq;
    }
    // Error ellipse (1-sigma) from covariance
    double tr = c11 + c22; double de = c11*c22 - c12*c12; double root = qSqrt(qMax(0.0, tr*tr/4.0 - de));
    double l1 = tr/2.0 + root; double l2 = tr/2.0 - root; // eigenvalues
    double a = qSqrt(qMax(0.0, l1)); double b = qSqrt(qMax(0.0, l2));
    double theta = 0.5 * qAtan2(2.0*c12, (c11 - c22)); double phiDeg = rad2deg_(theta);
    QString rep = QString("Resection (mixed)\nP = (%1, %2)\nOrientation θ = %3°\n")
        .arg(P.x(),0,'f',3).arg(P.y(),0,'f',3).arg(thetaDeg,0,'f',3);
    // Residual list with types
    QStringList resTxt; resTxt.reserve(resids.size());
    for (int i=0;i<resids.size();++i) resTxt << QString("%1=%2").arg(labels.value(i)).arg(resids[i],0,'f',4);
    rep += QString("Residuals: %1\n").arg(resTxt.join(", "));
    rep += QString("Sigma0^2: %1\n").arg(s0sq,0,'f',6);
    rep += QString("Ellipse (1σ): a=%1, b=%2, θ=%3°\n").arg(a,0,'f',4).arg(b,0,'f',4).arg(phiDeg,0,'f',2);
    m_resReport->setText(rep);
    Point newP(name, P.x(), P.y(), 0.0); m_pm->addPoint(newP); if (m_canvas) m_canvas->addPoint(newP);
}
