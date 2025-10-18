#include "polarinputdialog.h"
#include "pointmanager.h"
#include "canvaswidget.h"
#include "surveycalculator.h"
#include "appsettings.h"
#include "point.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QCheckBox>
#include <QMessageBox>
#include <QRegularExpression>

PolarInputDialog::PolarInputDialog(PointManager* pm, CanvasWidget* canvas, QWidget* parent)
    : QDialog(parent)
    , m_pm(pm)
    , m_canvas(canvas)
{
    setWindowTitle("Polar Input");
    setModal(false);
    setMinimumSize(560, 460);

    auto *layout = new QVBoxLayout(this);

    // Row: from point + name
    auto *row1 = new QHBoxLayout();
    row1->addWidget(new QLabel("From:"));
    m_fromBox = new QComboBox(this);
    row1->addWidget(m_fromBox, 1);
    row1->addSpacing(12);
    row1->addWidget(new QLabel("New name:"));
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("e.g. P100");
    row1->addWidget(m_nameEdit, 1);
    layout->addLayout(row1);

    // Row: distance + azimuth
    auto *row2 = new QHBoxLayout();
    row2->addWidget(new QLabel("Distance (m):"));
    m_distanceEdit = new QLineEdit(this);
    m_distanceEdit->setPlaceholderText("e.g. 25.6");
    row2->addWidget(m_distanceEdit);
    row2->addSpacing(12);
    row2->addWidget(new QLabel("Azimuth (deg or DMS):"));
    m_azimuthEdit = new QLineEdit(this);
    m_azimuthEdit->setPlaceholderText("e.g. 123.45 or 123 27 00");
    row2->addWidget(m_azimuthEdit);
    layout->addLayout(row2);

    // Row: Z + draw line
    auto *row3 = new QHBoxLayout();
    m_zEdit = new QLineEdit(this);
    m_zEdit->setPlaceholderText("Z (optional)");
    m_drawLineCheck = new QCheckBox("Draw line on canvas", this);
    m_drawLineCheck->setChecked(true);
    m_zLabel = new QLabel("Z:", this);
    row3->addWidget(m_zLabel);
    row3->addWidget(m_zEdit);
    row3->addStretch();
    row3->addWidget(m_drawLineCheck);
    layout->addLayout(row3);

    // Hint
    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);

    // Output
    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setMinimumHeight(220);
    layout->addWidget(m_output, 1);

    // Buttons
    auto *buttons = new QHBoxLayout();
    m_computeBtn = new QPushButton("Compute", this);
    m_addBtn = new QPushButton("Add Coordinate", this);
    m_closeBtn = new QPushButton("Close", this);
    buttons->addStretch();
    buttons->addWidget(m_computeBtn);
    buttons->addWidget(m_addBtn);
    buttons->addWidget(m_closeBtn);
    layout->addLayout(buttons);

    connect(m_computeBtn, &QPushButton::clicked, this, &PolarInputDialog::compute);
    connect(m_addBtn, &QPushButton::clicked, this, &PolarInputDialog::addCoordinate);
    connect(m_closeBtn, &QPushButton::clicked, this, &PolarInputDialog::close);

    reload();
}

void PolarInputDialog::reload()
{
    if (!m_pm) return;
    const QString selFrom = m_fromBox ? m_fromBox->currentText() : QString();
    const QStringList names = m_pm->getPointNames();
    m_fromBox->clear();
    m_fromBox->addItems(names);
    int idxFrom = names.indexOf(selFrom);
    if (idxFrom >= 0) m_fromBox->setCurrentIndex(idxFrom);

    const bool use3D = AppSettings::use3D();
    m_zEdit->setVisible(use3D);
    m_zEdit->setEnabled(use3D);
    if (m_zLabel) m_zLabel->setVisible(use3D);

    const bool gauss = AppSettings::gaussMode();
    QString ref = gauss ? "South" : "North";
    m_hintLabel->setText(QString("Azimuths are measured clockwise from 0° %1. DMS accepted as 'd m s'.").arg(ref));
}

static double parseDmsComponents(const QStringList &parts, bool *ok)
{
    // parts size 2 or 3: deg [min [sec]]
    bool okd=false, okm=true, oks=true;
    double d = parts.value(0).toDouble(&okd);
    double m = parts.value(1).toDouble(&okm);
    double s = parts.value(2).toDouble(&oks);
    if (!okd || (!okm && parts.size()>=2) || (!oks && parts.size()>=3)) { *ok=false; return 0.0; }
    *ok = true;
    double sign = d < 0 ? -1.0 : 1.0;
    d = qAbs(d);
    return sign * (d + m/60.0 + s/3600.0);
}

