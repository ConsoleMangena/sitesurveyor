#ifndef TRAVERSEDIALOG_H
#define TRAVERSEDIALOG_H

#include <QDialog>
#include <QVector>
#include <QPointF>
#include <QString>

class QComboBox;
class QTableWidget;
class QTextEdit;
class QPushButton;
class QCheckBox;
class QDoubleSpinBox;
class PointManager;
class CanvasWidget;
struct Point;

class TraverseDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TraverseDialog(PointManager* pm, CanvasWidget* canvas, QWidget* parent = nullptr);
    void reload();

private slots:
    void addRow();
    void removeSelectedRows();
    void computeTraverse();

private:
    struct Leg { double azimuthDeg{0.0}; double distance{0.0}; QString name; };
    bool getStartPoint(Point& out) const;
    bool getClosePoint(Point& out) const;
    QVector<Leg> collectLegs() const;
    void appendReportLine(const QString& s);
    bool parseAngleInput(const QString& text, double& outDegrees) const; // supports decimal or DMS (deg° min' sec")
    bool parseDistanceInput(const QString& text, double& outDistance) const; // supports plain number or with unit suffix

    PointManager* m_pm{nullptr};
    CanvasWidget* m_canvas{nullptr};

    QComboBox* m_startCombo{nullptr};
    QComboBox* m_closeCombo{nullptr};
    QComboBox* m_adjustCombo{nullptr};
    QDoubleSpinBox* m_scalePpm{nullptr};
    QCheckBox* m_seaLevelCheck{nullptr};
    QDoubleSpinBox* m_meanElev{nullptr};
    QTableWidget* m_legsTable{nullptr};
    QTextEdit* m_report{nullptr};
    QPushButton* m_addRowBtn{nullptr};
    QPushButton* m_removeRowBtn{nullptr};
    QPushButton* m_computeBtn{nullptr};
    QCheckBox* m_addPointsCheck{nullptr};
    QCheckBox* m_drawLinesCheck{nullptr};
    QCheckBox* m_applyAdjustedCheck{nullptr};
};

#endif // TRAVERSEDIALOG_H
