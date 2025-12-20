#include "auth/cloudfiledialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDateTime>

CloudFileDialog::CloudFileDialog(CloudManager* manager, Mode mode, QWidget *parent)
    : QDialog(parent), m_manager(manager), m_mode(mode)
{
    setWindowTitle(mode == Save ? "Save Project to Cloud" : "Open Project from Cloud");
    resize(600, 400);

    QVBoxLayout* layout = new QVBoxLayout(this);

    // List
    m_fileTable = new QTableWidget();
    m_fileTable->setColumnCount(3);
    m_fileTable->setHorizontalHeaderLabels({"Name", "Date", "Size"});
    m_fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fileTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_fileTable);

    connect(m_fileTable, &QTableWidget::itemSelectionChanged, this, &CloudFileDialog::onFileSelected);

    // Name input (Save mode only)
    if (mode == Save) {
        QHBoxLayout* nameLayout = new QHBoxLayout();
        nameLayout->addWidget(new QLabel("File Name:"));
        m_nameEdit = new QLineEdit();
        nameLayout->addWidget(m_nameEdit);
        layout->addLayout(nameLayout);
    } else {
        m_nameEdit = nullptr;
    }

    // Status
    m_statusLabel = new QLabel("Ready");
    layout->addWidget(m_statusLabel);

    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    
    m_deleteBtn = new QPushButton("Delete");
    m_deleteBtn->setEnabled(false);
    connect(m_deleteBtn, &QPushButton::clicked, this, &CloudFileDialog::onDelete);
    btnLayout->addWidget(m_deleteBtn);

    btnLayout->addStretch();
    
    QPushButton* refreshBtn = new QPushButton("Refresh");
    connect(refreshBtn, &QPushButton::clicked, this, &CloudFileDialog::refreshList);
    btnLayout->addWidget(refreshBtn);

    m_actionBtn = new QPushButton(mode == Save ? "Save" : "Open");
    m_actionBtn->setDefault(true);
    m_actionBtn->setEnabled(mode == Save); // For Open, need selection
    connect(m_actionBtn, &QPushButton::clicked, this, &CloudFileDialog::onAccept);
    btnLayout->addWidget(m_actionBtn);

    QPushButton* cancelBtn = new QPushButton("Cancel");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    layout->addLayout(btnLayout);

    // Connect signals
    connect(m_manager, &CloudManager::listFinished, this, &CloudFileDialog::onListFinished);
    connect(m_manager, &CloudManager::deleteFinished, this, &CloudFileDialog::onDeleteFinished);

    // Initial Load
    refreshList();
}

void CloudFileDialog::refreshList()
{
    m_fileTable->setRowCount(0);
    m_statusLabel->setText("Loading...");
    m_manager->listProjects();
}

void CloudFileDialog::onListFinished(bool success, const QVector<CloudFile>& files, const QString& message)
{
    if (success) {
        m_fileTable->setRowCount(files.size());
        for (int i = 0; i < files.size(); ++i) {
            const CloudFile& file = files[i];
            
            QTableWidgetItem* nameItem = new QTableWidgetItem(file.name);
            nameItem->setData(Qt::UserRole, file.id);
            
            QTableWidgetItem* dateItem = new QTableWidgetItem(file.createdAt.toString("yyyy-MM-dd HH:mm"));
            QString sizeStr = QString::number(file.size / 1024.0, 'f', 1) + " KB";
            QTableWidgetItem* sizeItem = new QTableWidgetItem(sizeStr);
            
            m_fileTable->setItem(i, 0, nameItem);
            m_fileTable->setItem(i, 1, dateItem);
            m_fileTable->setItem(i, 2, sizeItem);
        }
        m_statusLabel->setText(QString("Loaded %1 files").arg(files.size()));
    } else {
        m_statusLabel->setText("Error: " + message);
    }
}

void CloudFileDialog::onFileSelected()
{
    auto items = m_fileTable->selectedItems();
    if (items.isEmpty()) {
        m_selectedFileId.clear();
        m_selectedFileName.clear();
        m_deleteBtn->setEnabled(false);
        if (m_mode == Open) m_actionBtn->setEnabled(false);
    } else {
        int row = items.first()->row();
        m_selectedFileName = m_fileTable->item(row, 0)->text();
        m_selectedFileId = m_fileTable->item(row, 0)->data(Qt::UserRole).toString();
        m_deleteBtn->setEnabled(true);
        if (m_mode == Open) m_actionBtn->setEnabled(true);
        if (m_mode == Save && m_nameEdit->text().isEmpty()) {
            m_nameEdit->setText(m_selectedFileName);
        }
    }
}

void CloudFileDialog::onAccept()
{
    if (m_mode == Save) {
        m_selectedFileName = m_nameEdit->text();
        if (m_selectedFileName.isEmpty()) {
            QMessageBox::warning(this, "Save", "Please enter a file name");
            return;
        }
        if (!m_selectedFileName.endsWith(".ssp")) {
            m_selectedFileName += ".ssp";
        }
    } else {
        if (m_selectedFileId.isEmpty()) return;
    }
    accept();
}

void CloudFileDialog::onDelete()
{
    if (m_selectedFileId.isEmpty()) return;
    
    if (QMessageBox::question(this, "Delete", "Are you sure you want to delete '" + m_selectedFileName + "'?") 
        == QMessageBox::Yes) {
        m_statusLabel->setText("Deleting...");
        m_manager->deleteProject(m_selectedFileId);
    }
}

void CloudFileDialog::onDeleteFinished(bool success, const QString& message)
{
    if (success) {
        m_statusLabel->setText("Deleted");
        refreshList();
    } else {
        QMessageBox::critical(this, "Delete Error", message);
        m_statusLabel->setText("Delete failed");
    }
}
