#include "controlregisterdialog.h"
#include "pointmanager.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
bool editControlMetadata(QWidget* parent, Point& point, bool forceControl)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(forceControl ? QObject::tr("Mark Control Point") : QObject::tr("Edit Control Metadata"));
    dialog.setModal(true);

    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;

    auto* typeEdit = new QLineEdit(point.controlType, &dialog);
    form->addRow(QObject::tr("Control Class"), typeEdit);

    auto* sourceEdit = new QLineEdit(point.controlSource, &dialog);
    form->addRow(QObject::tr("Source"), sourceEdit);

    auto* hSpin = new QDoubleSpinBox(&dialog);
    hSpin->setSuffix(QObject::tr(" mm"));
    hSpin->setDecimals(1);
    hSpin->setMinimum(0.0);
    hSpin->setMaximum(5000.0);
    hSpin->setValue(point.controlHorizontalToleranceMm);
    form->addRow(QObject::tr("Horiz Tol"), hSpin);

    auto* vSpin = new QDoubleSpinBox(&dialog);
    vSpin->setSuffix(QObject::tr(" mm"));
    vSpin->setDecimals(1);
    vSpin->setMinimum(0.0);
    vSpin->setMaximum(5000.0);
    vSpin->setValue(point.controlVerticalToleranceMm);
    form->addRow(QObject::tr("Vert Tol"), vSpin);

    auto* notesEdit = new QLineEdit(point.notes, &dialog);
    form->addRow(QObject::tr("Notes"), notesEdit);

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    point.isControl = true;
    point.controlType = typeEdit->text().trimmed();
    point.controlSource = sourceEdit->text().trimmed();
    point.controlHorizontalToleranceMm = hSpin->value();
    point.controlVerticalToleranceMm = vSpin->value();
    point.notes = notesEdit->text().trimmed();
    return true;
}
}

ControlRegisterDialog::ControlRegisterDialog(PointManager* pointManager, QWidget* parent)
    : QDialog(parent), m_pointManager(pointManager)
{
    setWindowTitle(tr("Control Register"));
    resize(820, 420);

    auto* layout = new QVBoxLayout(this);
    m_table = new QTableWidget(this);
    m_table->setColumnCount(10);
    m_table->setHorizontalHeaderLabels({
        tr("Name"),
        tr("Easting"),
        tr("Northing"),
        tr("Elevation"),
        tr("Control"),
        tr("Class"),
        tr("Source"),
        tr("H Tol (mm)"),
        tr("V Tol (mm)"),
        tr("Notes")
    });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);

    auto* buttonRow = new QHBoxLayout;
    m_markButton = new QPushButton(tr("Mark Control"), this);
    m_clearButton = new QPushButton(tr("Clear Control"), this);
    m_editButton = new QPushButton(tr("Edit Metadata"), this);
    auto* closeButton = new QPushButton(tr("Close"), this);

    buttonRow->addWidget(m_markButton);
    buttonRow->addWidget(m_clearButton);
    buttonRow->addWidget(m_editButton);
    buttonRow->addStretch();
    buttonRow->addWidget(closeButton);
    layout->addLayout(buttonRow);

    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_markButton, &QPushButton::clicked, this, &ControlRegisterDialog::markAsControl);
    connect(m_clearButton, &QPushButton::clicked, this, &ControlRegisterDialog::clearControl);
    connect(m_editButton, &QPushButton::clicked, this, &ControlRegisterDialog::editMetadata);

    if (m_pointManager) {
        connect(m_pointManager, &PointManager::pointAdded, this, &ControlRegisterDialog::refresh);
        connect(m_pointManager, &PointManager::pointUpdated, this, &ControlRegisterDialog::refresh);
        connect(m_pointManager, &PointManager::pointRemoved, this, &ControlRegisterDialog::refresh);
        connect(m_pointManager, &PointManager::pointsCleared, this, &ControlRegisterDialog::refresh);
    }

    refresh();
}

