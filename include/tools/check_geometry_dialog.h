#ifndef CHECK_GEOMETRY_DIALOG_H
#define CHECK_GEOMETRY_DIALOG_H

#include <QDialog>
#include <QVector>
#include <QPointF>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>

class CanvasWidget;

class CheckGeometryDialog : public QDialog
{
    Q_OBJECT

public:
    // Mode enum
    enum CheckMode {
        CheckSelected, // Only check currently selected geometry
        CheckAll       // Check all geometries in canvas
    };

    explicit CheckGeometryDialog(CanvasWidget* canvas, CheckMode mode = CheckAll, QWidget *parent = nullptr);
    ~CheckGeometryDialog();

private slots:
    void onSelectionChanged();
    void onFixSelectedClicked();     // Simple fix for selected table rows
    void onAutoFixSelectedClicked(); // Auto fix (MakeValid) for selected table rows
    void onZoomToClicked();
    void onRefreshClicked();

private:
    void setupUi();
    void runAnalysis();
    void addIssueRow(int polyIndex, const QString& layer, const QString& error);

    CanvasWidget* m_canvas;
    CheckMode m_mode;
    
    struct Issue {
        int polyIndex;
        QString layer;
        QString error;
        QVector<QPointF> originalPoints;
        QPointF errorLocation; // For visual marker
    };
    QVector<Issue> m_issues; // Stores index in parallel with table rows
    
    QPointF parseErrorLocation(const QString& error); // Helper

    // UI Elements
    QLabel* m_statusLabel;
    QTableWidget* m_issueTable;
    QPushButton* m_fixBtn;
    QPushButton* m_autoFixBtn;
    QPushButton* m_zoomBtn;
    QPushButton* m_refreshBtn;
    QPushButton* m_closeBtn;
};

#endif // CHECK_GEOMETRY_DIALOG_H
