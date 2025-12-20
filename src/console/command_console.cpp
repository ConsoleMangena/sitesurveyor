#include "console/command_console.h"
#include "canvas/canvaswidget.h"
#include "app/mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QScrollBar>
#include <QSettings>
#include <QApplication>
#include <cmath>

CommandConsole::CommandConsole(CanvasWidget* canvas, MainWindow* mainWindow, QWidget* parent)
    : QDockWidget("Command", parent)
    , m_canvas(canvas)
    , m_mainWindow(mainWindow)
    , m_historyIndex(-1)
{
    // Initialize available commands for autocomplete
    m_availableCommands = {
        "PEG", "POINT", "LINE", "POLYLINE", "PLINE", "CIRCLE", "ARC",
        "ZOOM", "PAN", "SELECT", "DELETE", "ERASE",
        "UNDO", "REDO", "GRID", "SNAP", "ORTHO",
        "LAYER", "STATION", "STAKEOUT", "POLAR", "JOIN",
        "HELP", "CLEAR", "CLS", "REGEN", "REDRAW",
        "MOVE", "COPY", "ROTATE", "SCALE", "MIRROR",
        "OFFSET", "TRIM", "EXTEND", "FILLET", "CHAMFER",
        "DISTANCE", "AREA", "LIST", "ID",
        "SAVE", "OPEN", "NEW", "EXPORT", "IMPORT",
        "SETTINGS", "OPTIONS", "FIT", "EXTENTS",
        "CONTOUR", "VOLUME", "TIN", "PEGS"
    };
    m_availableCommands.sort();

    
    setupUI();
    setupCompleter();
    
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    
    // Welcome message
    appendMessage("SiteSurveyor Command Console. Type HELP for available commands.", "system");
    showPrompt("Command:");
}

CommandConsole::~CommandConsole()
{
}

void CommandConsole::setupUI()
{
    // Get current theme
    QSettings settings;
    QString theme = settings.value("appearance/theme", "light").toString();
    bool isDark = (theme == "dark");
    
    // Theme colors
    QString bgColor = isDark ? "#1e1e1e" : "#ffffff";
    QString textColor = isDark ? "#00cc00" : "#006600";
    QString borderColor = isDark ? "#00cc00" : "#006600";
    QString promptColor = isDark ? "#00ff00" : "#009900";
    
    QWidget* container = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);
    
    // History display (read-only text area)
    m_historyDisplay = new QTextEdit();
    m_historyDisplay->setReadOnly(true);
    m_historyDisplay->setMaximumHeight(150);
    m_historyDisplay->setMinimumHeight(80);
    m_historyDisplay->setStyleSheet(
        QString(
            "QTextEdit {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: 1px solid %3;"
            "  border-radius: 0px;"
            "  font-family: monospace;"
            "  font-size: 13px;"
            "  padding: 6px;"
            "  selection-background-color: %2;"
            "  selection-color: %1;"
            "}"
        ).arg(bgColor, textColor, borderColor)
    );
    layout->addWidget(m_historyDisplay);
    
    // Command input area
    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->setSpacing(6);
    
    // Prompt label
    QLabel* promptLabel = new QLabel(">");
    promptLabel->setStyleSheet(
        QString(
            "font-family: monospace;"
            "font-size: 14px;"
            "font-weight: bold;"
            "color: %1;"
            "background: transparent;"
        ).arg(promptColor)
    );
    inputLayout->addWidget(promptLabel);
    
    // Input field
    m_commandInput = new QLineEdit();
    m_commandInput->setPlaceholderText("Type command...");
    m_commandInput->setStyleSheet(
        QString(
            "QLineEdit {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: 1px solid %3;"
            "  border-radius: 0px;"
            "  font-family: monospace;"
            "  font-size: 13px;"
            "  padding: 6px 8px;"
            "  selection-background-color: %2;"
            "  selection-color: %1;"
            "}"
            "QLineEdit:focus {"
            "  border-color: %4;"
            "}"
        ).arg(bgColor, textColor, borderColor, promptColor)
    );
    m_commandInput->installEventFilter(this);
    connect(m_commandInput, &QLineEdit::returnPressed, this, &CommandConsole::onCommandEntered);
    inputLayout->addWidget(m_commandInput, 1);
    
    layout->addLayout(inputLayout);
    
    setWidget(container);
}

