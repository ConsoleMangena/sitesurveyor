#include "lsnetworkdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTextEdit>
#include <QLineEdit>
#include <QDateTime>
#include <QtMath>
#include "pointmanager.h"
#include "canvaswidget.h"
#include "surveycalculator.h"

LSNetworkDialog::LSNetworkDialog(PointManager* pm, CanvasWidget* canvas, QWidget* parent)
    : QDialog(parent), m_pm(pm), m_canvas(canvas)
{
    setWindowTitle("Network LS (Point)");
    resize(560, 420);
    QVBoxLayout* root = new QVBoxLayout(this);

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels(QStringList()<<"Control"<<"Type"<<"Value"<<"Sigma");
    m_table->horizontalHeader()->setStretchLastSection(true);
    root->addWidget(m_table);

    QHBoxLayout* rows = new QHBoxLayout();
    QPushButton* addRow = new QPushButton("Add Row", this);
    QPushButton* delRow = new QPushButton("Remove Selected", this);
    rows->addWidget(addRow); rows->addWidget(delRow); rows->addStretch();
    root->addLayout(rows);

    QFormLayout* opts = new QFormLayout();
    m_outName = new QLineEdit(this); m_outName->setPlaceholderText("New point name");
    opts->addRow("Out name:", m_outName);
    root->addLayout(opts);

    QHBoxLayout* act = new QHBoxLayout();
    QPushButton* computeBtn = new QPushButton("Compute", this); act->addStretch(); act->addWidget(computeBtn);
    root->addLayout(act);

    m_report = new QTextEdit(this); m_report->setReadOnly(true);
    root->addWidget(m_report);

    connect(addRow, &QPushButton::clicked, this, &LSNetworkDialog::addRow);
    connect(delRow, &QPushButton::clicked, this, &LSNetworkDialog::removeSelected);
    connect(computeBtn, &QPushButton::clicked, this, &LSNetworkDialog::compute);

    reload();
}

void LSNetworkDialog::reload()
{
    if (!m_pm) return;
    const QStringList names = m_pm->getPointNames();
    for (int r=0;r<m_table->rowCount();++r) {
        if (!m_table->cellWidget(r,0)) {
            QComboBox* cb = new QComboBox(m_table); cb->addItems(names); m_table->setCellWidget(r,0,cb);
        } else { QComboBox* cb = qobject_cast<QComboBox*>(m_table->cellWidget(r,0)); if (cb) { QSignalBlocker b(cb); cb->clear(); cb->addItems(names); } }
        if (!m_table->cellWidget(r,1)) {
            QComboBox* tp = new QComboBox(m_table); tp->addItems(QStringList()<<"Distance"<<"Bearing"); m_table->setCellWidget(r,1,tp);
        }
        if (!m_table->cellWidget(r,2)) {
            QDoubleSpinBox* val = new QDoubleSpinBox(m_table); val->setRange(-1e9, 1e9); val->setDecimals(6); m_table->setCellWidget(r,2,val);
        }
        if (!m_table->cellWidget(r,3)) {
            QDoubleSpinBox* sig = new QDoubleSpinBox(m_table); sig->setRange(0, 1e6); sig->setDecimals(6); sig->setValue(0.01); m_table->setCellWidget(r,3,sig);
        }
    }
}

void LSNetworkDialog::addRow()
{
    int r = m_table->rowCount(); m_table->insertRow(r);
    reload();
}

void LSNetworkDialog::removeSelected()
{
    auto sel = m_table->selectionModel()->selectedRows();
    for (int i=sel.size()-1;i>=0;--i) m_table->removeRow(sel.at(i).row());
}

