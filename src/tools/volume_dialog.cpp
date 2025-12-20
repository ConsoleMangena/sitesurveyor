#include "tools/volume_dialog.h"
#include "tools/tin3dviewer.h"
#include "canvas/canvaswidget.h"
#include "gdal/geosbridge.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QGroupBox>
#include <QMessageBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QCheckBox>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QApplication>
#include <QPrinter>
#include <QPainter>
#include <QTextDocument>
#include <QBuffer>


VolumeDialog::VolumeDialog(CanvasWidget* canvas, QWidget *parent)
    : QDialog(parent), m_canvas(canvas)
{
    setupUi();
    populateBoundaryList();
    populatePegTable();
}

void VolumeDialog::setupUi()
{
    setWindowTitle("Volume Calculation");
    resize(600, 550);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Instructions
    QLabel* infoLabel = new QLabel("Calculate cut and fill volumes between survey points and a design level using Delaunay triangulation.");
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);
    
    // ===== Survey Points Group =====
    QGroupBox* pointsGroup = new QGroupBox("Survey Points");
    QVBoxLayout* pointsLayout = new QVBoxLayout(pointsGroup);
    
    // Selection buttons
    QHBoxLayout* selBtnLayout = new QHBoxLayout();
    QPushButton* selectAllBtn = new QPushButton("Select All");
    connect(selectAllBtn, &QPushButton::clicked, this, &VolumeDialog::selectAllPegs);
    selBtnLayout->addWidget(selectAllBtn);
    
    QPushButton* deselectAllBtn = new QPushButton("Deselect All");
    connect(deselectAllBtn, &QPushButton::clicked, this, &VolumeDialog::deselectAllPegs);
    selBtnLayout->addWidget(deselectAllBtn);
    
    selBtnLayout->addStretch();
    
    m_selectedCountLabel = new QLabel("Selected: 0 points");
    selBtnLayout->addWidget(m_selectedCountLabel);
    
    pointsLayout->addLayout(selBtnLayout);
    
    // Peg table
    m_pegTable = new QTableWidget();
    m_pegTable->setColumnCount(5);
    m_pegTable->setHorizontalHeaderLabels({"", "Name", "X", "Y", "Z"});
    m_pegTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_pegTable->setColumnWidth(0, 30);
    m_pegTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_pegTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_pegTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_pegTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_pegTable->setAlternatingRowColors(true);
    m_pegTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pegTable->verticalHeader()->setVisible(false);
    m_pegTable->setMinimumHeight(100);
    m_pegTable->setMaximumHeight(150);
    pointsLayout->addWidget(m_pegTable);
    
    mainLayout->addWidget(pointsGroup);
    
    // ===== Parameters Group =====
    QGroupBox* paramsGroup = new QGroupBox("Parameters");
    QFormLayout* formLayout = new QFormLayout(paramsGroup);
    
    m_boundaryCombo = new QComboBox();
    m_boundaryCombo->addItem("Convex Hull (All Points)", -1);
    connect(m_boundaryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VolumeDialog::onBoundaryChanged);
    formLayout->addRow("Boundary:", m_boundaryCombo);
    
    m_designLevelSpin = new QDoubleSpinBox();
    m_designLevelSpin->setRange(-10000.0, 10000.0);
    m_designLevelSpin->setDecimals(3);
    m_designLevelSpin->setSuffix(" m");
    m_designLevelSpin->setValue(0.0);
    formLayout->addRow("Design Level:", m_designLevelSpin);
    
    mainLayout->addWidget(paramsGroup);
    
    // ===== Action Buttons =====
    QHBoxLayout* actionLayout = new QHBoxLayout();
    
    m_calculateBtn = new QPushButton("Calculate");
    connect(m_calculateBtn, &QPushButton::clicked, this, &VolumeDialog::calculate);
    actionLayout->addWidget(m_calculateBtn);
    
    m_showTinBtn = new QPushButton("Show TIN");
    connect(m_showTinBtn, &QPushButton::clicked, this, &VolumeDialog::showTIN);
    actionLayout->addWidget(m_showTinBtn);
    
    m_hideTinBtn = new QPushButton("Hide TIN");
    connect(m_hideTinBtn, &QPushButton::clicked, this, &VolumeDialog::hideTIN);
    actionLayout->addWidget(m_hideTinBtn);
    
    m_view3DBtn = new QPushButton("View 3D");
    connect(m_view3DBtn, &QPushButton::clicked, this, &VolumeDialog::view3D);
    actionLayout->addWidget(m_view3DBtn);
    
    mainLayout->addLayout(actionLayout);
    
    // ===== Results Group =====
    QGroupBox* resultGroup = new QGroupBox("Results");
    QVBoxLayout* resultLayout = new QVBoxLayout(resultGroup);
    
    m_resultText = new QTextEdit();
    m_resultText->setReadOnly(true);
    m_resultText->setMaximumHeight(100);
    m_resultText->setPlaceholderText("Select points and click 'Calculate' to see results...");
    resultLayout->addWidget(m_resultText);
    
    mainLayout->addWidget(resultGroup);
    
    // ===== Bottom Buttons =====
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    
    m_exportBtn = new QPushButton("Export Report (PDF)");
    connect(m_exportBtn, &QPushButton::clicked, this, &VolumeDialog::exportReport);
    bottomLayout->addWidget(m_exportBtn);
    
    bottomLayout->addStretch();
    
    QPushButton* closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(closeBtn);
    
    mainLayout->addLayout(bottomLayout);
}

