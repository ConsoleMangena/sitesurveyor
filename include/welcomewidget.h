
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

signals:
    void disciplineChanged();
    // Start actions
    void newProjectRequested();
    void openProjectRequested();
    void openPathRequested(const QString& path);
    void openTemplateRequested(const QString& resourcePath);
    void openPreferencesRequested();

private slots:
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
    QComboBox* m_disciplineCombo{nullptr};
    QWidget* m_leftPane{nullptr};
    QWidget* m_rightPane{nullptr};
    
};

#endif // WELCOMEWIDGET_H
