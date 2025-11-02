#ifndef INTERSECTRESECTIONDIALOG_H
#define INTERSECTRESECTIONDIALOG_H

#include <QDialog>
#include <QPointF>
#include <QVector>

class QComboBox;
class QDoubleSpinBox;
class QTableWidget;
class QTextEdit;
class QPushButton;
class QTabWidget;
class QCheckBox;
class QLineEdit;
class PointManager;
class CanvasWidget;

class IntersectResectionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit IntersectResectionDialog(PointManager* pm, CanvasWidget* canvas, QWidget* parent = nullptr);
    void reload();

private slots:
    void computeIntersection();
    void computeResection();

private:
    bool circleCircle(const QPointF& A, double rA, const QPointF& B, double rB, bool leftOfAB, QPointF& out) const;
    bool rayRay(const QPointF& A, double azAdeg, const QPointF& B, double azBdeg, QPointF& out) const;
    bool rayCircle(const QPointF& A, double azAdeg, const QPointF& B, double rB, bool leftOfAB, QPointF& out) const;
    enum ObsType { ObsBearing, ObsDistance, ObsBearDist, ObsDirection, ObsAngle };
    struct ResObs { int ctrlIndex; int ctrlIndex2; ObsType type; double bearingDeg; double distance; double sb; double sd; };
    bool resectionMixedLS(const QVector<QPointF>& ctrls, const QVector<ResObs>& obs,
                          QPointF& Pxy,
                          double& N11, double& N12, double& N22,
                          double& thetaDegOut,
                          QVector<double>* residualsOut = nullptr,
                          QStringList* labelsOut = nullptr) const;

    PointManager* m_pm{nullptr};
    CanvasWidget* m_canvas{nullptr};

    QTabWidget* m_tabs{nullptr};

    QComboBox* m_ptA{nullptr};
    QComboBox* m_ptB{nullptr};
    QComboBox* m_method{nullptr};
    QDoubleSpinBox* m_distA{nullptr};
    QDoubleSpinBox* m_distB{nullptr};
    QDoubleSpinBox* m_bearA{nullptr};
    QDoubleSpinBox* m_bearB{nullptr};
    QComboBox* m_side{nullptr};
    QLineEdit* m_outName{nullptr};
    QCheckBox* m_addPointCheck{nullptr};
    QCheckBox* m_drawLinesCheck{nullptr};
    QTextEdit* m_report{nullptr};

    QTableWidget* m_resTable{nullptr};
    QLineEdit* m_resOutName{nullptr};
    QTextEdit* m_resReport{nullptr};
};

#endif
