#include "welcomewidget.h"
#include "appsettings.h"
#include "iconmanager.h"

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
#include <QSettings>
#include <QUrlQuery>
#include <QUuid>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDateTime>
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

    // Right column: Settings
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

    


    rightLayout->addLayout(form);

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

    // Connections
    connect(m_disciplineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WelcomeWidget::onDisciplineChanged);

    refreshRecentList();
    reload();
}

 

void WelcomeWidget::signOut()
{
    emit disciplineChanged();
}

 

void WelcomeWidget::reload()
{
    const QString disc = AppSettings::discipline();
    if (m_disciplineCombo) {
        const QStringList discs = AppSettings::availableDisciplines();
        int idx = discs.indexOf(disc);
        if (idx >= 0) m_disciplineCombo->setCurrentIndex(idx);
    }
    refreshRecentList();
}


 


 


 
 


 
 
 

 

 



void WelcomeWidget::onDisciplineChanged(int index)
{
    Q_UNUSED(index);
    if (!m_disciplineCombo) return;
    const QString disc = m_disciplineCombo->currentText();
    AppSettings::setDiscipline(disc);
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
