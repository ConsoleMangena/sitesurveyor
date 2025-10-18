#ifndef POINT_H
#define POINT_H

#include <QString>
#include <QPointF>

struct Point {
    QString name;
    double x;
    double y;
    double z;
    
    Point() : x(0), y(0), z(0) {}
    Point(const QString& n, double px, double py, double pz = 0) 
        : name(n), x(px), y(py), z(pz) {}
    
    QPointF toQPointF() const { return QPointF(x, y); }
};

#endif // POINT_H