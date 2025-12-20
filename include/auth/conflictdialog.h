#ifndef CONFLICTDIALOG_H
#define CONFLICTDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "auth/cloudmanager.h"

class ConflictDialog : public QDialog
{
    Q_OBJECT

public:
    enum Resolution {
        StartNewProject,
        OverwriteRemote,
        Cancel
    };

    explicit ConflictDialog(const QString& localName, const QDateTime& localTime, 
                           const QString& remoteName, const QDateTime& remoteTime, 
                           QWidget *parent = nullptr);

    Resolution resolution() const { return m_resolution; }

private slots:
    void onNewProjectClicked();
    void onOverwriteClicked();
    void onCancelClicked();

private:
    Resolution m_resolution;
    void setupUI(const QString& localName, const QDateTime& localTime, const QString& remoteName, const QDateTime& remoteTime);
};

#endif // CONFLICTDIALOG_H
