#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QString>
#include <QStringList>
#include <QColor>

class AppSettings {
public:
    static bool gaussMode();
    static void setGaussMode(bool enabled);
    static bool use3D();
    static void setUse3D(bool enabled);
    // Theme
    static bool darkMode();
    static void setDarkMode(bool enabled);

    // UI sizes
    static int rightPanelWidth();
    static void setRightPanelWidth(int w);

    // Autosave
    static bool autosaveEnabled();
    static void setAutosaveEnabled(bool on);
    static int autosaveIntervalMinutes();
    static void setAutosaveIntervalMinutes(int minutes);

    // Engineering discipline preferences
    static QString measurementUnits();      // "metric" or "imperial"
    static void setMeasurementUnits(const QString& units);
    static QString angleFormat();           // "dms" or "decimal"
    static void setAngleFormat(const QString& fmt);
    static QString crs();                   // e.g., "EPSG:4326"
    static void setCrs(const QString& code);
    static QString crsName();
    static void setCrsName(const QString& name);
    static QString crsDatum();
    static void setCrsDatum(const QString& datum);
    static QString crsProjection();
    static void setCrsProjection(const QString& projection);
    static QString crsLinearUnits();
    static void setCrsLinearUnits(const QString& units);
    static bool engineeringPresetApplied();
    static void setEngineeringPresetApplied(bool applied);

    // Stakeout defaults
    static double stakeoutHorizontalToleranceMm();
    static void setStakeoutHorizontalToleranceMm(double value);
    static double stakeoutVerticalToleranceMm();
    static void setStakeoutVerticalToleranceMm(double value);
    static QStringList stakeoutStatusOptions();

    // OSNAP/polar persistence
    static bool osnapEnd();
    static void setOsnapEnd(bool on);
    static bool osnapMid();
    static void setOsnapMid(bool on);
    static bool osnapNearest();
    static void setOsnapNearest(bool on);
    static bool osnapIntersect();
    static void setOsnapIntersect(bool on);
    static bool osnapPerp();
    static void setOsnapPerp(bool on);
    static bool osnapTangent();
    static void setOsnapTangent(bool on);
    static bool osnapCenter();
    static void setOsnapCenter(bool on);
    static bool osnapQuadrant();
    static void setOsnapQuadrant(bool on);
    static bool polarMode();
    static void setPolarMode(bool on);
    static double polarIncrementDeg();
    static void setPolarIncrementDeg(double deg);
    static QColor osnapGlyphColor();
    static void setOsnapGlyphColor(const QColor& color);

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
    // Secure activation (validates key signature and stores hashed+signed state)
    static bool activateLicense(const QString& discipline, const QString& key, bool bindToMachine = true);
    // Recent/pinned files (Welcome Start page)
    static QStringList recentFiles();
    static void setRecentFiles(const QStringList& files);
    static void addRecentFile(const QString& path, int maxCount = 20);
    static void clearRecentFiles();
    static QStringList pinnedFiles();
    static void setPinnedFiles(const QStringList& files);
    static void pinFile(const QString& path, bool pin);

    // User profile (cached)
    static QString userFirstName();
    static void setUserFirstName(const QString& first);
    static QString userLastName();
    static void setUserLastName(const QString& last);
    static QString userName();
    static void setUserName(const QString& name);
    static QString userEmail();
    static void setUserEmail(const QString& email);
};

#endif // APPSETTINGS_H
