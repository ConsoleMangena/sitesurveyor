#ifndef STAKEOUTRECORD_H
#define STAKEOUTRECORD_H

#include <QString>
#include <QDateTime>
#include <QtGlobal>
#include <QtMath>

struct StakeoutRecord {
    QString id;
    QString designPoint;
    QString description;
    double designE{0.0};
    double designN{0.0};
    double designZ{0.0};
    double measuredE{qQNaN()};
    double measuredN{qQNaN()};
    double measuredZ{qQNaN()};
    QString instrument;
    QString setupDetails;
    QString method;
    QString crew;
    QString status;
    QString remarks;
    QDateTime createdAt;
    QDateTime observedAt;

    bool hasMeasurement() const { return !qIsNaN(measuredE) && !qIsNaN(measuredN); }
    double deltaE() const { return hasMeasurement() ? measuredE - designE : qQNaN(); }
    double deltaN() const { return hasMeasurement() ? measuredN - designN : qQNaN(); }
    double deltaZ() const { return hasMeasurement() ? measuredZ - designZ : qQNaN(); }
    double horizontalResidual() const {
        if (!hasMeasurement()) return qQNaN();
        return qSqrt(qPow(deltaE(), 2) + qPow(deltaN(), 2));
    }
    double verticalResidual() const {
        if (!hasMeasurement()) return qQNaN();
        return deltaZ();
    }
};

#endif // STAKEOUTRECORD_H
