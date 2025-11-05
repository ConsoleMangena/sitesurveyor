#include "commandprocessor.h"
#include "pointmanager.h"
#include "canvaswidget.h"
#include "point.h"
#include "surveycalculator.h"
#include "appsettings.h"
#include <QStringList>
#include <QRegularExpression>
#include <QtMath>
#include <algorithm>

CommandProcessor::CommandProcessor(PointManager* pointManager, CanvasWidget* canvas, QObject *parent)
    : QObject(parent), m_pointManager(pointManager), m_canvas(canvas)
{
}

// Helper: convert azimuth (degrees, 0-360) to quadrant bearing string like N xx° E
static QString toQuadrantBearing(double azDegrees)
{
    double az = SurveyCalculator::normalizeAngle(azDegrees);
    QString ns, ew;
    double theta;
    if (az >= 0 && az < 90) {
        ns = "N"; ew = "E"; theta = az;
    } else if (az >= 90 && az < 180) {
        ns = "S"; ew = "E"; theta = 180 - az;
    } else if (az >= 180 && az < 270) {
        ns = "S"; ew = "W"; theta = az - 180;
    } else {
        ns = "N"; ew = "W"; theta = 360 - az;
    }
    return QString("%1 %2 %3").arg(ns).arg(SurveyCalculator::toDMS(theta)).arg(ew);
}

QString CommandProcessor::processCommand(const QString& command)
{
    QString cmd = command.trimmed();
    if (cmd.isEmpty()) return "";
    
    emit commandProcessed(cmd);
    
    // Parse command
    QStringList parts = cmd.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return "";
    
    QString operation = parts[0].toLower();
    
    // Process different commands
    if (operation == "add" || operation == "point" || operation == "coordinate" || operation == "coord") {
        return addPoint(parts);
    } else if (operation == "polar") {
        return addPolarPoint(parts);
    } else if (operation == "dist" || operation == "distance") {
        return calculateDistance(parts);
    } else if (operation == "azimuth" || operation == "az" || operation == "bearing" || operation == "brg") {
        return calculateAzimuth(parts);
    } else if (operation == "area") {
        return calculateArea(parts);
    } else if (operation == "join" || operation == "joinpolar" || operation == "jp") {
        return joinPolar(parts);
    } else if (operation == "list" || operation == "ls") {
        return listPoints();
    } else if (operation == "clear") {
        return clearPoints();
    } else if (operation == "delete" || operation == "del" || operation == "removecoord") {
        return deletePoint(parts);
    } else if (operation == "line") {
        return drawLine(parts);
    } else if (operation == "help" || operation == "?") {
        return showHelp();
    } else {
        return QString("Unknown command: %1. Type 'help' for available commands.").arg(operation);
    }
}

QString CommandProcessor::addPoint(const QStringList& parts)
{
    if (parts.size() < 5) {
        return "Usage: add <name> <x> <y> <z>  (adds a coordinate)";
    }
    
    QString name = parts[1];
    bool ok;
    double x = parts[2].toDouble(&ok);
    if (!ok) return "Invalid X coordinate";
    double y = parts[3].toDouble(&ok);
    if (!ok) return "Invalid Y coordinate";
    double z = parts[4].toDouble(&ok);
    if (!ok) return "Invalid Z coordinate";
    
    // In Gauss/Zimbabwe mode, users enter Y X Z order; swap to internal X,Y
    if (AppSettings::gaussMode()) {
        std::swap(x, y);
    }
    
    // Check for duplicate name
    if (m_pointManager->hasPoint(name)) {
        return QString("Coordinate %1 already exists").arg(name);
    }
    
    Point point(name, x, y, z);
    m_pointManager->addPoint(point);
    m_canvas->addPoint(point);
    
    if (AppSettings::gaussMode()) {
        return QString("Added coordinate %1: (%.3f, %.3f, %.3f)")
            .arg(name)
            .arg(point.y, 0, 'f', 3)
            .arg(point.x, 0, 'f', 3)
            .arg(point.z, 0, 'f', 3);
    } else {
        return QString("Added coordinate %1: (%.3f, %.3f, %.3f)")
            .arg(name)
            .arg(point.x, 0, 'f', 3)
            .arg(point.y, 0, 'f', 3)
            .arg(point.z, 0, 'f', 3);
    }
}

