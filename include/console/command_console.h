#ifndef COMMAND_CONSOLE_H
#define COMMAND_CONSOLE_H

#include <QDockWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QCompleter>
#include <QStringList>
#include <QStringListModel>

class CanvasWidget;
class MainWindow;

/**
 * @brief CommandConsole - AutoCAD-style command line interface
 * 
 * Provides a text-based interface for entering commands like:
 * - PEG 1000,2000 - Add survey point
 * - ZOOM FIT - Zoom to fit
 * - LINE - Start line drawing
 */
class CommandConsole : public QDockWidget
{
    Q_OBJECT
    
public:
    explicit CommandConsole(CanvasWidget* canvas, MainWindow* mainWindow, QWidget* parent = nullptr);
    ~CommandConsole();
    
    // Show message in console
    void appendMessage(const QString& message, const QString& type = "info");
    
    // Show prompt for input
    void showPrompt(const QString& prompt);
    
    // Focus the input field
    void focusInput();
    
signals:
    // Emitted when user enters a command
    void commandEntered(const QString& command);
    
    // Emitted when coordinate input is received during a command
    void coordinateInput(const QPointF& point);
    
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    
private slots:
    void onCommandEntered();
    void processCommand(const QString& command);
    
private:
    void setupUI();
    void setupCompleter();
    void executeCommand(const QString& cmd, const QStringList& args);
    
    // Parse coordinate input (e.g., "1000,2000" or "1000 2000")
    bool parseCoordinate(const QString& input, double& x, double& y);
    
    // Resolve a point from an argument (can be "x,y" or a Peg Name)
    bool getPointFromArg(const QString& arg, QPointF& point);
    
    // Format coordinate string based on Swap X/Y setting
    QString formatCoordinate(double x, double y) const;
    QString formatCoordinate(const QPointF& point) const;
    
    // Command handlers
    void cmdPeg(const QStringList& args);
    void cmdLine(const QStringList& args);
    void cmdPolyline(const QStringList& args);
    void cmdCircle(const QStringList& args);
    void cmdZoom(const QStringList& args);
    void cmdPan(const QStringList& args);
    void cmdSelect(const QStringList& args);
    void cmdDelete(const QStringList& args);
    void cmdUndo(const QStringList& args);
    void cmdRedo(const QStringList& args);
    void cmdGrid(const QStringList& args);
    void cmdSnap(const QStringList& args);
    void cmdLayer(const QStringList& args);
    void cmdStation(const QStringList& args);
    void cmdStakeout(const QStringList& args);
    void cmdPolar(const QStringList& args);
    void cmdJoin(const QStringList& args);
    void cmdArea(const QStringList& args);
    void cmdDistance(const QStringList& args);
    void cmdHelp(const QStringList& args);
    void cmdClear(const QStringList& args);
    
    CanvasWidget* m_canvas;
    MainWindow* m_mainWindow;
    
    QTextEdit* m_historyDisplay;
    QLineEdit* m_commandInput;
    QCompleter* m_completer;
    QStringListModel* m_completerModel;
    
    QStringList m_commandHistory;
    int m_historyIndex;
    
    QString m_currentPrompt;
    QString m_pendingCommand;  // Command waiting for additional input
    
    // List of available commands for autocomplete
    QStringList m_availableCommands;
};

#endif // COMMAND_CONSOLE_H
