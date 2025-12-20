#ifndef VOLUMEDIALOG_H
#define VOLUMEDIALOG_H

#include <QDialog>
#include <QVector>
#include <QPointF>

class CanvasWidget;
class QLabel;
class QDoubleSpinBox;
class QComboBox;
class QPushButton;
class QTextEdit;
class QTableWidget;

/**
 * @brief Volume calculation dialog with peg selection and PDF reporting
 */
class VolumeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VolumeDialog(CanvasWidget* canvas, QWidget *parent = nullptr);

private slots:
    void calculate();
    void onBoundaryChanged(int index);
    void showTIN();
    void hideTIN();
    void view3D();
    void selectAllPegs();
    void deselectAllPegs();
    void exportReport();
    void updateSelectedCount();

private:
    void setupUi();
    void populateBoundaryList();
    void populatePegTable();
    void applyTheme();
    QVector<int> getSelectedPegIndices();
    
    CanvasWidget* m_canvas;
    
    // UI Elements
    QTableWidget* m_pegTable{nullptr};
    QComboBox* m_boundaryCombo{nullptr};
    QDoubleSpinBox* m_designLevelSpin{nullptr};
    QLabel* m_selectedCountLabel{nullptr};
    QTextEdit* m_resultText{nullptr};
    QPushButton* m_calculateBtn{nullptr};
    QPushButton* m_showTinBtn{nullptr};
    QPushButton* m_hideTinBtn{nullptr};
    QPushButton* m_view3DBtn{nullptr};
    QPushButton* m_exportBtn{nullptr};
    
    // Last calculation results for export
    double m_lastCutVol{0};
    double m_lastFillVol{0};
    double m_lastSurfaceArea{0};
    int m_lastTriangleCount{0};
};

#endif // VOLUMEDIALOG_H
