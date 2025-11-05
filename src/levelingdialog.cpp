#include "levelingdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QMap>
#include <QtMath>
#include <QPainter>
#include <QPixmap>
#include <algorithm>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QPdfWriter>
#include <QPageLayout>
#include <QPageSize>
#include "pointmanager.h"
#include "canvaswidget.h"
#include "surveycalculator.h"

LevelingDialog::LevelingDialog(PointManager* pm, CanvasWidget* canvas, QWidget* parent)
    : QDialog(parent), m_pm(pm), m_canvas(canvas)
{
    setWindowTitle("Levelling (Zimbabwe)");
    resize(560, 420);
    QVBoxLayout* root = new QVBoxLayout(this);

    QFormLayout* top = new QFormLayout();
    m_startCombo = new QComboBox(this);
    m_closeCombo = new QComboBox(this);
    m_inputMode = new QComboBox(this);
    m_inputMode->addItems(QStringList()
                          << "dH (ΔRL per leg)"
                          << "BS/IS/FS log");
    m_inputMode->setCurrentIndex(1);
    m_calcMethod = new QComboBox(this);
    m_calcMethod->addItems(QStringList()
                           << "HI (Height of Collimation)"
                           << "Rise and Fall");
    m_tolMmPerSqrtKm = new QDoubleSpinBox(this);
    m_tolMmPerSqrtKm->setRange(0.0, 50.0);
    m_tolMmPerSqrtKm->setDecimals(2);
    m_tolMmPerSqrtKm->setSingleStep(0.5);
    m_tolMmPerSqrtKm->setValue(4.0); // common tolerance
    top->addRow("Start point:", m_startCombo);
    top->addRow("Close to (optional):", m_closeCombo);
    top->addRow("Input:", m_inputMode);
    top->addRow("Method:", m_calcMethod);
    top->addRow("Tolerance (mm/√km):", m_tolMmPerSqrtKm);
    m_weightMethod = new QComboBox(this);
    m_weightMethod->addItems(QStringList()
                             << "By Distance"
                             << "Equal"
                             << "Per-leg σ"
                             << "By Setups");
    m_weightMethod->setCurrentIndex(0);
    top->addRow("Weighting:", m_weightMethod);
    root->addLayout(top);

    m_table = new QTableWidget(0, 5, this);
    m_table->horizontalHeader()->setStretchLastSection(true);
    rebuildTableForMode();
    root->addWidget(m_table);

    QHBoxLayout* rows = new QHBoxLayout();
    QPushButton* addRowBtn = new QPushButton("Add Row", this);
    QPushButton* delRowBtn = new QPushButton("Remove Selected", this);
    rows->addWidget(addRowBtn); rows->addWidget(delRowBtn); rows->addStretch();
    root->addLayout(rows);

    QHBoxLayout* opts = new QHBoxLayout();
    m_outPrefix = new QLineEdit(this); m_outPrefix->setPlaceholderText("Optional prefix for new points");
    m_applyZ = new QCheckBox("Apply RL to coordinates", this); m_applyZ->setChecked(true);
    QPushButton* computeBtn = new QPushButton("Compute", this);
    opts->addWidget(new QLabel("Prefix:")); opts->addWidget(m_outPrefix);
    opts->addStretch(); opts->addWidget(m_applyZ); opts->addWidget(computeBtn);
    root->addLayout(opts);

    m_report = new QTextEdit(this); m_report->setReadOnly(true);
    root->addWidget(m_report);
    m_profileLabel = new QLabel(this);
    m_profileLabel->setMinimumHeight(120);
    m_profileLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(m_profileLabel);

    // Import/Export and drawing actions
    QHBoxLayout* actions = new QHBoxLayout();
    QPushButton* importCsvBtn = new QPushButton("Import Levelling CSV", this);
    QPushButton* exportCsvBtn = new QPushButton("Export Levelling CSV", this);
    QPushButton* exportReportBtn = new QPushButton("Export Results CSV", this);
    QPushButton* exportPdfBtn = new QPushButton("Export Profile PDF", this);
    QPushButton* addProfileBtn = new QPushButton("Add Profile to Drawing", this);
    actions->addWidget(importCsvBtn);
    actions->addWidget(exportCsvBtn);
    actions->addWidget(exportReportBtn);
    actions->addWidget(exportPdfBtn);
    actions->addStretch();
    actions->addWidget(addProfileBtn);
    root->addLayout(actions);

    connect(addRowBtn, &QPushButton::clicked, this, &LevelingDialog::addRow);
    connect(delRowBtn, &QPushButton::clicked, this, &LevelingDialog::removeSelected);
    connect(computeBtn, &QPushButton::clicked, this, &LevelingDialog::computeAdjustment);
    connect(m_inputMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int){ rebuildTableForMode(); reload(); });
    connect(importCsvBtn, &QPushButton::clicked, this, &LevelingDialog::importCSV);
    connect(exportCsvBtn, &QPushButton::clicked, this, &LevelingDialog::exportCSV);
    connect(exportReportBtn, &QPushButton::clicked, this, &LevelingDialog::exportReportCSV);
    connect(exportPdfBtn, &QPushButton::clicked, this, &LevelingDialog::exportProfilePDF);
    connect(addProfileBtn, &QPushButton::clicked, this, &LevelingDialog::addProfileToDrawing);

    reload();
}

