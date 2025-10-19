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
            "#welcomeCard { border: 1px solid #555; border-radius: 3px; padding: 4px; background-color: #1e1e1e; font-size: 10px; font-family: JetBrains Mono, 'Fira Code', 'Cascadia Code', 'Source Code Pro', 'DejaVu Sans Mono', Monospace; }"
            "#welcomeCard QLineEdit { background-color: #2a2a2a; border: 1px solid #555; padding: 1px 2px; }"
            "#welcomeCard QComboBox, #welcomeCard QLineEdit, #welcomeCard QPushButton, #welcomeCard QLabel { font-size: 10px; }"
            "#welcomeCard QLabel { margin: 0; }"
            "#welcomeCard QPushButton { padding: 2px 4px; }"
        );
    } else {
        card->setStyleSheet(
            "#welcomeCard { border: 1px solid #777; border-radius: 3px; padding: 4px; font-size: 10px; font-family: JetBrains Mono, 'Fira Code', 'Cascadia Code', 'Source Code Pro', 'DejaVu Sans Mono', Monospace; }"
            "#welcomeCard QComboBox, #welcomeCard QLineEdit, #welcomeCard QPushButton, #welcomeCard QLabel { font-size: 10px; }"
            "#welcomeCard QLabel { margin: 0; }"
            "#welcomeCard QLineEdit { padding: 1px 2px; }"
            "#welcomeCard QPushButton { padding: 2px 4px; }"
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
        titleFont.setFamilies(QStringList()
            << "JetBrains Mono" << "Fira Code" << "Cascadia Code" << "Source Code Pro" << "DejaVu Sans Mono" << "Monospace");
        titleFont.setPointSize(14);
        titleFont.setWeight(QFont::DemiBold);
        m_title->setFont(titleFont);
        m_title->setStyleSheet("margin: 0px;");
        header->addWidget(m_title, 0, Qt::AlignVCenter | Qt::AlignLeft);
        header->addStretch();

        // Desktop-themed logo at the top-right corner
        QLabel* logo = new QLabel(this);
        QPixmap pm(24, 24);
        pm.fill(Qt::transparent);
        QIcon ico = IconManager::icon("desktop");
        logo->setPixmap(ico.pixmap(24, 24));
        header->addWidget(logo, 0, Qt::AlignTop | Qt::AlignRight);
        cardLayout->addLayout(header);
    }

    m_description = new QLabel(this);
    m_description->setWordWrap(true);
    m_description->setText("Modern Geomatics Desktop for Surveying and Mapping.");
    QFont descFont;
    descFont.setFamilies(QStringList()
        << "JetBrains Mono" << "Fira Code" << "Cascadia Code" << "Source Code Pro" << "DejaVu Sans Mono" << "Monospace");
    descFont.setPointSize(10);
    m_description->setFont(descFont);
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
        QLabel* whatsNew = new QLabel("What's New: Visit documentation for latest features.", this);
        whatsNew->setWordWrap(true);
        l->addWidget(whatsNew);
        QLabel* vids = new QLabel("Learning videos and tips coming soon.", this);
        vids->setWordWrap(true);
        l->addWidget(vids);
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

    // License field (inline with Show)
    QWidget* licField = new QWidget(this);
    auto* licRow = new QHBoxLayout(licField);
    licRow->setContentsMargins(0,0,0,0);
    licRow->setSpacing(2);
    m_keyEdit = new QLineEdit(this);
    m_keyEdit->setPlaceholderText("XXXX-XXXX-XXXX-XXXX");
    m_keyEdit->setEchoMode(QLineEdit::Password);
    m_keyEdit->setClearButtonEnabled(true);
    m_keyEdit->setFont(descFont);
    m_showCheck = new QCheckBox("Show", this);
    licRow->addWidget(m_keyEdit, 1);
    licRow->addWidget(m_showCheck);
    form->addRow(new QLabel("License:", this), licField);

    // Status line in form
    m_statusLabel = new QLabel("", this);
    form->addRow(new QLabel("Status:", this), m_statusLabel);

    cardLayout->addLayout(form);

    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(2);
    buttons->addStretch();
    m_buyButton = new QPushButton("Buy License", this);
    m_buyButton->setIcon(IconManager::icon("credit-card"));
    m_buyButton->setFont(descFont);
    buttons->addWidget(m_buyButton);
    m_activateButton = new QPushButton("Activate", this);
    m_activateButton->setFont(descFont);
    m_activateButton->setDefault(true);
    buttons->addWidget(m_activateButton);
    cardLayout->addLayout(buttons);

    connect(m_showCheck, &QCheckBox::toggled, this, &WelcomeWidget::toggleShow);
    connect(m_disciplineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WelcomeWidget::onDisciplineChanged);
    connect(m_buyButton, &QPushButton::clicked, this, &WelcomeWidget::openBuyPage);
    connect(m_activateButton, &QPushButton::clicked, this, &WelcomeWidget::saveKey);

    refreshRecentList();
    reload();
}

