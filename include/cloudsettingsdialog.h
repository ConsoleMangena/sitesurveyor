#ifndef CLOUDSETTINGSDIALOG_H
#define CLOUDSETTINGSDIALOG_H

#include <QDialog>

class QLineEdit;
class QCheckBox;
class QPushButton;

class CloudSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit CloudSettingsDialog(QWidget* parent = nullptr);

private slots:
    void saveAndClose();

private:
    void buildUi();
    void loadValues();

    QLineEdit* m_endpoint{nullptr};
    QLineEdit* m_projectId{nullptr};
    QCheckBox* m_selfSigned{nullptr};
    QPushButton* m_saveBtn{nullptr};
};

#endif
