#ifndef SHAREPROJECTDIALOG_H
#define SHAREPROJECTDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include "auth/cloudmanager.h"

class ShareProjectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShareProjectDialog(CloudManager* cloud, const QString& fileId, const QString& projectName, QWidget *parent = nullptr);

private slots:
    void onShareClicked();
    void onUnshareClicked();
    void onShareFinished(bool success, const QString& message);
    void onUnshareFinished(bool success, const QString& message);
    void onSharesListed(bool success, const QVector<ProjectShare>& shares, const QString& message);

private:
    void setupUI();
    void refreshShares();

    CloudManager* m_cloud;
    QString m_fileId;
    QString m_projectName;

    QLineEdit* m_emailInput;
    QComboBox* m_permissionCombo;
    QPushButton* m_shareBtn;
    QListWidget* m_sharesList;
    QPushButton* m_removeShareBtn;
    QPushButton* m_closeBtn;
};

#endif // SHAREPROJECTDIALOG_H
