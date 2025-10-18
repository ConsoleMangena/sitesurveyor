#include "layerpanel.h"
#include "layermanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QInputDialog>
#include <QColorDialog>
#include <QTableWidgetItem>

LayerPanel::LayerPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels(QStringList() << "Name" << "Color" << "Visible" << "Locked");
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);

    auto* buttons = new QHBoxLayout();
    m_addBtn = new QPushButton("Add", this);
    m_removeBtn = new QPushButton("Remove", this);
    m_renameBtn = new QPushButton("Rename", this);
    m_colorBtn = new QPushButton("Color", this);
    m_setCurrentBtn = new QPushButton("Set Current", this);

    buttons->addWidget(m_addBtn);
    buttons->addWidget(m_removeBtn);
    buttons->addWidget(m_renameBtn);
    buttons->addWidget(m_colorBtn);
    buttons->addStretch();
    buttons->addWidget(m_setCurrentBtn);
    layout->addLayout(buttons);

    connect(m_addBtn, &QPushButton::clicked, this, &LayerPanel::onAdd);
    connect(m_removeBtn, &QPushButton::clicked, this, &LayerPanel::onRemove);
    connect(m_renameBtn, &QPushButton::clicked, this, &LayerPanel::onRename);
    connect(m_colorBtn, &QPushButton::clicked, this, &LayerPanel::onSetColor);
    connect(m_setCurrentBtn, &QPushButton::clicked, this, &LayerPanel::onSetCurrent);
}

void LayerPanel::setLayerManager(LayerManager* lm)
{
    m_lm = lm;
    reload();
}

void LayerPanel::reload()
{
    if (!m_lm) return;
    m_table->setRowCount(0);
    const auto& ls = m_lm->layers();
    m_table->setRowCount(ls.size());
    for (int i = 0; i < ls.size(); ++i) {
        const Layer& L = ls[i];
        auto* nameItem = new QTableWidgetItem(L.name);
        m_table->setItem(i, 0, nameItem);

        auto* colorItem = new QTableWidgetItem("");
        colorItem->setBackground(L.color);
        m_table->setItem(i, 1, colorItem);

        auto* visItem = new QTableWidgetItem("");
        visItem->setCheckState(L.visible ? Qt::Checked : Qt::Unchecked);
        visItem->setFlags(visItem->flags() | Qt::ItemIsUserCheckable);
        m_table->setItem(i, 2, visItem);

        auto* lockItem = new QTableWidgetItem("");
        lockItem->setCheckState(L.locked ? Qt::Checked : Qt::Unchecked);
        lockItem->setFlags(lockItem->flags() | Qt::ItemIsUserCheckable);
        m_table->setItem(i, 3, lockItem);
    }
}

QString LayerPanel::layerNameAtRow(int row) const
{
    if (row < 0 || row >= m_table->rowCount()) return QString();
    auto* item = m_table->item(row, 0);
    return item ? item->text() : QString();
}

void LayerPanel::onAdd()
{
    if (!m_lm) return;
    bool ok=false;
    QString name = QInputDialog::getText(this, "Add Layer", "Layer name:", QLineEdit::Normal, "Layer1", &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    if (!m_lm->addLayer(name)) return;
    reload();
}

void LayerPanel::onRemove()
{
    if (!m_lm) return;
    int row = m_table->currentRow();
    QString name = layerNameAtRow(row);
    if (name.isEmpty()) return;
    if (!m_lm->removeLayer(name)) return;
    reload();
}

void LayerPanel::onRename()
{
    if (!m_lm) return;
    int row = m_table->currentRow();
    QString oldName = layerNameAtRow(row);
    if (oldName.isEmpty()) return;
    bool ok=false;
    QString newName = QInputDialog::getText(this, "Rename Layer", "New name:", QLineEdit::Normal, oldName, &ok);
    if (!ok || newName.trimmed().isEmpty()) return;
    if (!m_lm->renameLayer(oldName, newName)) return;
    reload();
}

void LayerPanel::onSetColor()
{
    if (!m_lm) return;
    int row = m_table->currentRow();
    QString name = layerNameAtRow(row);
    if (name.isEmpty()) return;
    QColor c = QColorDialog::getColor(Qt::white, this, QString("Layer %1 color").arg(name));
    if (!c.isValid()) return;
    if (!m_lm->setLayerColor(name, c)) return;
    reload();
}

void LayerPanel::onSetCurrent()
{
    if (!m_lm) return;
    int row = m_table->currentRow();
    QString name = layerNameAtRow(row);
    if (name.isEmpty()) return;
    if (!m_lm->setCurrentLayer(name)) return;
    emit requestSetCurrent(name);
}

void LayerPanel::onCellChanged(int row, int col)
{
    Q_UNUSED(row); Q_UNUSED(col);
    // Not used (we handle with buttons)
}
