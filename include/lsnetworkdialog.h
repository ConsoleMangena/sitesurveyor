#ifndef LSNETWORKDIALOG_H
#define LSNETWORKDIALOG_H

#include <QDialog>
#include <QVector>
#include <QPointF>

class QTableWidget;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QTextEdit;
class QPushButton;
class PointManager;
class CanvasWidget;

class LSNetworkDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LSNetworkDialog(PointManager* pm, CanvasWidget* canvas, QWidget* parent=nullptr);
    void reload();

private slots:
    void addRow();
    void removeSelected();
    void compute();

private:
    enum ObsType { Distance, Bearing };
    struct Obs { int ctrlIndex; ObsType type; double value; double sigma; };
    bool solveLS(const QVector<QPointF>& ctrls, const QVector<Obs>& obs,
                 QPointF& X, double& s0sq, double& c11, double& c12, double& c22,
                 QVector<double>* residualsOut = nullptr, QStringList* typesOut = nullptr) const;

    PointManager* m_pm{nullptr};
    CanvasWidget* m_canvas{nullptr};

    QTableWidget* m_table{nullptr};
    QLineEdit* m_outName{nullptr};
    QTextEdit* m_report{nullptr};
};

#endif // LSNETWORKDIALOG_H
