#ifndef VERSIONHISTORYDIALOG_H
#define VERSIONHISTORYDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include "auth/cloudmanager.h"

class VersionHistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VersionHistoryDialog(CloudManager* cloud, const QString& projectBaseName, QWidget *parent = nullptr);
    
    QString selectedFileId() const { return m_selectedFileId; }
    int selectedVersion() const { return m_selectedVersion; }

signals:
    void versionSelected(const QString& fileId, int version);
    void restoreRequested(const QString& fileId, int version);

private slots:
    void onVersionsLoaded(bool success, const QVector<CloudFile>& versions, const QString& message);
    void onSelectionChanged();
    void onRestoreClicked();
    void onDeleteClicked();

private:
    void setupUI();
    void refreshVersions();
    
    CloudManager* m_cloud;
    QString m_projectBaseName;
    QString m_selectedFileId;
    int m_selectedVersion;
    
    QTableWidget* m_table;
    QPushButton* m_restoreBtn;
    QPushButton* m_deleteBtn;
    QPushButton* m_closeBtn;
};

#endif // VERSIONHISTORYDIALOG_H
