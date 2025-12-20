#include "canvas/mapbackground.h"
#include <QDir>
#include <QCoreApplication>
#include <QQmlEngine>

MapBackground::MapBackground(QWidget *parent)
    : QQuickWidget(parent)
{
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    
    // Transparent background to allow stacking
    setAttribute(Qt::WA_AlwaysStackOnTop, false); 
    // Actually map should be at bottom, opaque. 
    // But setting source might take a moment.
    
    // Load QML
    // In production, should be in qrc. For dev, we load from file.
    // Try QRC first, fallback to local file
    
    setSource(QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/../../resources/qml/Map.qml"));
    
    if (status() == QQuickWidget::Error) {
        qWarning() << "Map QML error:" << errors();
        // Fallback path logic if needed
    }
}

void MapBackground::setCenter(double lat, double lon)
{
    m_lat = lat;
    m_lon = lon;
    if (rootObject()) {
        QMetaObject::invokeMethod(rootObject(), "setCenter",
            Q_ARG(QVariant, lat),
            Q_ARG(QVariant, lon));
    }
}

void MapBackground::setZoom(int level)
{
    m_zoom = level;
    if (rootObject()) {
        QMetaObject::invokeMethod(rootObject(), "setZoom",
            Q_ARG(QVariant, level));
    }
}
