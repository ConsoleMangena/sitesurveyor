#ifndef POLARINPUTDIALOG_H
#define POLARINPUTDIALOG_H

#include <QDialog>

class QComboBox;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QCheckBox;
class QLabel;
class PointManager;
class CanvasWidget;
class Point;

class PolarInputDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PolarInputDialog(PointManager* pm, CanvasWidget* canvas, QWidget* parent = nullptr);

public slots:
    void reload();

private slots:
    void compute();
    void addCoordinate();

private:
    bool parseInputs(Point& from, QString& newName, double& distance, double& azimuthDeg, double& z, QString& err) const;
    static double parseAngleDMS(const QString& text, bool* ok);
    QString formatResult(const Point& from, const Point& to, double distance, double azimuthDeg, double z) const;

    PointManager* m_pm;
    CanvasWidget* m_canvas;

    QComboBox* m_fromBox;
    QLineEdit* m_nameEdit;
    QLineEdit* m_distanceEdit;
    QLineEdit* m_azimuthEdit;
    QLineEdit* m_zEdit;
    QCheckBox* m_drawLineCheck;
    QLabel* m_zLabel;
    QLabel* m_hintLabel;
    QTextEdit* m_output;
    QPushButton* m_computeBtn;
    QPushButton* m_addBtn;
    QPushButton* m_closeBtn;
};

#endif // POLARINPUTDIALOG_H
