#include "tools/levellingdialog.h"
#include "canvas/canvaswidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QtMath>

LevellingDialog::LevellingDialog(CanvasWidget* canvas, QWidget* parent)
    : QDialog(parent), m_canvas(canvas)
{
    setWindowTitle("Levelling Tool");
    resize(900, 600);
    setupUI();
}

void LevellingDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // ========== Top Controls ==========
    QHBoxLayout* topLayout = new QHBoxLayout();
    
    QLabel* startLabel = new QLabel("Start RL:");
    m_startRL = new QDoubleSpinBox();
    m_startRL->setRange(-1000, 10000);
    m_startRL->setDecimals(3);
    m_startRL->setValue(100.000);
    m_startRL->setSuffix(" m");
    connect(m_startRL, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &LevellingDialog::recalculateAll);
    
    QLabel* methodLabel = new QLabel("Method:");
    m_methodCombo = new QComboBox();
    m_methodCombo->addItem("Rise & Fall");
    m_methodCombo->addItem("HPC (Height of Collimation)");
    connect(m_methodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LevellingDialog::methodChanged);
    
    m_adjustBtn = new QPushButton("Adjust Loop");
    connect(m_adjustBtn, &QPushButton::clicked, this, &LevellingDialog::adjustLoop);
    
    topLayout->addWidget(startLabel);
    topLayout->addWidget(m_startRL);
    topLayout->addSpacing(20);
    topLayout->addWidget(methodLabel);
    topLayout->addWidget(m_methodCombo);
    topLayout->addStretch();
    topLayout->addWidget(m_adjustBtn);
    
    mainLayout->addLayout(topLayout);
    
    // ========== Table ==========
    m_table = new QTableWidget(0, COL_COUNT);
    m_table->setHorizontalHeaderLabels({
        "Point", "BS", "IS", "FS", "Rise", "Fall", "HPC", "RL", "Adj RL", "Remarks"
    });
    
    m_table->horizontalHeader()->setSectionResizeMode(COL_POINT, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(COL_REMARKS, QHeaderView::Stretch);
    for (int i = COL_BS; i <= COL_ADJ_RL; ++i) {
        m_table->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
        m_table->setColumnWidth(i, 70);
    }
    
    connect(m_table, &QTableWidget::cellChanged, this, &LevellingDialog::onCellChanged);
    
    mainLayout->addWidget(m_table);
    
    // ========== Check Bar ==========
    QGroupBox* checkGroup = new QGroupBox("Arithmetic Check");
    QHBoxLayout* checkLayout = new QHBoxLayout(checkGroup);
    
    m_checkLabel = new QLabel("ΣBS - ΣFS = 0.000  |  ΣRise - ΣFall = 0.000  |  Last RL - First RL = 0.000");
    m_checkLabel->setStyleSheet("font-family: monospace;");
    
    m_statusLabel = new QLabel("Status: --");
    m_statusLabel->setStyleSheet("font-weight: bold; color: gray;");
    
    checkLayout->addWidget(m_checkLabel);
    checkLayout->addStretch();
    checkLayout->addWidget(m_statusLabel);
    
    mainLayout->addWidget(checkGroup);
    
    // ========== Buttons ==========
    QHBoxLayout* btnLayout = new QHBoxLayout();
    
    QPushButton* addBtn = new QPushButton("Add Row");
    connect(addBtn, &QPushButton::clicked, this, &LevellingDialog::addRow);
    
    QPushButton* delBtn = new QPushButton("Delete Row");
    connect(delBtn, &QPushButton::clicked, this, &LevellingDialog::deleteRow);
    
    m_sendBtn = new QPushButton("Send to Map");
    connect(m_sendBtn, &QPushButton::clicked, this, &LevellingDialog::sendToMap);
    
    QPushButton* closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(delBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_sendBtn);
    btnLayout->addWidget(closeBtn);
    
    mainLayout->addLayout(btnLayout);
    
    // Add initial rows
    for (int i = 0; i < 5; ++i) {
        addRow();
    }
}

