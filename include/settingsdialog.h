#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QPushButton;
class QComboBox;
class QLineEdit;
class CanvasWidget;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(CanvasWidget* canvas, QWidget *parent = nullptr);
    void reload();

private slots:
    void applyChanges();

signals:
    void settingsApplied();

private:
    void loadFromCanvas();

    CanvasWidget* m_canvas;
    QCheckBox* m_showGridCheck;
    QCheckBox* m_showLabelsCheck;
    QCheckBox* m_gaussModeCheck;
    QCheckBox* m_use3DCheck;
    QDoubleSpinBox* m_gridSizeSpin;
    // Engineering preferences
    QComboBox* m_unitsCombo;   // Metric/Imperial
    QComboBox* m_angleCombo;   // DMS/Decimal
    QLineEdit* m_crsEdit;      // EPSG code or proj string
    QPushButton* m_applyButton;
    QPushButton* m_closeButton;
};

#endif // SETTINGSDIALOG_H
