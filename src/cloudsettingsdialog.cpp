#include "cloudsettingsdialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QSettings>

CloudSettingsDialog::CloudSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Cloud Settings");
    resize(460, 220);
    buildUi();
    loadValues();
}

void CloudSettingsDialog::buildUi()
{
    auto* main = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    m_endpoint = new QLineEdit(this);
    m_endpoint->setPlaceholderText("https://cloud.appwrite.io");
    m_projectId = new QLineEdit(this);
    m_projectId->setPlaceholderText("Appwrite Project ID");
    m_selfSigned = new QCheckBox("Allow self-signed TLS (dev only)", this);

    form->addRow(new QLabel("Endpoint:", this), m_endpoint);
    form->addRow(new QLabel("Project ID:", this), m_projectId);
    form->addRow(new QLabel("Security:", this), m_selfSigned);

    main->addLayout(form);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &CloudSettingsDialog::saveAndClose);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    main->addWidget(box);
}

void CloudSettingsDialog::loadValues()
{
    QSettings s;
    const QString endpoint = s.value("cloud/appwriteEndpoint").toString().trimmed();
    const QString projectId = s.value("cloud/appwriteProjectId").toString().trimmed();
    const bool selfSigned = s.value("cloud/appwriteSelfSigned").toBool();
    if (m_endpoint) m_endpoint->setText(endpoint);
    if (m_projectId) m_projectId->setText(projectId);
    if (m_selfSigned) m_selfSigned->setChecked(selfSigned);
}

void CloudSettingsDialog::saveAndClose()
{
    QSettings s;
    s.setValue("cloud/appwriteEndpoint", m_endpoint ? m_endpoint->text().trimmed() : QString());
    s.setValue("cloud/appwriteProjectId", m_projectId ? m_projectId->text().trimmed() : QString());
    s.setValue("cloud/appwriteSelfSigned", m_selfSigned && m_selfSigned->isChecked());
    accept();
}
