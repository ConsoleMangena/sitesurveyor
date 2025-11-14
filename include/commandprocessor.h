#ifndef COMMANDPROCESSOR_H
#define COMMANDPROCESSOR_H

#include <QObject>
#include <QString>
#include <QStringList>

class PointManager;
class CanvasWidget;
class StakeoutManager;

class CommandProcessor : public QObject
{
    Q_OBJECT

public:
    explicit CommandProcessor(PointManager* pointManager,
                              CanvasWidget* canvas,
                              StakeoutManager* stakeoutManager,
                              QObject *parent = nullptr);
    
    QString processCommand(const QString& command);

signals:
    void commandProcessed(const QString& command);
    void outputMessage(const QString& message);
    void errorMessage(const QString& message);

private:
    QString addPoint(const QStringList& parts);
    QString addPolarPoint(const QStringList& parts);
    QString calculateDistance(const QStringList& parts);
    QString calculateAzimuth(const QStringList& parts);
    QString calculateArea(const QStringList& parts);
    QString joinPolar(const QStringList& parts);
    QString listPoints();
    QString clearPoints();
    QString deletePoint(const QStringList& parts);
    QString drawLine(const QStringList& parts);
    QString showHelp();
    QString handleStakeoutCommand(const QStringList& parts);
    QString handleControlCommand(const QStringList& parts);
    
    PointManager* m_pointManager;
    CanvasWidget* m_canvas;
    StakeoutManager* m_stakeoutManager;
};

#endif // COMMANDPROCESSOR_H
