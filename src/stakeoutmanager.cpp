#include "stakeoutmanager.h"
#include "pointmanager.h"
#include "point.h"

#include <QRandomGenerator>
#include <QTextStream>
#include <QStringList>
#include <QDateTime>
#include <QChar>

namespace {
QString formatTimestamp(const QDateTime& dt)
{
    if (!dt.isValid()) return QString();
    return dt.toLocalTime().toString(Qt::ISODate);
}
}

StakeoutManager::StakeoutManager(PointManager* pointManager, QObject* parent)
    : QObject(parent), m_pointManager(pointManager)
{
    if (m_pointManager) {
        connect(m_pointManager, &PointManager::pointRemoved,
                this, &StakeoutManager::onPointRemoved);
        connect(m_pointManager, &PointManager::pointsCleared,
                this, &StakeoutManager::onPointsCleared);
    }
}

QString StakeoutManager::generateId() const
{
    quint64 value = QRandomGenerator::global()->generate64();
    return QString::number(static_cast<qulonglong>(value), 36).toUpper();
}

QString StakeoutManager::createDesignRecord(const QString& pointName,
                                            double designE,
                                            double designN,
                                            double designZ,
                                            const QString& description,
                                            const QString& method,
                                            const QString& crew,
                                            const QString& remarks)
{
    StakeoutRecord record;
    record.id = generateId();
    record.designPoint = pointName;
    record.designE = designE;
    record.designN = designN;
    record.designZ = designZ;
    record.description = description.isEmpty() ? pointName : description;
    record.method = method;
    record.crew = crew;
    record.remarks = remarks;
    record.status = QStringLiteral("Planned");
    record.createdAt = QDateTime::currentDateTimeUtc();

    m_records.insert(record.id, record);
    emit recordAdded(record);
    updatePointStakeoutMetadata(record);
    return record.id;
}

bool StakeoutManager::recordMeasurement(const QString& id,
                                        double measuredE,
                                        double measuredN,
                                        double measuredZ,
                                        const QString& instrument,
                                        const QString& setupDetails,
                                        const QString& status,
                                        const QString& remarks)
{
    auto it = m_records.find(id);
    if (it == m_records.end()) return false;

    StakeoutRecord& record = it.value();
    record.measuredE = measuredE;
    record.measuredN = measuredN;
    record.measuredZ = measuredZ;
    record.instrument = instrument;
    record.setupDetails = setupDetails;
    record.observedAt = QDateTime::currentDateTimeUtc();
    if (!status.isEmpty()) {
        record.status = status;
    } else if (record.status.isEmpty() || record.status == QStringLiteral("Planned")) {
        record.status = QStringLiteral("Observed");
    }
    if (!remarks.isEmpty()) {
        if (!record.remarks.isEmpty()) record.remarks.append('\n');
        record.remarks.append(remarks);
    }

    emit recordUpdated(record);
    updatePointStakeoutMetadata(record);
    return true;
}

bool StakeoutManager::removeRecord(const QString& id)
{
    auto it = m_records.find(id);
    if (it == m_records.end()) return false;
    const StakeoutRecord record = it.value();
    m_records.erase(it);
    emit recordRemoved(id);

    if (m_pointManager && !record.designPoint.isEmpty()) {
        m_pointManager->setStakeoutStatus(record.designPoint, QString());
        m_pointManager->setStakeoutResiduals(record.designPoint, 0.0, 0.0, 0.0);
    }
    return true;
}

StakeoutRecord StakeoutManager::record(const QString& id) const
{
    return m_records.value(id);
}

QVector<StakeoutRecord> StakeoutManager::records() const
{
    QVector<StakeoutRecord> out;
    out.reserve(m_records.size());
    for (const auto& rec : m_records) {
        out.append(rec);
    }
    return out;
}

QVector<StakeoutRecord> StakeoutManager::pendingRecords() const
{
    QVector<StakeoutRecord> out;
    for (const auto& rec : m_records) {
        if (!rec.hasMeasurement()) out.append(rec);
    }
    return out;
}

QVector<StakeoutRecord> StakeoutManager::completedRecords() const
{
    QVector<StakeoutRecord> out;
    for (const auto& rec : m_records) {
        if (rec.hasMeasurement()) out.append(rec);
    }
    return out;
}

