#include "stakeoutdialog.h"
#include "stakeoutmanager.h"
#include "pointmanager.h"
#include "point.h"
#include "appsettings.h"
#include "stakeoutreportgenerator.h"
#include <QAbstractItemView>
#include <QBrush>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QTextStream>

namespace {
struct DesignDialogResult {
    QString pointName;
    QString description;
    QString method;
    QString crew;
    QString remarks;
    bool accepted{false};
};

DesignDialogResult requestDesign(QWidget* parent, PointManager* manager)
{
    DesignDialogResult result;
    if (!manager) return result;
    const QVector<Point> points = manager->getAllPoints();
    if (points.isEmpty()) {
        QMessageBox::information(parent, QObject::tr("Stakeout"), QObject::tr("No coordinates available to stage for stakeout."));
        return result;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("New Stakeout Design"));
    dialog.setModal(true);

    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;

    auto* pointCombo = new QComboBox(&dialog);
    for (const Point& p : points) {
        pointCombo->addItem(p.name);
    }
    form->addRow(QObject::tr("Design Point"), pointCombo);

    auto* descriptionEdit = new QLineEdit(&dialog);
    QObject::connect(pointCombo, &QComboBox::currentTextChanged, [&points, descriptionEdit](const QString& text) {
        for (const Point& p : points) {
            if (p.name == text) {
                descriptionEdit->setPlaceholderText(p.description.isEmpty() ? text : p.description);
                break;
            }
        }
    });
    form->addRow(QObject::tr("Description"), descriptionEdit);

    auto* methodEdit = new QLineEdit(&dialog);
    methodEdit->setPlaceholderText(QObject::tr("Total station / intersection / tape"));
    form->addRow(QObject::tr("Method"), methodEdit);

    auto* crewEdit = new QLineEdit(&dialog);
    crewEdit->setPlaceholderText(QObject::tr("Field crew"));
    form->addRow(QObject::tr("Crew"), crewEdit);

    auto* remarksEdit = new QPlainTextEdit(&dialog);
    remarksEdit->setPlaceholderText(QObject::tr("Design notes"));
    remarksEdit->setTabChangesFocus(true);
    form->addRow(QObject::tr("Remarks"), remarksEdit);

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        result.accepted = true;
    result.pointName = pointCombo->currentText();
        result.description = descriptionEdit->text().trimmed();
        if (result.description.isEmpty()) result.description = pointCombo->currentText();
        result.method = methodEdit->text().trimmed();
        result.crew = crewEdit->text().trimmed();
        result.remarks = remarksEdit->toPlainText().trimmed();
    }
    return result;
}

struct MeasurementDialogResult {
    double measuredE{0.0};
    double measuredN{0.0};
    double measuredZ{0.0};
    QString instrument;
    QString setup;
    QString status;
    QString remarks;
    bool accepted{false};
};

