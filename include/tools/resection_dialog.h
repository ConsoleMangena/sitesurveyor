#ifndef RESECTION_DIALOG_H
#define RESECTION_DIALOG_H

#include <QDialog>
#include <QPointF>

class CanvasWidget;
class QTableWidget;
class QDoubleSpinBox;
class QLabel;

class ResectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ResectionDialog(CanvasWidget* canvas, QWidget* parent = nullptr);

private slots:
    void calculate();
    void addPoint();
    void removePoint();
    void pickPointFromCanvas();

private:
    void setupUi();
    
    CanvasWidget* m_canvas;
    QTableWidget* m_pointsTable;
    QLabel* m_resultLabel;
    
    // Result coordinates
    double m_resultX{0.0};
    double m_resultY{0.0};
};

#endif // RESECTION_DIALOG_H
