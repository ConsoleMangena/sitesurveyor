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
class TraverseDialog;
class LicenseDialog;
class LayerManager;
class LayerPanel;
class PropertiesPanel;
class ProjectPlanPanel;
class QAction;
class QMenu;
class QToolButton;
class QComboBox;
class QUndoStack;
class QDockWidget;
class QStackedWidget;
class WelcomeWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void updateCoordinates(const QPointF& worldPos);
    void executeCommand();
    void showAddPointDialog();
    void clearAll();
    void newProject();
    void openProject();
    void saveProject();
    void importCoordinates();
    void importCoordinatesFrom(const QString& filePath);
    void exportCoordinates();
    void updatePointsTable();
    void updateStatus();
    void handleCanvasClick(const QPointF& worldPos);
    void appendToCommandOutput(const QString& text);
    void showAbout();
    void showSettings();
    void showLicense();
    void showJoinPolar();
    void showPolarInput();
    void showTraverse();
    void toggleProjectPlanPanel();
    void calculateDistance();
    void calculateArea();
    void calculateAzimuth();
    void onZoomChanged(double zoom);
    void onPointsTableSelectionChanged();
    void onLayerComboChanged(int index);
    void refreshLayerCombo();
    void resetLayout();
    void toggleDarkMode(bool on);
    void onDrawingDistanceChanged(double meters);
    void toggleLeftPanel();
    void toggleRightPanel();
    void toggleCommandPanel();
    void createPanelToggleButtons();
    void updateToggleButtonPositions();
    void updatePanelToggleStates();
    void onLicenseActivated();
    void onRightDockVisibilityChanged(bool visible);
    void setRightPanelsVisible(bool visible);
    void onSelectionChanged(int points, int lines);

private:
    void setupUI();
    void setupPointsDock();
    void setupCommandDock();
    void setupLayersDock();
    void setupPropertiesDock();
    void setupProjectPlanDock();
    void setupMenus();
    void setupToolbar();
    void setupConnections();
    void updateLayerStatusText();
    void updateLicenseStateUI();
    void applyEngineeringPresetIfNeeded();
    void updatePinnedGroupsUI();
    
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
    QLabel* m_selectionLabel;
    QStatusBar* m_statusBar;
    QDockWidget* m_pointsDock;
    QDockWidget* m_commandDock;
    QDockWidget* m_layersDock;
    QDockWidget* m_propertiesDock;
    QDockWidget* m_projectPlanDock;
    QStackedWidget* m_centerStack;
    WelcomeWidget* m_welcomeWidget;
    
    // Core components
    PointManager* m_pointManager;
    CommandProcessor* m_commandProcessor;
    SettingsDialog* m_settingsDialog;
    JoinPolarDialog* m_joinDialog;
    PolarInputDialog* m_polarDialog;
    TraverseDialog* m_traverseDialog;
    LicenseDialog* m_licenseDialog;
    LayerManager* m_layerManager;
    LayerPanel* m_layerPanel;
    PropertiesPanel* m_propertiesPanel;
    ProjectPlanPanel* m_projectPlanPanel;
    QComboBox* m_layerCombo;
    QUndoStack* m_undoStack;
    QAction* m_preferencesAction;
    QByteArray m_defaultLayoutState;
    bool m_darkMode{false};
    // Navigation/interaction actions
    QAction* m_selectToolAction;
    QAction* m_panToolAction;
    QAction* m_zoomWindowToolAction;
    QAction* m_lassoToolAction;
    QAction* m_crosshairToggleAction;
    // File/menu actions (standard)
    QAction* m_newProjectAction{nullptr};
    QAction* m_openProjectAction{nullptr};
    QAction* m_saveProjectAction{nullptr};
    QAction* m_importPointsAction{nullptr};
    QAction* m_exportPointsAction{nullptr};
    // Status bar toggles
    QAction* m_orthoAction;
    QAction* m_snapAction;
    QToolButton* m_orthoButton;
    QToolButton* m_snapButton;
    QAction* m_gridAction;
    QToolButton* m_gridButton;
    QAction* m_osnapAction;
    QToolButton* m_osnapButton;
    QAction* m_polarAction{nullptr};
    QToolButton* m_polarButton{nullptr};
    QAction* m_otrackAction{nullptr};
    QToolButton* m_otrackButton{nullptr};
    QAction* m_dynAction{nullptr};
    QToolButton* m_dynButton{nullptr};
    QAction* m_undoAction;
    QAction* m_redoAction;
    QAction* m_deleteSelectedAction;
    QAction* m_resetLayoutAction;
    QAction* m_darkModeAction;
    QAction* m_toggleProjectPlanAction;
    // Top bar and Draw group pinning
    QToolBar* m_topBar{nullptr};
    QToolButton* m_drawPinButton{nullptr};
    bool m_drawGroupPinned{false};
    QAction* m_drawAnchorAction{nullptr};
    QAction* m_drawLineToolAction{nullptr};
    QAction* m_drawPolyToolAction{nullptr};
    QAction* m_drawCircleToolAction{nullptr};
    QAction* m_drawArcToolAction{nullptr};
    QAction* m_drawRectToolAction{nullptr};
    QList<QAction*> m_drawInlineActions;
    // Measure/COGO actions
    QAction* m_calcDistanceAction{nullptr};
    QAction* m_calcAreaAction{nullptr};
    QAction* m_calcAzimuthAction{nullptr};
    // Menu pointers for license locking
    QMenu* m_fileMenu{nullptr};
    QMenu* m_editMenu{nullptr};
    QMenu* m_viewMenu{nullptr};
    QMenu* m_toolsMenu{nullptr};
    QMenu* m_helpMenu{nullptr};
    QMenu* m_settingsMenu{nullptr};
    QAction* m_licenseAction{nullptr};
    QAction* m_aboutAction{nullptr};
    QAction* m_exitAction{nullptr};
    // Panel toggle actions
    QAction* m_toggleLeftPanelAction;
    QAction* m_toggleRightPanelAction;
    QAction* m_toggleCommandPanelAction;
    QToolButton* m_leftPanelButton;
    QToolButton* m_rightPanelButton;
    bool m_syncingRightDock{false};
    bool m_rightDockClosingByUser{false};
};

#endif // MAINWINDOW_H
