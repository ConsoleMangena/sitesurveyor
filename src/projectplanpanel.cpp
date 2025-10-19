#include "projectplanpanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QComboBox>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDate>

ProjectPlanPanel::ProjectPlanPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(4, 4, 4, 4);
    main->setSpacing(4);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels(QStringList()
        << "Title" << "Status" << "Priority" << "Due");
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);

    connect(m_table, &QTableWidget::cellChanged, this, &ProjectPlanPanel::onCellChanged);

    main->addWidget(m_table, 1);

    auto* btnRow = new QHBoxLayout();
    m_addBtn = new QPushButton("Add", this);
    m_removeBtn = new QPushButton("Remove", this);
    m_upBtn = new QPushButton("Up", this);
    m_downBtn = new QPushButton("Down", this);
    m_saveBtn = new QPushButton("Save", this);

    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addWidget(m_upBtn);
    btnRow->addWidget(m_downBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_saveBtn);
    main->addLayout(btnRow);

    connect(m_addBtn, &QPushButton::clicked, this, &ProjectPlanPanel::addTask);
    connect(m_removeBtn, &QPushButton::clicked, this, &ProjectPlanPanel::removeSelectedTasks);
    connect(m_upBtn, &QPushButton::clicked, this, &ProjectPlanPanel::moveTaskUp);
    connect(m_downBtn, &QPushButton::clicked, this, &ProjectPlanPanel::moveTaskDown);
    connect(m_saveBtn, &QPushButton::clicked, this, &ProjectPlanPanel::save);

    load();
}

void ProjectPlanPanel::load()
{
    QSettings s;
    const QString json = s.value("projectPlan/json", QString()).toString();
    QVector<Task> tasks;
    if (!json.trimmed().isEmpty()) {
        const auto doc = QJsonDocument::fromJson(json.toUtf8());
        if (doc.isArray()) {
            const auto arr = doc.array();
            tasks.reserve(arr.size());
            for (const auto& v : arr) {
                if (!v.isObject()) continue;
                const auto o = v.toObject();
                Task t;
                t.title = o.value("title").toString();
                t.status = o.value("status").toString();
                t.priority = o.value("priority").toString();
                t.due = o.value("due").toString();
                tasks.append(t);
            }
        }
    }
    setTasks(tasks);
}

