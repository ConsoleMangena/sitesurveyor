#include "surveycalculator.h"
#include <QVector>
#include <QtMath>

SurveyCalculator::SurveyCalculator(QObject *parent) : QObject(parent)
{
}

QPointF SurveyCalculator::polarToRectangular(const QPointF& origin, double azimuthDeg, double distance)
{
    double azimuthRad = degreesToRadians(azimuthDeg);
    double dx = distance * qSin(azimuthRad);
    double dy = distance * qCos(azimuthRad);
    return QPointF(origin.x() + dx, origin.y() + dy);
}

QPair<double, double> SurveyCalculator::rectangularToPolar(const QPointF& from, const QPointF& to)
{
    double dx = to.x() - from.x();
    double dy = to.y() - from.y();
    double distance = qSqrt(dx * dx + dy * dy);
    double azimuth = radiansToDegrees(qAtan2(dx, dy));
    azimuth = normalizeAzimuth(azimuth);
    return qMakePair(azimuth, distance);
}

double SurveyCalculator::distance(const QPointF& p1, const QPointF& p2)
{
    double dx = p2.x() - p1.x();
    double dy = p2.y() - p1.y();
    return qSqrt(dx * dx + dy * dy);
}

double SurveyCalculator::azimuth(const QPointF& from, const QPointF& to)
{
    double dx = to.x() - from.x();
    double dy = to.y() - from.y();
    double azimuth = radiansToDegrees(qAtan2(dx, dy));
    return normalizeAzimuth(azimuth);
}

double SurveyCalculator::calculateArea(const QVector<QPointF>& points)
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

double SurveyCalculator::normalizeAzimuth(double azimuth)
{
    while (azimuth < 0) azimuth += 360.0;
    while (azimuth >= 360.0) azimuth -= 360.0;
    return azimuth;
}

double SurveyCalculator::degreesToRadians(double degrees)
{
    return degrees * M_PI / 180.0;
}

double SurveyCalculator::radiansToDegrees(double radians)
{
    return radians * 180.0 / M_PI;
}