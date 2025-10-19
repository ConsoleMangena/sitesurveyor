#include "licensedialog.h"
#include "appsettings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QIcon>
#include <QComboBox>

LicenseDialog::LicenseDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("License");
    setModal(true);
    setMinimumWidth(420);

    auto* layout = new QVBoxLayout(this);

    // Discipline selector
    auto* discRow = new QHBoxLayout();
    auto* discLabel = new QLabel("Discipline:", this);
    discLabel->setStyleSheet("font-weight: bold;");
    discRow->addWidget(discLabel);
    m_discCombo = new QComboBox(this);
    const QStringList discs = AppSettings::availableDisciplines();
    m_discCombo->addItems(discs);
    const QString curDisc = AppSettings::discipline();
    int curIdx = discs.indexOf(curDisc);
    if (curIdx < 0) curIdx = 0;
    m_discCombo->setCurrentIndex(curIdx);
    discRow->addWidget(m_discCombo, 1);
    layout->addLayout(discRow);

    auto* title = new QLabel("Enter your license key:", this);
    title->setStyleSheet("font-weight: bold; font-size: 14px;");
    layout->addWidget(title);

    m_keyEdit = new QLineEdit(this);
    m_keyEdit->setPlaceholderText("XXXX-XXXX-XXXX-XXXX");
    m_keyEdit->setEchoMode(QLineEdit::Password);
    m_keyEdit->setClearButtonEnabled(true);
    layout->addWidget(m_keyEdit);

    auto* optionsRow = new QHBoxLayout();
    m_showCheck = new QCheckBox("Show key", this);
    connect(m_showCheck, &QCheckBox::toggled, this, &LicenseDialog::toggleShow);
    optionsRow->addWidget(m_showCheck);
    optionsRow->addStretch();
    layout->addLayout(optionsRow);

    m_statusLabel = new QLabel("", this);
    layout->addWidget(m_statusLabel);

    auto* buttonsRow = new QHBoxLayout();
    buttonsRow->addStretch();
    m_saveButton = new QPushButton("Save", this);
    m_closeButton = new QPushButton("Close", this);
    connect(m_saveButton, &QPushButton::clicked, this, &LicenseDialog::saveKey);
    connect(m_closeButton, &QPushButton::clicked, this, &LicenseDialog::close);
    connect(m_discCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LicenseDialog::onDisciplineChanged);
    buttonsRow->addWidget(m_saveButton);
    buttonsRow->addWidget(m_closeButton);
    layout->addLayout(buttonsRow);

    reload();
}

void LicenseDialog::reload()
{
    const QString disc = AppSettings::discipline();
    if (m_discCombo) {
        const QStringList discs = AppSettings::availableDisciplines();
        int idx = discs.indexOf(disc);
        if (idx >= 0) m_discCombo->setCurrentIndex(idx);
    }
    // Do not display saved keys; show status only
    if (m_keyEdit) m_keyEdit->clear();
    const bool has = AppSettings::hasLicenseFor(disc);
    m_statusLabel->setText(has ? QString("A license key is saved for %1.").arg(disc) : QString());
}

void LicenseDialog::saveKey()
{
    const QString key = m_keyEdit->text().trimmed();
    // Basic validation stub: require non-empty and at least 8 chars
    if (key.isEmpty() || key.size() < 8) {
        m_statusLabel->setText("Please enter a valid license key.");
        // In dark mode keep text white; in light mode show red
        m_statusLabel->setStyleSheet(AppSettings::darkMode() ? "" : "color: #ff8080;");
        return;
    }
    const QString disc = m_discCombo ? m_discCombo->currentText() : AppSettings::discipline();
    // Enforce discipline-specific license prefix
    const QString prefix = AppSettings::licensePrefixFor(disc);
    const QString upperKey = key.toUpper();
    if (!prefix.isEmpty() && !upperKey.startsWith(prefix + "-")) {
        m_statusLabel->setText(QString("Invalid license for %1 (expected prefix %2-).")
                               .arg(disc, prefix));
        m_statusLabel->setStyleSheet(AppSettings::darkMode() ? "" : "color: #ff8080;");
        return;
    }
    AppSettings::setDiscipline(disc);
    AppSettings::setLicenseKeyFor(disc, key);
    m_statusLabel->setText(QString("License key saved for %1.").arg(disc));
    // In dark mode keep text white; in light mode show green
    m_statusLabel->setStyleSheet(AppSettings::darkMode() ? "" : "color: #80ff80;");
    emit licenseSaved();
}

void LicenseDialog::toggleShow(bool on)
{
    m_keyEdit->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
}

void LicenseDialog::onDisciplineChanged(int index)
{
    Q_UNUSED(index);
    if (!m_discCombo) return;
    const QString disc = m_discCombo->currentText();
    AppSettings::setDiscipline(disc);
    // Do not display saved keys; show status only
    if (m_keyEdit) m_keyEdit->clear();
    const bool has = AppSettings::hasLicenseFor(disc);
    m_statusLabel->setText(has ? QString("A license key is saved for %1.").arg(disc) : QString());
    emit disciplineChanged();
}
