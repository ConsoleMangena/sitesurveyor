#ifndef AUTHDIALOG_H
#define AUTHDIALOG_H

#include <QDialog>

class QLineEdit;
class QLabel;
class QPushButton;
class QTabWidget;
class QNetworkReply;
class AppwriteClient;

class AuthDialog : public QDialog {
    Q_OBJECT
public:
    explicit AuthDialog(AppwriteClient* client, QWidget* parent = nullptr);

private slots:
    void doLogin();
    void doSignup();
    void doLogout();

private:
    void buildUi();
    void attachDebug(QNetworkReply* r, const QString& op);

    AppwriteClient* m_client{nullptr};

    QTabWidget* m_tabs{nullptr};
    QLineEdit* m_loginEmail{nullptr};
    QLineEdit* m_loginPass{nullptr};
    QPushButton* m_loginBtn{nullptr};
    QLineEdit* m_signupEmail{nullptr};
    QLineEdit* m_signupPass{nullptr};
    QLineEdit* m_signupPass2{nullptr};
    QLineEdit* m_signupFirstName{nullptr};
    QLineEdit* m_signupLastName{nullptr};
    QPushButton* m_signupBtn{nullptr};
    QLabel* m_status{nullptr};
    QPushButton* m_logoutBtn{nullptr};
};

#endif
