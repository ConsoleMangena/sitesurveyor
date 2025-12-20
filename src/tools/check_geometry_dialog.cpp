#include "tools/check_geometry_dialog.h"
#include "canvas/canvaswidget.h"
#include "gdal/geosbridge.h"
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDebug>
#include <QRegularExpression>

CheckGeometryDialog::CheckGeometryDialog(CanvasWidget* canvas, CheckMode mode, QWidget *parent)
    : QDialog(parent), m_canvas(canvas), m_mode(mode)
{
    setupUi();
    runAnalysis();
}

CheckGeometryDialog::~CheckGeometryDialog()
{
}

void CheckGeometryDialog::setupUi()
{
    setWindowTitle("Geometry Check");
    resize(700, 500);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Header
    m_statusLabel = new QLabel(this);
    QFont headerFont = font();
    headerFont.setBold(true);
    headerFont.setPointSize(11);
    m_statusLabel->setFont(headerFont);
    mainLayout->addWidget(m_statusLabel);

    // Table
    m_issueTable = new QTableWidget(this);
    m_issueTable->setColumnCount(3);
    m_issueTable->setHorizontalHeaderLabels({"ID", "Layer", "Error Details"});
    m_issueTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_issueTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_issueTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_issueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_issueTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(m_issueTable, &QTableWidget::itemSelectionChanged, this, &CheckGeometryDialog::onSelectionChanged);
    connect(m_issueTable, &QTableWidget::cellDoubleClicked, this, &CheckGeometryDialog::onZoomToClicked);
    
    mainLayout->addWidget(m_issueTable);

    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    
    m_fixBtn = new QPushButton("Fix Selected (Simple)", this);
    m_fixBtn->setToolTip("Attempts to close rings / remove duplicates. Preserves shape.");
    connect(m_fixBtn, &QPushButton::clicked, this, &CheckGeometryDialog::onFixSelectedClicked);
    
    m_autoFixBtn = new QPushButton("Auto Fix Selected (Reconstruct)", this);
    m_autoFixBtn->setToolTip("Uses GEOS MakeValid to reconstruct topology. May modify shape.");
    connect(m_autoFixBtn, &QPushButton::clicked, this, &CheckGeometryDialog::onAutoFixSelectedClicked);
    
    m_zoomBtn = new QPushButton("Zoom To", this);
    connect(m_zoomBtn, &QPushButton::clicked, this, &CheckGeometryDialog::onZoomToClicked);
    
    m_refreshBtn = new QPushButton("Re-Check All", this);
    connect(m_refreshBtn, &QPushButton::clicked, this, &CheckGeometryDialog::onRefreshClicked);
    
    m_closeBtn = new QPushButton("Close", this);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    
    btnLayout->addWidget(m_fixBtn);
    btnLayout->addWidget(m_autoFixBtn);
    btnLayout->addWidget(m_zoomBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_refreshBtn);
    btnLayout->addWidget(m_closeBtn);
    
    mainLayout->addLayout(btnLayout);
    
    // Clear marker on close
    connect(this, &QDialog::finished, [this](int){
        if(m_canvas) m_canvas->clearTemporaryMarker();
    });
    
    onSelectionChanged(); // Update button states
}

void CheckGeometryDialog::runAnalysis()
{
    m_issueTable->setRowCount(0);
    m_issues.clear(); // Important: keep m_issues in sync with table rows
    
    if (!m_canvas) return;
    
    const auto& polylines = m_canvas->polylines();
    QVector<int> indicesToCheck;
    
    if (m_mode == CheckSelected) {
        indicesToCheck = m_canvas->getSelectedIndices();
    } else {
        for(int i=0; i<polylines.size(); ++i) indicesToCheck.append(i);
    }
    
    int processed = 0;
    int issuesFound = 0;
    
    // Disable updates for speed
    m_issueTable->setUpdatesEnabled(false);
    
    for (int idx : indicesToCheck) {
        if (idx < 0 || idx >= polylines.size()) continue;
        const auto& poly = polylines[idx];
        
        if (!poly.closed) continue; // Skip lines, only check polygons
        
        processed++;
        bool valid = GeosBridge::isValid(poly.points);
        if (!valid) {
             QString error = GeosBridge::lastError();
             
             Issue issue;
             issue.polyIndex = idx;
             issue.layer = poly.layer;
             issue.error = error;
             issue.originalPoints = poly.points;
             issue.errorLocation = parseErrorLocation(error);
             
             m_issues.append(issue); // Append to list
             addIssueRow(idx, poly.layer, error); // Add to table
             
             issuesFound++;
        }
    }
    
    m_issueTable->setUpdatesEnabled(true);
    
    QString modeStr = (m_mode == CheckAll) ? "Entire Drawing" : "Selection";
    if (issuesFound == 0) {
        m_statusLabel->setText(QString("Analyzed %1 geometries (%2). <span style='color:green'>No issues found.</span>").arg(processed).arg(modeStr));
        m_statusLabel->setTextFormat(Qt::RichText);
    } else {
        m_statusLabel->setText(QString("Analyzed %1 geometries (%2). FOUND <span style='color:red'>%3 ERROR(S)</span>.").arg(processed).arg(modeStr).arg(issuesFound));
        m_statusLabel->setTextFormat(Qt::RichText);
    }
}

void CheckGeometryDialog::addIssueRow(int polyIndex, const QString& layer, const QString& error)
{
    int row = m_issueTable->rowCount();
    m_issueTable->insertRow(row);
    
    QTableWidgetItem* idItem = new QTableWidgetItem(QString::number(polyIndex));
    idItem->setData(Qt::UserRole, polyIndex); // Store ID for safe keeping
    m_issueTable->setItem(row, 0, idItem);
    
    m_issueTable->setItem(row, 1, new QTableWidgetItem(layer));
    
    QTableWidgetItem* errItem = new QTableWidgetItem(error);
    errItem->setForeground(QBrush(Qt::red));
    m_issueTable->setItem(row, 2, errItem);
}

