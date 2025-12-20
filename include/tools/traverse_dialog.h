#ifndef TRAVERSE_DIALOG_H
#define TRAVERSE_DIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>

class CanvasWidget;

/**
 * @brief TraverseObservation - Single observation leg in a traverse
 */
struct TraverseObservation {
    QString stationId;
    double angle{0.0};          // Observed angle (degrees)
    double distance{0.0};       // Horizontal distance
    double bearing{0.0};        // Computed bearing
    double easting{0.0};        // Computed easting
    double northing{0.0};       // Computed northing
    double adjEasting{0.0};     // Adjusted easting
    double adjNorthing{0.0};    // Adjusted northing
};

/**
 * @brief TraverseDialog - Full traverse calculation tool
 * 
 * Supports:
 * - Open and closed (loop) traverses
 * - Interior angle observations
 * - Bearing/distance computations
 * - Bowditch (compass) adjustment
 * - Transit adjustment
 * - Closure error analysis
 */
class TraverseDialog : public QDialog
{
    Q_OBJECT

public:
    enum AdjustmentMethod {
        NoAdjustment,
        Bowditch,   // Compass rule - proportional to distance
        Transit     // Proportional to latitude/departure
    };
    
    enum TraverseType {
        OpenTraverse,
        ClosedLoop,         // Closes on starting point
        ClosedConnection    // Closes on different known point
    };

    explicit TraverseDialog(CanvasWidget* canvas, QWidget* parent = nullptr);
    ~TraverseDialog() = default;

private slots:
    void addStation();
    void removeStation();
    void calculate();
    void adjust();
    void sendToCanvas();
    void clearAll();
    void onCellChanged(int row, int col);
    void importFromPegs();
    void updateClosureDisplay();
    void exportCSV();
    void generateReport();

private:
    void setupUI();
    void setupTable();
    
    // Calculation methods
    void calculateForwardBearings();
    void calculateCoordinates();
    void calculateClosureError();
    void applyBowditchAdjustment();
    void applyTransitAdjustment();
    
    // Helpers
    double normalizeAngle(double angle) const;
    double degreesToRadians(double deg) const;
    double radiansToDegrees(double rad) const;
    QString formatBearing(double degrees) const;
    QString formatDMS(double degrees) const;
    
    // Table columns
    enum TableColumn {
        COL_STATION = 0,
        COL_ANGLE,
        COL_DISTANCE,
        COL_BEARING,
        COL_EASTING,
        COL_NORTHING,
        COL_ADJ_EASTING,
        COL_ADJ_NORTHING,
        COL_COUNT
    };
    
    CanvasWidget* m_canvas{nullptr};
    
    // UI Components
    QTableWidget* m_table{nullptr};
    
    // Start point
    QLineEdit* m_startStationEdit{nullptr};
    QDoubleSpinBox* m_startEasting{nullptr};
    QDoubleSpinBox* m_startNorthing{nullptr};
    QDoubleSpinBox* m_startBearing{nullptr};
    
    // End point (for closed connection)
    QDoubleSpinBox* m_endEasting{nullptr};
    QDoubleSpinBox* m_endNorthing{nullptr};
    
    // Options
    QComboBox* m_traverseTypeCombo{nullptr};
    QComboBox* m_adjustmentCombo{nullptr};
    QCheckBox* m_southAzimuthCheck{nullptr};
    QCheckBox* m_interiorAnglesCheck{nullptr};
    
    // Results
    QLabel* m_closureLabel{nullptr};
    QLabel* m_precisionLabel{nullptr};
    QLabel* m_angularErrorLabel{nullptr};
    
    // Calculation state
    QVector<TraverseObservation> m_observations;
    double m_closureEasting{0.0};
    double m_closureNorthing{0.0};
    double m_closureDistance{0.0};
    double m_totalDistance{0.0};
    double m_angularMisclose{0.0};
    bool m_calculated{false};
    bool m_updating{false};
};

#endif // TRAVERSE_DIALOG_H
