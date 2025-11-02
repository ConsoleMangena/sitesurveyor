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
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QSettings>
#include <QUrlQuery>
#include <QUuid>

WelcomeWidget::WelcomeWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* main = new QVBoxLayout(this);
    // Branded layout with a compact, bordered card
    main->setContentsMargins(6, 6, 6, 6);
    main->setSpacing(1);

    QFrame* card = new QFrame(this);
    card->setObjectName("welcomeCard");
    if (AppSettings::darkMode()) {
        card->setStyleSheet(
            "#welcomeCard { border: 1px solid #555; border-radius: 3px; padding: 6px; background-color: #1e1e1e; }"
            "#welcomeCard QLineEdit { background-color: #2a2a2a; border: 1px solid #555; padding: 2px 4px; }"
            "#welcomeCard QLabel { margin: 0; }"
            "#welcomeCard QPushButton { padding: 3px 6px; }"
        );
    } else {
        card->setStyleSheet(
            "#welcomeCard { border: 1px solid #777; border-radius: 3px; padding: 6px; }"
            "#welcomeCard QLabel { margin: 0; }"
            "#welcomeCard QLineEdit { padding: 2px 4px; }"
            "#welcomeCard QPushButton { padding: 3px 6px; }"
        );
    }
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

    // Start tab content
    auto* startLayout = new QVBoxLayout(startPage);
    startLayout->setContentsMargins(0,0,0,0);
    startLayout->setSpacing(6);
    // Create row: New, From Template, Open
    {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);
        m_newButton = new QToolButton(this);
        m_newButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        m_newButton->setIcon(IconManager::icon("file-plus"));
        m_newButton->setText("New Drawing");
        connect(m_newButton, &QToolButton::clicked, this, &WelcomeWidget::onCreateNew);
        row->addWidget(m_newButton);

        m_templateButton = new QToolButton(this);
        m_templateButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        m_templateButton->setIcon(IconManager::icon("file-plus"));
        m_templateButton->setText("From Template...");
        connect(m_templateButton, &QToolButton::clicked, this, &WelcomeWidget::onOpenTemplate);
        row->addWidget(m_templateButton);

        m_openButton = new QToolButton(this);
        m_openButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        m_openButton->setIcon(IconManager::icon("folder-open"));
        m_openButton->setText("Open...");
        connect(m_openButton, &QToolButton::clicked, this, &WelcomeWidget::onOpenProject);
        row->addWidget(m_openButton);

        row->addStretch();
        startLayout->addLayout(row);
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
        m_pinButton->setIcon(IconManager::icon("star"));
        m_pinButton->setToolTip("Pin selected");
        connect(m_pinButton, &QToolButton::clicked, this, &WelcomeWidget::onPinSelected);
        row->addWidget(m_pinButton);
        m_unpinButton = new QToolButton(this);
        m_unpinButton->setIcon(IconManager::icon("clear"));
        m_unpinButton->setToolTip("Unpin selected");
        connect(m_unpinButton, &QToolButton::clicked, this, &WelcomeWidget::onUnpinSelected);
        row->addWidget(m_unpinButton);
        startLayout->addLayout(row);
    }

    // Recent list
    m_recentList = new QListWidget(this);
    m_recentList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(m_recentList, &QListWidget::itemActivated, this, &WelcomeWidget::onRecentItemActivated);
    startLayout->addWidget(m_recentList, 1);

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

    // Compact form-style layout (License)
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

    cardLayout->addLayout(form);

    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(2);
    buttons->addStretch();
    m_studentButton = new QPushButton("Student Verification", this);
    m_studentButton->setIcon(IconManager::icon("star"));
    m_studentButton->setFont(descFont);
    buttons->addWidget(m_studentButton);
    m_buyButton = new QPushButton("Buy License", this);
    m_buyButton->setIcon(IconManager::icon("credit-card"));
    m_buyButton->setFont(descFont);
    buttons->addWidget(m_buyButton);
    m_signInButton = new QPushButton("Sign In", this);
    m_signInButton->setFont(descFont);
    buttons->addWidget(m_signInButton);
    m_activateButton = new QPushButton("Refresh Access", this);
    m_activateButton->setFont(descFont);
    m_activateButton->setDefault(true);
    buttons->addWidget(m_activateButton);
    cardLayout->addLayout(buttons);

    // Appwrite setup (configurable)
    m_appwrite = new AppwriteClient(this);
    {
        QSettings s;
        QString endpoint = s.value("cloud/appwriteEndpoint").toString().trimmed();
        QString projectId = s.value("cloud/appwriteProjectId").toString().trimmed();
        if (endpoint.isEmpty()) endpoint = QString::fromUtf8(qgetenv("APPWRITE_ENDPOINT"));
        if (projectId.isEmpty()) projectId = QString::fromUtf8(qgetenv("APPWRITE_PROJECT_ID"));
        if (endpoint.isEmpty()) endpoint = QStringLiteral("https://nyc.cloud.appwrite.io"); // default to NYC if unspecified
        m_appwrite->setEndpoint(endpoint);
        if (!projectId.isEmpty()) m_appwrite->setProjectId(projectId);
        m_appwrite->setSelfSigned(false);
    }

    // Connections
    connect(m_disciplineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WelcomeWidget::onDisciplineChanged);
    connect(m_studentButton, &QPushButton::clicked, this, &WelcomeWidget::openStudentPage);
    connect(m_buyButton, &QPushButton::clicked, this, &WelcomeWidget::openBuyPage);
    connect(m_signInButton, &QPushButton::clicked, this, &WelcomeWidget::openAuthDialog);
    connect(m_activateButton, &QPushButton::clicked, this, &WelcomeWidget::fetchLicense);


    refreshRecentList();
    reload();
    updateAuthUI();
}

