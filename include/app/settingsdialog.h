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
#include <QSlider>
#include <QLabel>

class CanvasWidget;
class AuthManager;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(CanvasWidget* canvas, AuthManager* auth, QWidget* parent = nullptr);
    ~SettingsDialog() = default;

private slots:
    void applySettings();
    void resetToDefaults();

private:
    void setupUI();
    void loadCurrentSettings();
    void applyChanges();

    CanvasWidget* m_canvas;
    AuthManager* m_auth;

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
    QCheckBox* m_southAzimuth;
    QCheckBox* m_swapXY;

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

    // Units settings
    QComboBox* m_lengthUnitCombo;
    QSpinBox* m_lengthPrecision;
    QComboBox* m_angleUnitCombo;
    QSpinBox* m_anglePrecision;
    QComboBox* m_directionBaseCombo;
    QCheckBox* m_clockwiseCheck;

    // Drafting settings
    QSlider* m_snapMarkerSizeSlider;
    QSlider* m_apertureSizeSlider;
    QCheckBox* m_autoSnapCheck;

    // Selection settings
    QSlider* m_pickboxSizeSlider;
    QSlider* m_gripSizeSlider;

    // Advanced Display settings
    QSlider* m_crosshairSizeSlider;

    // Account tab sign-in form (shown when not logged in)
    QLineEdit* m_settingsEmailEdit = nullptr;
    QLineEdit* m_settingsPasswordEdit = nullptr;
    QLabel* m_settingsStatusLabel = nullptr;
};

#endif // SETTINGSDIALOG_H
