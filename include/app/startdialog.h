#ifndef STARTDIALOG_H
#define STARTDIALOG_H

#include <QDialog>
#include <QString>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QSettings>

class AuthManager;

/**
 * @brief SurveyCategory - Project type categories for different surveying disciplines
 */
enum class SurveyCategory {
    Engineering,    // Construction, setting out, as-built
    Cadastral,      // Land boundaries, subdivision, title surveys
    Mining,         // Underground/opencast, volumetrics
    Topographic,    // Terrain mapping, contours
    Geodetic        // Control networks, GNSS
};

/**
 * @brief StartDialog - AutoCAD-style start/welcome screen
 * 
 * Shows on application startup with options to:
 * - Create a new project
 * - Open an existing project
 * - Open a recent project
 * - Create from template
 */
class StartDialog : public QDialog
{
    Q_OBJECT

public:
    enum StartResult {
        NewProject,
        OpenProject,
        OpenRecent,
        OpenTemplate,
        Cancelled
    };

    explicit StartDialog(AuthManager* auth = nullptr, QWidget *parent = nullptr);
    ~StartDialog() = default;

    // Get the result of the dialog
    StartResult startResult() const { return m_result; }
    
    // Get the selected file path (for OpenRecent or OpenProject)
    QString selectedFilePath() const { return m_selectedPath; }
    
    // Get the selected template name
    QString selectedTemplate() const { return m_selectedTemplate; }
    
    // Get the selected survey category
    SurveyCategory selectedCategory() const { return m_selectedCategory; }
    
    // Helper to convert category to string
    static QString categoryToString(SurveyCategory cat);
    static SurveyCategory stringToCategory(const QString& str);
    
    // Check if user wants to skip start dialog in future
    static bool shouldShowStartDialog();
    
signals:
    void newProjectRequested();
    void openProjectRequested(const QString& filePath);
    void openTemplateRequested(const QString& templateName);

private slots:
    void onNewProject();
    void onOpenProject();
    void onRecentItemDoubleClicked(QListWidgetItem* item);
    void onTemplateClicked(const QString& templateName);
    void onSkip();
    void startAnimations();
    void onCategoryChanged(int index);

private:
    void setupUI();
    void loadRecentProjects();
    void loadTemplates();
    void updateTemplatesForCategory();
    QWidget* createLeftPanel();
    QWidget* createCenterPanel();
    QWidget* createTemplateCard(const QString& name, const QString& description);
    
    // UI Components
    QListWidget* m_recentList{nullptr};
    QCheckBox* m_dontShowAgain{nullptr};
    QComboBox* m_categoryCombo{nullptr};
    QWidget* m_templatesContainer{nullptr};
    
    // Result
    StartResult m_result{Cancelled};
    QString m_selectedPath;
    QString m_selectedTemplate;
    SurveyCategory m_selectedCategory{SurveyCategory::Engineering};
    
    // Authentication
    AuthManager* m_auth{nullptr};
    
    // Settings
    QSettings m_settings{"SiteSurveyor", "SiteSurveyor"};
};

#endif // STARTDIALOG_H
