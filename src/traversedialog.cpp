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
        row->addWidget(new QLabel("Close to (optional):", this));
        m_closeCombo = new QComboBox(this);
        row->addWidget(m_closeCombo, 1);
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

    for (int i = 0; i < legs.size(); ++i) {
        const auto& L = legs[i];
        // Convert entered distance to meters for internal computation
        const double dMeters = L.distance * toMeters;
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

    // Closure analysis (optional)
    Point close;
    if (getClosePoint(close)) {
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