void VolumeDialog::populatePegTable()
{
    if (!m_canvas || !m_pegTable) return;
    
    const auto& pegs = m_canvas->pegs();
    m_pegTable->setRowCount(pegs.size());
    
    for (int i = 0; i < pegs.size(); ++i) {
        const auto& peg = pegs[i];
        
        // Checkbox
        QCheckBox* checkBox = new QCheckBox();
        checkBox->setChecked(true);
        connect(checkBox, &QCheckBox::checkStateChanged, this, &VolumeDialog::updateSelectedCount);
        QWidget* checkWidget = new QWidget();
        QHBoxLayout* checkLayout = new QHBoxLayout(checkWidget);
        checkLayout->addWidget(checkBox);
        checkLayout->setAlignment(Qt::AlignCenter);
        checkLayout->setContentsMargins(0, 0, 0, 0);
        m_pegTable->setCellWidget(i, 0, checkWidget);
        
        m_pegTable->setItem(i, 1, new QTableWidgetItem(peg.name));
        m_pegTable->setItem(i, 2, new QTableWidgetItem(QString::number(peg.position.x(), 'f', 3)));
        m_pegTable->setItem(i, 3, new QTableWidgetItem(QString::number(peg.position.y(), 'f', 3)));
        m_pegTable->setItem(i, 4, new QTableWidgetItem(QString::number(peg.z, 'f', 3)));
    }
    
    updateSelectedCount();
}

void VolumeDialog::populateBoundaryList()
{
    if (!m_canvas) return;
    
    const auto& polylines = m_canvas->polylines();
    for (int i = 0; i < polylines.size(); ++i) {
        if (polylines[i].closed && polylines[i].points.size() >= 3) {
            QString name = QString("Polygon %1 (%2 pts)")
                .arg(i + 1).arg(polylines[i].points.size());
            m_boundaryCombo->addItem(name, i);
        }
    }
}

void VolumeDialog::selectAllPegs()
{
    for (int i = 0; i < m_pegTable->rowCount(); ++i) {
        QWidget* widget = m_pegTable->cellWidget(i, 0);
        if (widget) {
            QCheckBox* cb = widget->findChild<QCheckBox*>();
            if (cb) cb->setChecked(true);
        }
    }
}

void VolumeDialog::deselectAllPegs()
{
    for (int i = 0; i < m_pegTable->rowCount(); ++i) {
        QWidget* widget = m_pegTable->cellWidget(i, 0);
        if (widget) {
            QCheckBox* cb = widget->findChild<QCheckBox*>();
            if (cb) cb->setChecked(false);
        }
    }
}

