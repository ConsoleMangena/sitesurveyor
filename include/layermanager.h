#ifndef LAYERMANAGER_H
#define LAYERMANAGER_H

#include <QObject>
#include <QString>
#include <QColor>
#include <QVector>

struct Layer {
    QString name;
    QColor color;
    bool visible;
    bool locked;
};

class LayerManager : public QObject {
    Q_OBJECT
public:
    explicit LayerManager(QObject* parent = nullptr);

    // Basic API
    bool addLayer(const QString& name, const QColor& color = QColor(200,200,200));
    bool removeLayer(const QString& name);
    bool renameLayer(const QString& oldName, const QString& newName);
    bool setLayerColor(const QString& name, const QColor& color);
    bool setLayerVisible(const QString& name, bool visible);
    bool setLayerLocked(const QString& name, bool locked);

    bool hasLayer(const QString& name) const;
    Layer getLayer(const QString& name) const; // returns default if missing
    const QVector<Layer>& layers() const { return m_layers; }

    // Current layer
    QString currentLayer() const { return m_currentLayer; }
    bool setCurrentLayer(const QString& name);

signals:
    void layersChanged();
    void currentLayerChanged(const QString& name);

private:
    int indexOf(const QString& name) const;
    QVector<Layer> m_layers;
    QString m_currentLayer;
};

#endif // LAYERMANAGER_H
