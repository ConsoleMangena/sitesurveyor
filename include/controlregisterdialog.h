#ifndef CONTROLREGISTERDIALOG_H
#define CONTROLREGISTERDIALOG_H

#include <QDialog>
#include <QVector>
#include "point.h"

class QTableWidget;
class QPushButton;
class PointManager;

class ControlRegisterDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ControlRegisterDialog(PointManager* pointManager, QWidget* parent = nullptr);

public slots:
    void refresh();

private slots:
    void markAsControl();
    void clearControl();
    void editMetadata();

private:
    QList<int> selectedRows() const;

    PointManager* m_pointManager{nullptr};
    QTableWidget* m_table{nullptr};
    QPushButton* m_markButton{nullptr};
    QPushButton* m_clearButton{nullptr};
    QPushButton* m_editButton{nullptr};
};

#endif // CONTROLREGISTERDIALOG_H