void VolumeDialog::updateSelectedCount()
{
    int count = getSelectedPegIndices().size();
    m_selectedCountLabel->setText(QString("Selected: %1 points").arg(count));
    
    bool canCalc = count >= 3;
    m_calculateBtn->setEnabled(canCalc);
    m_showTinBtn->setEnabled(canCalc);
    m_view3DBtn->setEnabled(canCalc);
}

QVector<int> VolumeDialog::getSelectedPegIndices()
{
    QVector<int> indices;
    for (int i = 0; i < m_pegTable->rowCount(); ++i) {
        QWidget* widget = m_pegTable->cellWidget(i, 0);
        if (widget) {
            QCheckBox* cb = widget->findChild<QCheckBox*>();
            if (cb && cb->isChecked()) {
                indices.append(i);
            }
        }
    }
    return indices;
}

void VolumeDialog::onBoundaryChanged(int) { updateSelectedCount(); }

void VolumeDialog::calculate()
{
    if (!m_canvas) return;
    
    QVector<int> selectedIndices = getSelectedPegIndices();
    if (selectedIndices.size() < 3) {
        QMessageBox::warning(this, "Volume Calculation", 
            "Need at least 3 selected points.");
        return;
    }
    
    QVector<GeosBridge::Point3D> surfacePoints;
    const auto& pegs = m_canvas->pegs();
    for (int idx : selectedIndices) {
        if (idx < pegs.size()) {
            const auto& peg = pegs[idx];
            surfacePoints.append(GeosBridge::Point3D(peg.position.x(), peg.position.y(), peg.z));
        }
    }
    
    QVector<QPointF> boundary;
    int boundaryIdx = m_boundaryCombo->currentData().toInt();
    if (boundaryIdx >= 0 && boundaryIdx < m_canvas->polylines().size()) {
        boundary = m_canvas->polylines()[boundaryIdx].points;
    }
    
    double designLevel = m_designLevelSpin->value();
    auto result = GeosBridge::calculateVolume(surfacePoints, designLevel, boundary);
    
    double cutVol = result.first;
    double fillVol = result.second;
    double netVol = cutVol - fillVol;
    
    QVector<QPointF> points2D;
    for (const auto& p : surfacePoints) {
        points2D.append(QPointF(p.x, p.y));
    }
    
    double surfaceArea = 0.0;
    auto triangles = GeosBridge::delaunayTriangulate(points2D);
    for (const auto& tri : triangles) {
        if (tri.size() != 3) continue;
        const auto& p0 = surfacePoints[tri[0]];
        const auto& p1 = surfacePoints[tri[1]];
        const auto& p2 = surfacePoints[tri[2]];
        
        if (!boundary.isEmpty()) {
            QPointF centroid((p0.x + p1.x + p2.x) / 3.0, (p0.y + p1.y + p2.y) / 3.0);
            if (!GeosBridge::pointInPolygon(centroid, boundary)) continue;
        }
        
        surfaceArea += qAbs((p0.x * (p1.y - p2.y) + p1.x * (p2.y - p0.y) + p2.x * (p0.y - p1.y)) / 2.0);
    }
    
    // Store results for export
    m_lastCutVol = cutVol;
    m_lastFillVol = fillVol;
    m_lastSurfaceArea = surfaceArea;
    m_lastTriangleCount = triangles.size();
    
    QString resultText;
    resultText += QString("Cut Volume:     %1 m3\n").arg(cutVol, 0, 'f', 2);
    resultText += QString("Fill Volume:    %1 m3\n").arg(fillVol, 0, 'f', 2);
    resultText += QString("Net Volume:     %1 m3 (%2)\n").arg(qAbs(netVol), 0, 'f', 2).arg(netVol > 0 ? "Net Cut" : "Net Fill");
    resultText += QString("Surface Area:   %1 m2\n").arg(surfaceArea, 0, 'f', 2);
    resultText += QString("Triangles:      %1\n").arg(triangles.size());
    
    m_resultText->setPlainText(resultText);
}

void VolumeDialog::showTIN()
{
    if (!m_canvas) return;
    m_canvas->generateTINFromPegs(m_designLevelSpin->value());
}

void VolumeDialog::hideTIN()
{
    if (!m_canvas) return;
    m_canvas->clearTIN();
}