void CommandConsole::setupCompleter()
{
    m_completerModel = new QStringListModel(m_availableCommands, this);
    m_completer = new QCompleter(m_completerModel, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_commandInput->setCompleter(m_completer);
}

bool CommandConsole::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_commandInput && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        
        // Up arrow - previous command in history
        if (keyEvent->key() == Qt::Key_Up) {
            if (!m_commandHistory.isEmpty()) {
                if (m_historyIndex < 0) {
                    m_historyIndex = m_commandHistory.size() - 1;
                } else if (m_historyIndex > 0) {
                    m_historyIndex--;
                }
                m_commandInput->setText(m_commandHistory[m_historyIndex]);
            }
            return true;
        }
        
        // Down arrow - next command in history
        if (keyEvent->key() == Qt::Key_Down) {
            if (!m_commandHistory.isEmpty() && m_historyIndex >= 0) {
                if (m_historyIndex < m_commandHistory.size() - 1) {
                    m_historyIndex++;
                    m_commandInput->setText(m_commandHistory[m_historyIndex]);
                } else {
                    m_historyIndex = -1;
                    m_commandInput->clear();
                }
            }
            return true;
        }
        
        // Escape - cancel current command
        if (keyEvent->key() == Qt::Key_Escape) {
            m_pendingCommand.clear();
            m_commandInput->clear();
            showPrompt("Command:");
            appendMessage("*Cancel*", "system");
            return true;
        }
    }
    
    return QDockWidget::eventFilter(obj, event);
}

void CommandConsole::appendMessage(const QString& message, const QString& type)
{
    // Check theme for contrast
    QSettings settings;
    bool isDark = (settings.value("appearance/theme", "light").toString() == "dark");
    
    QString color;
    if (isDark) {
        // Dark Mode Colors
        if (type == "error")        color = "#ff5555";  // Red
        else if (type == "success") color = "#50fa7b";  // Green
        else if (type == "system")  color = "#8be9fd";  // Cyan
        else if (type == "command") color = "#f1fa8c";  // Yellow (readable on dark)
        else if (type == "result")  color = "#bd93f9";  // Purple/Lilac (better than yellow)
        else                        color = "#f8f8f2";  // White/Off-white
    } else {
        // Light Mode Colors
        if (type == "error")        color = "#cc0000";  // Dark Red
        else if (type == "success") color = "#008000";  // Dark Green
        else if (type == "system")  color = "#000080";  // Navy Blue
        else if (type == "command") color = "#000000";  // Black
        else if (type == "result")  color = "#663399";  // Rebecca Purple
        else                        color = "#333333";  // Dark Gray
    }
    
    m_historyDisplay->append(QString("<span style='color: %1;'>%2</span>").arg(color, message.toHtmlEscaped()));
    
    // Scroll to bottom
    QScrollBar* scrollBar = m_historyDisplay->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void CommandConsole::showPrompt(const QString& prompt)
{
    m_currentPrompt = prompt;
}

void CommandConsole::focusInput()
{
    m_commandInput->setFocus();
}

void CommandConsole::onCommandEntered()
{
    QString input = m_commandInput->text().trimmed();
    if (input.isEmpty()) return;
    
    // Add to history
    m_commandHistory.append(input);
    m_historyIndex = -1;
    
    // Show in console
    appendMessage(m_currentPrompt + " " + input, "command");
    
    m_commandInput->clear();
    
    processCommand(input);
}

void CommandConsole::processCommand(const QString& command)
{
    // Parse command and arguments
    QStringList parts = command.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return;
    
    QString cmd = parts[0].toUpper();
    QStringList args = parts.mid(1);
    
    executeCommand(cmd, args);
}

bool CommandConsole::parseCoordinate(const QString& input, double& x, double& y)
{
    // Try "x,y" format
    QStringList parts = input.split(',');
    if (parts.size() == 2) {
        bool okX, okY;
        double v1 = parts[0].trimmed().toDouble(&okX);
        double v2 = parts[1].trimmed().toDouble(&okY);
        
        if (okX && okY) {
            QSettings settings;
            bool swapXY = settings.value("coordinates/swapXY", false).toBool();
            if (swapXY) {
                 x = v2;
                 y = v1;
            } else {
                 x = v1;
                 y = v2;
            }
            return true;
        }
    }
    return false;
}

QString CommandConsole::formatCoordinate(double x, double y) const
{
    QSettings settings;
    bool swapXY = settings.value("coordinates/swapXY", false).toBool();
    if (swapXY) {
        return QString("%1, %2").arg(y, 0, 'f', 3).arg(x, 0, 'f', 3);
    }
    return QString("%1, %2").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3);
}

QString CommandConsole::formatCoordinate(const QPointF& point) const
{
    return formatCoordinate(point.x(), point.y());
}

