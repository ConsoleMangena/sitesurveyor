#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointF>

class QLabel;
class QTableWidget;
class QTextEdit;
class QLineEdit;
class QStatusBar;
class CanvasWidget;
class PointManager;
class CommandProcessor;
class SettingsDialog;
class JoinPolarDialog;
class PolarInputDialog;
class LayerManager;
class LayerPanel;
class PropertiesPanel;
class QAction;
class QToolButton;
class QComboBox;
class QUndoStack;
class QDockWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updateCoordinates(const QPointF& worldPos);
    void executeCommand();
    void showAddPointDialog();
    void clearAll();
    void newProject();
    void updatePointsTable();
    void updateStatus();
    void handleCanvasClick(const QPointF& worldPos);
    void appendToCommandOutput(const QString& text);
    void showAbout();
    void showSettings();
    void showJoinPolar();
    void showPolarInput();
    void onZoomChanged(double zoom);
    void onPointsTableSelectionChanged();
    void onLayerComboChanged(int index);
    void refreshLayerCombo();
    void resetLayout();
    void toggleDarkMode(bool on);
    void onDrawingDistanceChanged(double meters);

private:
    void setupUI();
    void setupPointsDock();
    void setupCommandDock();
    void setupLayersDock();
    void setupPropertiesDock();
    void setupMenus();
    void setupToolbar();
    void setupConnections();
    void updateLayerStatusText();
    
    // UI components
    CanvasWidget* m_canvas;
    QTableWidget* m_pointsTable;
    QTextEdit* m_commandOutput;
    QLineEdit* m_commandInput;
    QLabel* m_coordLabel;
    QLabel* m_pointCountLabel;
    QLabel* m_zoomLabel;
    QLabel* m_layerStatusLabel;
    QLabel* m_measureLabel;
    QStatusBar* m_statusBar;
    QDockWidget* m_pointsDock;
    QDockWidget* m_commandDock;
    QDockWidget* m_layersDock;
    QDockWidget* m_propertiesDock;
    
    // Core components
    PointManager* m_pointManager;
    CommandProcessor* m_commandProcessor;
    SettingsDialog* m_settingsDialog;
    JoinPolarDialog* m_joinDialog;
    PolarInputDialog* m_polarDialog;
    LayerManager* m_layerManager;
    LayerPanel* m_layerPanel;
    PropertiesPanel* m_propertiesPanel;
    QComboBox* m_layerCombo;
    QUndoStack* m_undoStack;
    QAction* m_preferencesAction;
    QByteArray m_defaultLayoutState;
    bool m_darkMode{false};
    // Navigation/interaction actions
    QAction* m_selectToolAction;
    QAction* m_panToolAction;
    QAction* m_zoomWindowToolAction;
    QAction* m_crosshairToggleAction;
    // Status bar toggles
    QAction* m_orthoAction;
    QAction* m_snapAction;
    QToolButton* m_orthoButton;
    QToolButton* m_snapButton;
    QAction* m_osnapAction;
    QToolButton* m_osnapButton;
    QAction* m_undoAction;
    QAction* m_redoAction;
    QAction* m_resetLayoutAction;
    QAction* m_darkModeAction;
};

#endif // MAINWINDOW_H
