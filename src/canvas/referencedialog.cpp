#include "canvas/referencedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>

ReferenceDialog::ReferenceDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Set Reference Point"));
    setMinimumWidth(350);
    setupUI();
}

void ReferenceDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // GPS Coordinates Group
    QGroupBox* gpsGroup = new QGroupBox(tr("GPS Coordinates"), this);
    QFormLayout* gpsLayout = new QFormLayout(gpsGroup);

    m_latSpin = new QDoubleSpinBox(this);
    m_latSpin->setRange(-90.0, 90.0);
    m_latSpin->setDecimals(8);
    m_latSpin->setSuffix("°");
    m_latSpin->setValue(40.785091); // Default: Central Park
    gpsLayout->addRow(tr("Latitude:"), m_latSpin);

    m_lonSpin = new QDoubleSpinBox(this);
    m_lonSpin->setRange(-180.0, 180.0);
    m_lonSpin->setDecimals(8);
    m_lonSpin->setSuffix("°");
    m_lonSpin->setValue(-73.968285); // Default: Central Park
    gpsLayout->addRow(tr("Longitude:"), m_lonSpin);

    mainLayout->addWidget(gpsGroup);

    // Local Coordinates Group
    QGroupBox* localGroup = new QGroupBox(tr("Local Coordinates (at GPS Point)"), this);
    QFormLayout* localLayout = new QFormLayout(localGroup);

    m_localXSpin = new QDoubleSpinBox(this);
    m_localXSpin->setRange(-1e9, 1e9);
    m_localXSpin->setDecimals(3);
    m_localXSpin->setSuffix(" m");
    m_localXSpin->setValue(0.0);
    localLayout->addRow(tr("Easting (X):"), m_localXSpin);

    m_localYSpin = new QDoubleSpinBox(this);
    m_localYSpin->setRange(-1e9, 1e9);
    m_localYSpin->setDecimals(3);
    m_localYSpin->setSuffix(" m");
    m_localYSpin->setValue(0.0);
    localLayout->addRow(tr("Northing (Y):"), m_localYSpin);

    mainLayout->addWidget(localGroup);

    // Buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

double ReferenceDialog::latitude() const { return m_latSpin->value(); }
double ReferenceDialog::longitude() const { return m_lonSpin->value(); }
double ReferenceDialog::localX() const { return m_localXSpin->value(); }
double ReferenceDialog::localY() const { return m_localYSpin->value(); }

void ReferenceDialog::setLatitude(double lat) { m_latSpin->setValue(lat); }
void ReferenceDialog::setLongitude(double lon) { m_lonSpin->setValue(lon); }
void ReferenceDialog::setLocalX(double x) { m_localXSpin->setValue(x); }
void ReferenceDialog::setLocalY(double y) { m_localYSpin->setValue(y); }