void WelcomeWidget::reload()
{
    const QString disc = AppSettings::discipline();
    if (m_disciplineCombo) {
        const QStringList discs = AppSettings::availableDisciplines();
        int idx = discs.indexOf(disc);
        if (idx >= 0) m_disciplineCombo->setCurrentIndex(idx);
    }
    if (m_keyEdit) m_keyEdit->clear();
    const bool has = AppSettings::hasLicenseFor(disc);
    if (has) {
        m_statusLabel->setText(QString("A license key is saved for %1.").arg(disc));
        if (AppSettings::darkMode()) m_statusLabel->setStyleSheet(""); else m_statusLabel->setStyleSheet("color: #8fda8f;");
    } else {
        m_statusLabel->setText("");
        m_statusLabel->setStyleSheet("");
    }
    refreshRecentList();
}

void WelcomeWidget::saveKey()
{
    const QString key = m_keyEdit->text().trimmed();
    if (key.isEmpty() || key.size() < 8) {
        m_statusLabel->setText("Please enter a valid license key.");
        m_statusLabel->setStyleSheet(AppSettings::darkMode() ? "" : "color: #ff8080;");
        return;
    }
    const QString disc = m_disciplineCombo ? m_disciplineCombo->currentText() : AppSettings::discipline();
    AppSettings::setDiscipline(disc);
    // Enforce discipline-specific license prefix (e.g., ES-, CS-, RS-, GM-)
    const QString prefix = AppSettings::licensePrefixFor(disc);
    const QString upperKey = key.toUpper();
    if (!prefix.isEmpty() && !upperKey.startsWith(prefix + "-")) {
        m_statusLabel->setText(QString("Invalid license for %1 (expected prefix %2-).")
                               .arg(disc, prefix));
        m_statusLabel->setStyleSheet(AppSettings::darkMode() ? "" : "color: #ff8080;");
        return;
    }
    AppSettings::setLicenseKeyFor(disc, key);
    const bool ok = AppSettings::verifyLicenseFor(disc, key);
    if (!ok) {
        m_statusLabel->setText("License verification failed. Please try again.");
        m_statusLabel->setStyleSheet(AppSettings::darkMode() ? "" : "color: #ff8080;");
        return;
    }
    if (m_keyEdit) m_keyEdit->clear();
    m_statusLabel->setText(QString("License key saved for %1. Welcome!").arg(disc));
    m_statusLabel->setStyleSheet(AppSettings::darkMode() ? "" : "color: #8fda8f;");
    emit activated();
}

void WelcomeWidget::toggleShow(bool on)
{
    m_keyEdit->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
}

void WelcomeWidget::openBuyPage()
{
    // TODO: Replace with your real purchase URL
    const QString disc = m_disciplineCombo ? m_disciplineCombo->currentText() : AppSettings::discipline();
    const QUrl url(QStringLiteral("https://sitesurveyor.app/buy?discipline=%1").arg(QString::fromUtf8(QUrl::toPercentEncoding(disc))));
    QDesktopServices::openUrl(url);
}

void WelcomeWidget::onDisciplineChanged(int index)
{
    Q_UNUSED(index);
    if (!m_disciplineCombo) return;
    const QString disc = m_disciplineCombo->currentText();
    AppSettings::setDiscipline(disc);
    if (m_keyEdit) m_keyEdit->clear();
    const bool has = AppSettings::hasLicenseFor(disc);
    if (has) {
        m_statusLabel->setText(QString("A license key is saved for %1.").arg(disc));
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
