#include "pointmanager.h"

PointManager::PointManager(QObject *parent) : QObject(parent)
{
}

void PointManager::addPoint(const Point& point)
{
    Point copy = point;
    if (copy.description.isEmpty()) copy.description = copy.name;
    m_points[copy.name] = copy;
    emit pointAdded(copy);
}

bool PointManager::addPoint(const QString& name, double x, double y, double z)
{
    Point p(name, x, y, z);
    p.description = name;
    addPoint(p);
    return true;
}

bool PointManager::removePoint(const QString& name)
{
    if (m_points.remove(name) > 0) {
        emit pointRemoved(name);
        return true;
    }
    return false;
}

Point PointManager::getPoint(const QString& name) const
{
    if (m_points.contains(name)) {
        return m_points[name];
    }
    return Point();
}

bool PointManager::hasPoint(const QString& name) const
{
    return m_points.contains(name);
}

bool PointManager::updatePoint(const Point& point)
{
    if (!m_points.contains(point.name)) {
        return false;
    }
    Point copy = point;
    if (copy.description.isEmpty()) copy.description = copy.name;
    m_points[copy.name] = copy;
    emit pointUpdated(copy);
    return true;
}

bool PointManager::setPointDescription(const QString& name, const QString& description)
{
    auto it = m_points.find(name);
    if (it == m_points.end()) return false;
    it->description = description;
    emit pointUpdated(it.value());
    return true;
}

bool PointManager::setControlMetadata(const QString& name,
                                      bool isControl,
                                      const QString& controlType,
                                      const QString& controlSource,
                                      double horizontalTolMm,
                                      double verticalTolMm,
                                      const QString& notes)
{
    auto it = m_points.find(name);
    if (it == m_points.end()) return false;
    Point& p = it.value();
    p.isControl = isControl;
    if (isControl) {
        p.controlType = controlType;
        p.controlSource = controlSource;
        p.controlHorizontalToleranceMm = horizontalTolMm;
        p.controlVerticalToleranceMm = verticalTolMm;
        if (!notes.isEmpty()) p.notes = notes;
    } else {
        p.controlType.clear();
        p.controlSource.clear();
        p.controlHorizontalToleranceMm = 0.0;
        p.controlVerticalToleranceMm = 0.0;
        p.notes.clear();
    }
    emit pointUpdated(p);
    return true;
}

bool PointManager::clearControlMetadata(const QString& name)
{
    return setControlMetadata(name, false);
}

QVector<Point> PointManager::getControlPoints() const
{
    QVector<Point> out;
    for (const auto& p : m_points) {
        if (p.isControl) out.append(p);
    }
    return out;
}

bool PointManager::setStakeoutStatus(const QString& name, const QString& status)
{
    auto it = m_points.find(name);
    if (it == m_points.end()) return false;
    Point& p = it.value();
    p.stakeoutStatus = status;
    emit pointUpdated(p);
    return true;
}

bool PointManager::setStakeoutResiduals(const QString& name,
                                        double deltaE,
                                        double deltaN,
                                        double deltaZ,
                                        const QString& remarks)
{
    auto it = m_points.find(name);
    if (it == m_points.end()) return false;
    Point& p = it.value();
    p.stakeoutDeltaE = deltaE;
    p.stakeoutDeltaN = deltaN;
    p.stakeoutDeltaZ = deltaZ;
    if (!remarks.isEmpty()) p.stakeoutRemarks = remarks;
    emit pointUpdated(p);
    return true;
}

QVector<Point> PointManager::getAllPoints() const
{
    QVector<Point> points;
    for (const auto& point : m_points) {
        points.append(point);
    }
    return points;
}

QStringList PointManager::getPointNames() const
{
    return m_points.keys();
}

int PointManager::getPointCount() const
{
    return m_points.size();
}

void PointManager::clearAllPoints()
{
    m_points.clear();
    emit pointsCleared();
}
