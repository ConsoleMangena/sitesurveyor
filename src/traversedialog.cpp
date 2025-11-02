#include "traversedialog.h"
#include "pointmanager.h"
#include "canvaswidget.h"
#include "surveycalculator.h"
#include "appsettings.h"
#include "point.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QTextEdit>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QtMath>
#include <algorithm>

TraverseDialog::TraverseDialog(PointManager* pm, CanvasWidget* canvas, QWidget* parent)
    : QDialog(parent), m_pm(pm), m_canvas(canvas)
{
    setWindowTitle("Traverse Entry");
    setModal(false);
    setMinimumSize(720, 520);

    auto* main = new QVBoxLayout(this);

    // Start/Close selection
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("Start point:", this));
        m_startCombo = new QComboBox(this);
        row->addWidget(m_startCombo, 1);
        row->addSpacing(12);
        m_seaLevelCheck = new QCheckBox("Sea level scale", this);
        m_seaLevelCheck->setChecked(false);
        row->addWidget(m_seaLevelCheck);
        row->addWidget(new QLabel("Mean Elev (m):", this));
        m_meanElev = new QDoubleSpinBox(this);
        m_meanElev->setRange(-1000.0, 10000.0);
        m_meanElev->setDecimals(2);
        m_meanElev->setSingleStep(1.0);
        m_meanElev->setValue(0.0);
        row->addWidget(m_meanElev);
        row->addSpacing(12);
        row->addWidget(new QLabel("Close to (optional):", this));
        m_closeCombo = new QComboBox(this);
        row->addWidget(m_closeCombo, 1);
        main->addLayout(row);
    }

    // Adjustment method and apply checkbox
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("Adjustment:", this));
        m_adjustCombo = new QComboBox(this);
        m_adjustCombo->addItems(QStringList() << "None" << "Bowditch" << "Transit");
        row->addWidget(m_adjustCombo);
        row->addSpacing(12);
        row->addWidget(new QLabel("Scale (ppm):", this));
        m_scalePpm = new QDoubleSpinBox(this);
        m_scalePpm->setRange(-100000.0, 100000.0);
        m_scalePpm->setDecimals(3);
        m_scalePpm->setSingleStep(10.0);
        m_scalePpm->setValue(0.0);
        row->addWidget(m_scalePpm);
        row->addSpacing(12);
        m_applyAdjustedCheck = new QCheckBox("Apply adjusted", this);
        m_applyAdjustedCheck->setChecked(true);
        row->addWidget(m_applyAdjustedCheck);
        row->addStretch();
        main->addLayout(row);
    }

    // Legs table
    m_legsTable = new QTableWidget(this);
    m_legsTable->setColumnCount(3);
    QString unit = AppSettings::measurementUnits().compare("imperial", Qt::CaseInsensitive) == 0 ? "ft" : "m";
    m_legsTable->setHorizontalHeaderLabels(QStringList() << "Name" << "Azimuth (deg)" << QString("Distance (%1)").arg(unit));
    m_legsTable->horizontalHeader()->setStretchLastSection(true);
    m_legsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_legsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_legsTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    m_legsTable->setMinimumHeight(200);

    main->addWidget(m_legsTable, 1);

    // Buttons for table
    {
        auto* row = new QHBoxLayout();
        m_addRowBtn = new QPushButton("Add leg", this);
        m_removeRowBtn = new QPushButton("Remove selected", this);
        row->addWidget(m_addRowBtn);
        row->addWidget(m_removeRowBtn);
        row->addStretch();
        m_addPointsCheck = new QCheckBox("Add computed points", this);
        m_addPointsCheck->setChecked(true);
        m_drawLinesCheck = new QCheckBox("Draw lines", this);
        m_drawLinesCheck->setChecked(true);
        row->addWidget(m_addPointsCheck);
        row->addWidget(m_drawLinesCheck);
        m_computeBtn = new QPushButton("Compute", this);
        row->addWidget(m_computeBtn);
        main->addLayout(row);
    }

    // Report
    m_report = new QTextEdit(this);
    m_report->setReadOnly(true);
    m_report->setMinimumHeight(180);
    main->addWidget(m_report, 1);

    connect(m_addRowBtn, &QPushButton::clicked, this, &TraverseDialog::addRow);
    connect(m_removeRowBtn, &QPushButton::clicked, this, &TraverseDialog::removeSelectedRows);
    connect(m_computeBtn, &QPushButton::clicked, this, &TraverseDialog::computeTraverse);

    reload();
}

void TraverseDialog::reload()
{
    if (!m_pm) return;
    const QStringList names = m_pm->getPointNames();
    auto repop = [&](QComboBox* box, bool allowEmpty){
        const QString current = box ? box->currentText() : QString();
        if (!box) return;
        box->clear();
        if (allowEmpty) box->addItem(QString());
        box->addItems(names);
        int idx = box->findText(current);
        if (idx >= 0) box->setCurrentIndex(idx);
    };
    repop(m_startCombo, false);
    repop(m_closeCombo, true);
}

