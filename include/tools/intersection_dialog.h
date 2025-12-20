#ifndef INTERSECTION_DIALOG_H
#define INTERSECTION_DIALOG_H

#include <QDialog>
#include <QPointF>

class CanvasWidget;
class QDoubleSpinBox;
class QLabel;
class QRadioButton;

class IntersectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit IntersectionDialog(CanvasWidget* canvas, QWidget* parent = nullptr);

private slots:
    void calculate();
    void pickPointA();
    void pickPointB();
    void updateInputMode();

private:
    void setupUi();
    
    CanvasWidget* m_canvas;
    
    // Point A inputs
    QDoubleSpinBox* m_eastA;
    QDoubleSpinBox* m_northA;
    
    // Point B inputs
    QDoubleSpinBox* m_eastB;
    QDoubleSpinBox* m_northB;
    
    // Observations
    QRadioButton* m_modeDistDist;
    QRadioButton* m_modeBearingBearing;
    
    QDoubleSpinBox* m_obsA; // Dist or Bearing from A
    QDoubleSpinBox* m_obsB; // Dist or Bearing from B
    
    QLabel* m_resultLabel;
};

#endif // INTERSECTION_DIALOG_H