double PolarInputDialog::parseAngleDMS(const QString& text, bool* ok)
{
    QString t = text.trimmed();
    // Try full number first
    bool okNum=false; double val = t.toDouble(&okNum);
    if (okNum) { if (ok) *ok=true; return val; }

    // Replace symbols with spaces, split
    QString norm = t;
    norm.replace(QRegularExpression("[°'\"]"), " ");
    norm = norm.simplified();
    QStringList parts = norm.split(' ');
    if (parts.size() < 2 || parts.size() > 3) { if (ok) *ok=false; return 0.0; }
    bool okd=false; double deg = parseDmsComponents(parts, &okd);
    if (ok) *ok = okd;
    return deg;
}

bool PolarInputDialog::parseInputs(Point& from, QString& newName, double& distance, double& azimuthDeg, double& z, QString& err) const
{
    err.clear();
    if (!m_pm) { err = "No point manager"; return false; }
    newName = m_nameEdit->text().trimmed();
    if (newName.isEmpty()) { err = "Please enter a new coordinate name"; return false; }
    if (m_pm->hasPoint(newName)) { err = QString("Coordinate %1 already exists").arg(newName); return false; }

    from = m_pm->getPoint(m_fromBox->currentText());
    if (from.name.isEmpty()) { err = "Please choose a valid 'From' coordinate"; return false; }

    bool okd=false; distance = m_distanceEdit->text().trimmed().toDouble(&okd);
    if (!okd || distance <= 0) { err = "Please enter a valid distance"; return false; }

    bool oka=false; azimuthDeg = parseAngleDMS(m_azimuthEdit->text(), &oka);
    if (!oka) { err = "Please enter a valid azimuth (deg or DMS)"; return false; }

    bool use3D = AppSettings::use3D();
    if (use3D) {
        bool okz=false; z = m_zEdit->text().trimmed().isEmpty() ? from.z : m_zEdit->text().trimmed().toDouble(&okz);
        if (!okz) { err = "Please enter a valid Z"; return false; }
    } else {
        z = 0.0; // consistent with 2D workflows
    }
    return true;
}

QString PolarInputDialog::formatResult(const Point& from, const Point& to, double distance, double azimuthDeg, double z) const
{
    QString out;
    out += QString("Polar from %1 -> %2\n").arg(from.name).arg(to.name);
    out += QString("  Input: dist=%1 m, az=%2°, z=%3\n")
              .arg(QString::number(distance, 'f', 3))
              .arg(QString::number(azimuthDeg, 'f', 4))
              .arg(QString::number(z, 'f', 3));
    out += QString("  Result: (%1, %2, %3)\n")
        .arg(QString::number(to.x, 'f', 3))
        .arg(QString::number(to.y, 'f', 3))
        .arg(QString::number(to.z, 'f', 3));
    return out;
}

void PolarInputDialog::compute()
{
    Point from; QString newName; double dist=0, az=0, z=0; QString err;
    if (!parseInputs(from, newName, dist, az, z, err)) {
        QMessageBox::warning(this, "Polar Input", err);
        return;
    }
    Point to = SurveyCalculator::polarToRectangular(from, dist, az, z);
    to.name = newName;
    m_output->setPlainText(formatResult(from, to, dist, az, z));
}

void PolarInputDialog::addCoordinate()
{
    compute();
    // After compute(), m_output has result; recompute to get 'to'
    Point from; QString newName; double dist=0, az=0, z=0; QString err;
    if (!parseInputs(from, newName, dist, az, z, err)) return;
    Point to = SurveyCalculator::polarToRectangular(from, dist, az, z);
    to.name = newName;

    if (m_pm->hasPoint(newName)) {
        QMessageBox::warning(this, "Polar Input", QString("Coordinate %1 already exists").arg(newName));
        return;
    }
    m_pm->addPoint(to);
    if (m_canvas) {
        m_canvas->addPoint(to);
        if (m_drawLineCheck->isChecked()) m_canvas->addLine(from.toQPointF(), to.toQPointF());
    }
    QMessageBox::information(this, "Polar Input", QString("Added coordinate %1").arg(newName));
}
