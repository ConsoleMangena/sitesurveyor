#ifndef NETWORK_ADJUSTMENT_DIALOG_H
#define NETWORK_ADJUSTMENT_DIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QGroupBox>
#include "gama/gamaexporter.h"
#include "gama/gamarunner.h"

class CanvasWidget;

/**
 * @brief NetworkAdjustmentDialog - Full-featured GNU GAMA network adjustment interface
 * 
 * Provides:
 * - Point definition (fixed/adjusted)
 * - Distance observation input
 * - Direction/angle observation input
 * - Adjustment parameters configuration
 * - Results display with residuals
 */
class NetworkAdjustmentDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NetworkAdjustmentDialog(CanvasWidget* canvas, QWidget* parent = nullptr);
    ~NetworkAdjustmentDialog() = default;

private slots:
    void loadPointsFromCanvas();
    void addPointRow();
    void removePointRow();
    void addDistanceRow();
    void removeDistanceRow();
    void addDirectionRow();
    void removeDirectionRow();
    void addAngleRow();
    void removeAngleRow();
    void runAdjustment();
    void applyResults();
    void exportToXml();
    void importFromXml();

private:
    void setupUi();
    void setupPointsTab();
    void setupDistancesTab();
    void setupDirectionsTab();
    void setupAnglesTab();
    void setupParametersTab();
    void setupResultsTab();
    
    GamaNetwork buildNetwork() const;
    void displayResults(const GamaAdjustmentResults& results);
    QString formatDMS(double degrees) const;
    double parseDMS(const QString& dms) const;

    CanvasWidget* m_canvas;
    GamaAdjustmentResults m_lastResults;
    
    // UI elements
    QTabWidget* m_tabWidget;
    
    // Points tab
    QTableWidget* m_pointsTable;
    QPushButton* m_loadPointsBtn;
    QPushButton* m_addPointBtn;
    QPushButton* m_removePointBtn;
    
    // Distances tab
    QTableWidget* m_distancesTable;
    QPushButton* m_addDistanceBtn;
    QPushButton* m_removeDistanceBtn;
    
    // Directions tab
    QTableWidget* m_directionsTable;
    QPushButton* m_addDirectionBtn;
    QPushButton* m_removeDirectionBtn;
    
    // Angles tab
    QTableWidget* m_anglesTable;
    QPushButton* m_addAngleBtn;
    QPushButton* m_removeAngleBtn;
    
    // Parameters tab
    QDoubleSpinBox* m_sigmaAprSpin;
    QDoubleSpinBox* m_confidenceSpin;
    QComboBox* m_axesCombo;
    QComboBox* m_anglesCombo;
    
    // Results tab
    QTextEdit* m_resultsText;
    QTableWidget* m_adjustedPointsTable;
    QLabel* m_statusLabel;
    
    // Buttons
    QPushButton* m_runBtn;
    QPushButton* m_applyBtn;
    QPushButton* m_closeBtn;
};

#endif // NETWORK_ADJUSTMENT_DIALOG_H
