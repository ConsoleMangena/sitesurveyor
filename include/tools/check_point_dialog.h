#ifndef CHECK_POINT_DIALOG_H
#define CHECK_POINT_DIALOG_H

#include <QDialog>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>

#include "canvas/canvaswidget.h"

class CheckPointDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CheckPointDialog(const CanvasStation& station, 
                             const QPointF& checkPos, 
                             const QString& checkName, 
                             QWidget *parent = nullptr);

private slots:
    void calculateResiduals();

private:
    void setupUi();
    
    // Inputs
    CanvasStation m_station;
    QPointF m_checkPos;
    QString m_checkName;
    
    // Theoretical Values (calculated from coords)
    double m_theoBearing;
    double m_theoDist;
    double m_bsBearing; // Bearing from Stn to BS
    
    // UI Elements
    QLabel* m_targetInfoLabel;
    
    QDoubleSpinBox* m_obsBearing;
    QDoubleSpinBox* m_obsDist;
    
    // Results
    QLineEdit* m_resdAngle;
    QLineEdit* m_resdDist;
    QLineEdit* m_resdEast;
    QLineEdit* m_resdNorth;
};

#endif // CHECK_POINT_DIALOG_H
