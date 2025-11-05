#include "welcomewidget.h"
#include "appsettings.h"
#include "iconmanager.h"
#include "appwriteclient.h"
#include "authdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QPixmap>
#include <QDesktopServices>
#include <QUrl>
#include <QFont>
#include <QComboBox>
#include <QFrame>
#include <QFormLayout>
#include <QTabWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QToolButton>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QSettings>
#include <QUrlQuery>
#include <QUuid>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QTimer>
#include <QDateTime>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QProcess>

// Helper to open URLs without leaking child-process stdout/stderr to the terminal
static void openUrlExternalSilent(const QUrl& url)
{
#if defined(Q_OS_LINUX)
    auto shellQuote = [](const QString& s) {
        QString t = s;
        t.replace("'", "'\"'\"'");
        return "'" + t + "'";
    };
    const QString cmd = QStringLiteral("xdg-open %1 >/dev/null 2>&1 &").arg(shellQuote(url.toString()));
    QProcess::startDetached(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), cmd});
#elif defined(Q_OS_WIN)
    // Use Windows shell to open without binding to our console
    QProcess::startDetached(QStringLiteral("cmd.exe"), {QStringLiteral("/c"), QStringLiteral("start"), QString(), url.toString()});
#else // macOS and others
    QProcess::startDetached(QStringLiteral("open"), {url.toString()});
#endif
}