void TraverseDialog::addRow()
{
    int r = m_legsTable->rowCount();
    m_legsTable->insertRow(r);
    m_legsTable->setItem(r, 0, new QTableWidgetItem(QString("T%1").arg(r+1)));
    m_legsTable->setItem(r, 1, new QTableWidgetItem("0.0000"));
    m_legsTable->setItem(r, 2, new QTableWidgetItem("0.000"));
}

void TraverseDialog::removeSelectedRows()
{
    auto sel = m_legsTable->selectionModel()->selectedRows();
    std::sort(sel.begin(), sel.end(), [](const QModelIndex& a, const QModelIndex& b){ return a.row() > b.row(); });
    for (const auto& idx : sel) {
        m_legsTable->removeRow(idx.row());
    }
}

bool TraverseDialog::getStartPoint(Point& out) const
{
    if (!m_pm || !m_startCombo) return false;
    const QString n = m_startCombo->currentText();
    if (n.isEmpty() || !m_pm->hasPoint(n)) return false;
    out = m_pm->getPoint(n);
    return true;
}

bool TraverseDialog::getClosePoint(Point& out) const
{
    if (!m_pm || !m_closeCombo) return false;
    const QString n = m_closeCombo->currentText();
    if (n.isEmpty()) return false;
    if (!m_pm->hasPoint(n)) return false;
    out = m_pm->getPoint(n);
    return true;
}

QVector<TraverseDialog::Leg> TraverseDialog::collectLegs() const
{
    QVector<Leg> legs;
    for (int r = 0; r < m_legsTable->rowCount(); ++r) {
        auto* nameItem = m_legsTable->item(r, 0);
        auto* azItem = m_legsTable->item(r, 1);
        auto* distItem = m_legsTable->item(r, 2);
        if (!azItem || !distItem) continue;
        bool ok1=false, ok2=false;
        double az = azItem->text().trimmed().toDouble(&ok1);
        double d = distItem->text().trimmed().toDouble(&ok2);
        if (!ok1 || !ok2) continue;
        Leg L; L.azimuthDeg = az; L.distance = d; L.name = nameItem ? nameItem->text().trimmed() : QString();
        legs.append(L);
    }
    return legs;
}

void TraverseDialog::appendReportLine(const QString& s)
{
    if (!m_report) return;
    QString t = m_report->toPlainText();
    if (!t.isEmpty()) t += "\n";
    t += s;
    m_report->setPlainText(t);
}