void VolumeDialog::view3D()
{
    if (!m_canvas) return;
    
    if (getSelectedPegIndices().size() < 3) {
        QMessageBox::warning(this, "3D View", "Need at least 3 selected points.");
        return;
    }
    
    TIN3DViewerDialog dialog(m_canvas, this);
    dialog.exec();
}

void VolumeDialog::exportReport()
{
    // Show report options dialog
    QDialog optionsDialog(this);
    optionsDialog.setWindowTitle("Report Options");
    optionsDialog.setMinimumWidth(300);
    
    QVBoxLayout* optLayout = new QVBoxLayout(&optionsDialog);
    
    QLabel* titleLabel = new QLabel("Select sections to include:");
    optLayout->addWidget(titleLabel);
    
    QCheckBox* chkParams = new QCheckBox("Calculation Parameters");
    chkParams->setChecked(true);
    optLayout->addWidget(chkParams);
    
    QCheckBox* chkResults = new QCheckBox("Volume Results");
    chkResults->setChecked(true);
    optLayout->addWidget(chkResults);
    
    QCheckBox* chkPoints = new QCheckBox("Survey Points List");
    chkPoints->setChecked(true);
    optLayout->addWidget(chkPoints);
    
    QCheckBox* chkTinImage = new QCheckBox("TIN Surface Image (2D)");
    chkTinImage->setChecked(m_canvas && m_canvas->hasTIN());
    chkTinImage->setEnabled(m_canvas && m_canvas->hasTIN());
    if (!chkTinImage->isEnabled()) {
        chkTinImage->setText("TIN Surface Image (show TIN first)");
    }
    optLayout->addWidget(chkTinImage);
    
    QCheckBox* chk3DImage = new QCheckBox("3D Model View");
    chk3DImage->setChecked(false);
    chk3DImage->setEnabled(getSelectedPegIndices().size() >= 3);
    if (!chk3DImage->isEnabled()) {
        chk3DImage->setText("3D Model View (need 3+ points)");
    }
    optLayout->addWidget(chk3DImage);
    
    optLayout->addSpacing(10);
    
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* okBtn = new QPushButton("Export");
    QPushButton* cancelBtn = new QPushButton("Cancel");
    connect(okBtn, &QPushButton::clicked, &optionsDialog, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &optionsDialog, &QDialog::reject);
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    optLayout->addLayout(btnLayout);
    
    if (optionsDialog.exec() != QDialog::Accepted) return;
    
    // Get file name
    QString fileName = QFileDialog::getSaveFileName(this, "Export Volume Report",
        QString("volume_report_%1.pdf").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        "PDF Files (*.pdf);;All Files (*)");
    
    if (fileName.isEmpty()) return;
    
    // Capture TIN image if requested
    QString tinImageBase64;
    if (chkTinImage->isChecked() && m_canvas && m_canvas->hasTIN()) {
        QImage tinImage = m_canvas->grab().toImage();
        if (!tinImage.isNull()) {
            QByteArray byteArray;
            QBuffer buffer(&byteArray);
            buffer.open(QIODevice::WriteOnly);
            tinImage.save(&buffer, "PNG");
            tinImageBase64 = QString::fromLatin1(byteArray.toBase64());
        }
    }
    
    // Capture 3D view if requested
    QString view3DImageBase64;
    if (chk3DImage->isChecked() && m_canvas && getSelectedPegIndices().size() >= 3) {
        // Create a temporary 3D viewer and capture it
        TIN3DViewerDialog tempDialog(m_canvas, nullptr);
        tempDialog.resize(600, 400);
        tempDialog.show();
        QApplication::processEvents();
        
        QImage view3DImage = tempDialog.grab().toImage();
        if (!view3DImage.isNull()) {
            QByteArray byteArray;
            QBuffer buffer(&byteArray);
            buffer.open(QIODevice::WriteOnly);
            view3DImage.save(&buffer, "PNG");
            view3DImageBase64 = QString::fromLatin1(byteArray.toBase64());
        }
        tempDialog.close();
    }

    
    // Build HTML report
    QString html;
    html += "<!DOCTYPE html><html><head><style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; }";
    html += "h1 { text-align: center; color: #333; border-bottom: 2px solid #333; padding-bottom: 10px; }";
    html += "h2 { color: #555; margin-top: 20px; border-bottom: 1px solid #ddd; padding-bottom: 5px; }";
    html += "table { border-collapse: collapse; width: 100%; margin: 10px 0; }";
    html += "th, td { border: 1px solid #ccc; padding: 8px; text-align: left; }";
    html += "th { background-color: #f0f0f0; font-weight: bold; width: 40%; }";
    html += ".info { color: #666; font-size: 12px; text-align: center; }";
    html += ".highlight { background-color: #ffffcc; }";
    html += "img { max-width: 100%; border: 1px solid #ccc; margin: 10px 0; }";
    html += "</style></head><body>";
    
    // Title
    html += "<h1>VOLUME CALCULATION REPORT</h1>";
    html += QString("<p class='info'>Generated: %1 | Application: SiteSurveyor</p>")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    
    // Parameters
    if (chkParams->isChecked()) {
        html += "<h2>Calculation Parameters</h2>";
        html += "<table>";
        html += QString("<tr><th>Design Level</th><td>%1 m</td></tr>").arg(m_designLevelSpin->value(), 0, 'f', 3);
        html += QString("<tr><th>Points Used</th><td>%1</td></tr>").arg(getSelectedPegIndices().size());
        html += QString("<tr><th>Triangles</th><td>%1</td></tr>").arg(m_lastTriangleCount);
        html += "</table>";
    }
    
    // Volume Results
    if (chkResults->isChecked()) {
        html += "<h2>Volume Results</h2>";
        html += "<table>";
        html += QString("<tr><th>Cut Volume</th><td>%1 m&sup3;</td></tr>").arg(m_lastCutVol, 0, 'f', 2);
        html += QString("<tr><th>Fill Volume</th><td>%1 m&sup3;</td></tr>").arg(m_lastFillVol, 0, 'f', 2);
        double netVol = m_lastCutVol - m_lastFillVol;
        html += QString("<tr class='highlight'><th>Net Volume</th><td><b>%1 m&sup3; (%2)</b></td></tr>")
            .arg(qAbs(netVol), 0, 'f', 2).arg(netVol > 0 ? "Net Cut" : "Net Fill");
        html += QString("<tr><th>Surface Area</th><td>%1 m&sup2;</td></tr>").arg(m_lastSurfaceArea, 0, 'f', 2);
        html += "</table>";
    }
    
    // TIN Image
    if (chkTinImage->isChecked() && !tinImageBase64.isEmpty()) {
        html += "<h2>TIN Surface View (2D)</h2>";
        html += QString("<img src='data:image/png;base64,%1' />").arg(tinImageBase64);
    }
    
    // 3D Model Image
    if (chk3DImage->isChecked() && !view3DImageBase64.isEmpty()) {
        html += "<h2>3D Model View</h2>";
        html += QString("<img src='data:image/png;base64,%1' />").arg(view3DImageBase64);
    }

    
    // Survey Points
    if (chkPoints->isChecked()) {
        html += "<h2>Survey Points Used</h2>";
        html += "<table><tr><th>Name</th><th>X</th><th>Y</th><th>Z</th></tr>";
        
        const auto& pegs = m_canvas->pegs();
        QVector<int> selectedIndices = getSelectedPegIndices();
        for (int idx : selectedIndices) {
            if (idx < pegs.size()) {
                const auto& peg = pegs[idx];
                html += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td></tr>")
                    .arg(peg.name)
                    .arg(peg.position.x(), 0, 'f', 3)
                    .arg(peg.position.y(), 0, 'f', 3)
                    .arg(peg.z, 0, 'f', 3);
            }
        }
        html += "</table>";
    }
    
    html += "</body></html>";
    
    // Create PDF using QTextDocument
    QTextDocument doc;
    doc.setHtml(html);
    
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    printer.setPageSize(QPageSize::A4);
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
    
    doc.print(&printer);
    
    QMessageBox::information(this, "Report Exported", 
        QString("PDF report saved to:\n%1").arg(fileName));
}




void VolumeDialog::applyTheme() { /* Not used */ }