void LevellingDialog::addRow()
{
    m_recalculating = true;
    int row = m_table->rowCount();
    m_table->insertRow(row);
    
    // Point ID
    QTableWidgetItem* pointItem = new QTableWidgetItem(QString("P-%1").arg(row + 1));
    m_table->setItem(row, COL_POINT, pointItem);
    
    // Editable cells: BS, IS, FS
    for (int col : {COL_BS, COL_IS, COL_FS}) {
        QTableWidgetItem* item = new QTableWidgetItem("");
        m_table->setItem(row, col, item);
    }
    
    // Read-only cells: Rise, Fall, HPC, RL, Adj RL
    for (int col : {COL_RISE, COL_FALL, COL_HPC, COL_RL, COL_ADJ_RL}) {
        QTableWidgetItem* item = new QTableWidgetItem("");
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setBackground(QBrush(QColor(240, 240, 240)));
        m_table->setItem(row, col, item);
    }
    
    // Remarks
    QTableWidgetItem* remarkItem = new QTableWidgetItem("");
    m_table->setItem(row, COL_REMARKS, remarkItem);
    
    m_recalculating = false;
}

void LevellingDialog::deleteRow()
{
    int row = m_table->currentRow();
    if (row >= 0) {
        m_table->removeRow(row);
        recalculateAll();
    }
}

void LevellingDialog::onCellChanged(int row, int column)
{
    if (m_recalculating) return;
    
    // Only recalculate when BS, IS, or FS changes
    if (column == COL_BS || column == COL_IS || column == COL_FS) {
        recalculateAll();
    }
}

void LevellingDialog::methodChanged(int index)
{
    recalculateAll();
}

double LevellingDialog::getTableValue(int row, int col) const
{
    QTableWidgetItem* item = m_table->item(row, col);
    if (!item || item->text().isEmpty()) return 0.0;
    bool ok;
    double val = item->text().toDouble(&ok);
    return ok ? val : 0.0;
}

void LevellingDialog::setTableValue(int row, int col, double value)
{
    QTableWidgetItem* item = m_table->item(row, col);
    if (item) {
        item->setText(QString::number(value, 'f', 3));
    }
}

void LevellingDialog::setTableReadOnly(int row, int col, double value)
{
    QTableWidgetItem* item = m_table->item(row, col);
    if (item) {
        if (qAbs(value) < 0.0001) {
            item->setText("");
        } else {
            item->setText(QString::number(value, 'f', 3));
        }
    }
}

void LevellingDialog::recalculateAll()
{
    m_recalculating = true;
    
    if (m_methodCombo->currentIndex() == 0) {
        calculateRiseFall();
    } else {
        calculateHPC();
    }
    
    updateCheckBar();
    m_recalculating = false;
}

void LevellingDialog::calculateRiseFall()
{
    double currentRL = m_startRL->value();
    double currentHPC = 0.0;
    double prevReading = 0.0;
    bool firstSetup = true;
    
    for (int row = 0; row < m_table->rowCount(); ++row) {
        double bs = getTableValue(row, COL_BS);
        double is = getTableValue(row, COL_IS);
        double fs = getTableValue(row, COL_FS);
        
        double rise = 0.0, fall = 0.0;
        
        if (bs > 0.0001) {
            // Backsight - new instrument setup
            if (firstSetup) {
                // First setup - use start RL
                currentHPC = currentRL + bs;
                firstSetup = false;
            } else {
                // Continuing setup - calculate from previous FS
                currentHPC = currentRL + bs;
            }
            prevReading = bs;
            
            setTableReadOnly(row, COL_RISE, 0);
            setTableReadOnly(row, COL_FALL, 0);
            setTableReadOnly(row, COL_HPC, currentHPC);
            setTableReadOnly(row, COL_RL, currentRL);
            
        } else if (fs > 0.0001) {
            // Foresight - end of setup
            if (prevReading > fs) {
                rise = prevReading - fs;
            } else {
                fall = fs - prevReading;
            }
            
            currentRL = currentRL + rise - fall;
            prevReading = fs;
            
            setTableReadOnly(row, COL_RISE, rise);
            setTableReadOnly(row, COL_FALL, fall);
            setTableReadOnly(row, COL_HPC, 0);
            setTableReadOnly(row, COL_RL, currentRL);
            
        } else if (is > 0.0001) {
            // Intermediate Sight
            if (prevReading > is) {
                rise = prevReading - is;
            } else {
                fall = is - prevReading;
            }
            
            double tempRL = currentRL + rise - fall;
            currentRL = tempRL;
            prevReading = is;
            
            setTableReadOnly(row, COL_RISE, rise);
            setTableReadOnly(row, COL_FALL, fall);
            setTableReadOnly(row, COL_HPC, 0);
            setTableReadOnly(row, COL_RL, currentRL);
            
        } else {
            // Empty row
            setTableReadOnly(row, COL_RISE, 0);
            setTableReadOnly(row, COL_FALL, 0);
            setTableReadOnly(row, COL_HPC, 0);
            setTableReadOnly(row, COL_RL, 0);
        }
        
        // Clear adjusted RL
        setTableReadOnly(row, COL_ADJ_RL, 0);
    }
}