QString CommandProcessor::addPolarPoint(const QStringList& parts)
{
    if (parts.size() < 6) {
        return "Usage: polar <from_coordinate> <new_coordinate> <distance> <bearing> <z>";
    }
    
    QString fromName = parts[1];
    QString newName = parts[2];
    
    Point fromPoint = m_pointManager->getPoint(fromName);
    if (fromPoint.name.isEmpty()) {
        return QString("Coordinate %1 not found").arg(fromName);
    }
    
    bool ok;
    double distance = parts[3].toDouble(&ok);
    if (!ok || distance <= 0) return "Invalid distance (must be positive)";
    double azimuth = parts[4].toDouble(&ok);
    if (!ok) return "Invalid bearing";
    double z = parts[5].toDouble(&ok);
    if (!ok) return "Invalid Z coordinate";
    
    // Check for duplicate name
    if (m_pointManager->hasPoint(newName)) {
        return QString("Coordinate %1 already exists").arg(newName);
    }
    
    Point newPoint = SurveyCalculator::polarToRectangular(fromPoint, distance, azimuth, z);
    newPoint.name = newName;
    
    m_pointManager->addPoint(newPoint);
    m_canvas->addPoint(newPoint);
    
    const QString brgDms = SurveyCalculator::toDMS(azimuth);
    const QString distStr = QString::number(distance, 'f', 3);
    const QString zStr = QString::number(z, 'f', 3);
    const QString xStr = QString::number(newPoint.x, 'f', 3);
    const QString yStr = QString::number(newPoint.y, 'f', 3);
    const QString zOutStr = QString::number(newPoint.z, 'f', 3);
    if (AppSettings::gaussMode()) {
        return QString("Added coordinate %1 from polar: dist=%2, brg=%3, z=%4 -> (%5, %6, %7)")
            .arg(newName, distStr, brgDms, zStr, yStr, xStr, zOutStr);
    } else {
        return QString("Added coordinate %1 from polar: dist=%2, brg=%3, z=%4 -> (%5, %6, %7)")
            .arg(newName, distStr, brgDms, zStr, xStr, yStr, zOutStr);
    }
}

QString CommandProcessor::calculateDistance(const QStringList& parts)
{
    if (parts.size() < 3) {
        return "Usage: distance <coordinate1> <coordinate2>";
    }
    
    Point p1 = m_pointManager->getPoint(parts[1]);
    Point p2 = m_pointManager->getPoint(parts[2]);
    
    if (p1.name.isEmpty()) return QString("Coordinate %1 not found").arg(parts[1]);
    if (p2.name.isEmpty()) return QString("Coordinate %1 not found").arg(parts[2]);
    
    double dist2D = SurveyCalculator::distance2D(p1, p2);
    double dist3D = SurveyCalculator::distance3D(p1, p2);
    double dz = p2.z - p1.z;
    
    return QString("Distance between %1 and %2:\n  2D: %.3f m\n  3D: %.3f m\n  ΔZ: %.3f m")
        .arg(p1.name).arg(p2.name).arg(dist2D).arg(dist3D).arg(dz);
}

QString CommandProcessor::calculateAzimuth(const QStringList& parts)
{
    if (parts.size() < 3) {
        return "Usage: azimuth <from_coordinate> <to_coordinate>";
    }
    
    Point from = m_pointManager->getPoint(parts[1]);
    Point to = m_pointManager->getPoint(parts[2]);
    
    if (from.name.isEmpty()) return QString("Coordinate %1 not found").arg(parts[1]);
    if (to.name.isEmpty()) return QString("Coordinate %1 not found").arg(parts[2]);
    
    double azimuth = SurveyCalculator::azimuth(from, to);
    QString dms = SurveyCalculator::toDMS(azimuth);
    
    return QString("Bearing from %1 to %2: %3")
        .arg(from.name).arg(to.name).arg(dms);
}

