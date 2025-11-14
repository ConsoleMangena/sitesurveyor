#ifndef POINT_H
#define POINT_H

#include <QString>
#include <QPointF>

// Extended point record used throughout the application. In addition to the
// basic coordinate data we hold lightweight metadata to support control
// registers and stakeout QA without introducing parallel data stores.

struct Point {
    QString name;
    double x;
    double y;
    double z;
    QString description;
    bool isControl{false};
    QString controlSource;
    QString controlType;
    double controlHorizontalToleranceMm{0.0};
    double controlVerticalToleranceMm{0.0};
    QString stakeoutGroup;
    QString stakeoutStatus;
    double stakeoutDeltaE{0.0};
    double stakeoutDeltaN{0.0};
    double stakeoutDeltaZ{0.0};
    QString stakeoutRemarks;
    QString notes;
    
    Point() : x(0), y(0), z(0) {}
    Point(const QString& n, double px, double py, double pz = 0) 
        : name(n), x(px), y(py), z(pz) {}
    
    QPointF toQPointF() const { return QPointF(x, y); }
};

#endif // POINT_H