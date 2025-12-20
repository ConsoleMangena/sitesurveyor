#include "tools/traverse_dialog.h"
#include "canvas/canvaswidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QSettings>
#include <QScrollArea>
#include <QtMath>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>

TraverseDialog::TraverseDialog(CanvasWidget* canvas, QWidget* parent)
    : QDialog(parent), m_canvas(canvas)
{
    setWindowTitle("Traverse Calculation");
    resize(1000, 700);
    setupUI();
}

void TraverseDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    
    // ========== Top Section: Start Point & Options ==========
    QHBoxLayout* topLayout = new QHBoxLayout();
    
    // Start Point Group
    QGroupBox* startGroup = new QGroupBox("Starting Point");
    QGridLayout* startLayout = new QGridLayout(startGroup);
    
    QSettings settings;
    bool swapXY = settings.value("coordinates/swapXY", false).toBool();
    QString firstLabel = swapXY ? "Y:" : "X:";
    QString secondLabel = swapXY ? "X:" : "Y:";
    
    m_startStationEdit = new QLineEdit("A");
    m_startStationEdit->setMaximumWidth(80);
    startLayout->addWidget(new QLabel("Station:"), 0, 0);
    startLayout->addWidget(m_startStationEdit, 0, 1);
    
    m_startEasting = new QDoubleSpinBox();
    m_startEasting->setRange(-9999999.999, 9999999.999);
    m_startEasting->setDecimals(3);
    m_startEasting->setValue(1000.000);
    startLayout->addWidget(new QLabel(firstLabel), 1, 0);
    startLayout->addWidget(m_startEasting, 1, 1);
    
    m_startNorthing = new QDoubleSpinBox();
    m_startNorthing->setRange(-9999999.999, 9999999.999);
    m_startNorthing->setDecimals(3);
    m_startNorthing->setValue(1000.000);
    startLayout->addWidget(new QLabel(secondLabel), 2, 0);
    startLayout->addWidget(m_startNorthing, 2, 1);
    
    m_startBearing = new QDoubleSpinBox();
    m_startBearing->setRange(0.0, 360.0);
    m_startBearing->setDecimals(4);
    m_startBearing->setValue(0.0);
    m_startBearing->setSuffix("°");
    startLayout->addWidget(new QLabel("Initial Bearing:"), 3, 0);
    startLayout->addWidget(m_startBearing, 3, 1);
    
    // Use Current Station button
    QPushButton* useStationBtn = new QPushButton("Use Current Station");
    useStationBtn->setToolTip("Load coordinates from the current station setup");
    connect(useStationBtn, &QPushButton::clicked, this, [this]() {
        if (!m_canvas) return;
        const auto& stn = m_canvas->station();
        if (!stn.hasStation) {
            QMessageBox::information(this, "No Station", "No station is currently set.\n\nUse Survey → Station Setup first.");
            return;
        }
        m_startStationEdit->setText(stn.stationName);
        m_startEasting->setValue(stn.stationPos.x());
        m_startNorthing->setValue(stn.stationPos.y());
        
        if (stn.hasBacksight) {
            // Calculate bearing to backsight
            double dE = stn.backsightPos.x() - stn.stationPos.x();
            double dN = stn.backsightPos.y() - stn.stationPos.y();
            double rad = std::atan2(dE, dN);
            double deg = qRadiansToDegrees(rad);
            if (deg < 0) deg += 360.0;
            m_startBearing->setValue(deg);
        }
    });
    startLayout->addWidget(useStationBtn, 4, 0, 1, 2);
    
    topLayout->addWidget(startGroup);
    
    // Options Group
    QGroupBox* optionsGroup = new QGroupBox("Options");
    QFormLayout* optionsLayout = new QFormLayout(optionsGroup);
    
    m_traverseTypeCombo = new QComboBox();
    m_traverseTypeCombo->addItem("Open Traverse", OpenTraverse);
    m_traverseTypeCombo->addItem("Closed Loop", ClosedLoop);
    m_traverseTypeCombo->addItem("Closed Connection", ClosedConnection);
    optionsLayout->addRow("Traverse Type:", m_traverseTypeCombo);
    
    m_adjustmentCombo = new QComboBox();
    m_adjustmentCombo->addItem("No Adjustment", NoAdjustment);
    m_adjustmentCombo->addItem("Bowditch (Compass Rule)", Bowditch);
    m_adjustmentCombo->addItem("Transit Rule", Transit);
    optionsLayout->addRow("Adjustment:", m_adjustmentCombo);
    
    m_interiorAnglesCheck = new QCheckBox("Interior Angles (add to bearing)");
    m_interiorAnglesCheck->setChecked(true);
    optionsLayout->addRow(m_interiorAnglesCheck);
    
    m_southAzimuthCheck = new QCheckBox("South Bearing");
    m_southAzimuthCheck->setChecked(settings.value("coordinates/southAzimuth", false).toBool());
    optionsLayout->addRow(m_southAzimuthCheck);
    
    topLayout->addWidget(optionsGroup);
    
    // End Point Group (for closed connection)
    QGroupBox* endGroup = new QGroupBox("End Point (Closed Connection)");
    QGridLayout* endLayout = new QGridLayout(endGroup);
    
    m_endEasting = new QDoubleSpinBox();
    m_endEasting->setRange(-9999999.999, 9999999.999);
    m_endEasting->setDecimals(3);
    m_endEasting->setValue(1000.000);
    endLayout->addWidget(new QLabel(firstLabel), 0, 0);
    endLayout->addWidget(m_endEasting, 0, 1);
    
    m_endNorthing = new QDoubleSpinBox();
    m_endNorthing->setRange(-9999999.999, 9999999.999);
    m_endNorthing->setDecimals(3);
    m_endNorthing->setValue(1000.000);
    endLayout->addWidget(new QLabel(secondLabel), 1, 0);
    endLayout->addWidget(m_endNorthing, 1, 1);
    
    topLayout->addWidget(endGroup);
    
    mainLayout->addLayout(topLayout);
    
    // ========== Table ==========
    setupTable();
    mainLayout->addWidget(m_table);
    
    // ========== Table Controls ==========
    QHBoxLayout* tableControls = new QHBoxLayout();
    
    QPushButton* addBtn = new QPushButton("+ Add Station");
    connect(addBtn, &QPushButton::clicked, this, &TraverseDialog::addStation);
    tableControls->addWidget(addBtn);
    
    QPushButton* removeBtn = new QPushButton("- Remove Station");
    connect(removeBtn, &QPushButton::clicked, this, &TraverseDialog::removeStation);
    tableControls->addWidget(removeBtn);
    
    QPushButton* importBtn = new QPushButton("Import from Pegs");
    connect(importBtn, &QPushButton::clicked, this, &TraverseDialog::importFromPegs);
    tableControls->addWidget(importBtn);
    
    QPushButton* clearBtn = new QPushButton("Clear All");
    connect(clearBtn, &QPushButton::clicked, this, &TraverseDialog::clearAll);
    tableControls->addWidget(clearBtn);
    
    tableControls->addStretch();
    mainLayout->addLayout(tableControls);
    
    // ========== Results Section ==========
    QGroupBox* resultsGroup = new QGroupBox("Closure Analysis");
    QHBoxLayout* resultsLayout = new QHBoxLayout(resultsGroup);
    
    m_closureLabel = new QLabel("Linear Closure: --");
    m_closureLabel->setStyleSheet("font-weight: bold;");
    resultsLayout->addWidget(m_closureLabel);
    
    m_precisionLabel = new QLabel("Precision: --");
    m_precisionLabel->setStyleSheet("font-weight: bold;");
    resultsLayout->addWidget(m_precisionLabel);
    
    m_angularErrorLabel = new QLabel("Angular Misclose: --");
    m_angularErrorLabel->setStyleSheet("font-weight: bold;");
    resultsLayout->addWidget(m_angularErrorLabel);
    
    resultsLayout->addStretch();
    mainLayout->addWidget(resultsGroup);
    
    // ========== Buttons ==========
    QHBoxLayout* btnLayout = new QHBoxLayout();
    
    QPushButton* calcBtn = new QPushButton("Calculate");
    calcBtn->setStyleSheet("font-weight: bold; padding: 8px 20px;");
    connect(calcBtn, &QPushButton::clicked, this, &TraverseDialog::calculate);
    btnLayout->addWidget(calcBtn);
    
    QPushButton* adjustBtn = new QPushButton("Apply Adjustment");
    connect(adjustBtn, &QPushButton::clicked, this, &TraverseDialog::adjust);
    btnLayout->addWidget(adjustBtn);
    
    QPushButton* sendBtn = new QPushButton("Send to Canvas");
    connect(sendBtn, &QPushButton::clicked, this, &TraverseDialog::sendToCanvas);
    btnLayout->addWidget(sendBtn);
    
    btnLayout->addSpacing(20);
    
    QPushButton* csvBtn = new QPushButton("Export CSV");
    connect(csvBtn, &QPushButton::clicked, this, &TraverseDialog::exportCSV);
    btnLayout->addWidget(csvBtn);
    
    QPushButton* reportBtn = new QPushButton("Generate Report");
    connect(reportBtn, &QPushButton::clicked, this, &TraverseDialog::generateReport);
    btnLayout->addWidget(reportBtn);
    
    btnLayout->addStretch();
    
    QPushButton* closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(closeBtn);
    
    mainLayout->addLayout(btnLayout);
    
    // Add initial rows
    for (int i = 0; i < 5; ++i) {
        addStation();
    }
}