bool CommandConsole::getPointFromArg(const QString& arg, QPointF& point)
{
    // 1. Try coordinate format "x,y"
    double x, y;
    if (parseCoordinate(arg, x, y)) {
        point = QPointF(x, y);
        return true;
    }
    
    // 2. Try to find a peg by name
    if (m_canvas) {
        const auto& pegs = m_canvas->pegs();
        for (const auto& peg : pegs) {
            if (peg.name.compare(arg, Qt::CaseInsensitive) == 0) {
                point = peg.position;
                return true;
            }
        }
    }
    
    return false;
}

void CommandConsole::executeCommand(const QString& cmd, const QStringList& args)
{
    // Route to appropriate handler
    if (cmd == "PEG" || cmd == "POINT" || cmd == "P") {
        cmdPeg(args);
    } else if (cmd == "LINE" || cmd == "L") {
        cmdLine(args);
    } else if (cmd == "POLYLINE" || cmd == "PLINE" || cmd == "PL") {
        cmdPolyline(args);
    } else if (cmd == "CIRCLE" || cmd == "C") {
        cmdCircle(args);
    } else if (cmd == "ZOOM" || cmd == "Z") {
        cmdZoom(args);
    } else if (cmd == "PAN") {
        cmdPan(args);
    } else if (cmd == "SELECT" || cmd == "SEL" || cmd == "S") {
        cmdSelect(args);
    } else if (cmd == "DELETE" || cmd == "ERASE" || cmd == "DEL" || cmd == "E") {
        cmdDelete(args);
    } else if (cmd == "UNDO" || cmd == "U") {
        cmdUndo(args);
    } else if (cmd == "REDO") {
        cmdRedo(args);
    } else if (cmd == "GRID" || cmd == "G") {
        cmdGrid(args);
    } else if (cmd == "SNAP") {
        cmdSnap(args);
    } else if (cmd == "LAYER" || cmd == "LA") {
        cmdLayer(args);
    } else if (cmd == "STATION" || cmd == "ST") {
        cmdStation(args);
    } else if (cmd == "STAKEOUT" || cmd == "SO") {
        cmdStakeout(args);
    } else if (cmd == "POLAR") {
        cmdPolar(args);
    } else if (cmd == "JOIN" || cmd == "J") {
        cmdJoin(args);
    } else if (cmd == "AREA" || cmd == "AA") {
        cmdArea(args);
    } else if (cmd == "DISTANCE" || cmd == "DIST" || cmd == "DI") {
        cmdDistance(args);
    } else if (cmd == "ID" || cmd == "IDENTIFY") { 
        // Implement cmdId later or inline? Let's assume cmdId exists or handle here. 
        // Wait, cmdId wasn't in the original list but ID was in AvailableCommands
        // I will add handler for it.
        // For now, let's just create a new method cmdId later.
        // Actually I should allow executeCommand to fall through for now or add stubs.
        if (args.isEmpty()) {
             appendMessage("Usage: ID [point/peg]", "info");
        } else {
             QPointF pt;
             if (getPointFromArg(args[0], pt)) {
                 appendMessage(QString("ID Point: %1").arg(formatCoordinate(pt)), "result");
             } else {
                 appendMessage("Point not found.", "error");
             }
        }
    } else if (cmd == "HELP" || cmd == "?" || cmd == "H") {
        cmdHelp(args);
    } else if (cmd == "CLEAR" || cmd == "CLS") {
        cmdClear(args);
    } else if (cmd == "FIT" || cmd == "EXTENTS") {
        if (m_canvas) m_canvas->fitToWindow();
        appendMessage("Zoom to fit extents.", "success");
    } else if (cmd == "REGEN" || cmd == "REDRAW" || cmd == "R") {
        if (m_canvas) m_canvas->update();
        appendMessage("Regenerating display.", "success");
    } else if (cmd == "CONTOUR") {
        // Show contour info or generate
        if (m_canvas) {
            if (m_canvas->hasContours()) {
                appendMessage("Contours are displayed on canvas.", "success");
            } else {
                appendMessage("CONTOUR: Use Survey Tools > Contour Generator to create contours.", "info");
                appendMessage("Requires at least 3 pegs with Z coordinates.", "info");
            }
        }
    } else if (cmd == "VOLUME") {
        // Volume calculation info
        appendMessage("VOLUME: Use Survey Tools > Volume Calculation to calculate cut/fill volumes.", "info");
        appendMessage("Requires at least 3 pegs with Z coordinates.", "info");
    } else if (cmd == "TIN") {
        // TIN info or toggle
        if (m_canvas) {
            if (args.isEmpty()) {
                if (m_canvas->hasTIN()) {
                    appendMessage("TIN surface is visible.", "success");
                } else {
                    appendMessage("TIN: Use Survey Tools > Volume Calculation > Show TIN to display.", "info");
                }
            } else {
                QString option = args[0].toUpper();
                if (option == "CLEAR") {
                    m_canvas->clearTIN();
                    appendMessage("TIN cleared.", "success");
                } else {
                    appendMessage("Usage: TIN [CLEAR]", "info");
                }
            }
        }
    } else if (cmd == "PEGS") {
        // List all pegs
        if (m_canvas) {
            const auto& pegs = m_canvas->pegs();
            if (pegs.isEmpty()) {
                appendMessage("No pegs defined.", "info");
            } else {
                appendMessage(QString("=== %1 Pegs ===").arg(pegs.size()), "system");
                for (const auto& peg : pegs) {
                    appendMessage(QString("%1: %2, Z=%3")
                        .arg(peg.name)
                        .arg(formatCoordinate(peg.position))
                        .arg(peg.z, 0, 'f', 3), "result");
                }
            }
        }
    } else {
        appendMessage(QString("Unknown command: %1. Type HELP for list of commands.").arg(cmd), "error");
    }
    
    showPrompt("Command:");
}


