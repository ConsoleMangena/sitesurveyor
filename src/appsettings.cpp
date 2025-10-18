#include "appsettings.h"
#include <QSettings>

bool AppSettings::gaussMode()
{
    QSettings s; // uses QApplication org/app name
    return s.value("coords/gaussMode", false).toBool();
}

void AppSettings::setGaussMode(bool enabled)
{
    QSettings s;
    s.setValue("coords/gaussMode", enabled);
}

bool AppSettings::use3D()
{
    QSettings s;
    return s.value("coords/use3D", true).toBool();
}

void AppSettings::setUse3D(bool enabled)
{
    QSettings s;
    s.setValue("coords/use3D", enabled);
}