WelcomeWidget::WelcomeWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* main = new QVBoxLayout(this);
    // Branded layout with a compact, bordered card
    main->setContentsMargins(6, 6, 6, 6);
    main->setSpacing(1);

    QFrame* card = new QFrame(this);
    card->setObjectName("welcomeCard");
    card->setStyleSheet(
        "#welcomeCard { border: 1px solid #777; border-radius: 3px; padding: 6px; }"
        "#welcomeCard QLabel { margin: 0; }"
        "#welcomeCard QLineEdit { padding: 2px 4px; }"
        "#welcomeCard QPushButton { padding: 3px 6px; }"
    );
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(4, 4, 4, 4);
    cardLayout->setSpacing(1);
    main->addWidget(card);

    // Header row: logo + title
    {
        auto* header = new QHBoxLayout();
        header->setSpacing(2);
        header->setContentsMargins(0,0,0,0);

        m_title = new QLabel("SiteSurveyor Desktop", this);
        QFont titleFont;
        titleFont.setPointSize(14);
        titleFont.setWeight(QFont::DemiBold);
        m_title->setFont(titleFont);
        m_title->setStyleSheet("margin: 0px;");
        header->addWidget(m_title, 0, Qt::AlignVCenter | Qt::AlignLeft);
        header->addStretch();

        // Official logo at the top-right corner
        QLabel* logo = new QLabel(this);
        QPixmap pm = QIcon(":/branding/logo.svg").pixmap(24, 24);
        logo->setPixmap(pm);
        header->addWidget(logo, 0, Qt::AlignTop | Qt::AlignRight);
        cardLayout->addLayout(header);
    }

    m_description = new QLabel(this);
    m_description->setWordWrap(true);
    m_description->setText("Modern Geomatics Desktop for Surveying and Mapping.");
    QFont descFont;
    // Compact caption style
    m_description->setStyleSheet("margin: 0 0 1px 0;");
    cardLayout->addWidget(m_description);

    // Start/Learn tabs (AutoCAD-style Start page)
    m_tabs = new QTabWidget(this);
    QWidget* startPage = new QWidget(this);
    QWidget* learnPage = new QWidget(this);
    m_tabs->addTab(startPage, "Start");
    m_tabs->addTab(learnPage, "Learn");
    cardLayout->addWidget(m_tabs);

    // Start tab content: two columns (left: actions, right: account/status)
    auto* startRow = new QHBoxLayout(startPage);
    startRow->setContentsMargins(0,0,0,0);
    startRow->setSpacing(8);
    m_leftPane = new QWidget(this);
    m_rightPane = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(m_leftPane);
    QVBoxLayout* rightLayout = new QVBoxLayout(m_rightPane);
    leftLayout->setContentsMargins(0,0,0,0);
    leftLayout->setSpacing(6);
    rightLayout->setContentsMargins(0,0,0,0);
    rightLayout->setSpacing(6);
    startRow->addWidget(m_leftPane, 3);
    startRow->addWidget(m_rightPane, 2);
    // Create row: New, From Template, Open
    {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);
        m_newButton = new QToolButton(this);
        m_newButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        m_newButton->setIcon(IconManager::iconUnique("file-plus", "welcome_new", "N"));
        m_newButton->setText("New Drawing");
        connect(m_newButton, &QToolButton::clicked, this, &WelcomeWidget::onCreateNew);
        row->addWidget(m_newButton);

        m_templateButton = new QToolButton(this);
        m_templateButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        m_templateButton->setIcon(IconManager::iconUnique("file-plus", "welcome_template", "T"));
        m_templateButton->setText("From Template...");
        connect(m_templateButton, &QToolButton::clicked, this, &WelcomeWidget::onOpenTemplate);
        row->addWidget(m_templateButton);

        m_openButton = new QToolButton(this);
        m_openButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        m_openButton->setIcon(IconManager::iconUnique("folder-open", "welcome_open", "O"));
        m_openButton->setText("Open...");
        connect(m_openButton, &QToolButton::clicked, this, &WelcomeWidget::onOpenProject);
        row->addWidget(m_openButton);

        row->addStretch();
        leftLayout->addLayout(row);
    }

    // Search and Pin toolbar
    {
        auto* row = new QHBoxLayout();
        row->setSpacing(4);
        m_searchEdit = new QLineEdit(this);
        m_searchEdit->setPlaceholderText("Search recent files...");
        connect(m_searchEdit, &QLineEdit::textChanged, this, &WelcomeWidget::onSearchTextChanged);
        row->addWidget(m_searchEdit, 1);
        m_pinButton = new QToolButton(this);
        m_pinButton->setIcon(IconManager::iconUnique("star", "welcome_pin", "P"));
        m_pinButton->setToolTip("Pin selected");
        connect(m_pinButton, &QToolButton::clicked, this, &WelcomeWidget::onPinSelected);
        row->addWidget(m_pinButton);
        m_unpinButton = new QToolButton(this);
        m_unpinButton->setIcon(IconManager::iconUnique("clear", "welcome_unpin", "UP"));
        m_unpinButton->setToolTip("Unpin selected");
        connect(m_unpinButton, &QToolButton::clicked, this, &WelcomeWidget::onUnpinSelected);
        row->addWidget(m_unpinButton);
        leftLayout->addLayout(row);
    }

    // Recent list
    m_recentList = new QListWidget(this);
    m_recentList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(m_recentList, &QListWidget::itemActivated, this, &WelcomeWidget::onRecentItemActivated);
    leftLayout->addWidget(m_recentList, 1);

    // Learn tab content (simple placeholders)
    {
        auto* l = new QVBoxLayout(learnPage);
        l->setContentsMargins(0,0,0,0);
        QLabel* docs = new QLabel("<a href='https://sitesurveyor.dev/docs'>Documentation</a>", this);
        docs->setTextFormat(Qt::RichText);
        docs->setTextInteractionFlags(Qt::TextBrowserInteraction);
        docs->setOpenExternalLinks(true);
        l->addWidget(docs);
        QLabel* notes = new QLabel("<a href='https://sitesurveyor.dev/releases'>Release notes</a>", this);
        notes->setTextFormat(Qt::RichText);
        notes->setTextInteractionFlags(Qt::TextBrowserInteraction);
        notes->setOpenExternalLinks(true);
        l->addWidget(notes);
        l->addStretch();
    }

    // Right column: Account/Status/Diagnostics
    QFormLayout* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setHorizontalSpacing(3);
    form->setVerticalSpacing(2);

    // Discipline selector
    auto* discLabel = new QLabel("Discipline:", this);
    discLabel->setStyleSheet("font-weight: bold;");
    m_disciplineCombo = new QComboBox(this);
    const QStringList discs = AppSettings::availableDisciplines();
    m_disciplineCombo->addItems(discs);
    // Set current selection from settings
    const QString curDisc = AppSettings::discipline();
    int curIdx = discs.indexOf(curDisc);
    if (curIdx < 0) curIdx = 0;
    m_disciplineCombo->setCurrentIndex(curIdx);
    form->addRow(discLabel, m_disciplineCombo);

    


    // Account + Status lines in form
    m_accountLabel = new QLabel("Not signed in", this);
    form->addRow(new QLabel("Account:", this), m_accountLabel);
    m_statusLabel = new QLabel("", this);
    form->addRow(new QLabel("Status:", this), m_statusLabel);
    // Polling chip (awaiting access)
    m_pollChip = new QLabel("Awaiting access", this);
    m_pollChip->setVisible(false);
    m_pollChip->setStyleSheet("QLabel { border-radius: 8px; padding: 2px 8px; font-size: 11px; background: #2d6cdf; color: white; } ");
    form->addRow(new QLabel("", this), m_pollChip);

    // Diagnostics (read-only)
    m_diagEndpoint = new QLabel("", this);
    m_diagProject = new QLabel("", this);
    m_diagAccountId = new QLabel("", this);
    m_diagEmail = new QLabel("", this);
    m_diagLabels = new QLabel("", this);
    m_diagEndpoint->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_diagProject->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_diagAccountId->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_diagEmail->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_diagLabels->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_diagEndpointTitle = new QLabel("Endpoint:", this);
    m_diagProjectTitle = new QLabel("Project:", this);
    form->addRow(m_diagEndpointTitle, m_diagEndpoint);
    form->addRow(m_diagProjectTitle, m_diagProject);
    form->addRow(new QLabel("Account ID:", this), m_diagAccountId);
    form->addRow(new QLabel("Email:", this), m_diagEmail);
    form->addRow(new QLabel("Labels:", this), m_diagLabels);

    rightLayout->addLayout(form);

    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(2);
    buttons->addStretch();
    m_studentButton = new QPushButton("Student Verification", this);
    m_studentButton->setIcon(IconManager::iconUnique("star", "welcome_student", "ST"));
    m_studentButton->setFont(descFont);
    buttons->addWidget(m_studentButton);
    m_buyButton = new QPushButton("Buy License", this);
    m_buyButton->setIcon(IconManager::iconUnique("credit-card", "welcome_buy", "BY"));
    m_buyButton->setFont(descFont);
    buttons->addWidget(m_buyButton);
    // GitHub OAuth button
    m_githubButton = new QPushButton("Continue with GitHub", this);
    m_githubButton->setIcon(IconManager::iconUnique("github", "welcome_github", "GH"));
    m_githubButton->setFont(descFont);
    buttons->addWidget(m_githubButton);
    m_signInButton = new QPushButton("Sign In", this);
    m_signInButton->setFont(descFont);
    buttons->addWidget(m_signInButton);
    m_activateButton = new QPushButton("Refresh Access", this);
    m_activateButton->setFont(descFont);
    m_activateButton->setDefault(true);
    buttons->addWidget(m_activateButton);
    rightLayout->addLayout(buttons);

    // Load .env into process environment (optional convenience)
    {
        const QString envPath = QDir::current().absoluteFilePath(".env");
        QFile f(envPath);
        if (f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            while (!ts.atEnd()) {
                const QString line = ts.readLine();
                if (line.trimmed().isEmpty() || line.trimmed().startsWith('#')) continue;
                const int eq = line.indexOf('=');
                if (eq <= 0) continue;
                const QString key = line.left(eq).trimmed();
                const QString val = line.mid(eq+1).trimmed();
                if (!key.isEmpty()) qputenv(key.toUtf8().constData(), val.toUtf8());
            }
        }
    }

    // Secrets visibility (keep hidden for public builds)
    {
        const QString flag = QString::fromUtf8(qgetenv("SS_SHOW_SECRETS")).trimmed().toLower();
        m_showSecrets = (flag == "1" || flag == "true" || flag == "yes");
        if (!m_showSecrets) {
            if (m_diagEndpointTitle) m_diagEndpointTitle->setVisible(false);
            if (m_diagEndpoint) m_diagEndpoint->setVisible(false);
            if (m_diagProjectTitle) m_diagProjectTitle->setVisible(false);
            if (m_diagProject) m_diagProject->setVisible(false);
        }
    }

    // Appwrite setup (fixed/default)
    m_appwrite = new AppwriteClient(this);
    {
        QString endpoint = QString::fromUtf8(qgetenv("APPWRITE_ENDPOINT"));
        QString projectId = QString::fromUtf8(qgetenv("APPWRITE_PROJECT_ID"));
        if (endpoint.isEmpty()) endpoint = QStringLiteral("https://nyc.cloud.appwrite.io/v1");
        if (projectId.isEmpty()) projectId = QStringLiteral("690593fd000abc2a382f");
        m_appwrite->setEndpoint(endpoint);
        m_appwrite->setProjectId(projectId);
        m_appwrite->setSelfSigned(false);
    }

    // Connections
    connect(m_disciplineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WelcomeWidget::onDisciplineChanged);
    connect(m_studentButton, &QPushButton::clicked, this, &WelcomeWidget::openStudentPage);
    connect(m_buyButton, &QPushButton::clicked, this, &WelcomeWidget::openBuyPage);
    connect(m_githubButton, &QPushButton::clicked, this, &WelcomeWidget::openGithubOAuth);
    connect(m_signInButton, &QPushButton::clicked, this, &WelcomeWidget::openAuthDialog);
    connect(m_activateButton, &QPushButton::clicked, this, &WelcomeWidget::fetchLicense);


    refreshRecentList();
    reload();
    updateAuthUI();
}

 