void LevelingDialog::reload()
{
    if (!m_pm) return;
    QStringList names = m_pm->getPointNames();
    {
        QSignalBlocker b1(m_startCombo); m_startCombo->clear(); m_startCombo->addItems(names);
    }
    {
        QSignalBlocker b2(m_closeCombo); m_closeCombo->clear(); m_closeCombo->addItem(""); m_closeCombo->addItems(names);
    }
    // Fill editors depending on mode
    const bool logMode = (m_inputMode && m_inputMode->currentIndex() == 1);
    for (int r=0;r<m_table->rowCount();++r) {
        if (!m_table->cellWidget(r,0)) { QComboBox* cb = new QComboBox(m_table); cb->addItems(names); m_table->setCellWidget(r,0,cb); }
        else { if (auto* cb=qobject_cast<QComboBox*>(m_table->cellWidget(r,0))) { QSignalBlocker b(cb); cb->clear(); cb->addItems(names);} }
        if (logMode) {
            auto ensure = [&](int c, double minv, double maxv, int dec){ if (!m_table->cellWidget(r,c)) { QDoubleSpinBox* ds = new QDoubleSpinBox(m_table); ds->setRange(minv,maxv); ds->setDecimals(dec); m_table->setCellWidget(r,c,ds);} };
            ensure(1, 0.0, 1e6, 3); // BS
            ensure(2, 0.0, 1e6, 3); // IS
            ensure(3, 0.0, 1e6, 3); // FS
            if (!m_table->cellWidget(r,4)) { QDoubleSpinBox* d = new QDoubleSpinBox(m_table); d->setRange(0, 1e9); d->setDecimals(3); m_table->setCellWidget(r,4,d);} // Dist
            if (!m_table->cellWidget(r,5)) { QSpinBox* loop = new QSpinBox(m_table); loop->setRange(1, 999); loop->setValue(1); m_table->setCellWidget(r,5,loop);} // Loop
            if (!m_table->cellWidget(r,6)) { QComboBox* close = new QComboBox(m_table); close->addItem(""); close->addItems(names); m_table->setCellWidget(r,6,close);} // CloseTo
            if (!m_table->cellWidget(r,7)) { QDoubleSpinBox* sig = new QDoubleSpinBox(m_table); sig->setRange(0, 100.0); sig->setDecimals(3); sig->setValue(3.0); m_table->setCellWidget(r,7,sig);} // σ
            if (!m_table->cellWidget(r,8)) { QLineEdit* remark = new QLineEdit(m_table); m_table->setCellWidget(r,8,remark);} // Remarks
        } else {
            if (!m_table->cellWidget(r,1)) { QDoubleSpinBox* ds = new QDoubleSpinBox(m_table); ds->setRange(-1e6, 1e6); ds->setDecimals(4); m_table->setCellWidget(r,1,ds);} // dH
            if (!m_table->cellWidget(r,2)) { QDoubleSpinBox* ds2 = new QDoubleSpinBox(m_table); ds2->setRange(0, 1e9); ds2->setDecimals(3); m_table->setCellWidget(r,2,ds2);} // Dist
            if (!m_table->cellWidget(r,3)) { QSpinBox* loop = new QSpinBox(m_table); loop->setRange(1, 999); loop->setValue(1); m_table->setCellWidget(r,3,loop);} // Loop
            if (!m_table->cellWidget(r,4)) { QComboBox* close = new QComboBox(m_table); close->addItem(""); close->addItems(names); m_table->setCellWidget(r,4,close);} // CloseTo
            if (!m_table->cellWidget(r,5)) { QDoubleSpinBox* sig = new QDoubleSpinBox(m_table); sig->setRange(0, 100.0); sig->setDecimals(3); sig->setValue(3.0); m_table->setCellWidget(r,5,sig);} // σ
        }
    }
}

