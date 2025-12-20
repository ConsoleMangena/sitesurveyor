#include "auth/conflictdialog.h"
#include <QGroupBox>

ConflictDialog::ConflictDialog(const QString& localName, const QDateTime& localTime, 
                             const QString& remoteName, const QDateTime& remoteTime, 
                             QWidget *parent)
    : QDialog(parent), m_resolution(Cancel)
{
    setWindowTitle(tr("Save Conflict"));
    setupUI(localName, localTime, remoteName, remoteTime);
}

void ConflictDialog::setupUI(const QString& localName, const QDateTime& localTime, const QString& remoteName, const QDateTime& remoteTime)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    QLabel* warningLabel = new QLabel(tr("<b>Conflict Detected!</b><br>The file on the server has been modified since you last opened it."));
    warningLabel->setStyleSheet("color: red;");
    mainLayout->addWidget(warningLabel);
    
    // Comparison
    QHBoxLayout* compareLayout = new QHBoxLayout();
    
    // Remote
    QGroupBox* remoteGroup = new QGroupBox(tr("Remote Version (Server)"), this);
    QVBoxLayout* remoteLayout = new QVBoxLayout(remoteGroup);
    remoteLayout->addWidget(new QLabel(tr("Name: %1").arg(remoteName)));
    remoteLayout->addWidget(new QLabel(tr("Modified: %1").arg(remoteTime.toString())));
    compareLayout->addWidget(remoteGroup);
    
    // Local
    QGroupBox* localGroup = new QGroupBox(tr("Local Version (Your Changes)"), this);
    QVBoxLayout* localLayout = new QVBoxLayout(localGroup);
    localLayout->addWidget(new QLabel(tr("Name: %1").arg(localName)));
    localLayout->addWidget(new QLabel(tr("Modified: %1").arg(localTime.toString())));
    compareLayout->addWidget(localGroup);
    
    mainLayout->addLayout(compareLayout);
    
    mainLayout->addWidget(new QLabel(tr("What would you like to do?")));
    
    // Buttons
    QPushButton* newProjectBtn = new QPushButton(tr("Save as New Project"));
    newProjectBtn->setToolTip(tr("Saves your changes as a new file, keeping the server version intact."));
    connect(newProjectBtn, &QPushButton::clicked, this, &ConflictDialog::onNewProjectClicked);
    mainLayout->addWidget(newProjectBtn);
    
    QPushButton* overwriteBtn = new QPushButton(tr("Overwrite Remote Version"));
    overwriteBtn->setStyleSheet("color: red;");
    overwriteBtn->setToolTip(tr("Overwrites the file on the server with your changes. Remote changes will be lost!"));
    connect(overwriteBtn, &QPushButton::clicked, this, &ConflictDialog::onOverwriteClicked);
    mainLayout->addWidget(overwriteBtn);
    
    QPushButton* cancelBtn = new QPushButton(tr("Cancel"));
    connect(cancelBtn, &QPushButton::clicked, this, &ConflictDialog::onCancelClicked);
    mainLayout->addWidget(cancelBtn);
}

void ConflictDialog::onNewProjectClicked()
{
    m_resolution = StartNewProject;
    accept();
}

void ConflictDialog::onOverwriteClicked()
{
    m_resolution = OverwriteRemote;
    accept();
}

void ConflictDialog::onCancelClicked()
{
    m_resolution = Cancel;
    reject();
}