MeasurementDialogResult requestMeasurement(QWidget* parent, const StakeoutRecord& record)
{
    MeasurementDialogResult result;

    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Record Stakeout Measurement"));
    dialog.setModal(true);

    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;

    auto makeSpin = [&dialog](double value) {
        auto* spin = new QDoubleSpinBox(&dialog);
        spin->setDecimals(3);
        spin->setRange(-1e9, 1e9);
        spin->setValue(value);
        spin->setSingleStep(0.001);
        return spin;
    };

    auto* eSpin = makeSpin(record.designE);
    auto* nSpin = makeSpin(record.designN);
    auto* zSpin = makeSpin(record.designZ);

    form->addRow(QObject::tr("Measured Easting"), eSpin);
    form->addRow(QObject::tr("Measured Northing"), nSpin);
    form->addRow(QObject::tr("Measured Elevation"), zSpin);

    auto* instrumentEdit = new QLineEdit(record.instrument, &dialog);
    instrumentEdit->setPlaceholderText(QObject::tr("Instrument / prism"));
    form->addRow(QObject::tr("Instrument"), instrumentEdit);

    auto* setupEdit = new QLineEdit(record.setupDetails, &dialog);
    setupEdit->setPlaceholderText(QObject::tr("Station setup or backsight"));
    form->addRow(QObject::tr("Setup"), setupEdit);

    auto* statusCombo = new QComboBox(&dialog);
    const QStringList statuses = AppSettings::stakeoutStatusOptions();
    for (const QString& status : statuses) {
        statusCombo->addItem(status);
    }
    int statusIdx = statuses.indexOf(record.status);
    if (statusIdx >= 0) statusCombo->setCurrentIndex(statusIdx);
    form->addRow(QObject::tr("Status"), statusCombo);

    auto* remarksEdit = new QPlainTextEdit(record.remarks, &dialog);
    remarksEdit->setPlaceholderText(QObject::tr("QA observations"));
    remarksEdit->setTabChangesFocus(true);
    form->addRow(QObject::tr("Remarks"), remarksEdit);

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        result.accepted = true;
        result.measuredE = eSpin->value();
        result.measuredN = nSpin->value();
        result.measuredZ = zSpin->value();
        result.instrument = instrumentEdit->text().trimmed();
        result.setup = setupEdit->text().trimmed();
        result.status = statusCombo->currentText();
        result.remarks = remarksEdit->toPlainText().trimmed();
    }
    return result;
}

QString residualSummary(const StakeoutRecord& record)
{
    QString summary = QObject::tr("Stakeout %1 (%2)").arg(record.designPoint, record.description);
    if (record.hasMeasurement()) {
        summary += QObject::tr("\nΔE: %1 m\nΔN: %2 m\nΔZ: %3 m")
                       .arg(QString::number(record.deltaE(), 'f', 3))
                       .arg(QString::number(record.deltaN(), 'f', 3))
                       .arg(QString::number(record.deltaZ(), 'f', 3));
        summary += QObject::tr("\nHorizontal: %1 m\nVertical: %2 m")
                       .arg(QString::number(record.horizontalResidual(), 'f', 3))
                       .arg(QString::number(record.verticalResidual(), 'f', 3));
    } else {
        summary += QObject::tr("\nNo measurement recorded yet.");
    }
    if (!record.status.isEmpty()) {
        summary += QObject::tr("\nStatus: %1").arg(record.status);
    }
    if (!record.remarks.isEmpty()) {
        summary += QObject::tr("\nRemarks: %1").arg(record.remarks);
    }
    return summary;
}
}

StakeoutDialog::StakeoutDialog(StakeoutManager* manager, PointManager* pointManager, QWidget* parent)
    : QDialog(parent), m_stakeoutManager(manager), m_pointManager(pointManager)
{
    setWindowTitle(tr("Stakeout Manager"));
    resize(960, 520);

    auto* layout = new QVBoxLayout(this);
    m_table = new QTableWidget(this);
    m_table->setColumnCount(20);
    m_table->setHorizontalHeaderLabels({
        tr("Status"),
        tr("Point"),
        tr("Description"),
        tr("Design E"),
        tr("Design N"),
        tr("Design Z"),
        tr("Measured E"),
        tr("Measured N"),
        tr("Measured Z"),
        tr("ΔE"),
        tr("ΔN"),
        tr("ΔZ"),
        tr("Horiz"),
        tr("Vert"),
        tr("Instrument"),
        tr("Crew"),
        tr("Method"),
        tr("Setup"),
        tr("Created"),
        tr("Observed")
    });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);

    auto* buttonsLayout = new QHBoxLayout;
    auto* addButton = new QPushButton(tr("New Design"), this);
    m_measureButton = new QPushButton(tr("Record Measurement"), this);
    m_completeButton = new QPushButton(tr("Mark Complete"), this);
    auto* copyButton = new QPushButton(tr("Copy Residuals"), this);
    auto* exportButton = new QPushButton(tr("Export CSV"), this);
    auto* closeButton = new QPushButton(tr("Close"), this);

    buttonsLayout->addWidget(addButton);
    buttonsLayout->addWidget(m_measureButton);
    buttonsLayout->addWidget(m_completeButton);
    buttonsLayout->addWidget(copyButton);
    buttonsLayout->addWidget(exportButton);
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(closeButton);
    layout->addLayout(buttonsLayout);

    connect(addButton, &QPushButton::clicked, this, &StakeoutDialog::addDesignRecord);
    connect(m_measureButton, &QPushButton::clicked, this, &StakeoutDialog::recordMeasurement);
    connect(m_completeButton, &QPushButton::clicked, this, &StakeoutDialog::markComplete);
    connect(copyButton, &QPushButton::clicked, this, &StakeoutDialog::copyResidualSummary);
    connect(exportButton, &QPushButton::clicked, this, &StakeoutDialog::exportReport);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    if (m_stakeoutManager) {
        connect(m_stakeoutManager, &StakeoutManager::recordAdded, this, &StakeoutDialog::onRecordAdded);
        connect(m_stakeoutManager, &StakeoutManager::recordUpdated, this, &StakeoutDialog::onRecordUpdated);
        connect(m_stakeoutManager, &StakeoutManager::recordRemoved, this, &StakeoutDialog::onRecordRemoved);
    }

    refreshRecords();
}

