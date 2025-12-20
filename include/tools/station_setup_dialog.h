#ifndef STATION_SETUP_DIALOG_H
#define STATION_SETUP_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>

class CanvasWidget;

class StationSetupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StationSetupDialog(CanvasWidget* canvas, QWidget *parent = nullptr);

private slots:
    void onCalculateAzimuth();
    void onSetStation();
    void onClearStation();
    void onModeChanged(bool useCoords);

private:
    void setupUi();
    void loadCurrent();

    CanvasWidget* m_canvas;
    
    // Station controls
    QLineEdit* m_stnName;
    QDoubleSpinBox* m_stnE;
    QDoubleSpinBox* m_stnN;
    QDoubleSpinBox* m_stnZ;
    
    // Backsight controls
    QLineEdit* m_bsName;
    QDoubleSpinBox* m_bsE;
    QDoubleSpinBox* m_bsN;
    QDoubleSpinBox* m_bsZ;
    
    // Orientation
    QDoubleSpinBox* m_azimuth;
    QPushButton* m_calcAzimuthBtn;
    
    // Checkbox to use coordinates or manual azimuth
    QCheckBox* m_useBsCoords;
};

#endif // STATION_SETUP_DIALOG_H
