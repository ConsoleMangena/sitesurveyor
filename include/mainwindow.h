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
class LayerManager;
class LayerPanel;
class PropertiesPanel;
class ProjectPlanPanel;
class IntersectResectionDialog;
class LevelingDialog;
class LSNetworkDialog;
class TransformDialog;
class MassPolarDialog;
class QAction;
class QMenu;
class QToolButton;
class QToolBar;
class QComboBox;
class QUndoStack;
class QDockWidget;
class QStackedWidget;
class QTabWidget;
class WelcomeWidget;
class QTimer;
class QTableWidgetItem;

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
    void importDXF();
    void exportCoordinates();
    void exportGeoJSON();
    void updatePointsTable();
    void updateStatus();
    void handleCanvasClick(const QPointF& worldPos);
    void appendToCommandOutput(const QString& text);
    void showAbout();
    void showSettings();
    void showJoinPolar();
    void showPolarInput();
    void showMassPolar();
    void showTraverse();
    void showIntersectResection();
    void showLeveling();
    void showLSNetwork();
    void showTransformations();
    void toggleProjectPlanPanel();
    void calculateDistance();
    void calculateArea();
    void calculateAzimuth();
    void drawRegularPolygon();
    // Real tools
    void toolTrim();
    void toolExtend();
    void toolOffset();
    void toolFilletZero();
    void toolChamfer();
    void onZoomChanged(double zoom);
    void onPointsTableSelectionChanged();
    void onLayerComboChanged(int index);
    void showSelectedProperties();
    void refreshLayerCombo();
    void resetLayout();
    void toggleDarkMode(bool on);
    void onDrawingDistanceChanged(double meters);
    void onDrawingAngleChanged(double degrees);
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
    void deleteSelectedCoordinates();
    void onPointsCellChanged(QTableWidgetItem* item);

