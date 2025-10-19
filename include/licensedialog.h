#ifndef LICENSEDIALOG_H
#define LICENSEDIALOG_H

#include <QDialog>

class QLineEdit;
class QCheckBox;
class QPushButton;
class QLabel;
class QComboBox;

class LicenseDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LicenseDialog(QWidget* parent = nullptr);
    void reload();

signals:
    void licenseSaved();
    void disciplineChanged();

private slots:
    void saveKey();
    void toggleShow(bool on);
    void onDisciplineChanged(int index);

private:
    QLineEdit* m_keyEdit{nullptr};
    QCheckBox* m_showCheck{nullptr};
    QPushButton* m_saveButton{nullptr};
    QPushButton* m_closeButton{nullptr};
    QLabel* m_statusLabel{nullptr};
    QComboBox* m_discCombo{nullptr};
};

#endif // LICENSEDIALOG_H
