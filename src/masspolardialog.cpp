#include "masspolardialog.h"
#include "pointmanager.h"
#include "canvaswidget.h"
#include "surveycalculator.h"
#include "point.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QCheckBox>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QRegularExpression>

MassPolarDialog::MassPolarDialog(PointManager* pm, CanvasWidget* canvas, QWidget* parent)
    : QDialog(parent), m_pm(pm), m_canvas(canvas)
{
    setWindowTitle("Mass Polar Reductions");
    setModal(false);
    setMinimumSize(720, 520);

    auto* main = new QVBoxLayout(this);

    // From + options
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("From:", this));
        m_fromBox = new QComboBox(this);
        row->addWidget(m_fromBox, 1);
        row->addSpacing(12);
        m_drawLines = new QCheckBox("Draw lines", this);
        m_drawLines->setChecked(true);
        row->addWidget(m_drawLines);
        row->addStretch();
        main->addLayout(row);
    }

    // Table: Name, Distance, Bearing, Z(optional)
    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels(QStringList() << "Name" << "Distance (m)" << "Bearing (DMS)" << "Z (opt)" );
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    m_table->setMinimumHeight(240);
    main->addWidget(m_table, 1);

    // Row buttons
    {
        auto* row = new QHBoxLayout();
        QPushButton* addBtn = new QPushButton("Add Row", this);
        QPushButton* delBtn = new QPushButton("Remove Selected", this);
        QPushButton* importBtn = new QPushButton("Import CSV", this);
        QPushButton* exportBtn = new QPushButton("Export CSV", this);
        row->addWidget(addBtn);
        row->addWidget(delBtn);
        row->addStretch();
        row->addWidget(importBtn);
        row->addWidget(exportBtn);
        main->addLayout(row);
        connect(addBtn, &QPushButton::clicked, this, &MassPolarDialog::addRow);
        connect(delBtn, &QPushButton::clicked, this, &MassPolarDialog::removeSelected);
        connect(importBtn, &QPushButton::clicked, this, &MassPolarDialog::importCSV);
        connect(exportBtn, &QPushButton::clicked, this, &MassPolarDialog::exportCSV);
    }

    // Output and actions
    {
        auto* row = new QHBoxLayout();
        QPushButton* addCoordsBtn = new QPushButton("Add Coordinates", this);
        row->addStretch();
        row->addWidget(addCoordsBtn);
        main->addLayout(row);
        connect(addCoordsBtn, &QPushButton::clicked, this, &MassPolarDialog::addCoordinates);
    }

    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setMinimumHeight(160);
    main->addWidget(m_output, 1);

    reload();
}

void MassPolarDialog::reload()
{
    if (!m_pm) return;
    const QString current = m_fromBox ? m_fromBox->currentText() : QString();
    const QStringList names = m_pm->getPointNames();
    m_fromBox->clear();
    m_fromBox->addItems(names);
    int idx = names.indexOf(current);
    if (idx >= 0) m_fromBox->setCurrentIndex(idx);
}

void MassPolarDialog::addRow()
{
    int r = m_table->rowCount();
    m_table->insertRow(r);
    m_table->setItem(r, 0, new QTableWidgetItem(QString("P%1").arg(r+1)));
    m_table->setItem(r, 1, new QTableWidgetItem("0.000"));
    m_table->setItem(r, 2, new QTableWidgetItem("0.0000"));
    m_table->setItem(r, 3, new QTableWidgetItem(""));
}

void MassPolarDialog::removeSelected()
{
    auto sel = m_table->selectionModel()->selectedRows();
    std::sort(sel.begin(), sel.end(), [](const QModelIndex& a, const QModelIndex& b){ return a.row() > b.row(); });
    for (const auto& idx : sel) m_table->removeRow(idx.row());
}

void MassPolarDialog::importCSV()
{
    const QString fn = QFileDialog::getOpenFileName(this, "Import Mass Polar CSV", QString(), "CSV (*.csv);;All Files (*)");
    if (fn.isEmpty()) return;
    QFile f(fn);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { QMessageBox::warning(this, "Import CSV", QString("Failed to open %1").arg(fn)); return; }
    QTextStream in(&f);
    QString header = in.readLine(); if (header.isNull()) { QMessageBox::warning(this, "Import CSV", "Empty file"); return; }
    // Expect header: Name,Distance,Azimuth,Z
    QVector<QStringList> rows;
    while (!in.atEnd()) {
        const QString line = in.readLine(); if (line.trimmed().isEmpty()) continue; rows.append(line.split(','));
    }
    m_table->setRowCount(rows.size());
    for (int r=0;r<rows.size();++r) {
        const QStringList cols = rows[r];
        auto setT = [&](int c, const QString& v){ m_table->setItem(r, c, new QTableWidgetItem(v)); };
        if (cols.size()>=1) setT(0, cols.value(0));
        if (cols.size()>=2) setT(1, cols.value(1));
        if (cols.size()>=3) setT(2, cols.value(2));
        if (cols.size()>=4) setT(3, cols.value(3));
    }
}

