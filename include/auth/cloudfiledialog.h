#ifndef CLOUDFILEDIALOG_H
#define CLOUDFILEDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include "auth/cloudmanager.h"

class CloudFileDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode {
        Open,
        Save
    };

    explicit CloudFileDialog(CloudManager* manager, Mode mode, QWidget *parent = nullptr);
    
    QString selectedFileId() const { return m_selectedFileId; }
    QString selectedFileName() const { return m_selectedFileName; }

private slots:
    void refreshList();
    void onListFinished(bool success, const QVector<CloudFile>& files, const QString& message);
    void onFileSelected();
    void onAccept();
    void onDelete();
    void onDeleteFinished(bool success, const QString& message);

private:
    CloudManager* m_manager;
    Mode m_mode;
    QString m_selectedFileId;
    QString m_selectedFileName;

    QTableWidget* m_fileTable;
    QLineEdit* m_nameEdit;
    QLabel* m_statusLabel;
    QPushButton* m_actionBtn;
    QPushButton* m_deleteBtn;
};

#endif // CLOUDFILEDIALOG_H
