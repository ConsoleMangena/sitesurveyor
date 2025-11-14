#ifndef STAKEOUTDIALOG_H
#define STAKEOUTDIALOG_H

#include <QDialog>
#include "stakeoutrecord.h"

class QTableWidget;
class QPushButton;
class StakeoutManager;
class PointManager;

class StakeoutDialog : public QDialog
{
    Q_OBJECT
public:
    StakeoutDialog(StakeoutManager* manager, PointManager* pointManager, QWidget* parent = nullptr);

public slots:
    void refreshRecords();

private slots:
    void addDesignRecord();
    void recordMeasurement();
    void markComplete();
    void exportReport();
    void copyResidualSummary();
    void onRecordAdded(const StakeoutRecord& record);
    void onRecordUpdated(const StakeoutRecord& record);
    void onRecordRemoved(const QString& id);

private:
    QString selectedRecordId() const;
    void populateRow(int row, const StakeoutRecord& record);

    StakeoutManager* m_stakeoutManager{nullptr};
    PointManager* m_pointManager{nullptr};
    QTableWidget* m_table{nullptr};
    QPushButton* m_measureButton{nullptr};
    QPushButton* m_completeButton{nullptr};
};

#endif // STAKEOUTDIALOG_H
