#ifndef STAKEOUTMANAGER_H
#define STAKEOUTMANAGER_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QString>
#include "stakeoutrecord.h"

class PointManager;

class StakeoutManager : public QObject
{
    Q_OBJECT
public:
    explicit StakeoutManager(PointManager* pointManager, QObject* parent = nullptr);

    QString createDesignRecord(const QString& pointName,
                               double designE,
                               double designN,
                               double designZ,
                               const QString& description = QString(),
                               const QString& method = QString(),
                               const QString& crew = QString(),
                               const QString& remarks = QString());
    bool recordMeasurement(const QString& id,
                           double measuredE,
                           double measuredN,
                           double measuredZ,
                           const QString& instrument,
                           const QString& setupDetails,
                           const QString& status,
                           const QString& remarks = QString());
    bool removeRecord(const QString& id);
    StakeoutRecord record(const QString& id) const;
    QVector<StakeoutRecord> records() const;
    QVector<StakeoutRecord> pendingRecords() const;
    QVector<StakeoutRecord> completedRecords() const;
    bool setRecordStatus(const QString& id, const QString& status);
    bool appendRemarks(const QString& id, const QString& remarks);
    QByteArray exportCsv() const;
    void loadRecords(const QVector<StakeoutRecord>& records);

signals:
    void recordAdded(const StakeoutRecord& record);
    void recordUpdated(const StakeoutRecord& record);
    void recordRemoved(const QString& id);

private slots:
    void onPointRemoved(const QString& name);
    void onPointsCleared();

private:
    QString generateId() const;
    void updatePointStakeoutMetadata(const StakeoutRecord& record);

    PointManager* m_pointManager{nullptr};
    QMap<QString, StakeoutRecord> m_records;
};

#endif // STAKEOUTMANAGER_H
