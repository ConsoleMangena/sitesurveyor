#ifndef MASSPOLARDIALOG_H
#define MASSPOLARDIALOG_H

#include <QDialog>
#include <QString>

class QComboBox;
class QTableWidget;
class QTextEdit;
class QPushButton;
class QCheckBox;
class PointManager;
class CanvasWidget;

class MassPolarDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MassPolarDialog(PointManager* pm, CanvasWidget* canvas, QWidget* parent = nullptr);

public slots:
    void reload();

private slots:
    void addRow();
    void removeSelected();
    void importCSV();
    void exportCSV();
    void addCoordinates();

private:
    double parseAngleDMS(const QString& text, bool* ok) const;

    PointManager* m_pm{nullptr};
    CanvasWidget* m_canvas{nullptr};

    QComboBox* m_fromBox{nullptr};
    QCheckBox* m_drawLines{nullptr};
    QTableWidget* m_table{nullptr};
    QTextEdit* m_output{nullptr};
};

#endif // MASSPOLARDIALOG_H
