#include "auth/versionhistorydialog.h"
#include <QHeaderView>

VersionHistoryDialog::VersionHistoryDialog(CloudManager* cloud, const QString& projectBaseName, QWidget *parent)
    : QDialog(parent), m_cloud(cloud), m_projectBaseName(projectBaseName), m_selectedVersion(0)
{
    setWindowTitle(tr("Version History - %1").arg(projectBaseName));
    setMinimumSize(600, 400);
    setupUI();
    
    connect(m_cloud, &CloudManager::versionsListed, this, &VersionHistoryDialog::onVersionsLoaded);
    
    refreshVersions();
}

void VersionHistoryDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Header
    QLabel* header = new QLabel(tr("Select a version to restore or delete:"));
    mainLayout->addWidget(header);
    
    // Table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({tr("Version"), tr("Date"), tr("Size"), tr("ID")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setColumnHidden(3, true);  // Hide ID column
    mainLayout->addWidget(m_table);
    
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &VersionHistoryDialog::onSelectionChanged);
    
    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    
    m_restoreBtn = new QPushButton(tr("Restore Version"));
    m_restoreBtn->setEnabled(false);
    connect(m_restoreBtn, &QPushButton::clicked, this, &VersionHistoryDialog::onRestoreClicked);
    btnLayout->addWidget(m_restoreBtn);
    
    m_deleteBtn = new QPushButton(tr("Delete Version"));
    m_deleteBtn->setEnabled(false);
    connect(m_deleteBtn, &QPushButton::clicked, this, &VersionHistoryDialog::onDeleteClicked);
    btnLayout->addWidget(m_deleteBtn);
    
    btnLayout->addStretch();
    
    m_closeBtn = new QPushButton(tr("Close"));
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_closeBtn);
    
    mainLayout->addLayout(btnLayout);
}

void VersionHistoryDialog::refreshVersions()
{
    m_table->setRowCount(0);
    m_cloud->listVersions(m_projectBaseName);
}

void VersionHistoryDialog::onVersionsLoaded(bool success, const QVector<CloudFile>& versions, const QString& message)
{
    if (!success) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to load versions: %1").arg(message));
        return;
    }
    
    m_table->setRowCount(versions.size());
    
    for (int i = 0; i < versions.size(); ++i) {
        const CloudFile& v = versions[i];
        
        m_table->setItem(i, 0, new QTableWidgetItem(tr("v%1").arg(v.version)));
        m_table->setItem(i, 1, new QTableWidgetItem(v.createdAt.toString("yyyy-MM-dd hh:mm")));
        m_table->setItem(i, 2, new QTableWidgetItem(QString::number(v.size / 1024) + " KB"));
        m_table->setItem(i, 3, new QTableWidgetItem(v.id));
    }
    
    m_table->resizeColumnsToContents();
}

void VersionHistoryDialog::onSelectionChanged()
{
    QList<QTableWidgetItem*> selected = m_table->selectedItems();
    bool hasSelection = !selected.isEmpty();
    
    m_restoreBtn->setEnabled(hasSelection);
    m_deleteBtn->setEnabled(hasSelection);
    
    if (hasSelection) {
        int row = m_table->currentRow();
        m_selectedFileId = m_table->item(row, 3)->text();
        m_selectedVersion = m_table->item(row, 0)->text().mid(1).toInt();  // Remove 'v' prefix
    }
}

void VersionHistoryDialog::onRestoreClicked()
{
    if (m_selectedFileId.isEmpty()) return;
    
    int result = QMessageBox::question(this, tr("Restore Version"),
        tr("Are you sure you want to restore version %1?\nThis will load this version into the editor.").arg(m_selectedVersion),
        QMessageBox::Yes | QMessageBox::No);
    
    if (result == QMessageBox::Yes) {
        emit restoreRequested(m_selectedFileId, m_selectedVersion);
        accept();
    }
}

void VersionHistoryDialog::onDeleteClicked()
{
    if (m_selectedFileId.isEmpty()) return;
    
    int result = QMessageBox::warning(this, tr("Delete Version"),
        tr("Are you sure you want to permanently delete version %1?").arg(m_selectedVersion),
        QMessageBox::Yes | QMessageBox::No);
    
    if (result == QMessageBox::Yes) {
        m_cloud->deleteVersion(m_selectedFileId);
        refreshVersions();
    }
}
