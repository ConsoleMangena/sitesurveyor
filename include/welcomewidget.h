
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
class AppwriteClient;
class QNetworkReply;
class QTimer;
class QTcpServer;

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
    void openAuthDialog();
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
    void openGithubOAuth();

private:
    void updateAuthUI();
    void setFeaturesLocked(bool locked);
    void markLicensedLocally();
    void startLabelPolling();
    void stopLabelPolling();
    void stopOAuthServer();
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
    // Account panel
    AppwriteClient* m_appwrite{nullptr};
    QPushButton* m_signInButton{nullptr};
    QPushButton* m_githubButton{nullptr};
    QTcpServer* m_oauthServer{nullptr};
    QString m_oauthProvider;
    // License/activation controls
    QPushButton* m_activateButton{nullptr};
    QPushButton* m_buyButton{nullptr};
    QPushButton* m_studentButton{nullptr};
    QLabel* m_accountLabel{nullptr};
    QLabel* m_statusLabel{nullptr};
    QComboBox* m_disciplineCombo{nullptr};
    bool m_purchasePrompted{false};
    bool m_verified{false};
    // Realtime network op state to avoid UI overriding messages mid-request
    bool m_netActive{false};
    QString m_netOp;
    // Auto-refresh and diagnostics
    QTimer* m_labelPollTimer{nullptr};
    QTimer* m_chipTimer{nullptr};
    int m_chipPhase{0};
    QWidget* m_leftPane{nullptr};
    QWidget* m_rightPane{nullptr};
    QLabel* m_pollChip{nullptr};
    QLabel* m_diagEndpoint{nullptr};
    QLabel* m_diagProject{nullptr};
    QLabel* m_diagEndpointTitle{nullptr};
    QLabel* m_diagProjectTitle{nullptr};
    QLabel* m_diagAccountId{nullptr};
    QLabel* m_diagEmail{nullptr};
    QLabel* m_diagLabels{nullptr};
    bool m_showSecrets{false};
};

#endif // WELCOMEWIDGET_H
