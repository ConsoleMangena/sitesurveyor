#ifndef WELCOMEWIDGET_H
#define WELCOMEWIDGET_H

#include <QWidget>

class QLineEdit;
class QCheckBox;
class QPushButton;
class QLabel;
class QComboBox;
class QTabWidget;
class QListWidget;
class QListWidgetItem;
class QToolButton;

class WelcomeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WelcomeWidget(QWidget* parent = nullptr);
    void reload();

signals:
    void activated();
    void disciplineChanged();
    // Start actions
    void newProjectRequested();
    void openProjectRequested();
    void openPathRequested(const QString& path);

private slots:
    void saveKey();
    void toggleShow(bool on);
    void openBuyPage();
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
    // License panel (right column)
    QLineEdit* m_keyEdit{nullptr};
    QCheckBox* m_showCheck{nullptr};
    QPushButton* m_activateButton{nullptr};
    QPushButton* m_buyButton{nullptr};
    QLabel* m_statusLabel{nullptr};
    QComboBox* m_disciplineCombo{nullptr};
};

#endif // WELCOMEWIDGET_H
