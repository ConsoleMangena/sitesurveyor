#ifndef PROPERTIESPANEL_H
#define PROPERTIESPANEL_H

#include <QWidget>
#include <QPointF>

class QLabel;
class QComboBox;
class LayerManager;
class CanvasWidget;

class PropertiesPanel : public QWidget {
    Q_OBJECT
public:
    explicit PropertiesPanel(QWidget* parent = nullptr);
    void setLayerManager(LayerManager* lm);
    void setCanvas(CanvasWidget* canvas);

public slots:
    void onSelectedLineChanged(int index);
    void onLayersChanged();

private slots:
    void onLayerComboChanged(int idx);

private:
    void refreshLayerCombo();
    void updateLineInfo();

    LayerManager* m_lm{nullptr};
    CanvasWidget* m_canvas{nullptr};

    QLabel* m_typeLabel{nullptr};
    QLabel* m_lenLabel{nullptr};
    QLabel* m_azLabel{nullptr};
    QComboBox* m_layerCombo{nullptr};

    int m_currentLine{-1};
};

#endif // PROPERTIESPANEL_H
