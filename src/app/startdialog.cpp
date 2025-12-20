#include "app/startdialog.h"
#include "auth/authmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QFileDialog>
#include <QFileInfo>
#include <QDateTime>
#include <QFont>
#include <QPixmap>
#include <QIcon>
#include <QScrollArea>
#include <QPropertyAnimation>
#include <QApplication>
#include <QTimer>
#include <QSettings>
#include <QFile>

StartDialog::StartDialog(AuthManager* auth, QWidget *parent)
    : QDialog(parent)
    , m_auth(auth)
{
    setWindowTitle("SiteSurveyor - Start");
    setWindowFlags(Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    setWindowIcon(QIcon(":/branding/logo.ico"));
    setMinimumSize(900, 600);
    
    // Apply theme from settings
    QSettings settings;
    QString theme = settings.value("appearance/theme", "dark").toString();
    QString themePath = (theme == "dark") 
        ? ":/styles/dark-theme.qss" 
        : ":/styles/light-theme.qss";
    QFile styleFile(themePath);
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = QLatin1String(styleFile.readAll());
        qApp->setStyleSheet(styleSheet);
        styleFile.close();
    }
    
    // Start with transparent window for fade-in effect
    setWindowOpacity(0.0);
    
    setupUI();
    
    // Show maximized by default
    showMaximized();
    
    // Start fade-in animation after dialog is shown
    QTimer::singleShot(50, this, &StartDialog::startAnimations);
}

void StartDialog::startAnimations()
{
    // Simple window opacity fade-in animation
    QPropertyAnimation* fadeIn = new QPropertyAnimation(this, "windowOpacity");
    fadeIn->setDuration(350);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setEasingCurve(QEasingCurve::OutCubic);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
}

QString StartDialog::categoryToString(SurveyCategory cat)
{
    switch (cat) {
        case SurveyCategory::Engineering: return "Engineering";
        case SurveyCategory::Cadastral: return "Cadastral";
        case SurveyCategory::Mining: return "Mining";
        case SurveyCategory::Topographic: return "Topographic";
        case SurveyCategory::Geodetic: return "Geodetic";
    }
    return "Engineering";
}

SurveyCategory StartDialog::stringToCategory(const QString& str)
{
    if (str == "Cadastral") return SurveyCategory::Cadastral;
    if (str == "Mining") return SurveyCategory::Mining;
    if (str == "Topographic") return SurveyCategory::Topographic;
    if (str == "Geodetic") return SurveyCategory::Geodetic;
    return SurveyCategory::Engineering;
}

void StartDialog::onCategoryChanged(int index)
{
    m_selectedCategory = static_cast<SurveyCategory>(index);
    updateTemplatesForCategory();
}

void StartDialog::setupUI()
{
    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Get current theme
    QSettings themeSettings;
    QString theme = themeSettings.value("appearance/theme", "light").toString();
    bool isDark = (theme == "dark");
    
    // Main content area - use theme-appropriate colors
    QFrame* contentFrame = new QFrame();
    contentFrame->setObjectName("startDialogContent");
    if (isDark) {
        contentFrame->setStyleSheet(R"(
            #startDialogContent {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                    stop:0 #1e1e1e, stop:0.5 #252526, stop:1 #2d2d30);
            }
        )");
    } else {
        contentFrame->setStyleSheet(R"(
            #startDialogContent {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                    stop:0 #f5f5f5, stop:0.5 #fafafa, stop:1 #ffffff);
            }
        )");
    }
    
    QHBoxLayout* contentLayout = new QHBoxLayout(contentFrame);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    
    // Left panel (logo, buttons)
    QWidget* leftPanel = createLeftPanel();
    leftPanel->setObjectName("leftPanel");
    contentLayout->addWidget(leftPanel);
    
    // Separator line
    QFrame* separator = new QFrame();
    separator->setFrameShape(QFrame::VLine);
    separator->setStyleSheet(isDark ? "background-color: #3c3c3c; max-width: 1px;" : "background-color: #e0e0e0; max-width: 1px;");
    contentLayout->addWidget(separator);
    
    // Center panel (recent projects + templates)
    QWidget* centerPanel = createCenterPanel();
    centerPanel->setObjectName("centerPanel");
    contentLayout->addWidget(centerPanel, 1);
    
    mainLayout->addWidget(contentFrame, 1);
}