// ... (KEEP cmdPeg, cmdLine, etc.) ...

// Jump to replace cmdPolar and cmdJoin area since I can't replace the whole file easily.
// I will use replace_file_content for executeCommand (partial) and adding getPointFromArg 
// But actually I need to insert getPointFromArg before executeCommand.
// The file is large.


void CommandConsole::cmdPeg(const QStringList& args)
{
    if (!m_canvas) {
        appendMessage("Error: Canvas not available.", "error");
        return;
    }
    
    QSettings settings;
    bool swapXY = settings.value("coordinates/swapXY", false).toBool();
    
    if (args.size() >= 2) {
        // Direct coordinate input: PEG x y [z] [name] (or Y X if swapped)
        bool okX, okY;
        double v1 = args[0].toDouble(&okX);
        double v2 = args[1].toDouble(&okY);
        
        if (okX && okY) {
            double x = swapXY ? v2 : v1;
            double y = swapXY ? v1 : v2;
            double z = 0.0;
            QString name;
            
            // Check if third arg is a number (Z) or a name
            if (args.size() >= 3) {
                bool okZ;
                double maybeZ = args[2].toDouble(&okZ);
                if (okZ) {
                    z = maybeZ;
                    name = args.size() >= 4 ? args[3] : "";
                } else {
                    name = args[2];
                }
            }
            
            CanvasPeg peg;
            peg.position = QPointF(x, y);
            peg.z = z;
            peg.name = name.isEmpty() ? QString("P%1").arg(m_canvas->pegs().size() + 1) : name;
            peg.color = Qt::red;
            m_canvas->addPeg(peg);
            
            if (z != 0.0) {
                appendMessage(QString("Peg %1 added at (%2, Z: %3)").arg(peg.name).arg(formatCoordinate(x, y)).arg(z, 0, 'f', 3), "success");
            } else {
                appendMessage(QString("Peg %1 added at (%2)").arg(peg.name).arg(formatCoordinate(x, y)), "success");
            }
        } else {
            appendMessage("Invalid coordinates. Usage: PEG x y [z] [name]", "error");
        }
    } else if (args.size() == 1 && args[0].contains(',')) {
        // Comma-separated: PEG x,y or x,y,z
        QStringList parts = args[0].split(',');
        if (parts.size() >= 2) {
            bool okX, okY;
            double v1 = parts[0].trimmed().toDouble(&okX);
            double v2 = parts[1].trimmed().toDouble(&okY);
            
            if (okX && okY) {
                double x = swapXY ? v2 : v1;
                double y = swapXY ? v1 : v2;
                double z = 0.0;
                
                if (parts.size() >= 3) {
                    bool okZ;
                    z = parts[2].trimmed().toDouble(&okZ);
                    if (!okZ) z = 0.0;
                }
                
                CanvasPeg peg;
                peg.position = QPointF(x, y);
                peg.z = z;
                peg.name = QString("P%1").arg(m_canvas->pegs().size() + 1);
                peg.color = Qt::red;
                m_canvas->addPeg(peg);
                
                if (z != 0.0) {
                    appendMessage(QString("Peg %1 added at (%2, Z: %3)").arg(peg.name).arg(formatCoordinate(x, y)).arg(z, 0, 'f', 3), "success");
                } else {
                    appendMessage(QString("Peg %1 added at (%2)").arg(peg.name).arg(formatCoordinate(x, y)), "success");
                }
            } else {
                appendMessage("Invalid coordinates. Usage: PEG x,y[,z]", "error");
            }
        } else {
            appendMessage("Invalid coordinates. Usage: PEG x,y[,z]", "error");
        }
    } else {
        appendMessage("Usage: PEG x y [z] [name] or PEG x,y[,z]", "info");
        appendMessage("Example: PEG 1000 2000 100 P1", "info");
    }
}


