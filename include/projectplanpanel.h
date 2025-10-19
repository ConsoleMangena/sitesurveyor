#ifndef PROJECTPLANPANEL_H
#define PROJECTPLANPANEL_H

#include <QWidget>
#include <QVector>
#include <QString>

class QTableWidget;
class QPushButton;
class QComboBox;

class ProjectPlanPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ProjectPlanPanel(QWidget* parent = nullptr);

    void load();
    void save() const;

private slots:
    void addTask();
    void removeSelectedTasks();
    void moveTaskUp();
    void moveTaskDown();
    void onCellChanged(int row, int column);

private:
    struct Task {
        QString title;
        QString status;   // pending, in_progress, blocked, done
        QString priority; // high, medium, low
        QString due;      // ISO date string or free text
    };

    static QString statusDisplay(const QString& s);
    static QString statusCode(const QString& display);
    static QString priorityDisplay(const QString& p);
    static QString priorityCode(const QString& display);

    void rebuildCombosForRow(int row);
    QVector<Task> collectTasks() const;
    void setTasks(const QVector<Task>& tasks);

    QTableWidget* m_table{nullptr};
    QPushButton* m_addBtn{nullptr};
    QPushButton* m_removeBtn{nullptr};
    QPushButton* m_upBtn{nullptr};
    QPushButton* m_downBtn{nullptr};
    QPushButton* m_saveBtn{nullptr};
};

#endif // PROJECTPLANPANEL_H