void WelcomeWidget::signOut()
{
    if (!m_appwrite) return;
    // Immediately clear all local licenses and lock UI
    const QStringList discs = AppSettings::availableDisciplines();
    for (const QString& d : discs) AppSettings::clearLicenseFor(d);
    m_verified = false;
    setFeaturesLocked(true);
    if (m_statusLabel) { m_statusLabel->setText("Signing out..."); m_statusLabel->setStyleSheet(""); }
    if (m_signInButton) { m_signInButton->setText("Sign In"); m_signInButton->setEnabled(true); }
    if (m_accountLabel) m_accountLabel->setText("Not signed in");
    emit disciplineChanged();

    // Then terminate server session asynchronously
    QNetworkReply* r = m_appwrite->logout();
    connect(r, &QNetworkReply::finished, this, [this, r](){
        r->deleteLater();
        if (m_statusLabel) m_statusLabel->setText("Signed out");
    });
}

void WelcomeWidget::startLabelPolling()
{
    if (!m_appwrite) return;
    if (!m_labelPollTimer) {
        m_labelPollTimer = new QTimer(this);
        m_labelPollTimer->setInterval(10000); // 10s
        connect(m_labelPollTimer, &QTimer::timeout, this, [this](){
            if (!m_appwrite || !m_appwrite->isLoggedIn()) return;
            QNetworkReply* r = m_appwrite->getAccount();
            connect(r, &QNetworkReply::finished, this, [this, r](){
                const bool ok = (r->error() == QNetworkReply::NoError);
                const QByteArray data = r->readAll();
                r->deleteLater();
                if (!ok) {
                    // Offline grace countdown
                    QSettings s; const int graceDays = s.value("license/offlineGraceDays", 5).toInt();
                    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
                    const QDateTime last = QDateTime::fromString(s.value("license/lastVerifiedUtc").toString(), Qt::ISODate);
                    if (last.isValid()) {
                        qint64 secsLeft = graceDays * 86400 - last.secsTo(nowUtc);
                        if (secsLeft > 0) {
                            int days = int(secsLeft / 86400);
                            int hours = int((secsLeft % 86400) / 3600);
                            if (m_statusLabel) { m_statusLabel->setText(QString("Offline mode: %1d %2h left").arg(days).arg(hours)); m_statusLabel->setStyleSheet("color: #d97706;"); }
                        }
                    }
                    return;
                }
                QJsonParseError pe; QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
                QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject();
                QString raw = QString::fromUtf8(qgetenv("APPWRITE_ACCESS_LABEL")).trimmed(); if (raw.isEmpty()) raw = QStringLiteral("access");
                QStringList required = raw.split(',', Qt::SkipEmptyParts); for (QString& s : required) s = s.trimmed().toLower();
                bool granted = false;
                if (obj.contains("labels") && obj.value("labels").isArray()) {
                    const QJsonArray arr = obj.value("labels").toArray();
                    for (const QJsonValue& lv : arr) { const QString lab = lv.toString().trimmed().toLower(); if (required.contains(lab)) { granted = true; break; } }
                } else if (obj.value("labels").isString()) {
                    const QString lab = obj.value("labels").toString().trimmed().toLower(); if (required.contains(lab)) granted = true;
                }
                if (granted) {
                    m_verified = true;
                    if (m_statusLabel) { m_statusLabel->setText("Access granted via label."); m_statusLabel->setStyleSheet("color: #16a34a;"); }
                    if (m_activateButton) m_activateButton->setText("Save & Continue");
                    setFeaturesLocked(false);
                    stopLabelPolling(); if (m_pollChip) m_pollChip->setVisible(false);
                }
                // Update diagnostics
                if (m_diagAccountId) m_diagAccountId->setText(obj.value("$id").toString());
                if (m_diagEmail) m_diagEmail->setText(obj.value("email").toString());
                if (m_diagLabels) {
                    QStringList labs; if (obj.value("labels").isArray()) for (const QJsonValue& lv : obj.value("labels").toArray()) labs << lv.toString();
                    else if (obj.value("labels").isString()) labs << obj.value("labels").toString();
                    m_diagLabels->setText(labs.join(", "));
                }
            });
        });
    }
    if (!m_labelPollTimer->isActive()) m_labelPollTimer->start();
    // Start chip animation
    if (!m_chipTimer) {
        m_chipTimer = new QTimer(this);
        m_chipTimer->setInterval(500);
        connect(m_chipTimer, &QTimer::timeout, this, [this](){
            if (!m_pollChip || !m_pollChip->isVisible()) return;
            m_chipPhase = (m_chipPhase + 1) % 4;
            QString dots(m_chipPhase, '.');
            m_pollChip->setText(QString("Awaiting access%1").arg(dots));
        });
    }
    if (m_pollChip) m_pollChip->setVisible(true);
    if (!m_chipTimer->isActive()) m_chipTimer->start();
}

