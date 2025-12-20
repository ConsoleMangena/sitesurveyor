#ifndef JOIN_DIALOG_H
#define JOIN_DIALOG_H

#include <QDialog>

class QDoubleSpinBox;
class QLabel;
class QPushButton;
class CanvasWidget;

/**
 * @brief JoinDialog - Compute bearing and distance between two points
 * 
 * Given: Point A (E, N) and Point B (E, N)
 * Computes: Bearing from A to B, Distance A to B
 * Also known as: Inverse calculation
 */
class JoinDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit JoinDialog(CanvasWidget* canvas = nullptr, QWidget* parent = nullptr);
    
private slots:
    void calculate();
    void pickPointA();
    void pickPointB();
    
private:
    void setupUi();
    
    CanvasWidget* m_canvas;
    
    // Point A inputs
    QDoubleSpinBox* m_eastA;
    QDoubleSpinBox* m_northA;
    
    // Point B inputs
    QDoubleSpinBox* m_eastB;
    QDoubleSpinBox* m_northB;
    
    // Result
    QLabel* m_resultLabel;
};

#endif // JOIN_DIALOG_H
