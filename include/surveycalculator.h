#ifndef SURVEYCALCULATOR_H
#define SURVEYCALCULATOR_H

#include <QPointF>
#include <QVector>
#include <QString>
#include "point.h"

class SurveyCalculator
{
public:
    // Coordinate conversions
    static QPointF rectangularToPolar(const QPointF& from, const QPointF& to);
    static QPointF polarToRectangular(const QPointF& from, double distance, double azimuth);
    static Point polarToRectangular(const Point& from, double distance, double azimuth, double z);
    
    // Distance calculations
    static double distance(const QPointF& p1, const QPointF& p2);
    static double distance2D(const Point& p1, const Point& p2);
    static double distance3D(const Point& p1, const Point& p2);
    
    // Angle calculations
    static double azimuth(const QPointF& from, const QPointF& to);
    static double azimuth(const Point& from, const Point& to);
    static double angleBetween(const QPointF& p1, const QPointF& center, const QPointF& p2);
    
    // Area calculations
    static double area(const QVector<QPointF>& points);
    static double area(const QVector<Point>& points);
    
    // Utility functions
    static double degreesToRadians(double degrees);
    static double radiansToDegrees(double radians);
    static double normalizeAngle(double angle);
    static QString toDMS(double degrees);
};

#endif // SURVEYCALCULATOR_H