void CheckGeometryDialog::onSelectionChanged()
{
    bool hasSelection = !m_issueTable->selectedItems().isEmpty();
    m_fixBtn->setEnabled(hasSelection);
    m_autoFixBtn->setEnabled(hasSelection);
    m_zoomBtn->setEnabled(hasSelection);
    
    QList<QTableWidgetItem*> selected = m_issueTable->selectedItems();
    if (selected.isEmpty()) {
        m_canvas->clearTemporaryMarker();
        return;
    }
    
    // Find unique rows
    QSet<int> rows;
    for (auto item : selected) rows.insert(item->row());
    
    if (rows.size() == 1) {
         int row = *rows.begin();
         if (row >= 0 && row < m_issues.size()) {
             QPointF loc = m_issues[row].errorLocation;
             if (loc != QPointF(0,0)) {
                 m_canvas->setTemporaryMarker(loc);
             } else {
                 m_canvas->clearTemporaryMarker();
             }
         }
    } else {
         m_canvas->clearTemporaryMarker();
    }
}

void CheckGeometryDialog::onRefreshClicked()
{
    runAnalysis();
}

void CheckGeometryDialog::onZoomToClicked()
{
    QList<QTableWidgetItem*> selected = m_issueTable->selectedItems();
    if (selected.isEmpty()) return;
    
    // Find unique rows
    QSet<int> rows;
    for (auto item : selected) rows.insert(item->row());
    
    if (rows.isEmpty()) return;
    
    // Get first selected item index
    int row = *rows.begin();
    if (row >= 0 && row < m_issues.size()) {
        int polyIdx = m_issues[row].polyIndex;
        // Tell canvas to zoom or select
        m_canvas->clearSelection();
        m_canvas->addToSelection(polyIdx);
        
        // Zoom to error location if available
        QPointF loc = m_issues[row].errorLocation;
        if (loc != QPointF(0,0)) {
            m_canvas->zoomToPoint(loc);
        }
        
        m_canvas->update();
    }
}

void CheckGeometryDialog::onFixSelectedClicked()
{
    QList<QTableWidgetItem*> selected = m_issueTable->selectedItems();
    QSet<int> rows;
    for (auto item : selected) rows.insert(item->row());
    
    if (rows.isEmpty()) return;
    
    int fixedCount = 0;
    
    for (int row : rows) {
        if (row < 0 || row >= m_issues.size()) continue;
        const Issue& issue = m_issues[row];
        
        QVector<QPointF> points = issue.originalPoints;
        // Reload current points from canvas just in case?
        // No, rely on issue snapshot or re-check?
        // Better to re-fetch from canvas as IDs might be stable but content changed?
        // Actually, if we fix one, list might be stale if we don't refresh.
        // We'll proceed with best effort.
        
        // --- Simple Fix Logic ---
        QVector<QPointF> uniquePoints;
        if (!points.isEmpty()) {
            uniquePoints.append(points.first());
            for (int i = 1; i < points.size(); ++i) {
                if (QLineF(points[i], points[i-1]).length() > 1e-9) {
                    uniquePoints.append(points[i]);
                }
            }
        }
        if (!uniquePoints.isEmpty() && uniquePoints.first() != uniquePoints.last()) {
             uniquePoints.append(uniquePoints.first());
        }
        // ------------------------
        
        if (m_canvas->replacePolylinePoints(issue.polyIndex, uniquePoints)) {
            fixedCount++;
        }
    }
    
    runAnalysis(); // Refresh list
    QMessageBox::information(this, "Fix Complete", QString("Applied Simple Fix to %1 geometries.").arg(fixedCount));
}

void CheckGeometryDialog::onAutoFixSelectedClicked()
{
    QList<QTableWidgetItem*> selected = m_issueTable->selectedItems();
    QSet<int> rows;
    for (auto item : selected) rows.insert(item->row());
    
    if (rows.isEmpty()) return;
    
    int fixedCount = 0;
    
    for (int row : rows) {
        if (row < 0 || row >= m_issues.size()) continue;
        const Issue& issue = m_issues[row];
        
        // Re-fetch current points from canvas to ensure we are working on latest state
        // (Though replacePolylinePoints uses index, so it's fine).
        // We need existing points to run makeValid.
        // Accessing canvas directly:
        if (issue.polyIndex >= 0 && issue.polyIndex < m_canvas->polylines().size()) {
             QVector<QPointF> currentPoints = m_canvas->polylines()[issue.polyIndex].points;
             
             QVector<QPointF> fixed = GeosBridge::makeValid(currentPoints);
             if (!fixed.isEmpty()) {
                 if (m_canvas->replacePolylinePoints(issue.polyIndex, fixed)) {
                     fixedCount++;
                 }
             }
        }
    }
    
    runAnalysis(); // Refresh list
    QMessageBox::information(this, "Fix Complete", QString("Applied Auto Fix to %1 geometries.").arg(fixedCount));
}

QPointF CheckGeometryDialog::parseErrorLocation(const QString& error)
{
    // Example: "Self-intersection at or near point 39.53 1.19"
    static QRegularExpression re(R"(point\s+([-\d.]+)\s+([-\d.]+))");
    QRegularExpressionMatch match = re.match(error);
    if (match.hasMatch()) {
        bool okX, okY;
        double x = match.captured(1).toDouble(&okX);
        double y = match.captured(2).toDouble(&okY);
        if (okX && okY) return QPointF(x, y);
    }
    return QPointF(0,0);
}