void LevellingDialog::calculateHPC()
{
    double currentRL = m_startRL->value();
    double currentHPC = 0.0;
    bool hasHPC = false;
    
    for (int row = 0; row < m_table->rowCount(); ++row) {
        double bs = getTableValue(row, COL_BS);
        double is = getTableValue(row, COL_IS);
        double fs = getTableValue(row, COL_FS);
        
        if (bs > 0.0001) {
            // Backsight - new HPC
            currentHPC = currentRL + bs;
            hasHPC = true;
            
            setTableReadOnly(row, COL_RISE, 0);
            setTableReadOnly(row, COL_FALL, 0);
            setTableReadOnly(row, COL_HPC, currentHPC);
            setTableReadOnly(row, COL_RL, currentRL);
            
        } else if (fs > 0.0001) {
            // Foresight
            if (hasHPC) {
                currentRL = currentHPC - fs;
            }
            
            setTableReadOnly(row, COL_RISE, 0);
            setTableReadOnly(row, COL_FALL, 0);
            setTableReadOnly(row, COL_HPC, 0);
            setTableReadOnly(row, COL_RL, currentRL);
            
        } else if (is > 0.0001) {
            // Intermediate Sight - RL = HPC - IS
            double rl = hasHPC ? (currentHPC - is) : 0.0;
            
            setTableReadOnly(row, COL_RISE, 0);
            setTableReadOnly(row, COL_FALL, 0);
            setTableReadOnly(row, COL_HPC, 0);
            setTableReadOnly(row, COL_RL, rl);
            
        } else {
            setTableReadOnly(row, COL_RISE, 0);
            setTableReadOnly(row, COL_FALL, 0);
            setTableReadOnly(row, COL_HPC, 0);
            setTableReadOnly(row, COL_RL, 0);
        }
        
        setTableReadOnly(row, COL_ADJ_RL, 0);
    }
}

void LevellingDialog::updateCheckBar()
{
    double sumBS = 0.0, sumFS = 0.0;
    double sumRise = 0.0, sumFall = 0.0;
    double firstRL = m_startRL->value();
    double lastRL = firstRL;
    
    for (int row = 0; row < m_table->rowCount(); ++row) {
        sumBS += getTableValue(row, COL_BS);
        sumFS += getTableValue(row, COL_FS);
        sumRise += getTableValue(row, COL_RISE);
        sumFall += getTableValue(row, COL_FALL);
        
        double rl = getTableValue(row, COL_RL);
        if (rl > 0.0001 || rl < -0.0001) {
            lastRL = rl;
        }
    }
    
    double check1 = sumBS - sumFS;
    double check2 = sumRise - sumFall;
    double check3 = lastRL - firstRL;
    
    m_checkLabel->setText(QString("ΣBS - ΣFS = %1  |  ΣRise - ΣFall = %2  |  Last RL - First RL = %3")
        .arg(check1, 0, 'f', 3)
        .arg(check2, 0, 'f', 3)
        .arg(check3, 0, 'f', 3));
    
    // Check if all three values match (within tolerance)
    bool ok = qAbs(check1 - check2) < 0.002 && qAbs(check1 - check3) < 0.002;
    
    if (sumBS < 0.001 && sumFS < 0.001) {
        m_statusLabel->setText("Status: No data");
        m_statusLabel->setStyleSheet("font-weight: bold; color: gray;");
    } else if (ok) {
        m_statusLabel->setText("Status: ✓ Checks OK");
        m_statusLabel->setStyleSheet("font-weight: bold; color: green;");
    } else {
        m_statusLabel->setText("Status: ✗ Check Failed");
        m_statusLabel->setStyleSheet("font-weight: bold; color: red;");
    }
}