void MassPolarDialog::exportCSV()
{
    const QString fn = QFileDialog::getSaveFileName(this, "Export Mass Polar CSV", QString(), "CSV (*.csv);;All Files (*)");
    if (fn.isEmpty()) return;
    QFile f(fn);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { QMessageBox::warning(this, "Export CSV", QString("Failed to write %1").arg(fn)); return; }
    QTextStream out(&f);
    out << "Name,Distance,Bearing,Z\n";
    for (int r=0;r<m_table->rowCount();++r) {
        auto get = [&](int c){ if (auto* it=m_table->item(r,c)) return it->text(); return QString(); };
        out << get(0) << "," << get(1) << "," << get(2) << "," << get(3) << "\n";
    }
    f.close();
}

static double parseDmsComponents(const QStringList &parts, bool *ok)
{
    bool okd=false, okm=true, oks=true;
    double d = parts.value(0).toDouble(&okd);
    double m = parts.value(1).toDouble(&okm);
    double s = parts.value(2).toDouble(&oks);
    if (!okd || (!okm && parts.size()>=2) || (!oks && parts.size()>=3)) { *ok=false; return 0.0; }
    *ok = true;
    double sign = d < 0 ? -1.0 : 1.0;
    d = qAbs(d);
    return sign * (d + m/60.0 + s/3600.0);
}

double MassPolarDialog::parseAngleDMS(const QString& text, bool* ok) const
{
    QString t = text.trimmed();
    bool okNum=false; double val = t.toDouble(&okNum);
    if (okNum) { if (ok) *ok=true; return val; }
    QString norm = t;
    norm.replace(QRegularExpression("[°'\"]"), " ");
    norm = norm.simplified();
    QStringList parts = norm.split(' ');
    if (parts.size() < 2 || parts.size() > 3) { if (ok) *ok=false; return 0.0; }
    bool okd=false; double deg = parseDmsComponents(parts, &okd);
    if (ok) *ok = okd;
    return deg;
}

void MassPolarDialog::addCoordinates()
{
    if (!m_pm) { QMessageBox::warning(this, "Mass Polar", "No point manager."); return; }
    const QString fromName = m_fromBox ? m_fromBox->currentText() : QString();
    if (fromName.isEmpty() || !m_pm->hasPoint(fromName)) { QMessageBox::warning(this, "Mass Polar", "Select a valid From point."); return; }
    Point from = m_pm->getPoint(fromName);

    QString rep;
    int added = 0;
    for (int r=0;r<m_table->rowCount();++r) {
        auto* nameItem = m_table->item(r, 0);
        auto* distItem = m_table->item(r, 1);
        auto* azItem = m_table->item(r, 2);
        auto* zItem = m_table->item(r, 3);
        if (!distItem || !azItem) continue;
        const QString newName = nameItem ? nameItem->text().trimmed() : QString();
        bool okd=false; double dist = distItem->text().trimmed().toDouble(&okd); if (!okd || dist <= 0) continue;
        bool oka=false; double az = parseAngleDMS(azItem->text().trimmed(), &oka); if (!oka) continue;
        double z = from.z;
        if (zItem && !zItem->text().trimmed().isEmpty()) {
            bool okz=false; z = zItem->text().trimmed().toDouble(&okz); if (!okz) z = from.z;
        }
        Point to = SurveyCalculator::polarToRectangular(from, dist, az, z);
        to.name = newName.isEmpty() ? QString("P%1").arg(r+1) : newName;
        if (m_pm->hasPoint(to.name)) {
            rep += QString("Skip %1 (exists)\n").arg(to.name);
            continue;
        }
        m_pm->addPoint(to);
        if (m_canvas) {
            m_canvas->addPoint(to);
            if (m_drawLines && m_drawLines->isChecked()) m_canvas->addLine(from.toQPointF(), to.toQPointF());
        }
        rep += QString("%1: (%2, %3, %4)\n").arg(to.name)
              .arg(QString::number(to.x,'f',3))
              .arg(QString::number(to.y,'f',3))
              .arg(QString::number(to.z,'f',3));
        ++added;
    }
    if (m_output) m_output->setPlainText(rep);
    QMessageBox::information(this, "Mass Polar", QString("Added %1 coordinates.").arg(added));
}