void CommandConsole::cmdLine(const QStringList& args)
{
    if (!m_canvas) return;
    
    QSettings settings;
    bool swapXY = settings.value("coordinates/swapXY", false).toBool();
    
    if (args.size() >= 4) {
        // LINE x1 y1 x2 y2
        bool ok1, ok2, ok3, ok4;
        double v1 = args[0].toDouble(&ok1);
        double v2 = args[1].toDouble(&ok2);
        double v3 = args[2].toDouble(&ok3);
        double v4 = args[3].toDouble(&ok4);
        
        if (ok1 && ok2 && ok3 && ok4) {
            double x1 = swapXY ? v2 : v1;
            double y1 = swapXY ? v1 : v2;
            double x2 = swapXY ? v4 : v3;
            double y2 = swapXY ? v3 : v4;
            
            CanvasPolyline line;
            line.points = {QPointF(x1, y1), QPointF(x2, y2)};
            line.closed = false;
            line.color = Qt::white;
            m_canvas->addPolyline(line);
            appendMessage(QString("Line from (%1) to (%2)").arg(formatCoordinate(x1, y1)).arg(formatCoordinate(x2, y2)), "success");
        } else {
            appendMessage("Invalid coordinates.", "error");
        }
    } else {
        appendMessage("LINE: Click on canvas to draw, or use: LINE x1 y1 x2 y2", "info");
    }
}

void CommandConsole::cmdPolyline(const QStringList& args)
{
    Q_UNUSED(args);
    if (!m_canvas) return;
    appendMessage("POLYLINE: Use Polyline tool or right-click Add Point to draw.", "info");
}

void CommandConsole::cmdCircle(const QStringList& args)
{
    if (!m_canvas) return;
    
    if (args.size() >= 3) {
        // CIRCLE cx cy radius
        bool ok1, ok2, ok3;
        double v1 = args[0].toDouble(&ok1);
        double v2 = args[1].toDouble(&ok2);
        double radius = args[2].toDouble(&ok3);
        
        if (ok1 && ok2 && ok3 && radius > 0) {
             QSettings settings;
            bool swapXY = settings.value("coordinates/swapXY", false).toBool();
            double cx = swapXY ? v2 : v1;
            double cy = swapXY ? v1 : v2;
             
            // Create circle as a closed polyline approximation
            CanvasPolyline circle;
            int segments = 36;
            for (int i = 0; i <= segments; i++) {
                double angle = 2.0 * M_PI * i / segments;
                double x = cx + radius * cos(angle);
                double y = cy + radius * sin(angle);
                circle.points.append(QPointF(x, y));
            }
            circle.closed = true;
            circle.color = Qt::white;
            m_canvas->addPolyline(circle);
            appendMessage(QString("Circle at (%1) with radius %2").arg(formatCoordinate(cx, cy)).arg(radius), "success");
        } else {
            appendMessage("Invalid parameters.", "error");
        }
    } else {
        appendMessage("Usage: CIRCLE cx cy radius", "info");
    }
}

void CommandConsole::cmdZoom(const QStringList& args)
{
    if (!m_canvas) return;
    
    if (args.isEmpty()) {
        appendMessage("ZOOM options: IN, OUT, FIT, EXTENTS, [scale]", "info");
        return;
    }
    
    QString option = args[0].toUpper();
    if (option == "IN" || option == "I") {
        m_canvas->zoomIn();
        appendMessage("Zoom in.", "success");
    } else if (option == "OUT" || option == "O") {
        m_canvas->zoomOut();
        appendMessage("Zoom out.", "success");
    } else if (option == "FIT" || option == "EXTENTS" || option == "E" || option == "F") {
        m_canvas->fitToWindow();
        appendMessage("Zoom to fit extents.", "success");
    } else {
        // Scale factor handling
        appendMessage("Zoom scale: Use mouse wheel or toolbar.", "info");
    }
}

void CommandConsole::cmdPan(const QStringList& args)
{
    Q_UNUSED(args);
    if (!m_canvas) return;
    appendMessage("PAN: Use middle mouse button or Pan toolbar.", "info");
}

void CommandConsole::cmdSelect(const QStringList& args)
{
    Q_UNUSED(args);
    if (!m_canvas) return;
    m_canvas->startSelectMode();
    appendMessage("SELECT mode: Click to select objects.", "info");
}

void CommandConsole::cmdDelete(const QStringList& args)
{
    Q_UNUSED(args);
    if (!m_canvas) return;
    // Use canvas delete functionality if available
    appendMessage("Delete: Use DEL key or Delete toolbar button.", "info");
}