void WelcomeWidget::reload()
{
    if (m_appwrite) {
        QSettings s;
        QString endpoint = s.value("cloud/appwriteEndpoint").toString().trimmed();
        QString projectId = s.value("cloud/appwriteProjectId").toString().trimmed();
        if (endpoint.isEmpty()) endpoint = QString::fromUtf8(qgetenv("APPWRITE_ENDPOINT"));
        if (projectId.isEmpty()) projectId = QString::fromUtf8(qgetenv("APPWRITE_PROJECT_ID"));
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
        if (AppSettings::darkMode()) m_statusLabel->setStyleSheet(""); else m_statusLabel->setStyleSheet("color: #8fda8f;");
    } else {
        m_statusLabel->setText("");
        m_statusLabel->setStyleSheet("");
    }
    refreshRecentList();
}

void WelcomeWidget::updateAuthUI()
{
    const bool configured = m_appwrite && m_appwrite->isConfigured();
    const bool logged = m_appwrite && m_appwrite->isLoggedIn();
    if (m_signInButton) m_signInButton->setText(logged ? "Account" : "Sign In");
    if (m_signInButton) m_signInButton->setEnabled(configured);
    if (m_accountLabel) m_accountLabel->setText(logged ? "Loading account..." : "Not signed in");
    m_verified = false;
    if (m_activateButton) { m_activateButton->setText("Refresh Access"); m_activateButton->setEnabled(logged); }
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
            if (name.isEmpty()) name = AppSettings::userName();
            if (email.isEmpty()) email = AppSettings::userEmail();
            if (m_accountLabel) m_accountLabel->setText(name.isEmpty() ? QString("Signed in as %1").arg(email) : QString("Signed in as %1 (%2)").arg(name, email));
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
    m_statusLabel->setStyleSheet(AppSettings::darkMode() ? "" : "color: #8fda8f;");
    emit activated();
}

 

void WelcomeWidget::fetchLicense()
{
    if (!m_appwrite || !m_appwrite->isLoggedIn()) { updateAuthUI(); return; }
    if (m_verified) { saveAndContinue(); return; }
    if (m_statusLabel) m_statusLabel->setText("Checking verification status...");
    auto* r = m_appwrite->getAccount();
    connect(r, &QNetworkReply::finished, this, [this, r](){
        const bool ok = (r->error() == QNetworkReply::NoError);
        const QByteArray data = r->readAll();
        r->deleteLater();
        if (!ok) {
            if (m_statusLabel) { m_statusLabel->setText("Failed to read account status"); m_statusLabel->setStyleSheet(AppSettings::darkMode() ? "" : "color: #ff8080;"); }
            return;
        }
        QJsonParseError pe; QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
        QJsonObject acc = doc.isObject() ? doc.object() : QJsonObject();
        const bool verified = acc.value("emailVerification").toBool(false) || (acc.value("status").toString() == QStringLiteral("verified"));
        m_verified = verified;
        if (verified) {
            if (m_statusLabel) { m_statusLabel->setText("Email verified. Click 'Save & Continue' to proceed."); m_statusLabel->setStyleSheet(AppSettings::darkMode() ? "" : "color: #8fda8f;"); }
            if (m_activateButton) m_activateButton->setText("Save & Continue");
        } else {
            if (m_statusLabel) { m_statusLabel->setText("Email not verified in Appwrite yet. Verify, then click Refresh Access."); m_statusLabel->setStyleSheet(AppSettings::darkMode() ? "" : "color: #ffcc66;"); }
            if (m_activateButton) m_activateButton->setText("Refresh Access");
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

 
 
void WelcomeWidget::openAuthDialog()
{
    if (!m_appwrite) return;
    AuthDialog dlg(m_appwrite, this);
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
    QDesktopServices::openUrl(url);
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
        m_statusLabel->setStyleSheet(AppSettings::darkMode() ? "" : "color: #8fda8f;");
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
        if (pinned) it->setIcon(IconManager::icon("star")); else it->setIcon(IconManager::icon("folder-open"));
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
