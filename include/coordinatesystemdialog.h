#ifndef COORDINATESYSTEMDIALOG_H
#define COORDINATESYSTEMDIALOG_H

#include <QDialog>

class QLineEdit;
class QComboBox;
class QLabel;

class CoordinateSystemDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CoordinateSystemDialog(QWidget* parent = nullptr);

private slots:
    void accept() override;

private:
    void loadSettings();
    void updateSummary();

    QLineEdit* m_codeEdit{nullptr};
    QLineEdit* m_nameEdit{nullptr};
    QLineEdit* m_datumEdit{nullptr};
    QLineEdit* m_projectionEdit{nullptr};
    QComboBox* m_unitsCombo{nullptr};
    QLabel* m_summaryLabel{nullptr};
};

#endif // COORDINATESYSTEMDIALOG_H