void ControlRegisterDialog::refresh()
{
    if (!m_pointManager) return;
    const QVector<Point> points = m_pointManager->getAllPoints();
    m_table->setRowCount(points.size());
    int row = 0;
    for (const Point& p : points) {
        auto setItem = [&](int column, const QString& text) {
            auto* item = new QTableWidgetItem(text);
            if (column == 0) item->setData(Qt::UserRole, p.name);
            if (!p.isControl) item->setForeground(QBrush(Qt::gray));
            m_table->setItem(row, column, item);
        };
        setItem(0, p.name);
        setItem(1, QString::number(p.x, 'f', 3));
        setItem(2, QString::number(p.y, 'f', 3));
        setItem(3, QString::number(p.z, 'f', 3));
        setItem(4, p.isControl ? tr("Yes") : tr("No"));
        setItem(5, p.controlType);
        setItem(6, p.controlSource);
        setItem(7, p.isControl ? QString::number(p.controlHorizontalToleranceMm, 'f', 1) : QString());
        setItem(8, p.isControl ? QString::number(p.controlVerticalToleranceMm, 'f', 1) : QString());
        setItem(9, p.notes);
        ++row;
    }
    m_table->resizeColumnsToContents();
}

QList<int> ControlRegisterDialog::selectedRows() const
{
    QList<int> rows;
    if (!m_table || !m_table->selectionModel()) return rows;
    for (const QModelIndex& index : m_table->selectionModel()->selectedRows()) {
        rows.append(index.row());
    }
    return rows;
}

void ControlRegisterDialog::markAsControl()
{
    if (!m_pointManager) return;
    const QList<int> rows = selectedRows();
    if (rows.isEmpty()) {
        QMessageBox::information(this, tr("Control Register"), tr("Select at least one point to mark."));
        return;
    }
    bool anyChanged = false;
    for (int row : rows) {
        QTableWidgetItem* nameItem = m_table->item(row, 0);
        if (!nameItem) continue;
        const QString name = nameItem->data(Qt::UserRole).toString();
        Point p = m_pointManager->getPoint(name);
        if (p.name.isEmpty()) continue;
        if (!editControlMetadata(this, p, true)) continue;
        m_pointManager->setControlMetadata(p.name,
                                           true,
                                           p.controlType,
                                           p.controlSource,
                                           p.controlHorizontalToleranceMm,
                                           p.controlVerticalToleranceMm,
                                           p.notes);
        anyChanged = true;
    }
    if (anyChanged) refresh();
}

void ControlRegisterDialog::clearControl()
{
    if (!m_pointManager) return;
    const QList<int> rows = selectedRows();
    if (rows.isEmpty()) {
        QMessageBox::information(this, tr("Control Register"), tr("Select at least one point to clear."));
        return;
    }
    bool anyChanged = false;
    for (int row : rows) {
        QTableWidgetItem* nameItem = m_table->item(row, 0);
        if (!nameItem) continue;
        const QString name = nameItem->data(Qt::UserRole).toString();
        if (m_pointManager->clearControlMetadata(name)) {
            anyChanged = true;
        }
    }
    if (anyChanged) refresh();
}

void ControlRegisterDialog::editMetadata()
{
    if (!m_pointManager) return;
    const QList<int> rows = selectedRows();
    if (rows.size() != 1) {
        QMessageBox::information(this, tr("Control Register"), tr("Select a single point to edit."));
        return;
    }
    QTableWidgetItem* nameItem = m_table->item(rows.first(), 0);
    if (!nameItem) return;
    const QString name = nameItem->data(Qt::UserRole).toString();
    Point p = m_pointManager->getPoint(name);
    if (p.name.isEmpty()) return;
    if (!p.isControl) {
        if (!editControlMetadata(this, p, true)) return;
    } else {
        if (!editControlMetadata(this, p, false)) return;
    }
    m_pointManager->setControlMetadata(p.name,
                                       true,
                                       p.controlType,
                                       p.controlSource,
                                       p.controlHorizontalToleranceMm,
                                       p.controlVerticalToleranceMm,
                                       p.notes);
    refresh();
}