void StakeoutDialog::refreshRecords()
{
    if (!m_stakeoutManager) return;
    const QVector<StakeoutRecord> recs = m_stakeoutManager->records();
    m_table->setRowCount(recs.size());
    for (int row = 0; row < recs.size(); ++row) {
        populateRow(row, recs[row]);
    }
    if (!recs.isEmpty()) {
        m_table->selectRow(0);
    }
}

void StakeoutDialog::populateRow(int row, const StakeoutRecord& record)
{
    auto setItem = [&](int column, const QString& text) {
        auto* item = new QTableWidgetItem(text);
        if (column == 0) item->setData(Qt::UserRole, record.id);
        if (!record.hasMeasurement()) {
            item->setForeground(QBrush(Qt::darkGray));
        }
        m_table->setItem(row, column, item);
    };

    setItem(0, record.status.isEmpty() ? tr("Planned") : record.status);
    setItem(1, record.designPoint);
    setItem(2, record.description);
    setItem(3, QString::number(record.designE, 'f', 3));
    setItem(4, QString::number(record.designN, 'f', 3));
    setItem(5, QString::number(record.designZ, 'f', 3));
    setItem(6, record.hasMeasurement() ? QString::number(record.measuredE, 'f', 3) : QString());
    setItem(7, record.hasMeasurement() ? QString::number(record.measuredN, 'f', 3) : QString());
    setItem(8, record.hasMeasurement() ? QString::number(record.measuredZ, 'f', 3) : QString());
    setItem(9, record.hasMeasurement() ? QString::number(record.deltaE(), 'f', 3) : QString());
    setItem(10, record.hasMeasurement() ? QString::number(record.deltaN(), 'f', 3) : QString());
    setItem(11, record.hasMeasurement() ? QString::number(record.deltaZ(), 'f', 3) : QString());
    setItem(12, record.hasMeasurement() ? QString::number(record.horizontalResidual(), 'f', 3) : QString());
    setItem(13, record.hasMeasurement() ? QString::number(record.verticalResidual(), 'f', 3) : QString());
    setItem(14, record.instrument);
    setItem(15, record.crew);
    setItem(16, record.method);
    setItem(17, record.setupDetails);
    setItem(18, record.createdAt.isValid() ? record.createdAt.toLocalTime().toString(Qt::ISODate) : QString());
    setItem(19, record.observedAt.isValid() ? record.observedAt.toLocalTime().toString(Qt::ISODate) : QString());
}

QString StakeoutDialog::selectedRecordId() const
{
    if (!m_table || !m_table->selectionModel()) return QString();
    const QModelIndexList rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) return QString();
    QTableWidgetItem* statusItem = m_table->item(rows.first().row(), 0);
    if (!statusItem) return QString();
    return statusItem->data(Qt::UserRole).toString();
}

