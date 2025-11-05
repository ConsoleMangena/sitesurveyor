#include "authdialog.h"
#include "appwriteclient.h"
#include "iconmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDesktopServices>
#include <QUrl>

AuthDialog::AuthDialog(AppwriteClient* client, QWidget* parent)
    : QDialog(parent), m_client(client)
{
    setWindowTitle("Account");
    resize(480, 340);
    buildUi();
}

void AuthDialog::buildUi()
{
    auto* main = new QVBoxLayout(this);
    setStyleSheet(
        "QDialog { background-color: palette(base); }"
        " QLineEdit { padding: 4px 6px; }"
        " QPushButton { padding: 6px 10px; border-radius: 4px; }"
        " QPushButton:default { background-color: palette(highlight); color: palette(highlighted-text); }"
    );
    m_tabs = new QTabWidget(this);

    // Login tab
    QWidget* loginPage = new QWidget(this);
    auto* lf = new QFormLayout(loginPage);
    m_loginEmail = new QLineEdit(loginPage);
    m_loginEmail->setPlaceholderText("you@example.com");
    m_loginPass = new QLineEdit(loginPage);
    m_loginPass->setEchoMode(QLineEdit::Password);
    m_loginBtn = new QPushButton("Log In", loginPage);
    lf->addRow(new QLabel("Email:", loginPage), m_loginEmail);
    lf->addRow(new QLabel("Password:", loginPage), m_loginPass);
    lf->addRow(new QLabel("", loginPage), m_loginBtn);
    
    m_tabs->addTab(loginPage, "Log In");
    connect(m_loginBtn, &QPushButton::clicked, this, &AuthDialog::doLogin);

    // Signup tab
    QWidget* signPage = new QWidget(this);
    auto* sf = new QFormLayout(signPage);
    m_signupEmail = new QLineEdit(signPage);
    m_signupEmail->setPlaceholderText("you@example.com");
    m_signupPass = new QLineEdit(signPage);
    m_signupPass->setEchoMode(QLineEdit::Password);
    m_signupPass2 = new QLineEdit(signPage);
    m_signupPass2->setEchoMode(QLineEdit::Password);
    m_signupFirstName = new QLineEdit(signPage);
    m_signupFirstName->setPlaceholderText("First name");
    m_signupLastName = new QLineEdit(signPage);
    m_signupLastName->setPlaceholderText("Surname");
    m_signupBtn = new QPushButton("Create Account", signPage);
    sf->addRow(new QLabel("Email:", signPage), m_signupEmail);
    sf->addRow(new QLabel("Password:", signPage), m_signupPass);
    sf->addRow(new QLabel("Confirm Password:", signPage), m_signupPass2);
    sf->addRow(new QLabel("First name:", signPage), m_signupFirstName);
    sf->addRow(new QLabel("Surname:", signPage), m_signupLastName);
    sf->addRow(new QLabel("", signPage), m_signupBtn);
    
    m_tabs->addTab(signPage, "Sign Up");
    connect(m_signupBtn, &QPushButton::clicked, this, &AuthDialog::doSignup);

    m_tabs->setTabIcon(0, IconManager::iconUnique("key", "auth_tab_login", "IN"));
    m_tabs->setTabIcon(1, IconManager::iconUnique("file-plus", "auth_tab_signup", "SU"));
    m_loginBtn->setIcon(IconManager::iconUnique("key", "auth_login_btn", "IN"));
    m_signupBtn->setIcon(IconManager::iconUnique("file-plus", "auth_signup_btn", "SU"));

    main->addWidget(m_tabs);

    // Status + Logout row
    auto* bottom = new QHBoxLayout();
    m_status = new QLabel("", this);
    m_logoutBtn = new QPushButton("Log Out", this);
    m_logoutBtn->setIcon(IconManager::iconUnique("logout-2", "auth_logout_btn", "LO"));
    connect(m_logoutBtn, &QPushButton::clicked, this, &AuthDialog::doLogout);
    bottom->addWidget(m_status, 1);
    bottom->addWidget(m_logoutBtn, 0);
    main->addLayout(bottom);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    main->addWidget(box);
}

void AuthDialog::attachDebug(QNetworkReply* r, const QString& op)
{
    if (!r) return;
    const QString base = QStringLiteral("[%1] ").arg(op);
    if (m_status) m_status->setText(base + "Connecting...");
    connect(r, &QNetworkReply::metaDataChanged, this, [this, r, base](){
        const QVariant code = r->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        if (code.isValid() && m_status) m_status->setText(base + QStringLiteral("HTTP %1").arg(code.toInt()));
    });
    connect(r, &QNetworkReply::errorOccurred, this, [this, r, base](QNetworkReply::NetworkError){
        const QVariant code = r->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const int sc = code.isValid() ? code.toInt() : 0;
        QString msg;
        const QByteArray body = r->peek(1024*8);
        QJsonParseError pe; QJsonDocument jd = QJsonDocument::fromJson(body, &pe);
        if (pe.error == QJsonParseError::NoError && jd.isObject()) msg = jd.object().value("message").toString();
        if (msg.isEmpty()) msg = r->errorString();
        if (sc == 401) msg = QStringLiteral("Unauthorized (401). Check endpoint/region and project ID.");
        if (m_status) m_status->setText(base + msg);
    });
#ifndef QT_NO_SSL
    connect(r, &QNetworkReply::sslErrors, this, [this, base](const QList<QSslError>& errors){
        if (m_status && !errors.isEmpty()) m_status->setText(base + QStringLiteral("SSL: ") + errors.first().errorString());
    });
#endif
}

