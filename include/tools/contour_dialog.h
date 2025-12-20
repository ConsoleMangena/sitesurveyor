#ifndef CONTOURDIALOG_H
#define CONTOURDIALOG_H

#include <QDialog>
#include <QVector>
#include <QPointF>
#include "canvas/canvaswidget.h"

class QDoubleSpinBox;
class QSpinBox;
class QLabel;
class QPushButton;
class QCheckBox;

/**
 * @brief Contour Generator Dialog
 * 
 * Generates contour lines from TIN surface using linear interpolation
 * along triangle edges.
 */
class ContourDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ContourDialog(CanvasWidget* canvas, QWidget *parent = nullptr);

private slots:
    void generate();
    void clearContours();
    void applyToCanvas();

private:
    void setupUi();
    
    CanvasWidget* m_canvas;
    
    // UI
    QDoubleSpinBox* m_intervalSpin;
    QSpinBox* m_majorFactorSpin;
    QDoubleSpinBox* m_minElevSpin;
    QDoubleSpinBox* m_maxElevSpin;
    QCheckBox* m_autoRangeCheck;
    QLabel* m_statusLabel;
    QPushButton* m_generateBtn;
    QPushButton* m_applyBtn;
    
    // Generated contours (using CanvasWidget's ContourLine type)
    QVector<CanvasWidget::ContourLine> m_contours;
};

#endif // CONTOURDIALOG_H
