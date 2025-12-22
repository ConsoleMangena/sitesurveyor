#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QCheckBox>

class AuthManager;

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(AuthManager* auth, QWidget *parent = nullptr);

private slots:
    void onLoginClicked();
    void onLoginSuccess();
    void onLoginError(const QString& message);
    void onOfflineLoginSuccess();
    void onSkipClicked();

public:
    bool wasSkipped() const { return m_wasSkipped; }

private:
    void setupUi();
    void applyTheme();
    void tryOfflineLogin();

    AuthManager* m_auth;
    bool m_wasSkipped = false;

    QLineEdit* m_emailEdit;
    QLineEdit* m_passwordEdit;
    QLabel* m_statusLabel;
    QPushButton* m_loginBtn;
    QPushButton* m_skipBtn;
    QProgressBar* m_loadingBar;
    QCheckBox* m_rememberCheck;
};

#endif // LOGINDIALOG_H