void LevelingDialog::addRow()
{
    int r = m_table->rowCount(); m_table->insertRow(r);
    reload();
}

void LevelingDialog::removeSelected()
{
    auto sel = m_table->selectionModel()->selectedRows();
    for (int i=sel.size()-1;i>=0;--i) m_table->removeRow(sel.at(i).row());
}

QVector<LevelingDialog::Leg> LevelingDialog::collectLegs() const
{
    QVector<Leg> legs;
    const bool logMode = (m_inputMode && m_inputMode->currentIndex() == 1);
    if (!logMode) {
        for (int r=0;r<m_table->rowCount();++r) {
            QComboBox* cb = qobject_cast<QComboBox*>(m_table->cellWidget(r,0));
            QDoubleSpinBox* dH = qobject_cast<QDoubleSpinBox*>(m_table->cellWidget(r,1));
            QDoubleSpinBox* dist = qobject_cast<QDoubleSpinBox*>(m_table->cellWidget(r,2));
            QSpinBox* loop = qobject_cast<QSpinBox*>(m_table->cellWidget(r,3));
            QComboBox* close = qobject_cast<QComboBox*>(m_table->cellWidget(r,4));
            QDoubleSpinBox* sig = qobject_cast<QDoubleSpinBox*>(m_table->cellWidget(r,5));
            if (!cb || !dH || !dist) continue;
            QString to = cb->currentText(); if (to.isEmpty()) continue;
            Leg L; L.toName = to; L.dH = dH->value(); L.dist = dist->value(); L.loopId = loop?loop->value():1; L.closeTo = close?close->currentText():QString(); L.sigmaPerSqrtKm = sig?sig->value()/1000.0 : 0.003;
            legs.append(L);
        }
        return legs;
    }
    // BS/IS/FS mode handled elsewhere
    return legs;
}

