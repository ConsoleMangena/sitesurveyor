#ifndef TRANSFORMDIALOG_H
#define TRANSFORMDIALOG_H

#include <QDialog>
#include <QVector>
#include <QPointF>
#include <QString>

class QComboBox;
class QTableWidget;
class QTextEdit;
class QPushButton;
class QDoubleSpinBox;
class PointManager;
class CanvasWidget;

class TransformDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TransformDialog(PointManager* pm, CanvasWidget* canvas, QWidget* parent = nullptr);
public slots:
    void reload();

private slots:
    void addRow();
    void removeSelected();
    void computeTransform();
    void applyToPoints();
    void applyToDrawing();

private:
    struct Pair { QString src; QString dst; double weight{1.0}; };
    QVector<Pair> collectPairs() const;
    enum class Mode { Helmert2D, Affine2D };

    // Helmert (2D similarity): a = s cos, b = s sin, tx, ty
    bool solveHelmert(const QVector<QPointF>& src, const QVector<QPointF>& dst,
                      const QVector<double>& w, double& a, double& b, double& tx, double& ty,
                      double& rms, QString& report) const;
    // Affine (1st order): Xi = a1 x + a2 y + a3; Yi = b1 x + b2 y + b3
    bool solveAffine(const QVector<QPointF>& src, const QVector<QPointF>& dst,
                     const QVector<double>& w, double& a1, double& a2, double& a3,
                     double& b1, double& b2, double& b3,
                     double& rms, QString& report) const;

    // Apply transforms
    QPointF applyHelmertTo(const QPointF& p, double a, double b, double tx, double ty) const;
    QPointF applyAffineTo(const QPointF& p, double a1, double a2, double a3, double b1, double b2, double b3) const;

    // Helpers
    void fillPointCombosRow(int row);
    PointManager* m_pm{nullptr};
    CanvasWidget* m_canvas{nullptr};

    QComboBox* m_mode{nullptr};
    QComboBox* m_weighting{nullptr};
    QTableWidget* m_table{nullptr};
    QTextEdit* m_report{nullptr};
    QDoubleSpinBox* m_eps{nullptr};

    // Last solution
    Mode m_lastMode{Mode::Helmert2D};
    bool m_hasSolution{false};
    // Helmert
    double m_h_a{1.0}, m_h_b{0.0}, m_h_tx{0.0}, m_h_ty{0.0};
    // Affine
    double m_af_a1{1.0}, m_af_a2{0.0}, m_af_a3{0.0};
    double m_af_b1{0.0}, m_af_b2{1.0}, m_af_b3{0.0};
};

#endif // TRANSFORMDIALOG_H