QString CommandProcessor::joinPolar(const QStringList& parts)
{
    if (parts.size() < 3) {
        return "Usage: join <from_coordinate> <to_coordinate>";
    }
    const QString fromName = parts[1];
    const QString toName = parts[2];
    Point from = m_pointManager->getPoint(fromName);
    Point to = m_pointManager->getPoint(toName);
    if (from.name.isEmpty()) return QString("Coordinate %1 not found").arg(fromName);
    if (to.name.isEmpty()) return QString("Coordinate %1 not found").arg(toName);

    // Deltas
    double dx = to.x - from.x;
    double dy = to.y - from.y;
    double dz = to.z - from.z;

    // Distances
    double d2 = SurveyCalculator::distance2D(from, to);
    double d3 = SurveyCalculator::distance3D(from, to);

    // Azimuths (SurveyCalculator already accounts for Gauss/Zimbabwe mode)
    double azFwd = SurveyCalculator::azimuth(from, to);
    double azBack = SurveyCalculator::azimuth(to, from);
    QString azFwdDms = SurveyCalculator::toDMS(azFwd);
    QString azBackDms = SurveyCalculator::toDMS(azBack);
    QString azFwdBrg = toQuadrantBearing(azFwd);
    QString azBackBrg = toQuadrantBearing(azBack);

    // Slope/grade (with proper zero check)
    bool is3D = AppSettings::use3D();
    double slopeDeg = 0.0;
    double gradePct = 0.0;
    const double EPSILON = 1e-9;
    if (is3D && d2 > EPSILON) {
        slopeDeg = SurveyCalculator::normalizeAngle(SurveyCalculator::radiansToDegrees(qAtan2(dz, d2)));
        gradePct = (dz / d2) * 100.0;
    }

    // Format output according to Gauss display order
    const bool gauss = AppSettings::gaussMode();
    QString deltaLine;
    if (gauss) {
        deltaLine = QString("  ΔY: %.3f  ΔX: %.3f").arg(dy).arg(dx);
    } else {
        deltaLine = QString("  ΔX: %.3f  ΔY: %.3f").arg(dx).arg(dy);
    }
    if (is3D) deltaLine += QString("  ΔZ: %.3f").arg(dz);

    QString out;
    out += QString("Join %1 -> %2\n").arg(from.name).arg(to.name);
    out += deltaLine + "\n";
    out += QString("  Distance 2D: %.3f m\n").arg(d2);
    if (is3D) out += QString("  Distance 3D: %.3f m\n").arg(d3);
    out += QString("  Bearing FWD: %1\n").arg(azFwdDms);
    out += QString("  Bearing BKW: %1\n").arg(azBackDms);
    if (is3D) {
        out += QString("  Slope angle: %.4f°  Gradient: %.3f%%\n").arg(slopeDeg, 0, 'f', 4).arg(gradePct, 0, 'f', 3);
    }
    return out.trimmed();
}

QString CommandProcessor::calculateArea(const QStringList& parts)
{
    if (parts.size() < 4) {
        return "Usage: area <coordinate1> <coordinate2> <coordinate3> [coordinate4...]";
    }
    
    QVector<Point> points;
    for (int i = 1; i < parts.size(); ++i) {
        Point p = m_pointManager->getPoint(parts[i]);
        if (p.name.isEmpty()) {
            return QString("Coordinate %1 not found").arg(parts[i]);
        }
        points.append(p);
    }
    
    double area = SurveyCalculator::area(points);
    
    QString pointList;
    for (const auto& p : points) {
        if (!pointList.isEmpty()) pointList += "-";
        pointList += p.name;
    }
    
    return QString("Area of polygon %1: %.3f m² (%.3f ha)")
        .arg(pointList).arg(area).arg(area / 10000);
}