void LevelingDialog::computeAdjustment()
{
    if (!m_pm) return;
    QString sname = m_startCombo->currentText(); if (!m_pm->hasPoint(sname)) { m_report->setText("Pick a valid start point."); return; }
    Point sp = m_pm->getPoint(sname);
    QVector<Leg> legs;
    double sumBS=0.0, sumFS=0.0, sumRise=0.0, sumFall=0.0, totalDist=0.0;
    if (m_inputMode && m_inputMode->currentIndex() == 1) {
        m_lastSetupIds.clear();
        legs = collectLegsFromLog(sp.z, sumBS, sumFS, sumRise, sumFall, totalDist, &m_lastSetupIds);
    } else {
        legs = collectLegs();
        for (const auto& L : legs) totalDist += qMax(0.0, L.dist);
        // In dH mode, treat each leg as its own setup for optional setup-based weighting
        m_lastSetupIds.resize(legs.size());
        for (int i=0;i<m_lastSetupIds.size();++i) m_lastSetupIds[i] = i+1;
    }
    if (legs.isEmpty()) { m_report->setText("Add at least one leg."); return; }

    // Group by loop
    QMap<int, QVector<int>> loopRows; for (int i=0;i<legs.size();++i) loopRows[legs[i].loopId].append(i);
    QVector<double> corr(legs.size(), 0.0);
    QString rep; rep += QString("Start %1: Z=%2\n").arg(sp.name).arg(sp.z,0,'f',3);
    // Precompute rawZ per leg
    QVector<double> rawZ(legs.size(), sp.z);
    double zacc = sp.z; for (int i=0;i<legs.size();++i) { zacc += legs[i].dH; rawZ[i] = zacc; }
    // Arithmetic checks
    const double rlDiff = rawZ.isEmpty() ? 0.0 : (rawZ.last() - sp.z);
    if (m_inputMode && m_inputMode->currentIndex() == 1) {
        if (m_calcMethod && m_calcMethod->currentIndex() == 0) {
            rep += QString("Check (HI): ΣBS - ΣFS = %1  vs ΔRL = %2\n").arg(sumBS - sumFS,0,'f',4).arg(rlDiff,0,'f',4);
        } else {
            rep += QString("Check (Rise&Fall): ΣRise - ΣFall = %1  vs ΔRL = %2\n").arg(sumRise - sumFall,0,'f',4).arg(rlDiff,0,'f',4);
        }
    }
    // Tolerance
    const double tol_m = (m_tolMmPerSqrtKm ? m_tolMmPerSqrtKm->value() : 4.0) / 1000.0 * qSqrt(qMax(0.0, totalDist/1000.0));

    for (auto it = loopRows.begin(); it != loopRows.end(); ++it) {
        int loopId = it.key(); const auto idxs = it.value();
        // Determine closing bench for this loop
        QString cname = m_closeCombo->currentText();
        for (int j: idxs) if (!legs[j].closeTo.isEmpty()) { cname = legs[j].closeTo; }
        bool haveClose = (!cname.isEmpty() && m_pm->hasPoint(cname));
        double closeZ = haveClose ? m_pm->getPoint(cname).z : rawZ[idxs.isEmpty()?0:idxs.last()];
        double mis = haveClose ? (rawZ[idxs.last()] - closeZ) : 0.0;
        // Weighting
        double denom = 0.0;
        QMap<int,int> legsPerSetup;
        if (m_weightMethod->currentIndex() == 3) { // By Setups
            for (int j: idxs) { int sid = (j>=0 && j<m_lastSetupIds.size()) ? m_lastSetupIds[j] : (j+1); legsPerSetup[sid] += 1; }
        }
        for (int j: idxs) {
            double d = qMax(0.0, legs[j].dist);
            double v = 1.0;
            if (m_weightMethod->currentIndex() == 0) { // By Distance (variance ∝ d)
                v = d;
            } else if (m_weightMethod->currentIndex() == 1) { // Equal
                v = 1.0;
            } else if (m_weightMethod->currentIndex() == 2) { // Per-leg σ
                double km = d/1000.0; double sigma = legs[j].sigmaPerSqrtKm; v = sigma*sigma*km; // variance
            } else { // By Setups
                int sid = (j>=0 && j<m_lastSetupIds.size()) ? m_lastSetupIds[j] : (j+1);
                int cnt = qMax(1, legsPerSetup.value(sid, 1));
                v = 1.0 / double(cnt); // equal per-setup weight spread to legs in setup
            }
            denom += v;
        }
        if (denom <= 0.0) denom = 1.0;
        double ssr = 0.0; // sum of squared residuals / variance
        for (int j: idxs) {
            double d = qMax(0.0, legs[j].dist);
            double v = 1.0;
            if (m_weightMethod->currentIndex() == 0) v = d;
            else if (m_weightMethod->currentIndex() == 2) { double km = d/1000.0; double sigma = legs[j].sigmaPerSqrtKm; v = sigma*sigma*km; }
            else if (m_weightMethod->currentIndex() == 3) {
                int sid = (j>=0 && j<m_lastSetupIds.size()) ? m_lastSetupIds[j] : (j+1);
                int cnt = qMax(1, legsPerSetup.value(sid, 1)); v = 1.0 / double(cnt);
            }
            corr[j] = haveClose ? (-mis * (v / denom)) : 0.0;
            ssr += (v > 0.0) ? ((corr[j]*corr[j]) / v) : 0.0;
        }
        int dof = qMax(1, idxs.size() - 1);
        double sigma0 = qSqrt(ssr / double(dof));
        rep += QString("Loop %1: close=%2  misclosure=%3 m  (allowed ≈ %4 m)  sigma0≈%5\n")
               .arg(loopId)
               .arg(cname.isEmpty()?QString("(none)"):cname)
               .arg(mis,0,'f',4)
               .arg(tol_m,0,'f',4)
               .arg(sigma0,0,'f',4);
    }
    // Report per-leg
    QVector<double> cumDist; cumDist.reserve(legs.size()+1); QVector<double> rl; rl.reserve(legs.size()+1);
    double accDist = 0.0; cumDist.push_back(accDist); rl.push_back(sp.z);
    for (int i=0;i<legs.size();++i) {
        double zc = rawZ[i] + corr[i];
        rep += QString("%1: dH=%2  dist=%3  rawZ=%4  corr=%5  Z=%6\n")
              .arg(legs[i].toName)
              .arg(legs[i].dH,0,'f',4)
              .arg(legs[i].dist,0,'f',2)
              .arg(rawZ[i],0,'f',3)
              .arg(corr[i],0,'f',4)
              .arg(zc,0,'f',3);
        accDist += qMax(0.0, legs[i].dist);
        cumDist.push_back(accDist);
        rl.push_back(zc);
    }
    m_report->setText(rep);
    updateProfile(cumDist, rl);
    // Cache last results for export/drawing
    m_lastCumDist = cumDist;
    m_lastRL = rl;
    m_lastLegs = legs;
    m_lastRawZ = rawZ;
    m_lastCorr = corr;
    m_lastToNames.clear(); for (const auto& L : legs) m_lastToNames << L.toName;

    if (m_applyZ->isChecked()) {
        double zcum = sp.z;
        for (int i=0;i<legs.size();++i) {
            zcum += legs[i].dH; zcum += corr[i];
            if (!m_pm->hasPoint(legs[i].toName)) continue;
            Point p = m_pm->getPoint(legs[i].toName);
            const QString pref = m_outPrefix?m_outPrefix->text():QString();
            const QString newName = pref.isEmpty()? p.name : (pref + p.name);
            Point np(newName, p.x, p.y, zcum);
            m_pm->addPoint(np);
            if (m_canvas) m_canvas->addPoint(np);
        }
    }
}

