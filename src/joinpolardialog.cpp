#include "joinpolardialog.h"
#include "pointmanager.h"
#include "canvaswidget.h"
#include "point.h"
#include "surveycalculator.h"
#include "appsettings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QCheckBox>
#include <QMessageBox>

static QString toQuadrantBearingLocal(double azDegrees)
{
    double az = SurveyCalculator::normalizeAngle(azDegrees);
    QString ns, ew;
    double theta;
    if (az >= 0 && az < 90) {
        ns = "N"; ew = "E"; theta = az;
    } else if (az >= 90 && az < 180) {
        ns = "S"; ew = "E"; theta = 180 - az;
    } else if (az >= 180 && az < 270) {
        ns = "S"; ew = "W"; theta = az - 180;
    } else {
        ns = "N"; ew = "W"; theta = 360 - az;
    }
    return QString("%1 %2 %3").arg(ns).arg(SurveyCalculator::toDMS(theta)).arg(ew);
}

JoinPolarDialog::JoinPolarDialog(PointManager* pm, CanvasWidget* canvas, QWidget* parent)
    : QDialog(parent)
    , m_pm(pm)
    , m_canvas(canvas)
{
    setWindowTitle("Join (Polar) Calculation");
    setModal(false);
    setMinimumSize(520, 400);

    auto *layout = new QVBoxLayout(this);

    auto *row = new QHBoxLayout();
    row->addWidget(new QLabel("From:"));
    m_fromBox = new QComboBox(this);
    row->addWidget(m_fromBox, 1);
    row->addSpacing(12);
    row->addWidget(new QLabel("To:"));
    m_toBox = new QComboBox(this);
    row->addWidget(m_toBox, 1);
    layout->addLayout(row);

    m_drawLineCheck = new QCheckBox("Draw line on canvas", this);
    m_drawLineCheck->setChecked(true);
    layout->addWidget(m_drawLineCheck);

    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setMinimumHeight(220);
    layout->addWidget(m_output, 1);

    auto *buttons = new QHBoxLayout();
    m_computeBtn = new QPushButton("Compute", this);
    m_closeBtn = new QPushButton("Close", this);
    buttons->addStretch();
    buttons->addWidget(m_computeBtn);
    buttons->addWidget(m_closeBtn);
    layout->addLayout(buttons);

    connect(m_computeBtn, &QPushButton::clicked, this, &JoinPolarDialog::compute);
    connect(m_closeBtn, &QPushButton::clicked, this, &JoinPolarDialog::close);

    reload();
}

void JoinPolarDialog::reload()
{
    if (!m_pm) return;
    const QString selFrom = m_fromBox ? m_fromBox->currentText() : QString();
    const QString selTo = m_toBox ? m_toBox->currentText() : QString();

    const QStringList names = m_pm->getPointNames();
    m_fromBox->clear();
    m_toBox->clear();
    m_fromBox->addItems(names);
    m_toBox->addItems(names);

    int idxFrom = names.indexOf(selFrom);
    int idxTo = names.indexOf(selTo);
    if (idxFrom >= 0) m_fromBox->setCurrentIndex(idxFrom);
    if (idxTo >= 0) m_toBox->setCurrentIndex(idxTo);
}

QString JoinPolarDialog::formatJoinResult(const Point& from, const Point& to) const
{
    // Deltas
    double dx = to.x - from.x;
    double dy = to.y - from.y;
    double dz = to.z - from.z;

    // Distances
    double d2 = SurveyCalculator::distance2D(from, to);
    double d3 = SurveyCalculator::distance3D(from, to);

    // Azimuths (SurveyCalculator already accounts for Gauss/Zimbabwe mode)
    double azFwd = SurveyCalculator::azimuth(from, to);
    double azBack = SurveyCalculator::azimuth(to, from);
    QString azFwdDms = SurveyCalculator::toDMS(azFwd);
    QString azBackDms = SurveyCalculator::toDMS(azBack);
    QString azFwdBrg = toQuadrantBearingLocal(azFwd);
    QString azBackBrg = toQuadrantBearingLocal(azBack);

    // Slope/grade
    bool is3D = AppSettings::use3D();
    double slopeDeg = 0.0;
    double gradePct = 0.0;
    if (is3D && d2 > 1e-9) {
        slopeDeg = SurveyCalculator::normalizeAngle(SurveyCalculator::radiansToDegrees(qAtan2(dz, d2)));
        gradePct = (dz / d2) * 100.0;
    }

    const bool gauss = AppSettings::gaussMode();
    QString deltaLine;
    if (gauss) deltaLine = QString("  ΔY: %1  ΔX: %2")
                                .arg(QString::number(dy, 'f', 3))
                                .arg(QString::number(dx, 'f', 3));
    else deltaLine = QString("  ΔX: %1  ΔY: %2")
                                .arg(QString::number(dx, 'f', 3))
                                .arg(QString::number(dy, 'f', 3));
    if (is3D) deltaLine += QString("  ΔZ: %1").arg(QString::number(dz, 'f', 3));

    QString out;
    out += QString("Join %1 -> %2\n").arg(from.name).arg(to.name);
    out += deltaLine + "\n";
    out += QString("  Distance 2D: %1 m\n").arg(QString::number(d2, 'f', 3));
    if (is3D) out += QString("  Distance 3D: %1 m\n").arg(QString::number(d3, 'f', 3));
    out += QString("  Azimuth FWD: %1° (%2)  Bearing: %3\n")
              .arg(QString::number(azFwd, 'f', 4))
              .arg(azFwdDms)
              .arg(azFwdBrg);
    out += QString("  Azimuth BKW: %1° (%2)  Bearing: %3\n")
              .arg(QString::number(azBack, 'f', 4))
              .arg(azBackDms)
              .arg(azBackBrg);
    if (is3D) out += QString("  Slope angle: %1°  Gradient: %2%%\n")
                            .arg(QString::number(slopeDeg, 'f', 4))
                            .arg(QString::number(gradePct, 'f', 3));
    return out.trimmed();
}

void JoinPolarDialog::compute()
{
    if (!m_pm) return;
    if (m_fromBox->currentText().isEmpty() || m_toBox->currentText().isEmpty()) {
        QMessageBox::warning(this, "Join", "Please select both From and To coordinates.");
        return;
    }
    const Point from = m_pm->getPoint(m_fromBox->currentText());
    const Point to = m_pm->getPoint(m_toBox->currentText());
    if (from.name.isEmpty() || to.name.isEmpty()) {
        QMessageBox::warning(this, "Join", "Selected coordinates not found.");
        return;
    }

    const QString result = formatJoinResult(from, to);
    m_output->setPlainText(result);

    if (m_canvas && m_drawLineCheck->isChecked()) {
        m_canvas->addLine(from.toQPointF(), to.toQPointF());
    }
}
