#include "layermanager.h"

LayerManager::LayerManager(QObject* parent)
    : QObject(parent)
{
    // Default layer "0"
    m_layers.push_back(Layer{"0", QColor(200,200,200), true, false});
    m_currentLayer = "0";
}

int LayerManager::indexOf(const QString& name) const
{
    for (int i = 0; i < m_layers.size(); ++i) {
        if (m_layers[i].name.compare(name, Qt::CaseInsensitive) == 0) return i;
    }
    return -1;
}

bool LayerManager::addLayer(const QString& name, const QColor& color)
{
    if (name.trimmed().isEmpty()) return false;
    if (hasLayer(name)) return false;
    m_layers.push_back(Layer{name.trimmed(), color, true, false});
    emit layersChanged();
    return true;
}

bool LayerManager::removeLayer(const QString& name)
{
    if (name == "0") return false; // cannot remove default layer
    int idx = indexOf(name);
    if (idx < 0) return false;
    // If removing current layer, fallback to 0
    if (m_currentLayer.compare(name, Qt::CaseInsensitive) == 0) {
        m_currentLayer = "0";
        emit currentLayerChanged(m_currentLayer);
    }
    m_layers.remove(idx);
    emit layersChanged();
    return true;
}

bool LayerManager::renameLayer(const QString& oldName, const QString& newName)
{
    if (oldName == "0") return false; // do not rename default layer
    int idx = indexOf(oldName);
    if (idx < 0) return false;
    if (newName.trimmed().isEmpty()) return false;
    if (hasLayer(newName) && oldName.compare(newName, Qt::CaseInsensitive) != 0) return false;
    m_layers[idx].name = newName.trimmed();
    if (m_currentLayer.compare(oldName, Qt::CaseInsensitive) == 0) {
        m_currentLayer = m_layers[idx].name;
        emit currentLayerChanged(m_currentLayer);
    }
    emit layersChanged();
    return true;
}

bool LayerManager::setLayerColor(const QString& name, const QColor& color)
{
    int idx = indexOf(name);
    if (idx < 0) return false;
    m_layers[idx].color = color;
    emit layersChanged();
    return true;
}

bool LayerManager::setLayerVisible(const QString& name, bool visible)
{
    int idx = indexOf(name);
    if (idx < 0) return false;
    m_layers[idx].visible = visible;
    emit layersChanged();
    return true;
}

bool LayerManager::setLayerLocked(const QString& name, bool locked)
{
    int idx = indexOf(name);
    if (idx < 0) return false;
    m_layers[idx].locked = locked;
    emit layersChanged();
    return true;
}

bool LayerManager::hasLayer(const QString& name) const
{
    return indexOf(name) >= 0;
}

Layer LayerManager::getLayer(const QString& name) const
{
    int idx = indexOf(name);
    if (idx < 0) return Layer{"", QColor(200,200,200), true, false};
    return m_layers[idx];
}

bool LayerManager::setCurrentLayer(const QString& name)
{
    if (!hasLayer(name)) return false;
    if (m_currentLayer.compare(name, Qt::CaseInsensitive) == 0) return true;
    m_currentLayer = name;
    emit currentLayerChanged(m_currentLayer);
    return true;
}