void TraverseDialog::computeTraverse()
{
    m_report->clear();
    if (!m_pm) { appendReportLine("No point manager available."); return; }
    Point start;
    if (!getStartPoint(start)) { QMessageBox::warning(this, "Traverse", "Please select a valid start point."); return; }

    QVector<Leg> legs = collectLegs();
    if (legs.isEmpty()) { QMessageBox::information(this, "Traverse", "Add at least one leg."); return; }

    const bool imperial = AppSettings::measurementUnits().compare("imperial", Qt::CaseInsensitive) == 0;
    const QString unit = imperial ? "ft" : "m";
    const double toMeters = imperial ? 0.3048 : 1.0;

    appendReportLine(QString("Start: %1  (X=%2, Y=%3)")
                     .arg(start.name)
                     .arg(QString::number(start.x, 'f', 3))
                     .arg(QString::number(start.y, 'f', 3)));

    QVector<Point> computed;
    computed.reserve(legs.size());
    Point current = start;

    const double ppm = (m_scalePpm ? m_scalePpm->value() : 0.0);
    const bool useSL = (m_seaLevelCheck && m_seaLevelCheck->isChecked());
    const double H = (m_meanElev ? m_meanElev->value() : 0.0);
    const double Re = 6371000.0; // mean Earth radius (m)
    for (int i = 0; i < legs.size(); ++i) {
        const auto& L = legs[i];
        // Convert entered distance to meters for internal computation
        double dMeters = L.distance * toMeters;
        if (ppm != 0.0) dMeters = dMeters * (1.0 + ppm / 1e6);
        if (useSL) dMeters = dMeters * (Re / (Re + H));
        QPointF nextXY = SurveyCalculator::polarToRectangular(QPointF(current.x, current.y), dMeters, L.azimuthDeg);
        Point next(L.name.isEmpty() ? QString("T%1").arg(i+1) : L.name, nextXY.x(), nextXY.y(), current.z);
        computed.push_back(next);
        appendReportLine(QString("Leg %1: Az=%2°, Dist=%3 %4  -> %5 (X=%6, Y=%7)")
                         .arg(i+1)
                         .arg(QString::number(L.azimuthDeg, 'f', 4))
                         .arg(QString::number(L.distance, 'f', 3))
                         .arg(unit)
                         .arg(next.name)
                         .arg(QString::number(next.x, 'f', 3))
                         .arg(QString::number(next.y, 'f', 3)));
        current = next;
    }

    // Closure and optional adjustment
    Point close;
    const int methodIdx = m_adjustCombo ? m_adjustCombo->currentIndex() : 0; // 0=None,1=Bowditch,2=Transit
    bool haveClose = getClosePoint(close);
    if (haveClose) {
        double dx = current.x - close.x;
        double dy = current.y - close.y;
        double miscloseMeters = qSqrt(dx*dx + dy*dy);
        double az = SurveyCalculator::azimuth(QPointF(close.x, close.y), QPointF(current.x, current.y));
        const double miscloseDisplay = imperial ? miscloseMeters / 0.3048 : miscloseMeters;
        appendReportLine(QString("\nClosure to %1: Misclose=%2 %3  Az=%4°")
                         .arg(close.name)
                         .arg(QString::number(miscloseDisplay, 'f', 3))
                         .arg(unit)
                         .arg(QString::number(az, 'f', 4)));

        if (methodIdx > 0 && miscloseMeters > 0) {
            // Build leg vectors
            QVector<double> legLen; legLen.reserve(legs.size());
            QVector<double> legDx; legDx.reserve(legs.size());
            QVector<double> legDy; legDy.reserve(legs.size());
            Point prev = start;
            for (int i=0;i<computed.size();++i) {
                double dx_i = computed[i].x - prev.x;
                double dy_i = computed[i].y - prev.y;
                legDx.append(dx_i);
                legDy.append(dy_i);
                legLen.append(qSqrt(dx_i*dx_i + dy_i*dy_i));
                prev = computed[i];
            }
            double sumLen = 0.0, sumAbsDx = 0.0, sumAbsDy = 0.0;
            for (int i=0;i<legLen.size();++i) { sumLen += legLen[i]; sumAbsDx += qAbs(legDx[i]); sumAbsDy += qAbs(legDy[i]); }
            QVector<double> corrDx(legLen.size(), 0.0), corrDy(legLen.size(), 0.0);
            if (methodIdx == 1 && sumLen > 0) {
                // Bowditch
                for (int i=0;i<legLen.size();++i) { double r = legLen[i]/sumLen; corrDx[i] = -dx * r; corrDy[i] = -dy * r; }
            } else if (methodIdx == 2) {
                // Transit
                if (sumAbsDx <= 1e-9) sumAbsDx = 1.0;
                if (sumAbsDy <= 1e-9) sumAbsDy = 1.0;
                for (int i=0;i<legLen.size();++i) {
                    double rx = (qAbs(legDx[i]) / sumAbsDx);
                    double ry = (qAbs(legDy[i]) / sumAbsDy);
                    corrDx[i] = -dx * rx;
                    corrDy[i] = -dy * ry;
                }
            }
            // Build adjusted coordinates
            QVector<Point> adjusted; adjusted.reserve(computed.size());
            Point prevA = start;
            for (int i=0;i<computed.size();++i) {
                double ndx = legDx[i] + corrDx[i];
                double ndy = legDy[i] + corrDy[i];
                Point nxt = computed[i];
                nxt.x = prevA.x + ndx;
                nxt.y = prevA.y + ndy;
                adjusted.append(nxt);
                prevA = nxt;
                appendReportLine(QString("Adj Leg %1: dX=%2  dY=%3  CorrX=%4  CorrY=%5")
                                 .arg(i+1)
                                 .arg(QString::number(legDx[i], 'f', 4))
                                 .arg(QString::number(legDy[i], 'f', 4))
                                 .arg(QString::number(corrDx[i], 'f', 4))
                                 .arg(QString::number(corrDy[i], 'f', 4)));
            }
            // Replace if applying
            if (m_applyAdjustedCheck && m_applyAdjustedCheck->isChecked()) {
                computed = adjusted;
                current = computed.isEmpty() ? start : computed.back();
                appendReportLine("Applied adjusted coordinates.");
            } else {
                appendReportLine("Adjustment reported only (not applied).");
            }
        }
    }

    // Add/draw
    if (m_addPointsCheck && m_addPointsCheck->isChecked()) {
        for (const auto& p : computed) m_pm->addPoint(p);
    }
    if (m_canvas && m_drawLinesCheck && m_drawLinesCheck->isChecked()) {
        QPointF prev(start.x, start.y);
        for (const auto& p : computed) {
            m_canvas->addLine(prev, QPointF(p.x, p.y));
            prev = QPointF(p.x, p.y);
        }
    }
}