void CommandConsole::cmdUndo(const QStringList& args)
{
    Q_UNUSED(args);
    if (!m_canvas) return;
    m_canvas->undo();
    appendMessage("Undo.", "success");
}

void CommandConsole::cmdRedo(const QStringList& args)
{
    Q_UNUSED(args);
    if (!m_canvas) return;
    m_canvas->redo();
    appendMessage("Redo.", "success");
}

void CommandConsole::cmdGrid(const QStringList& args)
{
    if (!m_canvas) return;
    
    if (args.isEmpty()) {
        bool current = m_canvas->showGrid();
        m_canvas->setShowGrid(!current);
        appendMessage(QString("Grid %1.").arg(!current ? "ON" : "OFF"), "success");
    } else {
        QString option = args[0].toUpper();
        if (option == "ON" || option == "1") {
            m_canvas->setShowGrid(true);
            appendMessage("Grid ON.", "success");
        } else if (option == "OFF" || option == "0") {
            m_canvas->setShowGrid(false);
            appendMessage("Grid OFF.", "success");
        }
    }
}

void CommandConsole::cmdSnap(const QStringList& args)
{
    Q_UNUSED(args);
    if (!m_canvas) return;
    
    appendMessage("SNAP: Snapping is controlled via View menu.", "info");
}

void CommandConsole::cmdLayer(const QStringList& args)
{
    if (!m_canvas) return;
    
    if (args.isEmpty()) {
        appendMessage("LAYER options: NEW name, LIST", "info");
        return;
    }
    
    QString option = args[0].toUpper();
    if (option == "LIST" || option == "L") {
        appendMessage("Layers: Use Layers panel on the right.", "info");
    } else if (option == "NEW" || option == "N") {
        if (args.size() >= 2) {
            m_canvas->addLayer(args[1]);
            appendMessage(QString("Layer '%1' created.").arg(args[1]), "success");
        }
    } else {
        // Create new layer with this name
        m_canvas->addLayer(option);
        appendMessage(QString("Layer '%1' created.").arg(option), "success");
    }
}

void CommandConsole::cmdStation(const QStringList& args)
{
    Q_UNUSED(args);
    if (!m_canvas) return;
    m_canvas->startSetStationMode();
    appendMessage("STATION mode: Click to set station position.", "info");
}

void CommandConsole::cmdStakeout(const QStringList& args)
{
    Q_UNUSED(args);
    if (!m_canvas) return;
    m_canvas->startStakeoutMode();
    appendMessage("STAKEOUT mode activated.", "info");
}

void CommandConsole::cmdPolar(const QStringList& args)
{
    // Usage: POLAR x y bearing distance OR POLAR peg bearing distance
    if (args.size() == 4) {
        // POLAR startX startY bearing distance
        bool ok1, ok2, ok3, ok4;
        double v1 = args[0].toDouble(&ok1);
        double v2 = args[1].toDouble(&ok2);
        double bearing = args[2].toDouble(&ok3);  // In degrees
        double distance = args[3].toDouble(&ok4);
        
        if (ok1 && ok2 && ok3 && ok4) {
            QSettings settings;
            bool swapXY = settings.value("coordinates/swapXY", false).toBool();
            double startX = swapXY ? v2 : v1;
            double startY = swapXY ? v1 : v2;
            
            double bearingRad = bearing * M_PI / 180.0;
            // Survey calculation: Y = Y + d*cos(b), X = X + d*sin(b) (North=Y)
            double endY = startY + distance * std::cos(bearingRad);
            double endX = startX + distance * std::sin(bearingRad);
            
            appendMessage(QString("End point: (%1)").arg(formatCoordinate(endX, endY)), "result");
        } else {
            appendMessage("Invalid numeric values.", "error");
        }
    } else if (args.size() == 3) {
        // POLAR peg bearing distance
        QPointF startPt;
        if (getPointFromArg(args[0], startPt)) {
            bool ok3, ok4;
            double bearing = args[1].toDouble(&ok3);
            double distance = args[2].toDouble(&ok4);
            
            if (ok3 && ok4) {
                double bearingRad = bearing * M_PI / 180.0;
                double endY = startPt.y() + distance * std::cos(bearingRad);
                double endX = startPt.x() + distance * std::sin(bearingRad);
                
                appendMessage(QString("From %1 at (%2)").arg(args[0]).arg(formatCoordinate(startPt)), "info");
                appendMessage(QString("End point: (%1)").arg(formatCoordinate(endX, endY)), "result");
            }
        } else {
             appendMessage("Start point/peg not found.", "error");
        }
    } else {
        appendMessage("Usage: POLAR x y bearing distance OR POLAR peg_name bearing distance", "info");
    }
}