bool StakeoutManager::setRecordStatus(const QString& id, const QString& status)
{
    auto it = m_records.find(id);
    if (it == m_records.end()) return false;
    StakeoutRecord& record = it.value();
    record.status = status;
    emit recordUpdated(record);
    updatePointStakeoutMetadata(record);
    return true;
}

bool StakeoutManager::appendRemarks(const QString& id, const QString& remarks)
{
    auto it = m_records.find(id);
    if (it == m_records.end()) return false;
    StakeoutRecord& record = it.value();
    if (!record.remarks.isEmpty()) record.remarks.append('\n');
    record.remarks.append(remarks);
    emit recordUpdated(record);
    return true;
}

QByteArray StakeoutManager::exportCsv() const
{
    QStringList lines;
    lines << QStringLiteral("ID,Point,Description,DesignE,DesignN,DesignZ,MeasuredE,MeasuredN,MeasuredZ,DeltaE,DeltaN,DeltaZ,HorizontalResidual,VerticalResidual,Status,Instrument,Crew,Method,Setup,Created,Observed,Remarks");
    for (const auto& record : m_records) {
        const QString measuredE = record.hasMeasurement() ? QString::number(record.measuredE, 'f', 3) : QString();
        const QString measuredN = record.hasMeasurement() ? QString::number(record.measuredN, 'f', 3) : QString();
        const QString measuredZ = record.hasMeasurement() ? QString::number(record.measuredZ, 'f', 3) : QString();
        const QString deltaE = record.hasMeasurement() ? QString::number(record.deltaE(), 'f', 3) : QString();
        const QString deltaN = record.hasMeasurement() ? QString::number(record.deltaN(), 'f', 3) : QString();
        const QString deltaZ = record.hasMeasurement() ? QString::number(record.deltaZ(), 'f', 3) : QString();
        const QString horiz = record.hasMeasurement() ? QString::number(record.horizontalResidual(), 'f', 3) : QString();
        const QString vert = record.hasMeasurement() ? QString::number(record.verticalResidual(), 'f', 3) : QString();

        QStringList cols{
            record.id,
            record.designPoint,
            record.description,
            QString::number(record.designE, 'f', 3),
            QString::number(record.designN, 'f', 3),
            QString::number(record.designZ, 'f', 3),
            measuredE,
            measuredN,
            measuredZ,
            deltaE,
            deltaN,
            deltaZ,
            horiz,
            vert,
            record.status,
            record.instrument,
            record.crew,
            record.method,
            record.setupDetails,
            formatTimestamp(record.createdAt),
            formatTimestamp(record.observedAt),
            record.remarks
        };
        for (QString& col : cols) {
            QString escaped = col;
            escaped.replace('"', QString(2, QChar('"')));
            if (escaped.contains(',') || escaped.contains('\n') || escaped != col) {
                col = '"' + escaped + '"';
            } else {
                col = escaped;
            }
        }
        lines << cols.join(',');
    }
    return lines.join('\n').toUtf8();
}

void StakeoutManager::loadRecords(const QVector<StakeoutRecord>& records)
{
    if (!m_records.isEmpty()) {
        const QStringList previousIds = m_records.keys();
        m_records.clear();
        for (const QString& id : previousIds) {
            emit recordRemoved(id);
        }
    }
    for (StakeoutRecord record : records) {
        if (record.id.trimmed().isEmpty()) {
            record.id = generateId();
        }
        m_records.insert(record.id, record);
        emit recordAdded(record);
        updatePointStakeoutMetadata(record);
    }
}

void StakeoutManager::onPointRemoved(const QString& name)
{
    QStringList toRemove;
    for (auto it = m_records.cbegin(); it != m_records.cend(); ++it) {
        if (it.value().designPoint == name) {
            toRemove << it.key();
        }
    }
    for (const QString& id : toRemove) {
        removeRecord(id);
    }
}

void StakeoutManager::onPointsCleared()
{
    if (m_records.isEmpty()) return;
    const QStringList ids = m_records.keys();
    m_records.clear();
    for (const QString& id : ids) {
        emit recordRemoved(id);
    }
}

void StakeoutManager::updatePointStakeoutMetadata(const StakeoutRecord& record)
{
    if (!m_pointManager || record.designPoint.isEmpty()) return;

    m_pointManager->setStakeoutStatus(record.designPoint, record.status);
    if (record.hasMeasurement()) {
        m_pointManager->setStakeoutResiduals(record.designPoint,
                                             record.deltaE(),
                                             record.deltaN(),
                                             record.deltaZ(),
                                             record.remarks);
    }
}
