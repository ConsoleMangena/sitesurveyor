#include "surveycalculator.h"
#include "appsettings.h"
#include <QtMath>
#include <QString>

QPointF SurveyCalculator::rectangularToPolar(const QPointF& from, const QPointF& to)
{
    double dx = to.x() - from.x();
    double dy = to.y() - from.y();
    double distance = qSqrt(dx * dx + dy * dy);
    double azimuthRad = qAtan2(dx, dy);
    double azimuth = normalizeAngle(radiansToDegrees(azimuthRad));
    if (AppSettings::gaussMode()) {
        azimuth = normalizeAngle(azimuth - 180.0); // 0° South
    }
    return QPointF(distance, azimuth);
}

QPointF SurveyCalculator::polarToRectangular(const QPointF& from, double distance, double azimuth)
{
    double baseAz = AppSettings::gaussMode() ? normalizeAngle(azimuth + 180.0) : azimuth;
    double azimuthRad = degreesToRadians(baseAz);
    double dx = distance * qSin(azimuthRad);
    double dy = distance * qCos(azimuthRad);
    return QPointF(from.x() + dx, from.y() + dy);
}

Point SurveyCalculator::polarToRectangular(const Point& from, double distance, double azimuth, double z)
{
    double baseAz = AppSettings::gaussMode() ? normalizeAngle(azimuth + 180.0) : azimuth;
    double azimuthRad = degreesToRadians(baseAz);
    double dx = distance * qSin(azimuthRad);
    double dy = distance * qCos(azimuthRad);
    return Point("", from.x + dx, from.y + dy, z);
}

double SurveyCalculator::distance(const QPointF& p1, const QPointF& p2)
{
    double dx = p2.x() - p1.x();
    double dy = p2.y() - p1.y();
    return qSqrt(dx * dx + dy * dy);
}

double SurveyCalculator::distance2D(const Point& p1, const Point& p2)
{
    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;
    return qSqrt(dx * dx + dy * dy);
}

double SurveyCalculator::distance3D(const Point& p1, const Point& p2)
{
    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;
    double dz = p2.z - p1.z;
    return qSqrt(dx * dx + dy * dy + dz * dz);
}

double SurveyCalculator::azimuth(const QPointF& from, const QPointF& to)
{
    double dx = to.x() - from.x();
    double dy = to.y() - from.y();
    double azimuthRad = qAtan2(dx, dy);
    double az = normalizeAngle(radiansToDegrees(azimuthRad));
    if (AppSettings::gaussMode()) {
        az = normalizeAngle(az - 180.0); // 0° South
    }
    return az;
}

double SurveyCalculator::azimuth(const Point& from, const Point& to)
{
    return azimuth(from.toQPointF(), to.toQPointF());
}

double SurveyCalculator::angleBetween(const QPointF& p1, const QPointF& center, const QPointF& p2)
{
    double az1 = azimuth(center, p1);
    double az2 = azimuth(center, p2);
    double angle = az2 - az1;
    return normalizeAngle(angle);
}

double SurveyCalculator::area(const QVector<QPointF>& points)
{
    if (points.size() < 3) return 0.0;
    
    double area = 0.0;
    int n = points.size();
    
    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        area += points[i].x() * points[j].y();
        area -= points[j].x() * points[i].y();
    }
    
    return qAbs(area) / 2.0;
}

double SurveyCalculator::area(const QVector<Point>& points)
{
    QVector<QPointF> qpoints;
    for (const auto& p : points) {
        qpoints.append(p.toQPointF());
    }
    return area(qpoints);
}

double SurveyCalculator::degreesToRadians(double degrees)
{
    return degrees * M_PI / 180.0;
}

double SurveyCalculator::radiansToDegrees(double radians)
{
    return radians * 180.0 / M_PI;
}

double SurveyCalculator::normalizeAngle(double angle)
{
    while (angle < 0) angle += 360;
    while (angle >= 360) angle -= 360;
    return angle;
}

QString SurveyCalculator::toDMS(double degrees)
{
    int deg = static_cast<int>(degrees);
    double minFrac = (degrees - deg) * 60;
    int min = static_cast<int>(minFrac);
    double sec = (minFrac - min) * 60;
    return QString("%1°%2'%3\"").arg(deg).arg(min, 2, 10, QChar('0')).arg(sec, 5, 'f', 2, QChar('0'));
}