void CommandConsole::cmdJoin(const QStringList& args)
{
    // Usage: JOIN x1 y1 x2 y2 OR JOIN p1 p2
    QPointF start, end;
    bool okStart = false; 
    bool okEnd = false;
    
    if (args.size() >= 4) {
        // JOIN x1 y1 x2 y2
        bool ok1, ok2, ok3, ok4;
        double v1 = args[0].toDouble(&ok1);
        double v2 = args[1].toDouble(&ok2);
        double v3 = args[2].toDouble(&ok3);
        double v4 = args[3].toDouble(&ok4);
        
        if (ok1 && ok2 && ok3 && ok4) {
            QSettings settings;
            bool swapXY = settings.value("coordinates/swapXY", false).toBool();
            start = QPointF(swapXY ? v2 : v1, swapXY ? v1 : v2);
            end = QPointF(swapXY ? v4 : v3, swapXY ? v3 : v4);
            okStart = okEnd = true;
        }
    } else if (args.size() >= 2) {
        okStart = getPointFromArg(args[0], start);
        okEnd = getPointFromArg(args[1], end);
    }
    
    if (okStart && okEnd) {
        double dx = end.x() - start.x();
        double dy = end.y() - start.y();
        double distance = std::sqrt(dx*dx + dy*dy);
        
        double bearingRad = std::atan2(dx, dy);
        double bearingDeg = bearingRad * 180.0 / M_PI;
        if (bearingDeg < 0) bearingDeg += 360.0;
        
        // Draw the line
        if (m_canvas) {
            CanvasPolyline line;
            line.points = {start, end};
            line.closed = false;
            line.color = Qt::white;
            line.layer = "0"; // Default layer
            m_canvas->addPolyline(line);
        }
        
        appendMessage("--- Join Result ---", "info");
        appendMessage(QString("From: (%1)").arg(formatCoordinate(start)), "result");
        appendMessage(QString("To:   (%1)").arg(formatCoordinate(end)), "result");
        appendMessage(QString("Distance: %1 m").arg(distance, 0, 'f', 4), "success");
        appendMessage(QString("Bearing: %1°").arg(bearingDeg, 0, 'f', 4), "success");
    } else {
        appendMessage("Usage: JOIN x1 y1 x2 y2 OR JOIN p1 p2", "info");
    }
}


void CommandConsole::cmdHelp(const QStringList& args)
{
    Q_UNUSED(args);
    appendMessage("=== SiteSurveyor Commands ===", "system");
    appendMessage("", "info");
    appendMessage("DRAWING:", "system");
    appendMessage("  PEG x y [z] [name] - Add survey point/peg", "result");
    appendMessage("  LINE x1 y1 x2 y2   - Draw line", "result");
    appendMessage("  POLYLINE           - Start polyline tool", "result");
    appendMessage("  CIRCLE cx cy r     - Draw circle", "result");
    appendMessage("", "info");
    appendMessage("SURVEYING & COGO:", "system");
    appendMessage("  DIST P1 P2         - Distance & Bearing", "result");
    appendMessage("  JOIN P1 P2         - Draw line with Dist/Brg", "result");
    appendMessage("  POLAR P1 brg dist  - Create point from polar", "result");
    appendMessage("  AREA P1 P2 P3...   - Calculate polygon area", "result");
    appendMessage("  ID P1              - Identify point coords", "result");
    appendMessage("  STATION            - Set instrument station", "result");
    appendMessage("  STAKEOUT           - Start stakeout mode", "result");
    appendMessage("", "info");
    appendMessage("DTM & TERRAIN:", "system");
    appendMessage("  PEGS               - List all pegs with coords", "result");
    appendMessage("  TIN [CLEAR]        - Show TIN status / clear", "result");
    appendMessage("  CONTOUR            - Contour generation info", "result");
    appendMessage("  VOLUME             - Volume calculation info", "result");
    appendMessage("", "info");
    appendMessage("VIEW & EDIT:", "system");
    appendMessage("  ZOOM [IN/OUT/EXT]  - Zoom controls", "result");
    appendMessage("  PAN                - Pan mode", "result");
    appendMessage("  GRID [ON/OFF]      - Toggle grid", "result");
    appendMessage("  LAYER [NEW/LIST]   - Layer management", "result");
    appendMessage("  DELETE / ERASE     - Delete selected", "result");
    appendMessage("  UNDO / REDO        - History control", "result");
    appendMessage("  CLEAR / CLS        - Clear console", "result");
    appendMessage("", "info");
    appendMessage("Note: You can use Peg names (e.g. STN1) or Coordinates (x,y)", "info");
    appendMessage("  HELP               - Show this help", "result");
}