void ProjectPlanPanel::save() const
{
    const QVector<Task> tasks = collectTasks();
    QJsonArray arr;
    for (const auto& t : tasks) {
        QJsonObject o;
        o.insert("title", t.title);
        o.insert("status", t.status);
        o.insert("priority", t.priority);
        o.insert("due", t.due);
        arr.append(o);
    }
    QJsonDocument doc(arr);
    QSettings s;
    s.setValue("projectPlan/json", QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void ProjectPlanPanel::addTask()
{
    int r = m_table->rowCount();
    m_table->insertRow(r);
    m_table->setItem(r, 0, new QTableWidgetItem("New Task"));
    m_table->setItem(r, 3, new QTableWidgetItem(QDate::currentDate().toString(Qt::ISODate)));
    rebuildCombosForRow(r);
}

void ProjectPlanPanel::removeSelectedTasks()
{
    auto rows = m_table->selectionModel()->selectedRows();
    std::sort(rows.begin(), rows.end(), [](const QModelIndex& a, const QModelIndex& b){ return a.row() > b.row(); });
    for (const auto& idx : rows) m_table->removeRow(idx.row());
}

void ProjectPlanPanel::moveTaskUp()
{
    auto rows = m_table->selectionModel()->selectedRows();
    if (rows.size() != 1) return;
    int r = rows.first().row();
    if (r <= 0) return;
    m_table->insertRow(r-1);
    for (int c = 0; c < m_table->columnCount(); ++c) {
        QTableWidgetItem* it = m_table->takeItem(r+1, c);
        m_table->setItem(r-1, c, it);
    }
    // Recreate combos for moved row and previous
    rebuildCombosForRow(r-1);
    m_table->removeRow(r+1);
    m_table->selectRow(r-1);
}

void ProjectPlanPanel::moveTaskDown()
{
    auto rows = m_table->selectionModel()->selectedRows();
    if (rows.size() != 1) return;
    int r = rows.first().row();
    if (r >= m_table->rowCount()-1) return;
    m_table->insertRow(r+2);
    for (int c = 0; c < m_table->columnCount(); ++c) {
        QTableWidgetItem* it = m_table->takeItem(r, c);
        m_table->setItem(r+2, c, it);
    }
    rebuildCombosForRow(r+2);
    m_table->removeRow(r);
    m_table->selectRow(r+1);
}

void ProjectPlanPanel::onCellChanged(int row, int column)
{
    Q_UNUSED(row);
    Q_UNUSED(column);
    // No-op for now; combos handle status/priority
}

QString ProjectPlanPanel::statusDisplay(const QString& s)
{
    if (s.compare("in_progress", Qt::CaseInsensitive) == 0) return "In Progress";
    if (s.compare("blocked", Qt::CaseInsensitive) == 0) return "Blocked";
    if (s.compare("done", Qt::CaseInsensitive) == 0) return "Done";
    return "Pending";
}

QString ProjectPlanPanel::statusCode(const QString& display)
{
    if (display.compare("In Progress", Qt::CaseInsensitive) == 0) return "in_progress";
    if (display.compare("Blocked", Qt::CaseInsensitive) == 0) return "blocked";
    if (display.compare("Done", Qt::CaseInsensitive) == 0) return "done";
    return "pending";
}

QString ProjectPlanPanel::priorityDisplay(const QString& p)
{
    if (p.compare("high", Qt::CaseInsensitive) == 0) return "High";
    if (p.compare("low", Qt::CaseInsensitive) == 0) return "Low";
    return "Medium";
}

QString ProjectPlanPanel::priorityCode(const QString& display)
{
    if (display.compare("High", Qt::CaseInsensitive) == 0) return "high";
    if (display.compare("Low", Qt::CaseInsensitive) == 0) return "low";
    return "medium";
}

void ProjectPlanPanel::rebuildCombosForRow(int row)
{
    // Ensure items exist
    if (!m_table->item(row, 0)) m_table->setItem(row, 0, new QTableWidgetItem(""));
    if (!m_table->item(row, 1)) m_table->setItem(row, 1, new QTableWidgetItem("Pending"));
    if (!m_table->item(row, 2)) m_table->setItem(row, 2, new QTableWidgetItem("Medium"));
    if (!m_table->item(row, 3)) m_table->setItem(row, 3, new QTableWidgetItem(""));

    // Status combo
    auto* statusCombo = new QComboBox(m_table);
    statusCombo->addItems({"Pending", "In Progress", "Blocked", "Done"});
    int sIdx = statusCombo->findText(m_table->item(row, 1)->text());
    if (sIdx < 0) sIdx = 0;
    statusCombo->setCurrentIndex(sIdx);
    m_table->setCellWidget(row, 1, statusCombo);
    connect(statusCombo, &QComboBox::currentTextChanged, this, [this, row](const QString& t){
        if (m_table->item(row, 1)) m_table->item(row, 1)->setText(t);
    });

    // Priority combo
    auto* priCombo = new QComboBox(m_table);
    priCombo->addItems({"High", "Medium", "Low"});
    int pIdx = priCombo->findText(m_table->item(row, 2)->text());
    if (pIdx < 0) pIdx = 1; // default Medium
    priCombo->setCurrentIndex(pIdx);
    m_table->setCellWidget(row, 2, priCombo);
    connect(priCombo, &QComboBox::currentTextChanged, this, [this, row](const QString& t){
        if (m_table->item(row, 2)) m_table->item(row, 2)->setText(t);
    });
}

QVector<ProjectPlanPanel::Task> ProjectPlanPanel::collectTasks() const
{
    QVector<Task> tasks;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        Task t;
        t.title = m_table->item(r, 0) ? m_table->item(r, 0)->text() : QString();
        const QString sDisp = m_table->item(r, 1) ? m_table->item(r, 1)->text() : QString("Pending");
        t.status = statusCode(sDisp);
        const QString pDisp = m_table->item(r, 2) ? m_table->item(r, 2)->text() : QString("Medium");
        t.priority = priorityCode(pDisp);
        t.due = m_table->item(r, 3) ? m_table->item(r, 3)->text() : QString();
        if (!t.title.trimmed().isEmpty()) tasks.append(t);
    }
    return tasks;
}

void ProjectPlanPanel::setTasks(const QVector<ProjectPlanPanel::Task>& tasks)
{
    m_table->blockSignals(true);
    m_table->setRowCount(0);
    int r = 0;
    for (const auto& t : tasks) {
        m_table->insertRow(r);
        m_table->setItem(r, 0, new QTableWidgetItem(t.title));
        m_table->setItem(r, 1, new QTableWidgetItem(statusDisplay(t.status)));
        m_table->setItem(r, 2, new QTableWidgetItem(priorityDisplay(t.priority)));
        m_table->setItem(r, 3, new QTableWidgetItem(t.due));
        rebuildCombosForRow(r);
        ++r;
    }
    m_table->blockSignals(false);
}