void StakeoutDialog::addDesignRecord()
{
    if (!m_stakeoutManager || !m_pointManager) return;
    DesignDialogResult result = requestDesign(this, m_pointManager);
    if (!result.accepted) return;
    Point designPoint = m_pointManager->getPoint(result.pointName);
    if (designPoint.name.isEmpty()) {
        QMessageBox::warning(this, tr("Stakeout"), tr("Selected point could not be found."));
        return;
    }
    const QString id = m_stakeoutManager->createDesignRecord(designPoint.name,
                                                             designPoint.x,
                                                             designPoint.y,
                                                             designPoint.z,
                                                             result.description,
                                                             result.method,
                                                             result.crew,
                                                             result.remarks);
    if (!id.isEmpty()) {
        refreshRecords();
        const QVector<StakeoutRecord> recs = m_stakeoutManager->records();
        for (int row = 0; row < recs.size(); ++row) {
            if (recs[row].id == id) {
                m_table->selectRow(row);
                break;
            }
        }
    }
}

void StakeoutDialog::recordMeasurement()
{
    if (!m_stakeoutManager) return;
    const QString id = selectedRecordId();
    if (id.isEmpty()) {
        QMessageBox::information(this, tr("Stakeout"), tr("Select a design to update."));
        return;
    }
    const StakeoutRecord record = m_stakeoutManager->record(id);
    MeasurementDialogResult result = requestMeasurement(this, record);
    if (!result.accepted) return;
    if (!m_stakeoutManager->recordMeasurement(id,
                                             result.measuredE,
                                             result.measuredN,
                                             result.measuredZ,
                                             result.instrument,
                                             result.setup,
                                             result.status,
                                             result.remarks)) {
        QMessageBox::warning(this, tr("Stakeout"), tr("Failed to record measurement."));
        return;
    }
    refreshRecords();
}

void StakeoutDialog::markComplete()
{
    if (!m_stakeoutManager) return;
    const QString id = selectedRecordId();
    if (id.isEmpty()) {
        QMessageBox::information(this, tr("Stakeout"), tr("Select a design to mark."));
        return;
    }
    if (!m_stakeoutManager->setRecordStatus(id, tr("Complete"))) {
        QMessageBox::warning(this, tr("Stakeout"), tr("Unable to update status."));
        return;
    }
    refreshRecords();
}

void StakeoutDialog::exportReport()
{
    if (!m_stakeoutManager) return;
    const QString path = QFileDialog::getSaveFileName(this,
                                                      tr("Export Stakeout Report"),
                                                      QString(),
                                                      tr("CSV Files (*.csv);;HTML Files (*.html)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Stakeout"), tr("Could not write to %1").arg(path));
        return;
    }
    if (path.endsWith(QStringLiteral(".html"), Qt::CaseInsensitive) || path.endsWith(QStringLiteral(".htm"), Qt::CaseInsensitive)) {
        const QString html = StakeoutReportGenerator::generateHtml(m_stakeoutManager->records());
        QTextStream out(&file);
        out << html;
        out.flush();
    } else {
        const QByteArray data = m_stakeoutManager->exportCsv();
        file.write(data);
    }
    file.close();
    QMessageBox::information(this, tr("Stakeout"), tr("Export complete."));
}

void StakeoutDialog::copyResidualSummary()
{
    if (!m_stakeoutManager) return;
    const QString id = selectedRecordId();
    if (id.isEmpty()) {
        QMessageBox::information(this, tr("Stakeout"), tr("Select a design first."));
        return;
    }
    const StakeoutRecord record = m_stakeoutManager->record(id);
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(residualSummary(record));
    QMessageBox::information(this, tr("Stakeout"), tr("Residual summary copied to clipboard."));
}

void StakeoutDialog::onRecordAdded(const StakeoutRecord& record)
{
    Q_UNUSED(record);
    refreshRecords();
}

void StakeoutDialog::onRecordUpdated(const StakeoutRecord& record)
{
    Q_UNUSED(record);
    refreshRecords();
}

void StakeoutDialog::onRecordRemoved(const QString& id)
{
    Q_UNUSED(id);
    refreshRecords();
}