void CommandConsole::cmdClear(const QStringList& args)
{
    Q_UNUSED(args);
    m_historyDisplay->clear();
    appendMessage("Console cleared.", "system");
}

void CommandConsole::cmdArea(const QStringList& args)
{
    if (!m_canvas) {
        appendMessage("Error: Canvas not available.", "error");
        return;
    }
    
    // Check if arguments provided (coordinates or peg names)
    if (args.size() >= 3) {
        QVector<QPointF> points;
        bool allFound = true;
        
        for (const QString& arg : args) {
            QPointF pt;
            if (getPointFromArg(arg, pt)) {
                points.append(pt);
            } else {
                appendMessage(QString("Point '%1' not found, defaulting to 0,0.").arg(arg), "error");
                allFound = false;
            }
        }
        
        if (points.size() >= 3) {
            // Shoelace formula for area
            double area = 0.0;
            int n = points.size();
            for (int i = 0; i < n; i++) {
                int j = (i + 1) % n;
                area += points[i].x() * points[j].y();
                area -= points[j].x() * points[i].y();
            }
            area = std::abs(area) / 2.0;
            
            appendMessage(QString("--- Area for %1 points ---").arg(n), "info");
            appendMessage(QString("Area: %1 m²").arg(area, 0, 'f', 3), "result");
            appendMessage(QString("Area: %1 ha").arg(area / 10000.0, 0, 'f', 5), "result");
        } else {
            appendMessage("Need at least 3 valid points for polygon area.", "error");
        }
    } else {
        appendMessage("Usage: AREA p1 p2 p3...", "info");
        appendMessage("       (Calculate area from 3+ points/pegs)", "info");
    }
}

void CommandConsole::cmdDistance(const QStringList& args)
{
    if (!m_canvas) {
        appendMessage("Error: Canvas not available.", "error");
        return;
    }
    
    // DISTANCE start end (pegs or coordinates)
    // Usage: DIST P1 P2   or   DIST x1 y1 x2 y2
    
    QPointF start, end;
    bool okStart = false; 
    bool okEnd = false;
    
    if (args.size() >= 4) {
         // Handle "x1 y1 x2 y2"
         bool ok1, ok2, ok3, ok4;
         double v1 = args[0].toDouble(&ok1);
         double v2 = args[1].toDouble(&ok2);
         double v3 = args[2].toDouble(&ok3);
         double v4 = args[3].toDouble(&ok4);
         
         if (ok1 && ok2 && ok3 && ok4) {
             QSettings settings;
             bool swapXY = settings.value("coordinates/swapXY", false).toBool();
             start = QPointF(swapXY ? v2 : v1, swapXY ? v1 : v2);
             end = QPointF(swapXY ? v4 : v3, swapXY ? v3 : v4);
             okStart = okEnd = true;
         }
    } else if (args.size() >= 2) {
        // Handle "p1 p2" or "x,y x,y" (uses parseCoordinate/getPointFromArg which handle swapXY)
        okStart = getPointFromArg(args[0], start);
        okEnd = getPointFromArg(args[1], end);
    }

    if (okStart && okEnd) {
        double dx = end.x() - start.x();
        double dy = end.y() - start.y();
        double distance = std::sqrt(dx*dx + dy*dy);
        
        // Bearing calculation (surveying: 0 is North/Y, clockwise)
        double bearingRad = std::atan2(dx, dy);
        double bearingDeg = bearingRad * 180.0 / M_PI;
        if (bearingDeg < 0) bearingDeg += 360.0;
        
        appendMessage("--- Distance Calculation ---", "info");
        appendMessage(QString("From: (%1)").arg(formatCoordinate(start)), "result");
        appendMessage(QString("To:   (%1)").arg(formatCoordinate(end)), "result");
        appendMessage(QString("Distance: %1 m").arg(distance, 0, 'f', 4), "success");
        appendMessage(QString("Bearing: %1°").arg(bearingDeg, 0, 'f', 4), "success");
        
        QSettings s;
        bool swap = s.value("coordinates/swapXY", false).toBool();
        double dXDisplay = swap ? dy : dx;
        double dYDisplay = swap ? dx : dy;
        appendMessage(QString("Delta: %1=%2, %3=%4")
                .arg(swap ? "dY" : "dX").arg(dXDisplay, 0, 'f', 3)
                .arg(swap ? "dX" : "dY").arg(dYDisplay, 0, 'f', 3), "result");
    } else {
         appendMessage("Usage: DIST start_point end_point", "info");
         appendMessage("   or: DIST x1 y1 x2 y2", "info");
    }
}