void AuthDialog::doLogin()
{
    if (!m_client) return;
    const QString email = m_loginEmail ? m_loginEmail->text().trimmed() : QString();
    const QString pass = m_loginPass ? m_loginPass->text() : QString();
    if (email.isEmpty() || pass.length() < 6) {
        if (m_status) m_status->setText("Enter valid email and 6+ char password");
        return;
    }
    if (m_loginBtn) m_loginBtn->setEnabled(false);
    auto* r = m_client->login(email, pass);
    attachDebug(r, QStringLiteral("Login"));
    connect(r, &QNetworkReply::finished, this, [this, r](){
        const bool ok = (r->error() == QNetworkReply::NoError);
        const QVariant codeVar = r->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const int code = codeVar.isValid() ? codeVar.toInt() : 0;
        const QByteArray body = r->readAll();
        r->deleteLater();
        if (m_loginBtn) m_loginBtn->setEnabled(true);
        if (!ok) {
            QString msg;
            // Try to parse Appwrite error JSON
            QJsonParseError pe; QJsonDocument jd = QJsonDocument::fromJson(body, &pe);
            if (pe.error == QJsonParseError::NoError && jd.isObject()) msg = jd.object().value("message").toString();
            if (msg.isEmpty()) msg = r->errorString();
            if (code == 401) msg = QStringLiteral("Unauthorized (401). Check endpoint/region and project ID, then try again.");
            if (m_status) { m_status->setText(msg); m_status->setStyleSheet("color: #ff8080;"); }
            return;
        }
        if (m_status) { m_status->setText("Signed in"); m_status->setStyleSheet("color: #80ff80;"); }
    });
}

void AuthDialog::doSignup()
{
    if (!m_client) return;
    const QString email = m_signupEmail ? m_signupEmail->text().trimmed() : QString();
    const QString pass = m_signupPass ? m_signupPass->text() : QString();
    const QString pass2 = m_signupPass2 ? m_signupPass2->text() : QString();
    const QString first = m_signupFirstName ? m_signupFirstName->text().trimmed() : QString();
    const QString last = m_signupLastName ? m_signupLastName->text().trimmed() : QString();
    const QString name = (first + QStringLiteral(" ") + last).trimmed();
    auto setErr = [&](const QString& m){ if (m_status) { m_status->setText(m); m_status->setStyleSheet("color: #ff8080;"); } };
    // Basic validations
    if (email.isEmpty() || !email.contains('@') || !email.contains('.')) { setErr("Enter a valid email address"); return; }
    if (pass.length() < 8) { setErr("Password must be at least 8 characters"); return; }
    bool hasLetter = false, hasDigit = false;
    for (const QChar& ch : pass) { if (ch.isLetter()) hasLetter = true; if (ch.isDigit()) hasDigit = true; }
    if (!(hasLetter && hasDigit)) { setErr("Password must include letters and numbers"); return; }
    if (pass != pass2) { setErr("Passwords do not match"); return; }
    if (first.isEmpty() || last.isEmpty()) { setErr("First name and surname are required"); return; }

    if (m_signupBtn) m_signupBtn->setEnabled(false);
    auto* r = m_client->signUp(email, pass, name);
    attachDebug(r, QStringLiteral("Sign up"));
    connect(r, &QNetworkReply::finished, this, [this, email, pass, r](){
        const bool ok = (r->error() == QNetworkReply::NoError);
        const QVariant codeVar = r->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const int code = codeVar.isValid() ? codeVar.toInt() : 0;
        const QByteArray body = r->readAll();
        r->deleteLater();
        if (m_signupBtn) m_signupBtn->setEnabled(true);
        if (!ok) {
            QString msg;
            QJsonParseError pe; QJsonDocument jd = QJsonDocument::fromJson(body, &pe);
            if (pe.error == QJsonParseError::NoError && jd.isObject()) msg = jd.object().value("message").toString();
            if (msg.isEmpty()) msg = r->errorString();
            if (code == 401) msg = QStringLiteral("Unauthorized (401). Check APPWRITE_ENDPOINT region and project ID.");
            if (code == 409) msg = QStringLiteral("Email already in use");
            if (m_status) { m_status->setText(msg); m_status->setStyleSheet("color: #ff8080;"); }
            return;
        }
        if (m_status) { m_status->setText("Account created. Logging in..."); m_status->setStyleSheet(""); }
        auto* lr = m_client->login(email, pass);
        attachDebug(lr, QStringLiteral("Login"));
        connect(lr, &QNetworkReply::finished, this, [this, lr](){
            const bool ok2 = (lr->error() == QNetworkReply::NoError);
            const QVariant codeVar2 = lr->attribute(QNetworkRequest::HttpStatusCodeAttribute);
            const int code2 = codeVar2.isValid() ? codeVar2.toInt() : 0;
            const QByteArray lb = lr->readAll();
            lr->deleteLater();
            if (!ok2) {
                QString msg2;
                QJsonParseError pe2; QJsonDocument jd2 = QJsonDocument::fromJson(lb, &pe2);
                if (pe2.error == QJsonParseError::NoError && jd2.isObject()) msg2 = jd2.object().value("message").toString();
                if (msg2.isEmpty()) msg2 = lr->errorString();
                if (code2 == 401) msg2 = QStringLiteral("Login failed: Unauthorized (401)");
                if (m_status) { m_status->setText(msg2); m_status->setStyleSheet("color: #ff8080;"); }
                return;
            }
            if (m_status) { m_status->setText("Signed in"); m_status->setStyleSheet("color: #80ff80;"); }
        });
    });
}

void AuthDialog::doLogout()
{
    if (!m_client) return;
    auto* r = m_client->logout();
    attachDebug(r, QStringLiteral("Logout"));
    connect(r, &QNetworkReply::finished, this, [this, r](){
        r->deleteLater();
        if (m_status) m_status->setText("Signed out");
        emit loggedOut();
    });
}