QWidget* StartDialog::createLeftPanel()
{
    // Get current theme
    QSettings themeSettings;
    QString theme = themeSettings.value("appearance/theme", "light").toString();
    bool isDark = (theme == "dark");
    
    QWidget* leftPanel = new QWidget();
    leftPanel->setFixedWidth(280);
    leftPanel->setStyleSheet("background: transparent;");
    
    QVBoxLayout* layout = new QVBoxLayout(leftPanel);
    layout->setContentsMargins(30, 40, 30, 40);
    layout->setSpacing(20);
    
    // Logo
    QLabel* logoLabel = new QLabel();
    QPixmap logo(":/branding/logo-main.png");
    if (!logo.isNull()) {
        logoLabel->setPixmap(logo.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    logoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(logoLabel);
    
    // App title
    QLabel* titleLabel = new QLabel("SiteSurveyor");
    titleLabel->setStyleSheet(QString(R"(
        font-size: 26px;
        font-weight: 300;
        color: %1;
        letter-spacing: 1px;
    )").arg(isDark ? "#ffffff" : "#1a1a1a"));
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);
    
    // Version
    QLabel* versionLabel = new QLabel("Professional Edition v1.0.8");
    versionLabel->setStyleSheet(QString("font-size: 11px; color: %1;").arg(isDark ? "#888888" : "#666666"));
    versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(versionLabel);
    
    // User Info Section
    if (m_auth && m_auth->isAuthenticated()) {
        const UserProfile& profile = m_auth->userProfile();
        
        QFrame* userFrame = new QFrame();
        userFrame->setStyleSheet(QString(R"(
            QFrame {
                background-color: %1;
                border-radius: 10px;
                margin-top: 8px;
            }
        )").arg(isDark ? "#363636" : "#f0f0f0"));
        
        QVBoxLayout* userLayout = new QVBoxLayout(userFrame);
        userLayout->setContentsMargins(14, 12, 14, 12);
        userLayout->setSpacing(6);
        
        
        // Full Name (header style)
        QString displayName = profile.name.isEmpty() ? profile.username : profile.name;
        if (!displayName.isEmpty()) {
            QLabel* nameLabel = new QLabel(displayName);
            nameLabel->setStyleSheet(QString("font-size: 13px; font-weight: bold; color: %1;").arg(isDark ? "#ffffff" : "#222222"));
            userLayout->addWidget(nameLabel);
        }
        
        // Username
        if (!profile.username.isEmpty() && profile.username != displayName) {
            QLabel* usernameLabel = new QLabel("@" + profile.username);
            usernameLabel->setStyleSheet(QString("font-size: 11px; color: %1;").arg(isDark ? "#888888" : "#666666"));
            userLayout->addWidget(usernameLabel);
        }
        
        // Organization + Type on one line
        QString details;
        if (!profile.organization.isEmpty()) {
            details = profile.organization;
        }
        if (!profile.userType.isEmpty()) {
            if (!details.isEmpty()) details += " • ";
            details += profile.userType;
        }
        if (!details.isEmpty()) {
            QLabel* detailsLabel = new QLabel(details);
            detailsLabel->setStyleSheet(QString("font-size: 10px; color: %1;").arg(isDark ? "#666666" : "#888888"));
            userLayout->addWidget(detailsLabel);
        }
        
        // Location (City, Country)
        QString location;
        if (!profile.city.isEmpty() && !profile.country.isEmpty()) {
            location = profile.city + ", " + profile.country;
        } else if (!profile.city.isEmpty()) {
            location = profile.city;
        } else if (!profile.country.isEmpty()) {
            location = profile.country;
        }
        if (!location.isEmpty()) {
            QLabel* locationLabel = new QLabel(location);
            locationLabel->setStyleSheet(QString("font-size: 10px; color: %1;").arg(isDark ? "#666666" : "#888888"));
            userLayout->addWidget(locationLabel);
        }
        
        layout->addWidget(userFrame);
    }
    
    layout->addSpacing(20);
    
    // New Project button
    QPushButton* newBtn = new QPushButton("  + New Project");
    newBtn->setIcon(QIcon(":/icons/file-plus.svg"));
    newBtn->setIconSize(QSize(18, 18));
    newBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #0078d4;
            color: #ffffff;
            border: none;
            border-radius: 8px;
            padding: 14px 20px;
            font-size: 14px;
            font-weight: 500;
            text-align: left;
        }
        QPushButton:hover {
            background-color: #1a88e4;
        }
        QPushButton:pressed {
            background-color: #0068c4;
        }
    )");
    connect(newBtn, &QPushButton::clicked, this, &StartDialog::onNewProject);
    layout->addWidget(newBtn);
    
    // Open Project button
    QPushButton* openBtn = new QPushButton("  Open Project...");
    openBtn->setIcon(QIcon(":/icons/folder-open.svg"));
    openBtn->setIconSize(QSize(18, 18));
    if (isDark) {
        openBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #2d2d30;
                color: #ffffff;
                border: 1px solid #3c3c3c;
                border-radius: 8px;
                padding: 14px 20px;
                font-size: 14px;
                text-align: left;
            }
            QPushButton:hover {
                background-color: #3c3c3c;
                border-color: #4c4c4c;
            }
            QPushButton:pressed {
                background-color: #1e1e1e;
            }
        )");
    } else {
        openBtn->setStyleSheet(R"(
            QPushButton {
                background-color: #ffffff;
                color: #1a1a1a;
                border: 1px solid #d0d0d0;
                border-radius: 8px;
                padding: 14px 20px;
                font-size: 14px;
                text-align: left;
            }
            QPushButton:hover {
                background-color: #f0f0f0;
                border-color: #c0c0c0;
            }
            QPushButton:pressed {
                background-color: #e0e0e0;
            }
        )");
    }
    connect(openBtn, &QPushButton::clicked, this, &StartDialog::onOpenProject);
    layout->addWidget(openBtn);
    
    layout->addStretch();
    
    // Company branding
    QLabel* brandLabel = new QLabel("Eineva Incorporated");
    brandLabel->setStyleSheet(QString("font-size: 10px; color: %1;").arg(isDark ? "#666666" : "#888888"));
    brandLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(brandLabel);
    
    return leftPanel;
}