void TraverseDialog::setupTable()
{
    m_table = new QTableWidget(0, COL_COUNT);
    m_table->setHorizontalHeaderLabels({
        "Station", "Angle (°)", "Distance", "Bearing (°)", 
        "X", "Y", "Adj. X", "Adj. Y"
    });
    
    m_table->horizontalHeader()->setSectionResizeMode(COL_STATION, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(COL_ANGLE, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(COL_DISTANCE, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(COL_BEARING, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(COL_EASTING, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(COL_NORTHING, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(COL_ADJ_EASTING, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(COL_ADJ_NORTHING, QHeaderView::Stretch);
    
    connect(m_table, &QTableWidget::cellChanged, this, &TraverseDialog::onCellChanged);
}

void TraverseDialog::addStation()
{
    m_updating = true;
    int row = m_table->rowCount();
    m_table->insertRow(row);
    
    // Station ID
    QString stationId = QString("P%1").arg(row + 1);
    m_table->setItem(row, COL_STATION, new QTableWidgetItem(stationId));
    
    // Editable: Angle, Distance
    m_table->setItem(row, COL_ANGLE, new QTableWidgetItem("0.0000"));
    m_table->setItem(row, COL_DISTANCE, new QTableWidgetItem("0.000"));
    
    // Read-only columns
    for (int col = COL_BEARING; col < COL_COUNT; ++col) {
        QTableWidgetItem* item = new QTableWidgetItem("");
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setBackground(QBrush(QColor(45, 45, 48)));
        m_table->setItem(row, col, item);
    }
    
    m_updating = false;
}

void TraverseDialog::removeStation()
{
    int row = m_table->currentRow();
    if (row >= 0) {
        m_table->removeRow(row);
    }
}

void TraverseDialog::clearAll()
{
    m_table->setRowCount(0);
    m_observations.clear();
    m_calculated = false;
    updateClosureDisplay();
    
    // Add initial rows
    for (int i = 0; i < 5; ++i) {
        addStation();
    }
}

void TraverseDialog::onCellChanged(int row, int col)
{
    Q_UNUSED(row);
    if (m_updating) return;
    
    if (col == COL_ANGLE || col == COL_DISTANCE) {
        m_calculated = false;
    }
}

void TraverseDialog::importFromPegs()
{
    if (!m_canvas) {
        QMessageBox::warning(this, "Error", "Canvas not available");
        return;
    }
    
    const auto& pegs = m_canvas->pegs();
    if (pegs.isEmpty()) {
        QMessageBox::information(this, "Import", "No pegs found on canvas.");
        return;
    }
    
    m_table->setRowCount(0);
    m_updating = true;
    
    for (int i = 0; i < pegs.size(); ++i) {
        const auto& peg = pegs[i];
        int row = m_table->rowCount();
        m_table->insertRow(row);
        
        m_table->setItem(row, COL_STATION, new QTableWidgetItem(peg.name));
        m_table->setItem(row, COL_ANGLE, new QTableWidgetItem("0.0000"));
        m_table->setItem(row, COL_DISTANCE, new QTableWidgetItem("0.000"));
        
        for (int col = COL_BEARING; col < COL_COUNT; ++col) {
            QTableWidgetItem* item = new QTableWidgetItem("");
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            item->setBackground(QBrush(QColor(45, 45, 48)));
            m_table->setItem(row, col, item);
        }
    }
    
    m_updating = false;
    QMessageBox::information(this, "Import", QString("Imported %1 stations from pegs.").arg(pegs.size()));
}

void TraverseDialog::calculate()
{
    m_observations.clear();
    int rowCount = m_table->rowCount();
    
    if (rowCount < 2) {
        QMessageBox::warning(this, "Error", "At least 2 stations are required.");
        return;
    }
    
    // Collect observations from table
    for (int row = 0; row < rowCount; ++row) {
        TraverseObservation obs;
        obs.stationId = m_table->item(row, COL_STATION)->text();
        
        bool okAngle, okDist;
        obs.angle = m_table->item(row, COL_ANGLE)->text().toDouble(&okAngle);
        obs.distance = m_table->item(row, COL_DISTANCE)->text().toDouble(&okDist);
        
        if (!okAngle || !okDist) {
            QMessageBox::warning(this, "Error", 
                QString("Invalid numeric value at row %1").arg(row + 1));
            return;
        }
        
        m_observations.append(obs);
    }
    
    // Calculate forward bearings
    calculateForwardBearings();
    
    // Calculate coordinates
    calculateCoordinates();
    
    // Calculate closure error
    calculateClosureError();
    
    // Update table with results
    m_updating = true;
    for (int row = 0; row < m_observations.size(); ++row) {
        const auto& obs = m_observations[row];
        m_table->item(row, COL_BEARING)->setText(QString::number(obs.bearing, 'f', 4));
        m_table->item(row, COL_EASTING)->setText(QString::number(obs.easting, 'f', 3));
        m_table->item(row, COL_NORTHING)->setText(QString::number(obs.northing, 'f', 3));
    }
    m_updating = false;
    
    m_calculated = true;
    updateClosureDisplay();
}

void TraverseDialog::calculateForwardBearings()
{
    if (m_observations.isEmpty()) return;
    
    double currentBearing = m_startBearing->value();
    bool useInteriorAngles = m_interiorAnglesCheck->isChecked();
    
    // First station has the starting bearing
    m_observations[0].bearing = currentBearing;
    
    for (int i = 1; i < m_observations.size(); ++i) {
        double angle = m_observations[i].angle;
        
        if (useInteriorAngles) {
            // Interior angle: forward bearing = back bearing + 180 + angle
            currentBearing = currentBearing + 180.0 + angle;
        } else {
            // Deflection angle
            currentBearing = currentBearing + angle;
        }
        
        currentBearing = normalizeAngle(currentBearing);
        m_observations[i].bearing = currentBearing;
    }
}

void TraverseDialog::calculateCoordinates()
{
    if (m_observations.isEmpty()) return;
    
    double currentE = m_startEasting->value();
    double currentN = m_startNorthing->value();
    
    m_totalDistance = 0.0;
    
    // First point is the starting point
    m_observations[0].easting = currentE;
    m_observations[0].northing = currentN;
    
    for (int i = 1; i < m_observations.size(); ++i) {
        // Use previous bearing and this leg's distance
        double bearing = m_observations[i-1].bearing;
        double distance = m_observations[i].distance;
        
        double bearingRad = degreesToRadians(bearing);
        
        // Calculate departure (dE) and latitude (dN)
        double dE = distance * qSin(bearingRad);
        double dN = distance * qCos(bearingRad);
        
        currentE += dE;
        currentN += dN;
        
        m_observations[i].easting = currentE;
        m_observations[i].northing = currentN;
        
        m_totalDistance += distance;
    }
}

void TraverseDialog::calculateClosureError()
{
    if (m_observations.size() < 2) return;
    
    TraverseType type = static_cast<TraverseType>(m_traverseTypeCombo->currentData().toInt());
    
    double lastE = m_observations.last().easting;
    double lastN = m_observations.last().northing;
    
    double expectedE, expectedN;
    
    if (type == ClosedLoop) {
        // Should close on starting point
        expectedE = m_startEasting->value();
        expectedN = m_startNorthing->value();
    } else if (type == ClosedConnection) {
        // Should close on end point
        expectedE = m_endEasting->value();
        expectedN = m_endNorthing->value();
    } else {
        // Open traverse - no closure error
        m_closureEasting = 0.0;
        m_closureNorthing = 0.0;
        m_closureDistance = 0.0;
        return;
    }
    
    m_closureEasting = lastE - expectedE;
    m_closureNorthing = lastN - expectedN;
    m_closureDistance = qSqrt(m_closureEasting * m_closureEasting + 
                               m_closureNorthing * m_closureNorthing);
    
    // Angular misclose for closed loop
    if (type == ClosedLoop) {
        double sumAngles = 0.0;
        for (const auto& obs : m_observations) {
            sumAngles += obs.angle;
        }
        int n = m_observations.size();
        double expectedSum = (n - 2) * 180.0;  // Sum of interior angles
        m_angularMisclose = sumAngles - expectedSum;
    }
}

void TraverseDialog::adjust()
{
    if (!m_calculated) {
        QMessageBox::warning(this, "Error", "Please calculate the traverse first.");
        return;
    }
    
    AdjustmentMethod method = static_cast<AdjustmentMethod>(m_adjustmentCombo->currentData().toInt());
    
    if (method == NoAdjustment) {
        // Copy unadjusted to adjusted
        for (auto& obs : m_observations) {
            obs.adjEasting = obs.easting;
            obs.adjNorthing = obs.northing;
        }
    } else if (method == Bowditch) {
        applyBowditchAdjustment();
    } else if (method == Transit) {
        applyTransitAdjustment();
    }
    
    // Update table
    m_updating = true;
    for (int row = 0; row < m_observations.size(); ++row) {
        const auto& obs = m_observations[row];
        m_table->item(row, COL_ADJ_EASTING)->setText(QString::number(obs.adjEasting, 'f', 3));
        m_table->item(row, COL_ADJ_NORTHING)->setText(QString::number(obs.adjNorthing, 'f', 3));
    }
    m_updating = false;
    
    QMessageBox::information(this, "Adjustment", "Adjustment applied successfully.");
}

void TraverseDialog::applyBowditchAdjustment()
{
    // Bowditch (Compass) Rule:
    // Correction proportional to distance traveled
    // correction_E_i = -(closure_E * cumulative_distance_i / total_distance)
    // correction_N_i = -(closure_N * cumulative_distance_i / total_distance)
    
    if (m_totalDistance < 0.001) return;
    
    double cumDistance = 0.0;
    
    // First point is fixed
    m_observations[0].adjEasting = m_observations[0].easting;
    m_observations[0].adjNorthing = m_observations[0].northing;
    
    for (int i = 1; i < m_observations.size(); ++i) {
        cumDistance += m_observations[i].distance;
        
        double corrE = -(m_closureEasting * cumDistance / m_totalDistance);
        double corrN = -(m_closureNorthing * cumDistance / m_totalDistance);
        
        m_observations[i].adjEasting = m_observations[i].easting + corrE;
        m_observations[i].adjNorthing = m_observations[i].northing + corrN;
    }
}

void TraverseDialog::applyTransitAdjustment()
{
    // Transit Rule:
    // Correction proportional to absolute values of latitude/departure
    // Used when angles are more precise than distances
    
    double sumAbsDep = 0.0;  // Sum of absolute departures (dE)
    double sumAbsLat = 0.0;  // Sum of absolute latitudes (dN)
    
    for (int i = 1; i < m_observations.size(); ++i) {
        double dE = m_observations[i].easting - m_observations[i-1].easting;
        double dN = m_observations[i].northing - m_observations[i-1].northing;
        sumAbsDep += qAbs(dE);
        sumAbsLat += qAbs(dN);
    }
    
    if (sumAbsDep < 0.001) sumAbsDep = 0.001;
    if (sumAbsLat < 0.001) sumAbsLat = 0.001;
    
    // First point is fixed
    m_observations[0].adjEasting = m_observations[0].easting;
    m_observations[0].adjNorthing = m_observations[0].northing;
    
    double cumCorrE = 0.0;
    double cumCorrN = 0.0;
    
    for (int i = 1; i < m_observations.size(); ++i) {
        double dE = m_observations[i].easting - m_observations[i-1].easting;
        double dN = m_observations[i].northing - m_observations[i-1].northing;
        
        double corrE = -(m_closureEasting * qAbs(dE) / sumAbsDep);
        double corrN = -(m_closureNorthing * qAbs(dN) / sumAbsLat);
        
        cumCorrE += corrE;
        cumCorrN += corrN;
        
        m_observations[i].adjEasting = m_observations[i].easting + cumCorrE;
        m_observations[i].adjNorthing = m_observations[i].northing + cumCorrN;
    }
}

void TraverseDialog::updateClosureDisplay()
{
    if (!m_calculated) {
        m_closureLabel->setText("Linear Closure: --");
        m_precisionLabel->setText("Precision: --");
        m_angularErrorLabel->setText("Angular Misclose: --");
        m_closureLabel->setStyleSheet("font-weight: bold; color: gray;");
        m_precisionLabel->setStyleSheet("font-weight: bold; color: gray;");
        return;
    }
    
    TraverseType type = static_cast<TraverseType>(m_traverseTypeCombo->currentData().toInt());
    
    if (type == OpenTraverse) {
        m_closureLabel->setText("Linear Closure: N/A (Open Traverse)");
        m_precisionLabel->setText("Precision: N/A");
        m_angularErrorLabel->setText("Angular Misclose: N/A");
        m_closureLabel->setStyleSheet("font-weight: bold; color: gray;");
        m_precisionLabel->setStyleSheet("font-weight: bold; color: gray;");
    } else {
        m_closureLabel->setText(QString("Linear Closure: %1m (dX=%2, dY=%3)")
            .arg(m_closureDistance, 0, 'f', 4)
            .arg(m_closureEasting, 0, 'f', 4)
            .arg(m_closureNorthing, 0, 'f', 4));
        
        // Precision ratio
        double precision = (m_closureDistance > 0.001) ? (m_totalDistance / m_closureDistance) : 999999;
        QString precisionStr = QString("1:%1").arg(qRound(precision));
        
        // Color based on precision
        QString color;
        if (precision > 10000) {
            color = "lime";
        } else if (precision > 5000) {
            color = "yellow";
        } else if (precision > 2500) {
            color = "orange";
        } else {
            color = "red";
        }
        
        m_closureLabel->setStyleSheet(QString("font-weight: bold; color: %1;").arg(color));
        m_precisionLabel->setText(QString("Precision: %1").arg(precisionStr));
        m_precisionLabel->setStyleSheet(QString("font-weight: bold; color: %1;").arg(color));
        
        if (type == ClosedLoop) {
            m_angularErrorLabel->setText(QString("Angular Misclose: %1\"")
                .arg(m_angularMisclose * 3600, 0, 'f', 1));
        }
    }
}

void TraverseDialog::sendToCanvas()
{
    if (!m_canvas) {
        QMessageBox::warning(this, "Error", "Canvas not available");
        return;
    }
    
    if (!m_calculated || m_observations.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please calculate the traverse first.");
        return;
    }
    
    // Use adjusted coordinates if available, otherwise use computed
    bool hasAdjusted = (m_adjustmentCombo->currentData().toInt() != NoAdjustment);
    
    int addedCount = 0;
    for (const auto& obs : m_observations) {
        double e = hasAdjusted ? obs.adjEasting : obs.easting;
        double n = hasAdjusted ? obs.adjNorthing : obs.northing;
        
        if (qAbs(e) > 0.001 || qAbs(n) > 0.001) {
            CanvasPeg peg;
            peg.name = obs.stationId;
            peg.position = QPointF(e, n);
            peg.color = Qt::red;
            m_canvas->addPeg(peg);
            addedCount++;
        }
    }
    
    // Also create a polyline connecting all points
    if (m_observations.size() >= 2) {
        CanvasPolyline line;
        for (const auto& obs : m_observations) {
            double e = hasAdjusted ? obs.adjEasting : obs.easting;
            double n = hasAdjusted ? obs.adjNorthing : obs.northing;
            line.points.append(QPointF(e, n));
        }
        
        TraverseType type = static_cast<TraverseType>(m_traverseTypeCombo->currentData().toInt());
        line.closed = (type == ClosedLoop);
        line.color = Qt::green;
        line.layer = "TRAVERSE";
        
        m_canvas->addPolyline(line);
    }
    
    QMessageBox::information(this, "Send to Canvas", 
        QString("Added %1 stations and traverse line to canvas.").arg(addedCount));
}

double TraverseDialog::normalizeAngle(double angle) const
{
    while (angle < 0.0) angle += 360.0;
    while (angle >= 360.0) angle -= 360.0;
    return angle;
}

double TraverseDialog::degreesToRadians(double deg) const
{
    return deg * M_PI / 180.0;
}

double TraverseDialog::radiansToDegrees(double rad) const
{
    return rad * 180.0 / M_PI;
}

QString TraverseDialog::formatBearing(double degrees) const
{
    degrees = normalizeAngle(degrees);
    return QString::number(degrees, 'f', 4) + "°";
}

QString TraverseDialog::formatDMS(double degrees) const
{
    bool negative = degrees < 0;
    degrees = qAbs(degrees);
    
    int d = static_cast<int>(degrees);
    double minFloat = (degrees - d) * 60.0;
    int m = static_cast<int>(minFloat);
    double s = (minFloat - m) * 60.0;
    
    QString result = QString("%1°%2'%3\"")
        .arg(d)
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 5, 'f', 2, QChar('0'));
    
    if (negative) result = "-" + result;
    return result;
}

void TraverseDialog::exportCSV()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Export Traverse CSV", "", "CSV Files (*.csv)");
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not save file.");
        return;
    }
    
    QTextStream out(&file);
    
    // Header
    QStringList headers;
    for (int i = 0; i < m_table->columnCount(); ++i) {
        headers << m_table->horizontalHeaderItem(i)->text();
    }
    out << headers.join(",") << "\n";
    
    // Data
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QStringList rowData;
        for (int col = 0; col < m_table->columnCount(); ++col) {
            QTableWidgetItem* item = m_table->item(row, col);
            rowData << (item ? item->text() : "");
        }
        out << rowData.join(",") << "\n";
    }
    
    file.close();
    QMessageBox::information(this, "Success", "Traverse data exported successfully.");
}

void TraverseDialog::generateReport()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save Report", "", "Text Files (*.txt)");
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not save file.");
        return;
    }
    
    QTextStream out(&file);
    out << "TRAVERSE CALCULATION REPORT\n";
    out << "===========================\n";
    out << "Date: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm") << "\n\n";
    
    out << "PARAMETERS:\n";
    out << "Start Station: " << m_startStationEdit->text() << "\n";
    out << "Start Coords: " << QString::number(m_startEasting->value(), 'f', 3) << " E, " 
        << QString::number(m_startNorthing->value(), 'f', 3) << " N\n";
    out << "Internal Bearings: " << (m_interiorAnglesCheck->isChecked() ? "Yes" : "No") << "\n";
    out << "Adjustment Method: " << m_adjustmentCombo->currentText() << "\n\n";
    
    out << "CLOSURE RESULTS:\n";
    if (m_calculated) {
        out << m_closureLabel->text() << "\n";
        out << m_precisionLabel->text() << "\n";
        out << m_angularErrorLabel->text() << "\n\n";
    } else {
        out << "Not calculated yet.\n\n";
    }
    
    out << "COORDINATES:\n";
    out << QString("%1 | %2 | %3 | %4 | %5\n")
        .arg("Station", 10).arg("Obs. E", 12).arg("Obs. N", 12).arg("Adj. E", 12).arg("Adj. N", 12);
    out << QString("-").repeated(65) << "\n";
    
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QString stn = m_table->item(row, COL_STATION)->text();
        QString rawE = m_table->item(row, COL_EASTING)->text();
        QString rawN = m_table->item(row, COL_NORTHING)->text();
        QString adjE = m_table->item(row, COL_ADJ_EASTING)->text();
        QString adjN = m_table->item(row, COL_ADJ_NORTHING)->text();
        
        out << QString("%1 | %2 | %3 | %4 | %5\n")
            .arg(stn, 10).arg(rawE, 12).arg(rawN, 12).arg(adjE, 12).arg(adjN, 12);
    }
    
    file.close();
    QMessageBox::information(this, "Success", "Report generated successfully.");
}