void LevellingDialog::adjustLoop()
{
    // Find first and last RL
    double startRL = m_startRL->value();
    double endRL = startRL;
    int numPoints = 0;
    
    for (int row = 0; row < m_table->rowCount(); ++row) {
        double rl = getTableValue(row, COL_RL);
        if (qAbs(rl) > 0.0001) {
            endRL = rl;
            numPoints++;
        }
    }
    
    if (numPoints < 2) {
        QMessageBox::information(this, "Adjust Loop", "Need at least 2 points with RL values.");
        return;
    }
    
    // Calculate misclosure (assuming loop returns to start)
    double misclosure = endRL - startRL;
    
    QString msg = QString("Misclosure: %1 m\n\n"
                          "This will distribute the error of %1 m across %2 points.\n\n"
                          "Continue?")
        .arg(misclosure, 0, 'f', 4)
        .arg(numPoints);
    
    if (QMessageBox::question(this, "Adjust Loop", msg) != QMessageBox::Yes) {
        return;
    }
    
    // Distribute error proportionally
    m_recalculating = true;
    int pointNum = 0;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        double rl = getTableValue(row, COL_RL);
        if (qAbs(rl) > 0.0001) {
            pointNum++;
            double correction = -misclosure * (static_cast<double>(pointNum) / numPoints);
            double adjRL = rl + correction;
            setTableReadOnly(row, COL_ADJ_RL, adjRL);
        }
    }
    m_recalculating = false;
    
    QMessageBox::information(this, "Adjustment Complete", 
        QString("Applied adjustment: %1 m per point\n"
                "Total correction: %2 m")
            .arg(-misclosure / numPoints, 0, 'f', 4)
            .arg(-misclosure, 0, 'f', 4));
}

void LevellingDialog::sendToMap()
{
    if (!m_canvas) {
        QMessageBox::warning(this, "Error", "No canvas available.");
        return;
    }
    
    int addedCount = 0;
    double xOffset = 0.0;
    
    for (int row = 0; row < m_table->rowCount(); ++row) {
        // Only send IS points (ground details)
        double is = getTableValue(row, COL_IS);
        if (is < 0.0001) continue;
        
        // Get point name
        QTableWidgetItem* pointItem = m_table->item(row, COL_POINT);
        QString pointName = pointItem ? pointItem->text() : QString("LVL-%1").arg(row);
        
        // Get RL (use adjusted if available)
        double rl = getTableValue(row, COL_ADJ_RL);
        if (qAbs(rl) < 0.0001) {
            rl = getTableValue(row, COL_RL);
        }
        
        // Create peg at offset position (since we don't have X,Y coordinates)
        // In real use, these would be linked to existing survey points
        CanvasPeg peg;
        peg.name = pointName;
        peg.position = QPointF(xOffset, rl * 10);  // Scale Z for visibility
        m_canvas->addPeg(peg);
        xOffset += 5.0;  // Space out points
        addedCount++;
    }
    
    if (addedCount > 0) {
        QMessageBox::information(this, "Send to Map", 
            QString("Added %1 points to the map.\n\n"
                    "Note: Points are placed along X-axis since no XY coordinates are available.\n"
                    "Y-coordinate represents the Reduced Level (scaled by 10 for visibility).")
                .arg(addedCount));
    } else {
        QMessageBox::information(this, "Send to Map", 
            "No Intermediate Sight (IS) points found to export.");
    }
}