QWidget* StartDialog::createCenterPanel()
{
    // Get current theme
    QSettings themeSettings;
    QString theme = themeSettings.value("appearance/theme", "light").toString();
    bool isDark = (theme == "dark");
    
    // Main container for the center panel
    QWidget* centerPanel = new QWidget();
    centerPanel->setStyleSheet("background: transparent;");
    
    QVBoxLayout* mainLayout = new QVBoxLayout(centerPanel);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Create scroll area for content
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    
    // Content widget inside scroll area
    QWidget* contentWidget = new QWidget();
    contentWidget->setStyleSheet("background: transparent;");
    
    QVBoxLayout* layout = new QVBoxLayout(contentWidget);
    layout->setContentsMargins(40, 40, 40, 40);
    layout->setSpacing(24);
    
    // Recent Projects section
    QLabel* recentTitle = new QLabel("Recent Projects");
    recentTitle->setStyleSheet(QString("font-size: 18px; font-weight: 600; color: %1; margin-bottom: 8px;").arg(isDark ? "#ffffff" : "#1a1a1a"));
    layout->addWidget(recentTitle);
    
    m_recentList = new QListWidget();
    if (isDark) {
        m_recentList->setStyleSheet(R"(
            QListWidget {
                background-color: #252526;
                border: 1px solid #3c3c3c;
                border-radius: 10px;
                padding: 10px;
            }
            QListWidget::item {
                background-color: transparent;
                color: #ffffff;
                border-radius: 8px;
                padding: 14px;
                margin: 3px 0;
            }
            QListWidget::item:hover {
                background-color: #3c3c3c;
            }
            QListWidget::item:selected {
                background-color: #0078d4;
            }
        )");
    } else {
        m_recentList->setStyleSheet(R"(
            QListWidget {
                background-color: #ffffff;
                border: 1px solid #e0e0e0;
                border-radius: 10px;
                padding: 10px;
            }
            QListWidget::item {
                background-color: transparent;
                color: #1a1a1a;
                border-radius: 8px;
                padding: 14px;
                margin: 3px 0;
            }
            QListWidget::item:hover {
                background-color: #f0f0f0;
            }
            QListWidget::item:selected {
                background-color: #0078d4;
                color: #ffffff;
            }
        )");
    }
    m_recentList->setMinimumHeight(200);
    m_recentList->setMaximumHeight(280);
    connect(m_recentList, &QListWidget::itemDoubleClicked, this, &StartDialog::onRecentItemDoubleClicked);
    layout->addWidget(m_recentList);
    
    loadRecentProjects();
    
    layout->addSpacing(16);
    
    // Project Type section
    QLabel* categoryTitle = new QLabel("Project Type");
    categoryTitle->setStyleSheet(QString("font-size: 18px; font-weight: 600; color: %1; margin-bottom: 8px;").arg(isDark ? "#ffffff" : "#1a1a1a"));
    layout->addWidget(categoryTitle);
    
    // Category selector dropdown
    m_categoryCombo = new QComboBox();
    m_categoryCombo->addItem("Engineering Surveying", static_cast<int>(SurveyCategory::Engineering));
    m_categoryCombo->addItem("Cadastral Surveying", static_cast<int>(SurveyCategory::Cadastral));
    m_categoryCombo->addItem("Mining Surveying", static_cast<int>(SurveyCategory::Mining));
    m_categoryCombo->addItem("Topographic Surveying", static_cast<int>(SurveyCategory::Topographic));
    m_categoryCombo->addItem("Geodetic Surveying", static_cast<int>(SurveyCategory::Geodetic));
    if (isDark) {
        m_categoryCombo->setStyleSheet(R"(
            QComboBox {
                background-color: #252526;
                border: 1px solid #3c3c3c;
                border-radius: 8px;
                padding: 12px 16px;
                font-size: 14px;
                color: #ffffff;
                min-width: 280px;
            }
            QComboBox:hover { border-color: #0078d4; }
            QComboBox::drop-down { border: none; padding-right: 12px; }
            QComboBox::down-arrow { image: none; border-left: 5px solid transparent; border-right: 5px solid transparent; border-top: 6px solid #888888; margin-right: 12px; }
            QComboBox QAbstractItemView { background-color: #252526; border: 1px solid #3c3c3c; selection-background-color: #0078d4; color: #ffffff; padding: 6px; }
            QComboBox QAbstractItemView::item { padding: 10px; }
            QComboBox QAbstractItemView::item:hover { background-color: #3c3c3c; }
        )");
    } else {
        m_categoryCombo->setStyleSheet(R"(
            QComboBox {
                background-color: #ffffff;
                border: 1px solid #d0d0d0;
                border-radius: 8px;
                padding: 12px 16px;
                font-size: 14px;
                color: #1a1a1a;
                min-width: 280px;
            }
            QComboBox:hover { border-color: #0078d4; }
            QComboBox::drop-down { border: none; padding-right: 12px; }
            QComboBox::down-arrow { image: none; border-left: 5px solid transparent; border-right: 5px solid transparent; border-top: 6px solid #666666; margin-right: 12px; }
            QComboBox QAbstractItemView { background-color: #ffffff; border: 1px solid #d0d0d0; selection-background-color: #0078d4; selection-color: #ffffff; color: #1a1a1a; padding: 6px; }
            QComboBox QAbstractItemView::item { padding: 10px; }
            QComboBox QAbstractItemView::item:hover { background-color: #f0f0f0; }
        )");
    }
    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &StartDialog::onCategoryChanged);
    layout->addWidget(m_categoryCombo);
    
    layout->addSpacing(24);
    
    // Templates section
    QLabel* templatesTitle = new QLabel("Start from Template");
    templatesTitle->setStyleSheet(QString("font-size: 18px; font-weight: 600; color: %1; margin-bottom: 8px;").arg(isDark ? "#ffffff" : "#1a1a1a"));
    layout->addWidget(templatesTitle);
    
    // Templates container (will be populated dynamically)
    m_templatesContainer = new QWidget();
    layout->addWidget(m_templatesContainer);
    
    updateTemplatesForCategory();
    
    layout->addStretch();
    
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);
    
    return centerPanel;
}


void StartDialog::updateTemplatesForCategory()
{
    // Clear existing templates
    if (m_templatesContainer->layout()) {
        QLayoutItem* item;
        while ((item = m_templatesContainer->layout()->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete m_templatesContainer->layout();
    }
    
    QHBoxLayout* templatesLayout = new QHBoxLayout(m_templatesContainer);
    templatesLayout->setSpacing(12);
    templatesLayout->setContentsMargins(0, 0, 0, 0);
    
    switch (m_selectedCategory) {
        case SurveyCategory::Engineering:
            templatesLayout->addWidget(createTemplateCard("Blank", "Empty engineering project"));
            templatesLayout->addWidget(createTemplateCard("Setting Out", "Construction setout layout"));
            templatesLayout->addWidget(createTemplateCard("As-Built", "As-built survey"));
            break;
            
        case SurveyCategory::Cadastral:
            templatesLayout->addWidget(createTemplateCard("Blank", "Empty cadastral project"));
            templatesLayout->addWidget(createTemplateCard("Subdivision", "Land subdivision survey"));
            templatesLayout->addWidget(createTemplateCard("Diagram", "SG diagram survey"));
            break;
            
        case SurveyCategory::Mining:
            templatesLayout->addWidget(createTemplateCard("Blank", "Empty mining project"));
            templatesLayout->addWidget(createTemplateCard("Opencast", "Surface mining survey"));
            templatesLayout->addWidget(createTemplateCard("Underground", "Underground survey"));
            break;
            
        case SurveyCategory::Topographic:
            templatesLayout->addWidget(createTemplateCard("Blank", "Empty topo project"));
            templatesLayout->addWidget(createTemplateCard("Feature Survey", "Topographic features"));
            templatesLayout->addWidget(createTemplateCard("Contour", "Contour mapping"));
            break;
            
        case SurveyCategory::Geodetic:
            templatesLayout->addWidget(createTemplateCard("Blank", "Empty geodetic project"));
            templatesLayout->addWidget(createTemplateCard("Control Network", "Control point network"));
            templatesLayout->addWidget(createTemplateCard("GNSS Survey", "GPS/GNSS observations"));
            break;
    }
    
    templatesLayout->addStretch();
}


QWidget* StartDialog::createTemplateCard(const QString& name, const QString& description)
{
    // Get current theme
    QSettings themeSettings;
    QString theme = themeSettings.value("appearance/theme", "light").toString();
    bool isDark = (theme == "dark");
    
    QFrame* card = new QFrame();
    card->setFixedSize(180, 110);
    card->setCursor(Qt::PointingHandCursor);
    if (isDark) {
        card->setStyleSheet(R"(
            QFrame {
                background-color: rgba(45, 45, 48, 0.8);
                border: none;
                border-radius: 10px;
            }
            QFrame:hover {
                background-color: rgba(60, 60, 60, 0.9);
            }
        )");
    } else {
        card->setStyleSheet(R"(
            QFrame {
                background-color: rgba(255, 255, 255, 0.8);
                border: 1px solid #e0e0e0;
                border-radius: 10px;
            }
            QFrame:hover {
                background-color: rgba(240, 240, 240, 0.9);
                border-color: #c0c0c0;
            }
        )");
    }
    
    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(14, 12, 14, 12);
    cardLayout->setSpacing(4);
    
    // Template name
    QLabel* nameLabel = new QLabel(name);
    nameLabel->setStyleSheet(QString("font-size: 14px; font-weight: 600; color: %1; background: transparent; border: none;")
        .arg(isDark ? "#ffffff" : "#1a1a1a"));
    cardLayout->addWidget(nameLabel);
    
    // Description
    QLabel* descLabel = new QLabel(description);
    descLabel->setStyleSheet(QString("font-size: 11px; color: %1; background: transparent; border: none;")
        .arg(isDark ? "#a0a0a0" : "#666666"));
    descLabel->setWordWrap(true);
    cardLayout->addWidget(descLabel);
    
    cardLayout->addStretch();
    
    // Invisible click area overlay
    QPushButton* clickArea = new QPushButton(card);
    clickArea->setGeometry(0, 0, 180, 110);
    clickArea->setStyleSheet("background: transparent; border: none;");
    clickArea->setCursor(Qt::PointingHandCursor);
    
    // Pass the actual template name for selection
    connect(clickArea, &QPushButton::clicked, this, [this, name]() {
        onTemplateClicked(name);
    });

    
    return card;
}


void StartDialog::loadRecentProjects()
{
    m_recentList->clear();
    
    QStringList recentFiles = m_settings.value("recentProjects").toStringList();
    
    if (recentFiles.isEmpty()) {
        QListWidgetItem* emptyItem = new QListWidgetItem("No recent projects");
        emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
        emptyItem->setForeground(QColor("#6a6a8a"));
        m_recentList->addItem(emptyItem);
        return;
    }
    
    for (const QString& filePath : recentFiles) {
        if (!QFile::exists(filePath)) continue;
        
        QFileInfo info(filePath);
        QString displayName = info.fileName();
        QString lastModified = info.lastModified().toString("MMM d, yyyy");
        QString directory = info.absolutePath();
        
        // Shorten directory path if too long
        if (directory.length() > 50) {
            directory = "..." + directory.right(47);
        }
        
        QListWidgetItem* item = new QListWidgetItem();
        item->setText(QString("%1\n%2  •  %3").arg(displayName, directory, lastModified));
        item->setData(Qt::UserRole, filePath);
        item->setIcon(QIcon(":/icons/file-plus.svg"));
        
        m_recentList->addItem(item);
    }
}

void StartDialog::loadTemplates()
{
    // Templates are hardcoded for now, could be loaded from resources
}

void StartDialog::onNewProject()
{
    m_result = NewProject;
    m_selectedPath.clear();
    m_selectedTemplate.clear();
    accept();
}

void StartDialog::onOpenProject()
{
    QString filePath = QFileDialog::getOpenFileName(this,
        "Open Project", QString(),
        "SiteSurveyor Project (*.ssp);;All Files (*)");
    
    if (!filePath.isEmpty()) {
        m_result = OpenProject;
        m_selectedPath = filePath;
        accept();
    }
}

void StartDialog::onRecentItemDoubleClicked(QListWidgetItem* item)
{
    QString filePath = item->data(Qt::UserRole).toString();
    if (!filePath.isEmpty() && QFile::exists(filePath)) {
        m_result = OpenRecent;
        m_selectedPath = filePath;
        accept();
    }
}

void StartDialog::onTemplateClicked(const QString& templateName)
{
    m_result = OpenTemplate;
    m_selectedTemplate = templateName;
    accept();
}

void StartDialog::onSkip()
{
    m_result = NewProject;  // Skip means open blank
    accept();
}

bool StartDialog::shouldShowStartDialog()
{
    QSettings settings("SiteSurveyor", "SiteSurveyor");
    return settings.value("showStartDialog", true).toBool();
}