QVector<LevelingDialog::Leg> LevelingDialog::collectLegsFromLog(double startZ,
                                    double& sumBS,
                                    double& sumFS,
                                    double& sumRise,
                                    double& sumFall,
                                    double& totalDist,
                                    QVector<int>* outSetupIds) const
{
    QVector<Leg> legs;
    sumBS = sumFS = sumRise = sumFall = 0.0; totalDist = 0.0;
    double curRL = startZ; double curHI = 0.0; bool haveHI = false; bool havePrev = false; double prevRead = 0.0;
    const bool useHI = (!m_calcMethod || m_calcMethod->currentIndex() == 0);
    int setupId = 0;
    for (int r=0;r<m_table->rowCount();++r) {
        QComboBox* cb = qobject_cast<QComboBox*>(m_table->cellWidget(r,0));
        if (!cb) continue; QString to = cb->currentText(); if (to.isEmpty()) continue;
        auto getD = [&](int c){ if (auto* ds=qobject_cast<QDoubleSpinBox*>(m_table->cellWidget(r,c))) return ds->value(); return 0.0; };
        auto getI = [&](int c){ if (auto* isb=qobject_cast<QSpinBox*>(m_table->cellWidget(r,c))) return isb->value(); return 1; };
        auto getC = [&](int c){ if (auto* co=qobject_cast<QComboBox*>(m_table->cellWidget(r,c))) return co->currentText(); return QString(); };
        double bs = 0.0, isv = 0.0, fs = 0.0;
        // Columns: 0 To, 1 BS, 2 IS, 3 FS, 4 Dist, 5 Loop, 6 CloseTo, 7 σ
        bs = getD(1); isv = getD(2); fs = getD(3);
        double dist = getD(4);
        int loopId = getI(5);
        QString closeTo = getC(6);
        double sigma = getD(7)/1000.0; // mm -> m per √km

        if (useHI) {
            // HI method
            if (bs > 0.0) { curHI = curRL + bs; haveHI = true; sumBS += bs; prevRead = bs; havePrev = true; setupId += 1; continue; }
            double reading = (isv > 0.0 ? isv : (fs > 0.0 ? fs : 0.0));
            if (reading <= 0.0 || !haveHI) continue;
            double nextRL = curHI - reading;
            double dH = nextRL - curRL;
            Leg L{to, dH, dist, loopId, closeTo, sigma}; legs.append(L);
            if (outSetupIds) outSetupIds->append(setupId>0?setupId:1);
            totalDist += qMax(0.0, dist);
            if (fs > 0.0) { sumFS += fs; haveHI = false; }
            curRL = nextRL; prevRead = reading; havePrev = true;
        } else {
            // Rise & Fall (differences between sequential readings)
            double reading = 0.0; bool hasRead=false; if (bs>0.0){reading=bs;hasRead=true; sumBS+=bs; setupId += 1;} if (isv>0.0){reading=isv;hasRead=true;} if (fs>0.0){reading=fs;hasRead=true; sumFS+=fs;}
            if (!hasRead) continue;
            if (!havePrev) { prevRead = reading; havePrev = true; continue; }
            double diff = prevRead - reading; // + = rise
            if (diff >= 0) sumRise += diff; else sumFall += -diff;
            double nextRL = curRL + diff;
            double dH = diff;
            Leg L{to, dH, dist, loopId, closeTo, sigma}; legs.append(L);
            if (outSetupIds) outSetupIds->append(setupId>0?setupId:1);
            totalDist += qMax(0.0, dist);
            curRL = nextRL; prevRead = reading;
        }
    }
    return legs;
}

