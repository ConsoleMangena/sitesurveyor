#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class CanvasWidget;
class QLabel;
class QDockWidget;
class QListWidget;
class QTreeWidget;
class QAction;
class QToolBar;
class QMenu;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

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

private:
    void setupMenus();
    void setupToolbar();
    void setupStatusBar();
    void setupLayerPanel();
    void setupPropertiesPanel();
    void setupToolsPanel();
    void addToRecentProjects(const QString& filePath);
    void updatePropertiesPanel();

    CanvasWidget* m_canvas{nullptr};
    QLabel* m_coordLabel{nullptr};
    QLabel* m_zoomLabel{nullptr};
    QLabel* m_crsLabel{nullptr};
    QLabel* m_snapLabel{nullptr};
    
    // Dockable panels
    QDockWidget* m_layerDock{nullptr};
    QDockWidget* m_propertiesDock{nullptr};
    QDockWidget* m_toolsDock{nullptr};
    QListWidget* m_layerList{nullptr};
    QTreeWidget* m_propertiesTree{nullptr};
    
    QAction* m_snapAction{nullptr};
    QToolBar* m_toolbar{nullptr};
    QMenu* m_recentMenu{nullptr};
};

#endif // MAINWINDOW_H
