#ifndef APPSETTINGS_H
#define APPSETTINGS_H

class AppSettings {
public:
    static bool gaussMode();
    static void setGaussMode(bool enabled);
    static bool use3D();
    static void setUse3D(bool enabled);
};

#endif // APPSETTINGS_H