void LevelingDialog::rebuildTableForMode()
{
    const bool logMode = (m_inputMode && m_inputMode->currentIndex() == 1);
    m_table->clear();
    if (logMode) {
        m_table->setColumnCount(9);
        m_table->setHorizontalHeaderLabels(QStringList() << "To" << "BS" << "IS" << "FS" << "Dist (m)" << "Loop" << "CloseTo" << "σ (mm/√km)" << "Remarks");
    } else {
        m_table->setColumnCount(6);
        m_table->setHorizontalHeaderLabels(QStringList() << "To" << "dH (m)" << "Dist (m)" << "Loop" << "CloseTo" << "σ (mm/√km)");
    }
}

void LevelingDialog::updateProfile(const QVector<double>& cumDist, const QVector<double>& rl)
{
    if (!m_profileLabel) return; if (cumDist.size() < 2 || rl.size() != cumDist.size()) { m_profileLabel->clear(); return; }
    int W = qMax(320, m_profileLabel->width()); int H = qMax(120, m_profileLabel->height());
    QPixmap pm(W, H); pm.fill(Qt::transparent);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    // background
    p.fillRect(pm.rect(), QColor(20,20,20,30));
    double dMin = cumDist.first(), dMax = cumDist.last();
    double zMin = *std::min_element(rl.begin(), rl.end());
    double zMax = *std::max_element(rl.begin(), rl.end());
    if (qFuzzyCompare(zMin, zMax)) { zMin -= 0.5; zMax += 0.5; }
    QRect plot = pm.rect().adjusted(36, 12, -12, -24);
    // axes
    p.setPen(QPen(QColor(120,120,120), 1));
    p.drawRect(plot);
    auto mapX = [&](double d){ return plot.left() + (d - dMin) / qMax(1e-9, (dMax - dMin)) * plot.width(); };
    auto mapY = [&](double z){ return plot.bottom() - (z - zMin) / qMax(1e-9, (zMax - zMin)) * plot.height(); };
    // polyline
    p.setPen(QPen(QColor(74,144,217), 2));
    for (int i=1;i<cumDist.size();++i) p.drawLine(QPointF(mapX(cumDist[i-1]), mapY(rl[i-1])), QPointF(mapX(cumDist[i]), mapY(rl[i])));
    // labels
    p.setPen(QPen(QColor(90,90,90))); p.drawText(4, 14, QString("d= %1 m").arg(dMax,0,'f',1)); p.drawText(4, H-6, QString("Z: %1 .. %2 m").arg(zMin,0,'f',2).arg(zMax,0,'f',2));
    p.end();
    m_profileLabel->setPixmap(pm);
}