QString CommandProcessor::listPoints()
{
    auto points = m_pointManager->getAllPoints();
    if (points.isEmpty()) {
        return "No coordinates defined";
    }
    
    QString result = QString("Coordinates (%1):\n").arg(points.size());
    for (const auto& p : points) {
        if (AppSettings::gaussMode()) {
            result += QString("  %1: (%.3f, %.3f, %.3f)\n")
                .arg(p.name, -10).arg(p.y, 10, 'f', 3).arg(p.x, 10, 'f', 3).arg(p.z, 8, 'f', 3);
        } else {
            result += QString("  %1: (%.3f, %.3f, %.3f)\n")
                .arg(p.name, -10).arg(p.x, 10, 'f', 3).arg(p.y, 10, 'f', 3).arg(p.z, 8, 'f', 3);
        }
    }
    return result.trimmed();
}

QString CommandProcessor::clearPoints()
{
    m_pointManager->clearAllPoints();
    m_canvas->clearAll();
    return "All coordinates cleared";
}

QString CommandProcessor::deletePoint(const QStringList& parts)
{
    if (parts.size() < 2) {
        return "Usage: delete <coordinate_name>";
    }
    
    QString name = parts[1];
    if (m_pointManager->removePoint(name)) {
        // Remove only this point from canvas to preserve layer assignments
        if (m_canvas) m_canvas->removePointByName(name);
        return QString("Deleted coordinate %1").arg(name);
    } else {
        return QString("Coordinate %1 not found").arg(name);
    }
}

QString CommandProcessor::drawLine(const QStringList& parts)
{
    if (parts.size() < 3) {
        return "Usage: line <coordinate1> <coordinate2>";
    }
    
    Point p1 = m_pointManager->getPoint(parts[1]);
    Point p2 = m_pointManager->getPoint(parts[2]);
    
    if (p1.name.isEmpty()) return QString("Coordinate %1 not found").arg(parts[1]);
    if (p2.name.isEmpty()) return QString("Coordinate %1 not found").arg(parts[2]);
    
    m_canvas->addLine(p1.toQPointF(), p2.toQPointF());
    
    return QString("Drew line from %1 to %2").arg(p1.name).arg(p2.name);
}

QString CommandProcessor::showHelp()
{
    const bool gauss = AppSettings::gaussMode();
    QString addUsage = gauss ? "add <name> <y> <x> <z>" : "add <name> <x> <y> <z>";
    QString notes = gauss
        ? "Current mode: Gauss/Zimbabwe (0° South). Input/listing order: Y X Z."
        : "Current mode: Standard (0° North). Input/listing order: X Y Z.";

    QString help;
    help += "Available Commands:\n";
    help += QString("  %1           - Add a coordinate (aliases: point, coordinate, coord)\n").arg(addUsage);
    help += "  polar <from> <name> <dist> <bearing> <z> - Add coordinate using polar data\n";
    help += "  join <from> <to>                 - Polar join between coordinates (aliases: joinpolar, jp)\n";
    help += "  distance <coord1> <coord2>       - Calculate distance (alias: dist)\n";
    help += "  bearing <from> <to>              - Calculate bearing (aliases: brg, azimuth, az)\n";
    help += "  area <c1> <c2> <c3> [c4...]      - Calculate area of polygon\n";
    help += "  line <coord1> <coord2>           - Draw a line between coordinates\n";
    help += "  list                             - List all coordinates (alias: ls)\n";
    help += "  delete <name>                    - Delete a coordinate (aliases: del, removecoord)\n";
    help += "  clear                            - Clear all coordinates\n";
    help += "  help                             - Show this help message (alias: ?)\n\n";
    help += notes;
    return help;
}