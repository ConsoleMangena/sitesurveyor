#include "app/logindialog.h"
#include "auth/authmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QString>
#include <QDesktopServices>
#include <QUrl>
#include <QIcon>
#include <QSettings>
#include <QTimer>


LoginDialog::LoginDialog(AuthManager* auth, QWidget *parent)
    : QDialog(parent), m_auth(auth)
{
    setupUi();

    connect(m_auth, &AuthManager::loginSuccess, this, &LoginDialog::onLoginSuccess);
    connect(m_auth, &AuthManager::loginError, this, &LoginDialog::onLoginError);
    connect(m_auth, &AuthManager::offlineLoginSuccess, this, &LoginDialog::onOfflineLoginSuccess);
}


void LoginDialog::applyTheme()
{
    QSettings settings;
    bool isDark = (settings.value("appearance/theme", "light").toString() == "dark");

    if (isDark) {
        setStyleSheet(R"(
            LoginDialog {
                background-color: #1e1e1e;
            }
            LoginDialog QLabel {
                color: #e0e0e0;
                font-family: 'Segoe UI', 'Arial', sans-serif;
                background-color: transparent;
            }
            LoginDialog QLineEdit {
                padding: 12px;
                border: 2px solid #3c3c3c;
                border-radius: 6px;
                font-size: 14px;
                background-color: #2d2d30;
                color: #ffffff;
            }
            LoginDialog QLineEdit:focus {
                border: 2px solid #0078d4;
                background-color: #252526;
            }
            LoginDialog QPushButton {
                background-color: #0078d4;
                color: #ffffff;
                padding: 14px;
                border-radius: 6px;
                font-weight: bold;
                font-size: 14px;
                border: none;
                min-height: 20px;
            }
            LoginDialog QPushButton:hover { background-color: #106ebe; }
            LoginDialog QPushButton:pressed { background-color: #005a9e; }
            LoginDialog QPushButton:disabled { background-color: #3c3c3c; color: #888888; }
            LoginDialog QProgressBar {
                background-color: #3c3c3c;
                border: none;
                border-radius: 2px;
            }
            LoginDialog QProgressBar::chunk {
                background-color: #0078d4;
                border-radius: 2px;
            }
            LoginDialog QCheckBox {
                color: #e0e0e0;
                font-size: 13px;
            }
            LoginDialog QCheckBox::indicator {
                width: 16px;
                height: 16px;
            }
        )");
    } else {
        setStyleSheet(R"(
            LoginDialog {
                background-color: #ffffff;
            }
            LoginDialog QLabel {
                color: #333333;
                font-family: 'Segoe UI', 'Arial', sans-serif;
                background-color: transparent;
            }
            LoginDialog QLineEdit {
                padding: 12px;
                border: 2px solid #d0d0d0;
                border-radius: 6px;
                font-size: 14px;
                background-color: #f9f9f9;
                color: #000000;
            }
            LoginDialog QLineEdit:focus {
                border: 2px solid #0078d4;
                background-color: #ffffff;
            }
            LoginDialog QPushButton {
                background-color: #0078d4;
                color: #ffffff;
                padding: 14px;
                border-radius: 6px;
                font-weight: bold;
                font-size: 14px;
                border: none;
                min-height: 20px;
            }
            LoginDialog QPushButton:hover { background-color: #106ebe; }
            LoginDialog QPushButton:pressed { background-color: #005a9e; }
            LoginDialog QPushButton:disabled { background-color: #cccccc; color: #666666; }
            LoginDialog QProgressBar {
                background-color: #e0e0e0;
                border: none;
                border-radius: 2px;
            }
            LoginDialog QProgressBar::chunk {
                background-color: #0078d4;
                border-radius: 2px;
            }
            LoginDialog QCheckBox {
                color: #333333;
                font-size: 13px;
            }
        )");
    }
}

void LoginDialog::setupUi()
{
    setWindowTitle("Sign In - SiteSurveyor");
    setFixedSize(420, 560);
    setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);

    // Apply theme-aware styling
    applyTheme();

    QSettings settings;
    bool isDark = (settings.value("appearance/theme", "light").toString() == "dark");

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(50, 40, 50, 40);
    layout->setSpacing(12);

    // Icon
    QLabel* iconLabel = new QLabel();
    iconLabel->setPixmap(QIcon(":/icons/account.svg").pixmap(64, 64));
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    layout->addSpacing(8);

    // Title
    QLabel* title = new QLabel("Welcome Back");
    title->setStyleSheet(QString("font-size: 24px; font-weight: bold; color: %1;").arg(isDark ? "#ffffff" : "#1a1a1a"));
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // Subtitle
    QLabel* subtitle = new QLabel("Sign in to your account");
    subtitle->setStyleSheet(QString("font-size: 13px; color: %1;").arg(isDark ? "#aaaaaa" : "#555555"));
    subtitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(subtitle);

    layout->addSpacing(8);

    // Inputs
    layout->addWidget(new QLabel("Email Address"));
    m_emailEdit = new QLineEdit();
    m_emailEdit->setPlaceholderText("name@company.com");
    layout->addWidget(m_emailEdit);

    layout->addWidget(new QLabel("Password"));
    m_passwordEdit = new QLineEdit();
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("Enter your password");
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);
    layout->addWidget(m_passwordEdit);

    // Remember Me + Forgot Password row
    QHBoxLayout* optionsRow = new QHBoxLayout();
    m_rememberCheck = new QCheckBox("Remember me");
    m_rememberCheck->setChecked(settings.value("auth/rememberMe", false).toBool());
    optionsRow->addWidget(m_rememberCheck);

    optionsRow->addStretch();

    QLabel* forgotLink = new QLabel("<a href=\"https://sitesurveyor.dev/forgot-password/\" style=\"color: #0078d4;\">Forgot password?</a>");
    forgotLink->setOpenExternalLinks(true);
    forgotLink->setStyleSheet("font-size: 12px;");
    optionsRow->addWidget(forgotLink);

    layout->addLayout(optionsRow);

    // Status & Progress
    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("color: #d83b01; font-size: 13px;");
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);

    m_loadingBar = new QProgressBar();
    m_loadingBar->setFixedHeight(4);
    m_loadingBar->setTextVisible(false);
    m_loadingBar->setRange(0, 0); // Indeterminate
    m_loadingBar->hide();
    layout->addWidget(m_loadingBar);

    layout->addStretch();

    // Sign In Button
    m_loginBtn = new QPushButton("Sign In");
    connect(m_loginBtn, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    layout->addWidget(m_loginBtn);

    // Registration Link
    QLabel* registerLabel = new QLabel("Don't have an account? <a href=\"https://sitesurveyor.dev/login/\" style=\"color: #0078d4;\">Sign up at sitesurveyor.dev</a>");
    registerLabel->setAlignment(Qt::AlignCenter);
    registerLabel->setOpenExternalLinks(true);
    registerLabel->setStyleSheet(QString("font-size: 12px; color: %1;").arg(isDark ? "#888888" : "#666666"));
    layout->addWidget(registerLabel);

    // Load saved email if Remember Me was checked
    if (settings.value("auth/rememberMe", false).toBool()) {
        m_emailEdit->setText(settings.value("auth/savedEmail", "").toString());
    }
}

void LoginDialog::onLoginClicked()
{
    QString email = m_emailEdit->text().trimmed();
    QString pass = m_passwordEdit->text();

    if (email.isEmpty() || pass.isEmpty()) {
        m_statusLabel->setText("Please enter both email and password.");
        return;
    }

    // Save Remember Me preference
    QSettings settings;
    settings.setValue("auth/rememberMe", m_rememberCheck->isChecked());
    if (m_rememberCheck->isChecked()) {
        settings.setValue("auth/savedEmail", email);
    } else {
        settings.remove("auth/savedEmail");
    }

    m_statusLabel->clear();
    m_loadingBar->show();
    m_loginBtn->setEnabled(false);
    m_emailEdit->setEnabled(false);
    m_passwordEdit->setEnabled(false);

    m_auth->login(email, pass);
}

void LoginDialog::onLoginSuccess()
{
    m_loadingBar->hide();
    accept();
}

void LoginDialog::onLoginError(const QString& message)
{
    QString lowerMsg = message.toLower();

    // Check if this is a network error - try offline login
    if (lowerMsg.contains("network") || lowerMsg.contains("connection") ||
        lowerMsg.contains("timeout") || lowerMsg.contains("host") ||
        lowerMsg.contains("unable to") || lowerMsg.contains("could not")) {
        // Try offline login
        tryOfflineLogin();
        return;
    }

    m_loadingBar->hide();
    m_loginBtn->setEnabled(true);
    m_emailEdit->setEnabled(true);
    m_passwordEdit->setEnabled(true);

    // Check if error indicates account doesn't exist
    if (lowerMsg.contains("user") && (lowerMsg.contains("not found") || lowerMsg.contains("invalid"))) {
        m_statusLabel->setText(
            "Account not found. <a href=\"https://sitesurveyor.dev/login/\" style=\"color: #0078d4;\">Create a new account</a>"
        );
        m_statusLabel->setTextFormat(Qt::RichText);
        m_statusLabel->setOpenExternalLinks(true);
    } else if (lowerMsg.contains("password") || lowerMsg.contains("credentials")) {
        m_statusLabel->setText("Invalid email or password. Please try again.");
    } else {
        m_statusLabel->setText(message);
    }
}

void LoginDialog::tryOfflineLogin()
{
    QString email = m_emailEdit->text().trimmed();
    QString password = m_passwordEdit->text();

    if (m_auth->hasOfflineCredentials() && m_auth->tryOfflineLogin(email, password)) {
        // Offline login will emit offlineLoginSuccess which calls onOfflineLoginSuccess
        return;
    }

    // Offline login failed - show network error
    m_loadingBar->hide();
    m_loginBtn->setEnabled(true);
    m_emailEdit->setEnabled(true);
    m_passwordEdit->setEnabled(true);

    if (m_auth->hasOfflineCredentials()) {
        m_statusLabel->setText("Offline mode: Invalid email or password.");
    } else {
        m_statusLabel->setText("No internet connection. Please connect to the internet to log in for the first time.");
    }
}

void LoginDialog::onOfflineLoginSuccess()
{
    m_loadingBar->hide();
    m_statusLabel->setStyleSheet("color: #107c10; font-size: 13px;");
    m_statusLabel->setText("Signed in offline. Some features may be limited.");

    // Brief delay to show the message, then accept
    QTimer::singleShot(1000, this, &QDialog::accept);
}