bool LSNetworkDialog::solveLS(const QVector<QPointF>& ctrls, const QVector<Obs>& obs,
                 QPointF& X, double& s0sq, double& c11, double& c12, double& c22,
                 QVector<double>* residualsOut, QStringList* typesOut) const
{
    if (ctrls.isEmpty() || obs.isEmpty()) return false;
    // Initial guess: centroid
    QPointF x(0,0); for (const auto& c : ctrls) { x += c; } x /= double(ctrls.size());
    for (int iter=0; iter<20; ++iter) {
        double N11=0,N12=0,N22=0, b1=0, b2=0; double maxdx=0;
        for (const auto& o : obs) {
            const QPointF C = ctrls[o.ctrlIndex]; QPointF v = x - C; double dx=v.x(), dy=v.y(); double r2 = dx*dx+dy*dy; double r = qSqrt(qMax(1e-12, r2));
            if (o.type == Distance) {
                double comp = r; double vi = o.value - comp; double w = (o.sigma>0? 1.0/(o.sigma*o.sigma) : 1.0);
                double dax = dx/r; double day = dy/r;
                N11 += w*dax*dax; N12 += w*dax*day; N22 += w*day*day; b1 += w*dax*vi; b2 += w*day*vi;
            } else {
                double comp = SurveyCalculator::radiansToDegrees(qAtan2(dx, dy));
                double vi = SurveyCalculator::normalizeAngle(o.value - comp); double w = (o.sigma>0? 1.0/(o.sigma*o.sigma) : 100.0);
                double dadx =  (180.0/M_PI) * (-dy/r2);
                double dady =  (180.0/M_PI) * ( dx/r2);
                N11 += w*dadx*dadx; N12 += w*dadx*dady; N22 += w*dady*dady; b1 += w*dadx*vi; b2 += w*dady*vi;
            }
        }
        double det = N11*N22 - N12*N12; if (qAbs(det) < 1e-18) break;
        double dx = ( N22*b1 - N12*b2)/det; double dy = (-N12*b1 + N11*b2)/det;
        x.setX(x.x() + dx); x.setY(x.y() + dy);
        maxdx = qMax(qAbs(dx), qAbs(dy)); if (maxdx < 1e-8) break;
    }
    X = x;
    // Variance and covariance
    double N11=0,N12=0,N22=0; double vTPlv=0; int m=0;
    if (residualsOut) residualsOut->clear(); if (typesOut) typesOut->clear();
    for (const auto& o : obs) {
        const QPointF C = ctrls[o.ctrlIndex]; QPointF d = x - C; double dx=d.x(), dy=d.y(); double r2 = dx*dx+dy*dy; double r = qSqrt(qMax(1e-12, r2));
        if (o.type == Distance) {
            double comp=r; double vi=o.value-comp; double w=(o.sigma>0? 1.0/(o.sigma*o.sigma):1.0); double dax=dx/r, day=dy/r; N11+=w*dax*dax; N12+=w*dax*day; N22+=w*day*day; vTPlv+=w*vi*vi; ++m; if (residualsOut) residualsOut->append(vi); if (typesOut) typesOut->append("D");
        } else {
            double comp=SurveyCalculator::radiansToDegrees(qAtan2(dx, dy)); double vi=SurveyCalculator::normalizeAngle(o.value-comp); double w=(o.sigma>0?1.0/(o.sigma*o.sigma):100.0);
            double dadx=(180.0/M_PI)*(-dy/r2); double dady=(180.0/M_PI)*(dx/r2); N11+=w*dadx*dadx; N12+=w*dadx*dady; N22+=w*dady*dady; vTPlv+=w*vi*vi; ++m; if (residualsOut) residualsOut->append(vi); if (typesOut) typesOut->append("B");
        }
    }
    double det = N11*N22 - N12*N12; if (qAbs(det) < 1e-18) { s0sq=1.0; c11=c12=c22=0; return true; }
    int u = 2; s0sq = (m>u) ? (vTPlv / double(m-u)) : 1.0;
    c11 =  N22/det * s0sq; c12 = -N12/det * s0sq; c22 =  N11/det * s0sq;
    return true;
}

void LSNetworkDialog::compute()
{
    if (!m_pm) return;
    QStringList names = m_pm->getPointNames();
    QVector<QPointF> ctrls; ctrls.reserve(names.size());
    for (const auto& n : names) { if (m_pm->hasPoint(n)) ctrls.append(m_pm->getPoint(n).toQPointF()); }
    QVector<Obs> obs; obs.reserve(m_table->rowCount());
    for (int r=0;r<m_table->rowCount();++r) {
        QComboBox* cb = qobject_cast<QComboBox*>(m_table->cellWidget(r,0));
        QComboBox* tp = qobject_cast<QComboBox*>(m_table->cellWidget(r,1));
        QDoubleSpinBox* val = qobject_cast<QDoubleSpinBox*>(m_table->cellWidget(r,2));
        QDoubleSpinBox* sig = qobject_cast<QDoubleSpinBox*>(m_table->cellWidget(r,3));
        if (!cb || !tp || !val) continue; int idx = cb->currentIndex(); if (idx < 0 || idx >= names.size()) continue;
        ObsType t = (tp->currentIndex()==0) ? Distance : Bearing;
        double sigma = sig?sig->value(): (t==Distance?0.01:0.1);
        obs.append(Obs{idx, t, val->value(), sigma});
    }
    if (ctrls.isEmpty() || obs.isEmpty()) { m_report->setText("Add observations."); return; }
    QPointF X; double s0sq=1.0,c11=0,c12=0,c22=0; QVector<double> resids; QStringList types;
    if (!solveLS(ctrls, obs, X, s0sq, c11, c12, c22, &resids, &types)) { m_report->setText("Failed to solve."); return; }

    double tr=c11+c22; double de=c11*c22-c12*c12; double root=qSqrt(qMax(0.0, tr*tr/4.0 - de));
    double l1=tr/2.0+root; double l2=tr/2.0-root; double a=qSqrt(qMax(0.0,l1)); double b=qSqrt(qMax(0.0,l2));
    double theta=0.5*qAtan2(2.0*c12,(c11-c22)); double thetaDeg=SurveyCalculator::radiansToDegrees(theta);

    QString name = m_outName->text().trimmed(); if (name.isEmpty()) name = QString("LS_%1").arg(QDateTime::currentDateTime().toString("HHmmss"));
    QString rep = QString("LS Solution\nP=(%1,%2)\nSigma0^2=%3\nEllipse (1σ): a=%4, b=%5, θ=%6°\n")
        .arg(X.x(),0,'f',3).arg(X.y(),0,'f',3).arg(s0sq,0,'f',6).arg(a,0,'f',4).arg(b,0,'f',4).arg(thetaDeg,0,'f',2);
    if (!resids.isEmpty()) {
        QStringList lines; for (int i=0;i<resids.size();++i) lines << QString("%1=%2").arg(types.value(i)).arg(resids[i],0,'f',4);
        rep += QString("Residuals: %1\n").arg(lines.join(", "));
    }
    m_report->setText(rep);

    Point p(name, X.x(), X.y(), 0.0);
    m_pm->addPoint(p);
    if (m_canvas) m_canvas->addPoint(p);
}
