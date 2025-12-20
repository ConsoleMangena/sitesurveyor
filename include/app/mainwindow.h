#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "app/startdialog.h"

class CanvasWidget;
class QLabel;
class QDockWidget;
class QListWidget;
class QTreeWidget;
class QTableWidget;
class QAction;
class QToolBar;
class QMenu;
class AuthManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(AuthManager* auth, QWidget *parent = nullptr);
    ~MainWindow();
    
    // Public accessors for startup integration
    CanvasWidget* canvas() const { return m_canvas; }
    AuthManager* authManager() const { return m_auth; }
    void addToRecentProjects(const QString& filePath);
    void updateToolbarIcons();  // Update icons based on current theme
    
    // Set project category and filter menus accordingly
    void setCategory(SurveyCategory category);
    SurveyCategory category() const { return m_category; }

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void importDXF();
    void importGDAL();
    void updateCoordinates(const QPointF& pos);
    void updateZoom(double zoom);
    void updateLayerPanel();
    void onLayerItemChanged();
    void onLayerContextMenu(const QPoint& pos);
    void toggleSnapping(bool enabled);
    void offsetPolyline();
    void runGamaAdjustment();
    void showKeyboardShortcuts();
    void openRecentProject();
    void updateRecentProjectsMenu();
    void importCSVPoints();


private:
    void setupMenus();
    void setupToolbar();
    void setupStatusBar();
    void setupLayerPanel();
    void setupPropertiesPanel();
    void setupPegPanel();         // Peg coordinate list panel
    void updatePropertiesPanel();
    void updatePegPanel();        // Refresh peg list
    void applyMenuFilters();

    CanvasWidget* m_canvas{nullptr};
    QLabel* m_coordLabel{nullptr};
    QLabel* m_zoomLabel{nullptr};
    QLabel* m_crsLabel{nullptr};
    QLabel* m_snapLabel{nullptr};
    
    // Dockable panels
    QDockWidget* m_layerDock{nullptr};
    QDockWidget* m_propertiesDock{nullptr};
    QDockWidget* m_pegDock{nullptr};      // Peg coordinate list
    QDockWidget* m_consoleDock{nullptr};  // Command console
    QListWidget* m_layerList{nullptr};
    QTreeWidget* m_propertiesTree{nullptr};
    QTableWidget* m_pegTable{nullptr};    // Table for peg coordinates
    
    QAction* m_snapAction{nullptr};
    QToolBar* m_toolbar{nullptr};
    QMenu* m_recentMenu{nullptr};
    
    // Project category
    SurveyCategory m_category{SurveyCategory::Engineering};
    
    // Filterable actions
    QAction* m_offsetAction{nullptr};
    QAction* m_partitionAction{nullptr};
    QAction* m_levellingAction{nullptr};
    QAction* m_gamaAction{nullptr};
    QAction* m_stakeoutAction{nullptr};
    QAction* m_resectionAction{nullptr};
    QAction* m_intersectionAction{nullptr};
    QAction* m_polarAction{nullptr};
    QAction* m_joinCalcAction{nullptr};
    
    // Authentication
    AuthManager* m_auth{nullptr};
};

#endif // MAINWINDOW_H
