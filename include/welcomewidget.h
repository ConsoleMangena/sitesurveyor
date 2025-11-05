
#ifndef WELCOMEWIDGET_H
#define WELCOMEWIDGET_H

#include <QWidget>
#include <QString>

class QLineEdit;
class QPushButton;
class QLabel;
class QComboBox;
class QTabWidget;
class QListWidget;
class QListWidgetItem;
class QToolButton;
class QTimer;

class WelcomeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WelcomeWidget(QWidget* parent = nullptr);
    void reload();
    void signOut();

signals:
    void activated();
    void disciplineChanged();
    // Start actions
    void newProjectRequested();
    void openProjectRequested();
    void openPathRequested(const QString& path);

private slots:
    void fetchLicense();
    void saveAndContinue();
    // UI helpers
    void openBuyPage();
    void openStudentPage();
    void onDisciplineChanged(int index);
    void onCreateNew();
    void onOpenProject();
    void onOpenTemplate();
    void onSearchTextChanged(const QString& text);
    void onRecentItemActivated(QListWidgetItem* item);
    void onPinSelected();
    void onUnpinSelected();
    void refreshRecentList();
    

private:
    void updateAuthUI();
    void setFeaturesLocked(bool locked);
    void markLicensedLocally();
    
    // Header
    QLabel* m_title{nullptr};
    QLabel* m_description{nullptr};
    QToolButton* m_accountButton{nullptr};
    // Tabs
    QTabWidget* m_tabs{nullptr};
    // Start tab widgets
    QToolButton* m_newButton{nullptr};
    QToolButton* m_templateButton{nullptr};
    QToolButton* m_openButton{nullptr};
    QLineEdit* m_searchEdit{nullptr};
    QListWidget* m_recentList{nullptr};
    QToolButton* m_pinButton{nullptr};
    QToolButton* m_unpinButton{nullptr};
    // Account panel (offline)
    QLineEdit* m_licenseEdit{nullptr};
    QPushButton* m_activateButton{nullptr};
    QPushButton* m_buyButton{nullptr};
    QPushButton* m_studentButton{nullptr};
    QLabel* m_accountLabel{nullptr};
    QLabel* m_statusLabel{nullptr};
    QComboBox* m_disciplineCombo{nullptr};
    bool m_purchasePrompted{false};
    bool m_verified{false};
    QWidget* m_leftPane{nullptr};
    QWidget* m_rightPane{nullptr};
    
};

#endif // WELCOMEWIDGET_H
