#include "coordinatesystemdialog.h"
#include "appsettings.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QStringList>

CoordinateSystemDialog::CoordinateSystemDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Coordinate Reference System"));
    setModal(true);
    auto* mainLayout = new QVBoxLayout(this);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    mainLayout->addWidget(m_summaryLabel);

    auto* form = new QFormLayout;
    m_codeEdit = new QLineEdit(this);
    form->addRow(tr("Authority Code"), m_codeEdit);

    m_nameEdit = new QLineEdit(this);
    form->addRow(tr("CRS Name"), m_nameEdit);

    m_datumEdit = new QLineEdit(this);
    form->addRow(tr("Reference Datum"), m_datumEdit);

    m_projectionEdit = new QLineEdit(this);
    form->addRow(tr("Projection"), m_projectionEdit);

    m_unitsCombo = new QComboBox(this);
    m_unitsCombo->addItems({tr("Meters"), tr("International Feet"), tr("US Survey Feet")});
    form->addRow(tr("Linear Units"), m_unitsCombo);

    mainLayout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &CoordinateSystemDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &CoordinateSystemDialog::reject);
    mainLayout->addWidget(buttons);

    connect(m_codeEdit, &QLineEdit::textChanged, this, &CoordinateSystemDialog::updateSummary);
    connect(m_nameEdit, &QLineEdit::textChanged, this, &CoordinateSystemDialog::updateSummary);
    connect(m_datumEdit, &QLineEdit::textChanged, this, &CoordinateSystemDialog::updateSummary);
    connect(m_projectionEdit, &QLineEdit::textChanged, this, &CoordinateSystemDialog::updateSummary);
    connect(m_unitsCombo, &QComboBox::currentTextChanged, this, &CoordinateSystemDialog::updateSummary);

    loadSettings();
}

void CoordinateSystemDialog::loadSettings()
{
    m_codeEdit->setText(AppSettings::crs());
    m_nameEdit->setText(AppSettings::crsName());
    m_datumEdit->setText(AppSettings::crsDatum());
    m_projectionEdit->setText(AppSettings::crsProjection());

    const QString units = AppSettings::crsLinearUnits();
    int idx = m_unitsCombo->findText(units, Qt::MatchFixedString);
    if (idx < 0) {
        idx = 0;
    }
    m_unitsCombo->setCurrentIndex(idx);
    updateSummary();
}

void CoordinateSystemDialog::updateSummary()
{
    QStringList parts;
    const QString code = m_codeEdit->text().trimmed();
    if (!code.isEmpty()) parts << code;
    const QString name = m_nameEdit->text().trimmed();
    if (!name.isEmpty()) parts << name;
    const QString datum = m_datumEdit->text().trimmed();
    if (!datum.isEmpty()) parts << datum;
    const QString proj = m_projectionEdit->text().trimmed();
    if (!proj.isEmpty()) parts << proj;
    const QString units = m_unitsCombo->currentText();
    if (!units.isEmpty()) parts << units;

    m_summaryLabel->setText(parts.isEmpty() ? tr("No CRS metadata configured yet.")
                                            : parts.join(tr(" • ")));
}

void CoordinateSystemDialog::accept()
{
    AppSettings::setCrs(m_codeEdit->text().trimmed());
    AppSettings::setCrsName(m_nameEdit->text().trimmed());
    AppSettings::setCrsDatum(m_datumEdit->text().trimmed());
    AppSettings::setCrsProjection(m_projectionEdit->text().trimmed());
    AppSettings::setCrsLinearUnits(m_unitsCombo->currentText());
    QDialog::accept();
}
