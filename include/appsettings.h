#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QString>
#include <QStringList>

class AppSettings {
public:
    static bool gaussMode();
    static void setGaussMode(bool enabled);
    static bool use3D();
    static void setUse3D(bool enabled);
    // Theme
    static bool darkMode();
    static void setDarkMode(bool enabled);

    // Engineering discipline preferences
    static QString measurementUnits();      // "metric" or "imperial"
    static void setMeasurementUnits(const QString& units);
    static QString angleFormat();           // "dms" or "decimal"
    static void setAngleFormat(const QString& fmt);
    static QString crs();                   // e.g., "EPSG:4326"
    static void setCrs(const QString& code);
    static bool engineeringPresetApplied();
    static void setEngineeringPresetApplied(bool applied);

    // License settings
    // Backward-compatible helpers (use current discipline)
    static QString licenseKey();
    static void setLicenseKey(const QString& key);
    static bool hasLicense();

    // Discipline selection and per-discipline license management
    static QStringList availableDisciplines();
    static QString discipline();
    static void setDiscipline(const QString& name);
    // Returns empty string; keys are not exposed. Kept for legacy compatibility.
    static QString licenseKeyFor(const QString& discipline);
    static void setLicenseKeyFor(const QString& discipline, const QString& key);
    static bool hasLicenseFor(const QString& discipline);
    static bool verifyLicenseFor(const QString& discipline, const QString& key);
    static void clearLicenseFor(const QString& discipline);
    static QString licensePrefixFor(const QString& discipline);
    // Recent/pinned files (Welcome Start page)
    static QStringList recentFiles();
    static void setRecentFiles(const QStringList& files);
    static void addRecentFile(const QString& path, int maxCount = 20);
    static void clearRecentFiles();
    static QStringList pinnedFiles();
    static void setPinnedFiles(const QStringList& files);
    static void pinFile(const QString& path, bool pin);
};

#endif // APPSETTINGS_H
