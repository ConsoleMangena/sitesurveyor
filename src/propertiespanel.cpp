#include "propertiespanel.h"
#include "layermanager.h"
#include "canvaswidget.h"
#include "surveycalculator.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPixmap>

PropertiesPanel::PropertiesPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // Type row
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("Type:"));
        m_typeLabel = new QLabel("-");
        row->addWidget(m_typeLabel);
        row->addStretch(1);
        layout->addLayout(row);
    }
    // Layer row
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("Layer:"));
        m_layerCombo = new QComboBox(this);
        row->addWidget(m_layerCombo);
        row->addStretch(1);
        layout->addLayout(row);
        connect(m_layerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PropertiesPanel::onLayerComboChanged);
    }
    // Length row
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("Length:"));
        m_lenLabel = new QLabel("-");
        row->addWidget(m_lenLabel);
        row->addStretch(1);
        layout->addLayout(row);
    }
    // Azimuth row
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel("Azimuth:"));
        m_azLabel = new QLabel("-");
        row->addWidget(m_azLabel);
        row->addStretch(1);
        layout->addLayout(row);
    }

    setLayout(layout);
}

void PropertiesPanel::setLayerManager(LayerManager* lm)
{
    m_lm = lm;
    refreshLayerCombo();
    if (m_lm) {
        connect(m_lm, &LayerManager::layersChanged, this, &PropertiesPanel::onLayersChanged);
    }
}

void PropertiesPanel::setCanvas(CanvasWidget* canvas)
{
    m_canvas = canvas;
    if (m_canvas) {
        connect(m_canvas, &CanvasWidget::selectedLineChanged, this, &PropertiesPanel::onSelectedLineChanged);
    }
}

void PropertiesPanel::onSelectedLineChanged(int index)
{
    m_currentLine = index;
    refreshLayerCombo();
    updateLineInfo();
}

void PropertiesPanel::onLayersChanged()
{
    refreshLayerCombo();
}

void PropertiesPanel::refreshLayerCombo()
{
    if (!m_layerCombo) return;
    m_layerCombo->blockSignals(true);
    m_layerCombo->clear();
    if (m_lm) {
        for (const auto& L : m_lm->layers()) {
            QPixmap pm(12, 12); pm.fill(L.color);
            m_layerCombo->addItem(QIcon(pm), L.name);
        }
    }
    const bool hasSel = m_canvas && m_currentLine >= 0;
    // Always enable: when no selection, this controls the current layer
    m_layerCombo->setEnabled(true);
    QString current;
    if (hasSel && m_canvas) current = m_canvas->lineLayer(m_currentLine);
    else if (m_lm) current = m_lm->currentLayer();
    if (!current.isEmpty()) {
        int idx = m_layerCombo->findText(current, Qt::MatchFixedString);
        if (idx >= 0) m_layerCombo->setCurrentIndex(idx);
    }
    m_layerCombo->blockSignals(false);
}

void PropertiesPanel::updateLineInfo()
{
    if (!m_canvas || m_currentLine < 0) {
        m_typeLabel->setText("-");
        m_lenLabel->setText("-");
        m_azLabel->setText("-");
        return;
    }
    QPointF a, b;
    if (!m_canvas->lineEndpoints(m_currentLine, a, b)) {
        m_typeLabel->setText("-");
        m_lenLabel->setText("-");
        m_azLabel->setText("-");
        return;
    }
    m_typeLabel->setText("Line");
    const double len = SurveyCalculator::distance(a, b);
    const double az = SurveyCalculator::azimuth(a, b);
    m_lenLabel->setText(QString("%1 m").arg(len, 0, 'f', 3));
    m_azLabel->setText(QString("%1°").arg(az, 0, 'f', 4));
}

void PropertiesPanel::onLayerComboChanged(int idx)
{
    if (!m_lm) return;
    const QString name = m_layerCombo->itemText(idx);
    if (name.isEmpty()) return;
    // If a line is selected, change its layer; otherwise, set current drawing layer
    if (m_canvas && m_currentLine >= 0) {
        m_canvas->setLineLayer(m_currentLine, name);
        updateLineInfo();
    } else {
        m_lm->setCurrentLayer(name);
    }
}
