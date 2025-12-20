#ifndef LEVELLINGDIALOG_H
#define LEVELLINGDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVector>

class CanvasWidget;

struct LevelReading {
    QString pointId;
    double bs{0.0};     // Backsight
    double is{0.0};     // Intermediate Sight
    double fs{0.0};     // Foresight
    double dist{0.0};   // Distance (for adjustment)
    double rise{0.0};   // Rise (calculated)
    double fall{0.0};   // Fall (calculated)
    double hpc{0.0};    // Height of Plane of Collimation (calculated)
    double rl{0.0};     // Reduced Level (calculated)
    double adjRl{0.0};  // Adjusted RL (after loop adjustment)
    QString remarks;
    
    bool hasBs() const { return bs > 0.0001; }
    bool hasIs() const { return is > 0.0001; }
    bool hasFs() const { return fs > 0.0001; }
};

class LevellingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LevellingDialog(CanvasWidget* canvas, QWidget* parent = nullptr);
    ~LevellingDialog() = default;

private slots:
    void addRow();
    void deleteRow();
    void onCellChanged(int row, int column);
    void recalculateAll();
    void adjustLoop();
    void sendToMap();
    void clearTable();
    void exportCSV();
    void methodChanged(int index);

private:
    void setupUI();
    void updateCheckBar();
    void calculateRiseFall();
    void calculateHPC();
    double getTableValue(int row, int col) const;
    void setTableValue(int row, int col, double value);
    void setTableReadOnly(int row, int col, double value);
    
    // UI Elements
    QDoubleSpinBox* m_startRL;
    QComboBox* m_methodCombo;
    QTableWidget* m_table;
    QLabel* m_checkLabel;
    QLabel* m_statusLabel;
    QPushButton* m_adjustBtn;
    QPushButton* m_sendBtn;
    
    // Data
    CanvasWidget* m_canvas;
    bool m_recalculating{false};
    
    // Column indices
    enum Column {
        COL_POINT = 0,
        COL_BS,
        COL_IS,
        COL_FS,
        COL_DIST,
        COL_RISE,
        COL_FALL,
        COL_HPC,
        COL_RL,
        COL_ADJ_RL,
        COL_REMARKS,
        COL_COUNT
    };
};

#endif // LEVELLINGDIALOG_H
