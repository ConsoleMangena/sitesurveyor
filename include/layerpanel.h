#ifndef LAYERPANEL_H
#define LAYERPANEL_H

#include <QWidget>
#include <QColor>

class QTableWidget;
class QPushButton;
class QComboBox;
class LayerManager;

class LayerPanel : public QWidget
{
    Q_OBJECT
public:
    explicit LayerPanel(QWidget* parent = nullptr);
    void setLayerManager(LayerManager* lm);
    void reload();

signals:
    void requestSetCurrent(const QString& name);

private slots:
    void onAdd();
    void onRemove();
    void onRename();
    void onSetColor();
    void onSetCurrent();
    void onCellChanged(int row, int col);

private:
    QString layerNameAtRow(int row) const;
    void populateTable();

    LayerManager* m_lm{nullptr};
    QTableWidget* m_table{nullptr};
    QPushButton* m_addBtn{nullptr};
    QPushButton* m_removeBtn{nullptr};
    QPushButton* m_renameBtn{nullptr};
    QPushButton* m_colorBtn{nullptr};
    QPushButton* m_setCurrentBtn{nullptr};
    bool m_blockUpdates{false};
};

#endif // LAYERPANEL_H
