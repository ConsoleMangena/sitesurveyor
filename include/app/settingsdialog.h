#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QColorDialog>
#include <QPushButton>
#include <QLineEdit>

class CanvasWidget;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(CanvasWidget* canvas, QWidget* parent = nullptr);
    ~SettingsDialog() = default;

private slots:
    void applySettings();
    void resetToDefaults();

private:
    void setupUI();
    void loadCurrentSettings();
    
    CanvasWidget* m_canvas;
    
    // Snapping settings
    QCheckBox* m_snapEnabled;
    QCheckBox* m_snapEndpoint;
    QCheckBox* m_snapMidpoint;
    QCheckBox* m_snapEdge;
    QCheckBox* m_snapIntersection;
    QDoubleSpinBox* m_snapTolerance;
    
    // Display settings
    QCheckBox* m_showGrid;
    QSpinBox* m_gridSize;
    QDoubleSpinBox* m_pegMarkerSize;
    
    // Stakeout settings
    QSpinBox* m_bearingPrecision;
    QSpinBox* m_distancePrecision;
    
    // Colors
    QPushButton* m_stationColorBtn;
    QPushButton* m_backsightColorBtn;
    QPushButton* m_pegColorBtn;
    QColor m_stationColor;
    QColor m_backsightColor;
    QColor m_pegColor;
    
    // Coordinate System settings
    QComboBox* m_crsCombo;
    QComboBox* m_targetCrsCombo;
    QDoubleSpinBox* m_scaleFactor;
    QLineEdit* m_customEpsg;
    
    // Appearance settings
    QComboBox* m_themeCombo;
    
    // GAMA settings
    QCheckBox* m_gamaEnabled;
    QLineEdit* m_gamaPath;
};

#endif // SETTINGSDIALOG_H