void WelcomeWidget::stopLabelPolling()
{
    if (m_labelPollTimer && m_labelPollTimer->isActive()) m_labelPollTimer->stop();
    if (m_chipTimer && m_chipTimer->isActive()) m_chipTimer->stop();
    if (m_pollChip) m_pollChip->setVisible(false);
}

void WelcomeWidget::stopOAuthServer()
{
    if (m_oauthServer) {
        if (m_oauthServer->isListening()) m_oauthServer->close();
        m_oauthServer->deleteLater();
        m_oauthServer = nullptr;
    }
}

void WelcomeWidget::reload()
{
    if (m_appwrite) {
        QString endpoint = QString::fromUtf8(qgetenv("APPWRITE_ENDPOINT"));
        QString projectId = QString::fromUtf8(qgetenv("APPWRITE_PROJECT_ID"));
        if (!endpoint.isEmpty()) m_appwrite->setEndpoint(endpoint);
        if (!projectId.isEmpty()) m_appwrite->setProjectId(projectId);
        m_appwrite->setSelfSigned(false);
        updateAuthUI();
    }
    const QString disc = AppSettings::discipline();
    if (m_disciplineCombo) {
        const QStringList discs = AppSettings::availableDisciplines();
        int idx = discs.indexOf(disc);
        if (idx >= 0) m_disciplineCombo->setCurrentIndex(idx);
    }
    const bool has = AppSettings::hasLicenseFor(disc);
    if (has) {
        m_statusLabel->setText(QString("A license is active for %1.").arg(disc));
        m_statusLabel->setStyleSheet("color: #8fda8f;");
    } else {
        m_statusLabel->setText("");
        m_statusLabel->setStyleSheet("");
    }
    refreshRecentList();
}

