#ifndef POLAR_DIALOG_H
#define POLAR_DIALOG_H

#include <QDialog>

class QDoubleSpinBox;
class QLabel;
class QPushButton;
class CanvasWidget;

/**
 * @brief PolarDialog - Compute coordinates from starting point, bearing, and distance
 * 
 * Given: Starting point (E, N), Bearing, Distance
 * Computes: Ending point (E, N)
 */
class PolarDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit PolarDialog(CanvasWidget* canvas = nullptr, QWidget* parent = nullptr);
    
private slots:
    void calculate();
    void pickStartPoint();
    void addToCanvas();
    
private:
    void setupUi();
    
    CanvasWidget* m_canvas;
    
    // Start point inputs
    QDoubleSpinBox* m_startE;
    QDoubleSpinBox* m_startN;
    
    // Bearing inputs (degrees, minutes, seconds)
    QDoubleSpinBox* m_bearingDeg;
    QDoubleSpinBox* m_bearingMin;
    QDoubleSpinBox* m_bearingSec;
    
    // Distance input
    QDoubleSpinBox* m_distance;
    
    // Result
    QLabel* m_resultLabel;
    double m_resultE{0.0};
    double m_resultN{0.0};
};

#endif // POLAR_DIALOG_H