void LevelingDialog::importCSV()
{
    const QString fn = QFileDialog::getOpenFileName(this, "Import Levelling CSV", QString(), "CSV (*.csv);;All Files (*)");
    if (fn.isEmpty()) return;
    QFile f(fn); if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { QMessageBox::warning(this, "Import CSV", QString("Failed to open %1").arg(fn)); return; }
    QTextStream in(&f);
    QString header = in.readLine(); if (header.isNull()) { QMessageBox::warning(this, "Import CSV", "Empty file"); return; }
    const bool logMode = header.contains("BS", Qt::CaseInsensitive) || header.contains("FS", Qt::CaseInsensitive);
    if (m_inputMode) m_inputMode->setCurrentIndex(logMode ? 1 : 0);
    rebuildTableForMode();
    QVector<QStringList> rows;
    while (!in.atEnd()) {
        const QString line = in.readLine(); if (line.trimmed().isEmpty()) continue; rows.append(line.split(','));
    }
    m_table->setRowCount(rows.size());
    reload();
    for (int r=0;r<rows.size();++r) {
        const QStringList cols = rows[r];
        auto setD = [&](int c, double v){ if (auto* ds=qobject_cast<QDoubleSpinBox*>(m_table->cellWidget(r,c))) ds->setValue(v); };
        auto setI = [&](int c, int v){ if (auto* isb=qobject_cast<QSpinBox*>(m_table->cellWidget(r,c))) isb->setValue(v); };
        auto setC = [&](int c, const QString& v){ if (auto* co=qobject_cast<QComboBox*>(m_table->cellWidget(r,c))) { int idx = co->findText(v); if (idx<0) co->addItem(v); co->setCurrentText(v);} };
        if (logMode) {
            // Expect: To,BS,IS,FS,Dist,Loop,CloseTo,Sigma[,Remarks]
            if (cols.size()>=1) setC(0, cols.value(0));
            if (cols.size()>=2) setD(1, cols.value(1).toDouble());
            if (cols.size()>=3) setD(2, cols.value(2).toDouble());
            if (cols.size()>=4) setD(3, cols.value(3).toDouble());
            if (cols.size()>=5) setD(4, cols.value(4).toDouble());
            if (cols.size()>=6) setI(5, cols.value(5).toInt());
            if (cols.size()>=7) setC(6, cols.value(6));
            if (cols.size()>=8) setD(7, cols.value(7).toDouble());
            if (cols.size()>=9) { if (auto* le=qobject_cast<QLineEdit*>(m_table->cellWidget(r,8))) le->setText(cols.value(8)); }
        } else {
            // Expect: To,dH,Dist,Loop,CloseTo,Sigma
            if (cols.size()>=1) setC(0, cols.value(0));
            if (cols.size()>=2) setD(1, cols.value(1).toDouble());
            if (cols.size()>=3) setD(2, cols.value(2).toDouble());
            if (cols.size()>=4) setI(3, cols.value(3).toInt());
            if (cols.size()>=5) setC(4, cols.value(4));
            if (cols.size()>=6) setD(5, cols.value(5).toDouble());
        }
    }
}

void LevelingDialog::exportCSV()
{
    const QString fn = QFileDialog::getSaveFileName(this, "Export Levelling CSV", QString(), "CSV (*.csv);;All Files (*)");
    if (fn.isEmpty()) return;
    QFile f(fn); if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { QMessageBox::warning(this, "Export CSV", QString("Failed to write %1").arg(fn)); return; }
    QTextStream out(&f);
    const bool logMode = (m_inputMode && m_inputMode->currentIndex() == 1);
    if (logMode) out << "To,BS,IS,FS,Dist,Loop,CloseTo,Sigma,Remarks\n";
    else out << "To,dH,Dist,Loop,CloseTo,Sigma\n";
    for (int r=0;r<m_table->rowCount();++r) {
        auto getD = [&](int c){ if (auto* ds=qobject_cast<QDoubleSpinBox*>(m_table->cellWidget(r,c))) return ds->value(); return 0.0; };
        auto getI = [&](int c){ if (auto* isb=qobject_cast<QSpinBox*>(m_table->cellWidget(r,c))) return isb->value(); return 1; };
        auto getC = [&](int c){ if (auto* co=qobject_cast<QComboBox*>(m_table->cellWidget(r,c))) return co->currentText(); return QString(); };
        auto getT = [&](int c){ if (auto* le=qobject_cast<QLineEdit*>(m_table->cellWidget(r,c))) return le->text(); return QString(); };
        if (logMode) {
            out << getC(0) << "," << getD(1) << "," << getD(2) << "," << getD(3) << "," << getD(4) << "," << getI(5) << "," << getC(6) << "," << getD(7) << "," << getT(8) << "\n";
        } else {
            out << getC(0) << "," << getD(1) << "," << getD(2) << "," << getI(3) << "," << getC(4) << "," << getD(5) << "\n";
        }
    }
    f.close();
}

