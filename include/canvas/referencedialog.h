#ifndef REFERENCEDIALOG_H
#define REFERENCEDIALOG_H

#include <QDialog>
#include <QDoubleSpinBox>
#include <QLineEdit>

class ReferenceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReferenceDialog(QWidget *parent = nullptr);

    double latitude() const;
    double longitude() const;
    double localX() const;
    double localY() const;

    void setLatitude(double lat);
    void setLongitude(double lon);
    void setLocalX(double x);
    void setLocalY(double y);

private:
    QDoubleSpinBox* m_latSpin;
    QDoubleSpinBox* m_lonSpin;
    QDoubleSpinBox* m_localXSpin;
    QDoubleSpinBox* m_localYSpin;

    void setupUI();
};

#endif // REFERENCEDIALOG_H