void WelcomeWidget::updateAuthUI()
{
    const bool logged = m_appwrite && m_appwrite->isLoggedIn();
    if (m_signInButton) m_signInButton->setText(logged ? "Account" : "Sign In");
    if (m_signInButton) m_signInButton->setEnabled(true);
    if (m_accountLabel) m_accountLabel->setText(logged ? "Loading account..." : "Not signed in");
    m_verified = false;
    if (m_activateButton) { m_activateButton->setText("Refresh Access"); m_activateButton->setEnabled(logged); }
    setFeaturesLocked(true);
    // Diagnostics base (endpoint/project)
    if (m_diagEndpoint) m_diagEndpoint->setText(m_appwrite ? m_appwrite->endpoint() : QString());
    if (m_diagProject) m_diagProject->setText(m_appwrite ? m_appwrite->projectId() : QString());
    if (logged && m_appwrite) {
        QNetworkReply* r = m_appwrite->getAccount();
        connect(r, &QNetworkReply::finished, this, [this, r](){
            const bool ok = (r->error() == QNetworkReply::NoError);
            const QByteArray data = r->readAll();
            r->deleteLater();
            if (!ok) { if (m_accountLabel) m_accountLabel->setText("Signed in (name unavailable)"); return; }
            QJsonParseError pe; QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
            QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject();
            QString name = obj.value("name").toString().trimmed();
            QString email = obj.value("email").toString().trimmed();
            const QString accountId = obj.value("$id").toString().trimmed();
            if (name.isEmpty()) name = AppSettings::userName();
            if (email.isEmpty()) email = AppSettings::userEmail();
            if (m_accountLabel) m_accountLabel->setText(name.isEmpty() ? QString("Signed in as %1").arg(email) : QString("Signed in as %1 (%2)").arg(name, email));
            if (m_diagAccountId) m_diagAccountId->setText(accountId);
            if (m_diagEmail) m_diagEmail->setText(email);
            // Update labels diagnostics
            QStringList labs;
            if (obj.contains("labels") && obj.value("labels").isArray()) {
                for (const QJsonValue& lv : obj.value("labels").toArray()) labs << lv.toString();
            } else if (obj.value("labels").isString()) {
                labs << obj.value("labels").toString();
            }
            if (m_diagLabels) m_diagLabels->setText(labs.join(", "));

            // Access gating via labels
            QString raw = QString::fromUtf8(qgetenv("APPWRITE_ACCESS_LABEL")).trimmed();
            if (raw.isEmpty()) raw = QStringLiteral("access");
            QStringList required = raw.split(',', Qt::SkipEmptyParts);
            for (QString& s : required) s = s.trimmed().toLower();

            bool hasAccess = false;
            if (obj.contains("labels") && obj.value("labels").isArray()) {
                const QJsonArray arr = obj.value("labels").toArray();
                for (const QJsonValue& lv : arr) {
                    const QString lab = lv.toString().trimmed().toLower();
                    if (required.contains(lab)) { hasAccess = true; break; }
                }
            } else if (obj.value("labels").isString()) {
                const QString lab = obj.value("labels").toString().trimmed().toLower();
                if (required.contains(lab)) hasAccess = true;
            }

            m_verified = hasAccess;
            if (m_activateButton) m_activateButton->setText(hasAccess ? "Save & Continue" : "Refresh Access");
            if (m_statusLabel) m_statusLabel->setText(hasAccess ? "Access granted via label." : "Your account is not labeled for access.");
            if (m_statusLabel) m_statusLabel->setStyleSheet(hasAccess ? "color: #16a34a;" : "color: #d97706;");
            setFeaturesLocked(!hasAccess);
            // Start/stop auto polling + chip visibility
            if (!hasAccess) { startLabelPolling(); if (m_pollChip) m_pollChip->setVisible(true); }
            else { stopLabelPolling(); if (m_pollChip) m_pollChip->setVisible(false); }
        });
    }
}

 