void LevelingDialog::exportReportCSV()
{
    if (m_lastLegs.isEmpty() || m_lastRL.size() < 2) { QMessageBox::information(this, "Export Report", "Compute first."); return; }
    const QString fn = QFileDialog::getSaveFileName(this, "Export Report CSV", QString(), "CSV (*.csv);;All Files (*)");
    if (fn.isEmpty()) return;
    QFile f(fn); if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { QMessageBox::warning(this, "Export Report", QString("Failed to write %1").arg(fn)); return; }
    QTextStream out(&f);
    out << "To,Dist,RawZ,Corr,AdjZ\n";
    for (int i=0;i<m_lastLegs.size();++i) {
        const double raw = (i<m_lastRawZ.size()? m_lastRawZ[i] : 0.0);
        const double corr = (i<m_lastCorr.size()? m_lastCorr[i] : 0.0);
        const double adj = raw + corr;
        out << m_lastLegs[i].toName << "," << m_lastLegs[i].dist << "," << raw << "," << corr << "," << adj << "\n";
    }
    f.close();
}

void LevelingDialog::exportProfilePDF()
{
    if (m_lastCumDist.size() < 2 || m_lastRL.size() != m_lastCumDist.size()) { QMessageBox::information(this, "Export Profile", "Compute first."); return; }
    const QString fn = QFileDialog::getSaveFileName(this, "Export Profile PDF", QString(), "PDF (*.pdf)");
    if (fn.isEmpty()) return;
    QPdfWriter pdf(fn); pdf.setPageSize(QPageSize(QPageSize::A4)); pdf.setPageOrientation(QPageLayout::Landscape);
    QPainter p(&pdf);
    QRect page = QRect(QPoint(0,0), pdf.pageLayout().paintRectPixels(96).size());
    QRect plot = page.adjusted(60, 40, -40, -60);
    // background
    p.fillRect(page, Qt::white);
    p.setPen(QPen(Qt::black, 1)); p.drawRect(plot);
    double dMin = m_lastCumDist.first(), dMax = m_lastCumDist.last();
    double zMin = *std::min_element(m_lastRL.begin(), m_lastRL.end());
    double zMax = *std::max_element(m_lastRL.begin(), m_lastRL.end()); if (qFuzzyCompare(zMin, zMax)) { zMin -= 0.5; zMax += 0.5; }
    auto mapX = [&](double d){ return plot.left() + (d - dMin) / qMax(1e-9, (dMax - dMin)) * plot.width(); };
    auto mapY = [&](double z){ return plot.bottom() - (z - zMin) / qMax(1e-9, (zMax - zMin)) * plot.height(); };
    p.setPen(QPen(QColor(50,120,200), 2));
    for (int i=1;i<m_lastCumDist.size();++i) p.drawLine(QPointF(mapX(m_lastCumDist[i-1]), mapY(m_lastRL[i-1])), QPointF(mapX(m_lastCumDist[i]), mapY(m_lastRL[i])));
    p.setPen(Qt::black); p.drawText(plot.adjusted(0,-24,0,0), Qt::AlignLeft|Qt::AlignTop, QString("Distance: %1 m").arg(dMax,0,'f',1));
    p.drawText(plot.adjusted(0,0,0,24), Qt::AlignLeft|Qt::AlignBottom, QString("RL: %1 .. %2 m").arg(zMin,0,'f',2).arg(zMax,0,'f',2));
    p.end();
}

void LevelingDialog::addProfileToDrawing()
{
    if (!m_canvas) { QMessageBox::information(this, "Profile", "Canvas not available."); return; }
    if (m_lastCumDist.size() < 2 || m_lastRL.size() != m_lastCumDist.size()) { QMessageBox::information(this, "Profile", "Compute first."); return; }
    QVector<QPointF> pts; pts.reserve(m_lastCumDist.size());
    for (int i=0;i<m_lastCumDist.size();++i) pts.append(QPointF(m_lastCumDist[i], m_lastRL[i]));
    m_canvas->addPolylineEntity(pts, false, QStringLiteral("Profile"));
}
