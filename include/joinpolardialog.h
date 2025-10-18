#ifndef JOINPOLARDIALOG_H
#define JOINPOLARDIALOG_H

#include <QDialog>

class QComboBox;
class QPushButton;
class QTextEdit;
class QCheckBox;
class PointManager;
class CanvasWidget;
class Point;

class JoinPolarDialog : public QDialog
{
    Q_OBJECT
public:
    explicit JoinPolarDialog(PointManager* pm, CanvasWidget* canvas, QWidget* parent = nullptr);

public slots:
    void reload();

private slots:
    void compute();

private:
    QString formatJoinResult(const Point& from, const Point& to) const;

    PointManager* m_pm;
    CanvasWidget* m_canvas;

    QComboBox* m_fromBox;
    QComboBox* m_toBox;
    QCheckBox* m_drawLineCheck;
    QTextEdit* m_output;
    QPushButton* m_computeBtn;
    QPushButton* m_closeBtn;
};

#endif // JOINPOLARDIALOG_H
