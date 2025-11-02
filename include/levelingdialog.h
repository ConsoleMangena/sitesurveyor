#ifndef LEVELINGDIALOG_H
#define LEVELINGDIALOG_H

#include <QDialog>
#include <QVector>
#include <QPointF>

class QComboBox;
class QTableWidget;
class QTextEdit;
class QPushButton;
class QCheckBox;
class QLineEdit;
class QDoubleSpinBox;
class QLabel;
class PointManager;
class CanvasWidget;

class LevelingDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LevelingDialog(PointManager* pm, CanvasWidget* canvas, QWidget* parent = nullptr);
    void reload();

private slots:
    void addRow();
    void removeSelected();
    void computeAdjustment();
    void importCSV();
    void exportCSV();
    void exportReportCSV();
    void exportProfilePDF();
    void addProfileToDrawing();

private:
    struct Leg { QString toName; double dH{0.0}; double dist{0.0}; int loopId{1}; QString closeTo; double sigmaPerSqrtKm{0.003}; };
    QVector<Leg> collectLegs() const;
    QVector<Leg> collectLegsFromLog(double startZ,
                                    double& sumBS,
                                    double& sumFS,
                                    double& sumRise,
                                    double& sumFall,
                                    double& totalDist,
                                    QVector<int>* outSetupIds) const;
    void rebuildTableForMode();
    void updateProfile(const QVector<double>& cumDist, const QVector<double>& rl);

    PointManager* m_pm{nullptr};
    CanvasWidget* m_canvas{nullptr};

    QComboBox* m_startCombo{nullptr};
    QComboBox* m_closeCombo{nullptr}; // optional global close (used if per-row close empty)
    QTableWidget* m_table{nullptr};
    QLineEdit* m_outPrefix{nullptr};
    QCheckBox* m_applyZ{nullptr};
    QComboBox* m_weightMethod{nullptr};
    QComboBox* m_inputMode{nullptr};
    QComboBox* m_calcMethod{nullptr};
    QDoubleSpinBox* m_tolMmPerSqrtKm{nullptr};
    QTextEdit* m_report{nullptr};
    QLabel* m_profileLabel{nullptr};

    // Last computation cache for export/drawing
    QVector<double> m_lastCumDist;
    QVector<double> m_lastRL;
    QVector<int> m_lastSetupIds;
    QVector<Leg> m_lastLegs;
    QVector<double> m_lastRawZ;
    QVector<double> m_lastCorr;
    QStringList m_lastToNames;
};

#endif // LEVELINGDIALOG_H
