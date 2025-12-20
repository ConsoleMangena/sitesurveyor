#ifndef MAPBACKGROUND_H
#define MAPBACKGROUND_H

#include <QQuickWidget>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickItem>

class MapBackground : public QQuickWidget
{
    Q_OBJECT

public:
    explicit MapBackground(QWidget *parent = nullptr);

    void setCenter(double lat, double lon);
    void setZoom(int level);

    double currentLatitude() const { return m_lat; }
    double currentLongitude() const { return m_lon; }

private:
    double m_lat{40.785091};
    double m_lon{-73.968285};
    int m_zoom{16};
};

#endif // MAPBACKGROUND_H
