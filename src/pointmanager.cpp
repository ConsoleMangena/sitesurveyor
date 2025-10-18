#include "pointmanager.h"

PointManager::PointManager(QObject *parent) : QObject(parent)
{
}

void PointManager::addPoint(const Point& point)
{
    m_points[point.name] = point;
    emit pointAdded(point);
}

bool PointManager::addPoint(const QString& name, double x, double y, double z)
{
    addPoint(Point(name, x, y, z));
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
