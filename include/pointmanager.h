#ifndef POINTMANAGER_H
#define POINTMANAGER_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QString>
#include <QStringList>
#include "point.h"

class PointManager : public QObject {
    Q_OBJECT

public:
    explicit PointManager(QObject *parent = nullptr);
    
    void addPoint(const Point& point);
    bool addPoint(const QString& name, double x, double y, double z = 0);
    bool removePoint(const QString& name);
    Point getPoint(const QString& name) const;
    bool hasPoint(const QString& name) const;
    bool updatePoint(const Point& point);
    bool setPointDescription(const QString& name, const QString& description);
    bool setControlMetadata(const QString& name,
                            bool isControl,
                            const QString& controlType = QString(),
                            const QString& controlSource = QString(),
                            double horizontalTolMm = 0.0,
                            double verticalTolMm = 0.0,
                            const QString& notes = QString());
    bool clearControlMetadata(const QString& name);
    QVector<Point> getControlPoints() const;
    bool setStakeoutStatus(const QString& name, const QString& status);
    bool setStakeoutResiduals(const QString& name,
                              double deltaE,
                              double deltaN,
                              double deltaZ,
                              const QString& remarks = QString());
    
    QVector<Point> getAllPoints() const;
    QStringList getPointNames() const;
    int getPointCount() const;
    void clearAllPoints();
    
signals:
    void pointAdded(const Point& point);
    void pointUpdated(const Point& point);
    void pointRemoved(const QString& name);
    void pointsCleared();
    
private:
    QMap<QString, Point> m_points;
};

#endif // POINTMANAGER_H