private:
    void setupUI();
    void setupPointsDock();
    void setupCommandDock();
    void setupLayersDock();
    void setupProjectPlanDock();
    void setupMenus();
    void setupToolbar();
    void setupConnections();
    void updateLayerStatusText();
    void updateLicenseStateUI();
    void applyUiStyling();
    void applyEngineeringPresetIfNeeded();
    void updatePinnedGroupsUI();
    void updateMeasureLabelText();
    void enableOverflowTearOff(QToolBar* bar);
    void updateMoreDock();
    void updateToolSelectionUI();
    // UI animation helpers
    void fadeInWidget(QWidget* w, int duration = 180);
    void pulseLabel(QWidget* w, int duration = 220);
    void animateRightDockToWidth(int targetWidth);
    void animateRightDockClose();
    void showToast(const QString& msg, int durationMs = 2000);
    // Autosave helpers
    QString autosavePath() const;
    void autosaveNow();
    void setupAutosave();
    void tryRecoverAutosave();
    
    // UI components
    CanvasWidget* m_canvas{nullptr};
    QTableWidget* m_pointsTable{nullptr};
    bool m_updatingPointsTable{false};
    QTextEdit* m_commandOutput{nullptr};
    QLineEdit* m_commandInput{nullptr};
    QLabel* m_coordLabel{nullptr};
    QLabel* m_pointCountLabel{nullptr};
    QLabel* m_zoomLabel{nullptr};
    QLabel* m_layerStatusLabel{nullptr};
    QLabel* m_measureLabel{nullptr};
    QLabel* m_selectionLabel{nullptr};
    QStatusBar* m_statusBar{nullptr};
    QDockWidget* m_pointsDock{nullptr};
    QDockWidget* m_commandDock{nullptr};
    QDockWidget* m_layersDock{nullptr};
    QDockWidget* m_propertiesDock{nullptr}; // unused when merged into m_layersDock (tabs)
    QDockWidget* m_projectPlanDock{nullptr};
    QStackedWidget* m_centerStack{nullptr};
    QTabWidget* m_rightTabs{nullptr};
    WelcomeWidget* m_welcomeWidget{nullptr};
    
    // Core components
    PointManager* m_pointManager{nullptr};
    CommandProcessor* m_commandProcessor{nullptr};
    SettingsDialog* m_settingsDialog{nullptr};
    JoinPolarDialog* m_joinDialog{nullptr};
    PolarInputDialog* m_polarDialog{nullptr};
    TraverseDialog* m_traverseDialog{nullptr};
    LayerManager* m_layerManager{nullptr};
    LayerPanel* m_layerPanel{nullptr};
    PropertiesPanel* m_propertiesPanel{nullptr};
    ProjectPlanPanel* m_projectPlanPanel{nullptr};
    QComboBox* m_layerCombo{nullptr};
    QComboBox* m_lineTypeCombo{nullptr};
    QComboBox* m_lineWidthCombo{nullptr};
    QUndoStack* m_undoStack{nullptr};
    QAction* m_preferencesAction{nullptr};
    QAction* m_intersectResectionAction{nullptr};
    QAction* m_levelingAction{nullptr};
    QAction* m_lsNetworkAction{nullptr};
    QByteArray m_defaultLayoutState;
    bool m_darkMode{false};
    // Navigation/interaction actions
    QAction* m_selectToolAction{nullptr};
    QAction* m_panToolAction{nullptr};
    QAction* m_zoomWindowToolAction{nullptr};
    QAction* m_lassoToolAction{nullptr};
    QAction* m_crosshairToggleAction{nullptr};
    // File/menu actions (standard)
    QAction* m_newProjectAction{nullptr};
    QAction* m_openProjectAction{nullptr};
    QAction* m_saveProjectAction{nullptr};
    QAction* m_importPointsAction{nullptr};
    QAction* m_exportPointsAction{nullptr};
    QAction* m_exportGeoJsonAction{nullptr};
    // Status bar toggles
    QAction* m_orthoAction{nullptr};
    QAction* m_snapAction{nullptr};
    QToolButton* m_orthoButton{nullptr};
    QToolButton* m_snapButton{nullptr};
    QAction* m_gridAction{nullptr};
    QToolButton* m_gridButton{nullptr};
    QAction* m_osnapAction{nullptr};
    QToolButton* m_osnapButton{nullptr};
    QAction* m_polarAction{nullptr};
    QToolButton* m_polarButton{nullptr};
    QAction* m_otrackAction{nullptr};
    QToolButton* m_otrackButton{nullptr};
    QAction* m_dynAction{nullptr};
    QToolButton* m_dynButton{nullptr};
    QAction* m_undoAction{nullptr};
    QAction* m_redoAction{nullptr};
    QAction* m_deleteSelectedAction{nullptr};
    QAction* m_darkModeAction{nullptr};
    QAction* m_toggleProjectPlanAction{nullptr};
    QAction* m_showStartPageAction{nullptr};
    // Top bar and Draw group pinning
    QToolBar* m_topBar{nullptr};
    QToolBar* m_bottomBar{nullptr};
    QToolButton* m_drawPinButton{nullptr};
    bool m_drawGroupPinned{false};
    QAction* m_drawAnchorAction{nullptr};
    QAction* m_drawLineToolAction{nullptr};
    QAction* m_drawPolyToolAction{nullptr};
    QAction* m_drawCircleToolAction{nullptr};
    QAction* m_drawArcToolAction{nullptr};
    QAction* m_drawRectToolAction{nullptr};
    QAction* m_drawRegularPolygonAction{nullptr};
    QList<QAction*> m_drawInlineActions;
    // Measure/COGO actions
    QAction* m_calcDistanceAction{nullptr};
    QAction* m_calcAreaAction{nullptr};
    QAction* m_calcAzimuthAction{nullptr};
    // Modify tool actions (menu)
    QAction* m_toolTrimAction{nullptr};
    QAction* m_toolExtendAction{nullptr};
    QAction* m_toolOffsetAction{nullptr};
    QAction* m_toolFilletZeroAction{nullptr};
    QAction* m_toolChamferAction{nullptr};
    // Toolbar tool actions (for selection highlight)
    QAction* m_lengthenToolAction{nullptr};
    QAction* m_trimToolbarAction{nullptr};
    QAction* m_extendToolbarAction{nullptr};
    QAction* m_offsetToolbarAction{nullptr};
    QAction* m_filletToolbarAction{nullptr};
    QAction* m_chamferToolbarAction{nullptr};
    // Menu pointers for license locking
    QMenu* m_fileMenu{nullptr};
    QMenu* m_editMenu{nullptr};
    QMenu* m_viewMenu{nullptr};
    QMenu* m_toolsMenu{nullptr};
    QMenu* m_helpMenu{nullptr};
    QMenu* m_settingsMenu{nullptr};
    QAction* m_aboutAction{nullptr};
    QAction* m_exitAction{nullptr};
    // Autosave
    QTimer* m_autosaveTimer{nullptr};
    // Debounce for OSNAP hint
    QTimer* m_osnapHintTimer{nullptr};
    QString m_pendingOsnapHint;
    // Debounce right panel width persistence (avoid frequent QSettings writes)
    QTimer* m_rightDockResizeDebounce{nullptr};
    int m_pendingRightPanelWidth{0};
    // Panel toggle actions
    QAction* m_toggleLeftPanelAction{nullptr};
    QAction* m_toggleRightPanelAction{nullptr};
    QAction* m_toggleCommandPanelAction{nullptr};
    QToolButton* m_leftPanelButton{nullptr};
    QToolButton* m_rightPanelButton{nullptr};
    QPointF m_lastMouseWorld{0.0, 0.0};
    QDockWidget* m_moreDock{nullptr};
    QToolButton* m_moreButton{nullptr};
    QAction* m_morePinAction{nullptr};
    bool m_morePinned{false};
    bool m_syncingRightDock{false};
    bool m_rightDockClosingByUser{false};
    // Live measure HUD
    double m_liveDistanceMeters{0.0};
    double m_liveAngleDegrees{0.0};
    // Dialogs (created on demand)
    IntersectResectionDialog* m_intersectResectionDlg{nullptr};
    LevelingDialog* m_levelingDlg{nullptr};
    LSNetworkDialog* m_lsNetworkDlg{nullptr};
    TransformDialog* m_transformDlg{nullptr};
    MassPolarDialog* m_massPolarDlg{nullptr};
};

#endif // MAINWINDOW_H