void WelcomeWidget::markLicensedLocally()
{
    const QString disc = m_disciplineCombo ? m_disciplineCombo->currentText() : AppSettings::discipline();
    AppSettings::setDiscipline(disc);
    // Cache a local access token to satisfy license gating
    const QString token = QStringLiteral("VERIFIED-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    AppSettings::setLicenseKeyFor(disc, token);
    m_statusLabel->setText(QString("Access saved for %1. Welcome!").arg(disc));
    m_statusLabel->setStyleSheet("color: #16a34a;");
    // Update offline grace start timestamp
    QSettings s; s.setValue("license/lastVerifiedUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!s.contains("license/offlineGraceDays")) s.setValue("license/offlineGraceDays", 5);
    emit activated();
}

 

void WelcomeWidget::fetchLicense()
{
    if (!m_appwrite) { updateAuthUI(); return; }
    if (m_verified) { saveAndContinue(); return; }
    if (m_statusLabel) m_statusLabel->setText("Checking access status...");
    QNetworkReply* r = m_appwrite->getAccount();
    connect(r, &QNetworkReply::finished, this, [this, r](){
        const bool ok = (r->error() == QNetworkReply::NoError);
        const QByteArray data = r->readAll();
        r->deleteLater();
        if (!ok) {
            // Offline grace period countdown
            QSettings s; const int graceDays = s.value("license/offlineGraceDays", 5).toInt();
            const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
            const QDateTime last = QDateTime::fromString(s.value("license/lastVerifiedUtc").toString(), Qt::ISODate);
            if (last.isValid()) {
                qint64 secsLeft = graceDays * 86400 - last.secsTo(nowUtc);
                if (secsLeft > 0) {
                    int days = int(secsLeft / 86400);
                    int hours = int((secsLeft % 86400) / 3600);
                    if (m_statusLabel) { m_statusLabel->setText(QString("Offline mode: %1d %2h left").arg(days).arg(hours)); m_statusLabel->setStyleSheet("color: #d97706;"); }
                    return;
                }
            }
            if (m_statusLabel) { m_statusLabel->setText("Failed to read access status"); m_statusLabel->setStyleSheet("color: #dc2626;"); }
            return;
        }
        QJsonParseError pe; QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
        QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject();
        QString raw = QString::fromUtf8(qgetenv("APPWRITE_ACCESS_LABEL")).trimmed();
        if (raw.isEmpty()) raw = QStringLiteral("access");
        QStringList required = raw.split(',', Qt::SkipEmptyParts);
        for (QString& s : required) s = s.trimmed().toLower();
        bool granted = false;
        if (obj.contains("labels") && obj.value("labels").isArray()) {
            const QJsonArray arr = obj.value("labels").toArray();
            for (const QJsonValue& lv : arr) {
                const QString lab = lv.toString().trimmed().toLower();
                if (required.contains(lab)) { granted = true; break; }
            }
        } else if (obj.value("labels").isString()) {
            const QString lab = obj.value("labels").toString().trimmed().toLower();
            if (required.contains(lab)) granted = true;
        }
        m_verified = granted;
        // Diagnostics labels and account ID/email
        if (m_diagEndpoint) m_diagEndpoint->setText(m_appwrite ? m_appwrite->endpoint() : QString());
        if (m_diagProject) m_diagProject->setText(m_appwrite ? m_appwrite->projectId() : QString());
        if (m_diagAccountId) m_diagAccountId->setText(obj.value("$id").toString());
        if (m_diagEmail) m_diagEmail->setText(obj.value("email").toString());
        if (m_diagLabels) {
            QStringList labs; if (obj.value("labels").isArray()) for (const QJsonValue& lv : obj.value("labels").toArray()) labs << lv.toString();
            else if (obj.value("labels").isString()) labs << obj.value("labels").toString();
            m_diagLabels->setText(labs.join(", "));
        }
        if (granted) {
            if (m_statusLabel) { m_statusLabel->setText("Access granted via label. Click 'Save & Continue'."); m_statusLabel->setStyleSheet("color: #16a34a;"); }
            if (m_activateButton) m_activateButton->setText("Save & Continue");
            setFeaturesLocked(false);
            stopLabelPolling(); if (m_pollChip) m_pollChip->setVisible(false);
        } else {
            if (m_statusLabel) { m_statusLabel->setText("Your account is not labeled for access."); m_statusLabel->setStyleSheet("color: #d97706;"); }
            if (m_activateButton) m_activateButton->setText("Refresh Access");
            setFeaturesLocked(true);
            startLabelPolling(); if (m_pollChip) m_pollChip->setVisible(true);
        }
    });
}

 
 
void WelcomeWidget::saveAndContinue()
{
    // Fetch account to cache user info, then save access and proceed
    if (m_appwrite && m_appwrite->isLoggedIn()) {
        QNetworkReply* r = m_appwrite->getAccount();
        connect(r, &QNetworkReply::finished, this, [this, r](){
            const QByteArray data = r->readAll();
            r->deleteLater();
            QJsonParseError pe; QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
            QJsonObject acc = doc.isObject() ? doc.object() : QJsonObject();
            const QString name = acc.value("name").toString().trimmed();
            const QString email = acc.value("email").toString().trimmed();
            QSettings s; if (!name.isEmpty()) s.setValue("user/name", name); if (!email.isEmpty()) s.setValue("user/email", email);
            markLicensedLocally();
        });
    } else {
        markLicensedLocally();
    }
}

void WelcomeWidget::setFeaturesLocked(bool locked)
{
    if (m_newButton) m_newButton->setEnabled(!locked);
    if (m_openButton) m_openButton->setEnabled(!locked);
    if (m_templateButton) m_templateButton->setEnabled(!locked);
    if (m_pinButton) m_pinButton->setEnabled(!locked);
    if (m_unpinButton) m_unpinButton->setEnabled(!locked);
}

 
 
void WelcomeWidget::openAuthDialog()
{
    if (!m_appwrite) return;
    AuthDialog dlg(m_appwrite, this);
    connect(&dlg, &AuthDialog::loggedOut, this, [this](){ this->signOut(); });
    dlg.exec();
    updateAuthUI();
}

 

 


void WelcomeWidget::openBuyPage()
{
    const QString disc = m_disciplineCombo ? m_disciplineCombo->currentText() : AppSettings::discipline();
    QUrl url(QStringLiteral("https://sitesurveyor.app/buy"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("discipline"), disc);
    q.addQueryItem(QStringLiteral("source"), QStringLiteral("desktop"));
    url.setQuery(q);
    openUrlExternalSilent(url);
}

void WelcomeWidget::openStudentPage()
{
    QUrl url(QStringLiteral("https://sitesurveyor.app/student"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("source"), QStringLiteral("desktop"));
    url.setQuery(q);
    QDesktopServices::openUrl(url);
}

void WelcomeWidget::onDisciplineChanged(int index)
{
    Q_UNUSED(index);
    if (!m_disciplineCombo) return;
    const QString disc = m_disciplineCombo->currentText();
    AppSettings::setDiscipline(disc);
    const bool has = AppSettings::hasLicenseFor(disc);
    if (has) {
        m_statusLabel->setText(QString("A license is active for %1.").arg(disc));
        m_statusLabel->setStyleSheet("color: #8fda8f;");
    } else {
        m_statusLabel->setText("");
        m_statusLabel->setStyleSheet("");
    }
    emit disciplineChanged();
}

void WelcomeWidget::onCreateNew()
{
    emit newProjectRequested();
}

void WelcomeWidget::onOpenProject()
{
    emit openProjectRequested();
}

void WelcomeWidget::onOpenTemplate()
{
    // Placeholder: route to Open for now
    emit openProjectRequested();
}

void WelcomeWidget::onSearchTextChanged(const QString&)
{
    refreshRecentList();
}

void WelcomeWidget::openGithubOAuth()
{
    if (!m_appwrite) return;

    // Start a local loopback HTTP listener to capture Appwrite OAuth redirection with userId & secret
    if (!m_oauthServer) m_oauthServer = new QTcpServer(this);
    if (m_oauthServer->isListening()) m_oauthServer->close();
    if (!m_oauthServer->listen(QHostAddress::LocalHost, 0)) {
        if (m_statusLabel) m_statusLabel->setText("Failed to start local OAuth listener");
        return;
    }
    const quint16 port = m_oauthServer->serverPort();
    m_oauthProvider = QStringLiteral("github");

    connect(m_oauthServer, &QTcpServer::newConnection, this, [this]() {
        while (m_oauthServer && m_oauthServer->hasPendingConnections()) {
            QTcpSocket* sock = m_oauthServer->nextPendingConnection();
            if (!sock) continue;
            QObject::connect(sock, &QTcpSocket::readyRead, sock, [this, sock]() {
                QByteArray req = sock->readAll();
                // Very small HTTP parser for GET line
                QList<QByteArray> lines = req.split('\n');
                if (lines.isEmpty()) return;
                QList<QByteArray> parts = lines.first().trimmed().split(' ');
                if (parts.size() >= 2 && parts.first() == "GET") {
                    QByteArray path = parts.at(1);
                    QUrl url(QStringLiteral("http://127.0.0.1") + QString::fromUtf8(path));
                    const QString p = url.path();
                    const QUrlQuery q(url.query());
                    const QString htmlOk = "<html><body>Sign-in complete. You can close this window and return to SiteSurveyor.</body></html>";
                    const QString htmlFail = "<html><body>Sign-in failed. You can close this window and return to SiteSurveyor.</body></html>";
                    auto writeResp = [sock](int code, const QString& body){
                        QByteArray resp = QByteArray()
                            + "HTTP/1.1 " + QByteArray::number(code) + " \r\n"
                            + "Content-Type: text/html; charset=UTF-8\r\n"
                            + "Content-Length: " + QByteArray::number(body.toUtf8().size()) + "\r\n"
                            + "Connection: close\r\n\r\n" + body.toUtf8();
                        sock->write(resp);
                        sock->flush();
                        sock->disconnectFromHost();
                    };

                    if (p == "/oauth/success") {
                        QString userId = q.queryItemValue(QStringLiteral("userId"));
                        QString secretParam = q.queryItemValue(QStringLiteral("secret"));
                        // Some Appwrite deployments return only 'key' and a Base64(JSON) 'secret' containing id+secret
                        if (userId.isEmpty() && !secretParam.isEmpty()) {
                            QByteArray b64 = secretParam.toUtf8();
                            // Normalize possible URL-safe base64 and missing padding
                            QByteArray norm = b64;
                            norm.replace('-', '+');
                            norm.replace('_', '/');
                            while (norm.size() % 4 != 0) norm.append('=');
                            QByteArray decoded = QByteArray::fromBase64(norm, QByteArray::Base64Encoding);
                            QJsonParseError pe; QJsonDocument jd = QJsonDocument::fromJson(decoded, &pe);
                            if (pe.error == QJsonParseError::NoError && jd.isObject()) {
                                QJsonObject o = jd.object();
                                const QString idField = o.value(QStringLiteral("id")).toString(o.value(QStringLiteral("$id")).toString());
                                const QString secField = o.value(QStringLiteral("secret")).toString();
                                if (!idField.isEmpty() && !secField.isEmpty()) { userId = idField; secretParam = secField; }
                            }
                        }
                        writeResp(200, htmlOk);
                        stopOAuthServer();
                        if (userId.isEmpty() || secretParam.isEmpty()) {
                            if (m_statusLabel) { m_statusLabel->setText("OAuth success callback missing credentials"); m_statusLabel->setStyleSheet("color: #d97706;"); }
                            return;
                        }
                        if (m_statusLabel) { m_statusLabel->setText("Finalizing GitHub sign-in..."); m_statusLabel->setStyleSheet(""); }
                        QNetworkReply* r = m_appwrite->oauth2CreateToken(m_oauthProvider, userId, secretParam);
                        connect(r, &QNetworkReply::finished, this, [this, r]() {
                            const bool ok = (r->error() == QNetworkReply::NoError);
                            r->deleteLater();
                            if (!ok) {
                                if (m_statusLabel) { m_statusLabel->setText("GitHub sign-in failed to finalize"); m_statusLabel->setStyleSheet("color: #dc2626;"); }
                                return;
                            }
                            if (m_statusLabel) { m_statusLabel->setText("Signed in with GitHub"); m_statusLabel->setStyleSheet("color: #16a34a;"); }
                            updateAuthUI();
                        });
                    } else if (p == "/oauth/failure") {
                        writeResp(200, htmlFail);
                        stopOAuthServer();
                        if (m_statusLabel) { m_statusLabel->setText("GitHub sign-in canceled or failed"); m_statusLabel->setStyleSheet("color: #dc2626;"); }
                    } else {
                        // Favicon or other
                        writeResp(200, "<html></html>");
                    }
                }
            });
        }
    });

    // Build Appwrite OAuth URL with our loopback success/failure URLs
    QString endpoint = m_appwrite->endpoint();
    if (endpoint.isEmpty()) endpoint = QStringLiteral("https://nyc.cloud.appwrite.io");
    if (endpoint.endsWith('/')) endpoint.chop(1);
    if (endpoint.endsWith("/v1")) endpoint.chop(3);

    QUrl url(endpoint + QStringLiteral("/v1/account/sessions/oauth2/github"));
    QUrlQuery q;
    const QString project = m_appwrite->projectId();
    if (!project.isEmpty()) q.addQueryItem(QStringLiteral("project"), project);
    const QString loopHost = [](){
        QString h = QString::fromUtf8(qgetenv("SS_OAUTH_LOOPBACK_HOST")).trimmed();
        if (h.isEmpty()) h = QStringLiteral("127.0.0.1");
        return h;
    }();
    const QString base = QStringLiteral("http://%1:%2").arg(loopHost).arg(port);
    // Appwrite expects 'success' and 'failure' query params for redirect URLs
    q.addQueryItem(QStringLiteral("success"), base + QStringLiteral("/oauth/success"));
    q.addQueryItem(QStringLiteral("failure"), base + QStringLiteral("/oauth/failure"));
    url.setQuery(q);

    openUrlExternalSilent(url);
    if (m_statusLabel) { m_statusLabel->setText(QStringLiteral("Complete GitHub sign-in in your browser...")); m_statusLabel->setStyleSheet(""); }
}

void WelcomeWidget::refreshRecentList()
{
    if (!m_recentList) return;
    m_recentList->clear();
    const QString query = m_searchEdit ? m_searchEdit->text().trimmed() : QString();
    QStringList pins = AppSettings::pinnedFiles();
    QStringList rec = AppSettings::recentFiles();
    // Build list: pinned first, then recent not pinned
    auto addItem = [&](const QString& path, bool pinned){
        if (!query.isEmpty() && !path.contains(query, Qt::CaseInsensitive) && !QFileInfo(path).fileName().contains(query, Qt::CaseInsensitive)) return;
        QListWidgetItem* it = new QListWidgetItem(QFileInfo(path).fileName(), m_recentList);
        it->setToolTip(path);
        it->setData(Qt::UserRole, path);
        if (pinned) it->setIcon(IconManager::iconUnique("star", QString("recent_pin_%1").arg(path), "P"));
        else it->setIcon(IconManager::iconUnique("folder-open", QString("recent_file_%1").arg(path), "R"));
        m_recentList->addItem(it);
    };
    for (const QString& p : pins) addItem(p, true);
    for (const QString& p : rec) { if (!pins.contains(p)) addItem(p, false); }
}

void WelcomeWidget::onRecentItemActivated(QListWidgetItem* item)
{
    if (!item) return;
    const QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) return;
    emit openPathRequested(path);
}

void WelcomeWidget::onPinSelected()
{
    QList<QListWidgetItem*> sel = m_recentList ? m_recentList->selectedItems() : QList<QListWidgetItem*>();
    for (QListWidgetItem* it : sel) {
        const QString path = it->data(Qt::UserRole).toString();
        AppSettings::pinFile(path, true);
    }
    refreshRecentList();
}

void WelcomeWidget::onUnpinSelected()
{
    QList<QListWidgetItem*> sel = m_recentList ? m_recentList->selectedItems() : QList<QListWidgetItem*>();
    for (QListWidgetItem* it : sel) {
        const QString path = it->data(Qt::UserRole).toString();
        AppSettings::pinFile(path, false);
    }
    refreshRecentList();
}
