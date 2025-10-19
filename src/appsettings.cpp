#include "appsettings.h"
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QSysInfo>
#include <QByteArray>
#include <QSet>

namespace {
// Derive a per-machine pepper so hashes are not portable across machines
QByteArray licensePepper()
{
    QByteArray pepper("SS-PEPPER-v1");
    QByteArray mid = QSysInfo::machineUniqueId();
    if (mid.isEmpty()) mid = QSysInfo::machineHostName().toUtf8();
    pepper += mid;
    return pepper;
}

// --- Recent / Pinned files (Welcome Start) ---
static QString recentKey() { return QStringLiteral("ui/recentFiles"); }
static QString pinnedKey() { return QStringLiteral("ui/pinnedFiles"); }

QByteArray generateSalt(int len = 16)
{
    QByteArray salt;
    salt.resize(len);
    for (int i = 0; i < len; ++i) {
        salt[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return salt;
}

QByteArray computeDigest(const QByteArray& salt, const QString& key)
{
    QByteArray data = salt + key.toUtf8() + licensePepper();
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

QString hashedSaltPath(const QString& disc)
{
    return QString("license/hashed/%1/salt").arg(disc);
}

QString hashedDigestPath(const QString& disc)
{
    return QString("license/hashed/%1/digest").arg(disc);
}

bool hasHashedFor(const QString& disc)
{
    QSettings s;
    return s.contains(hashedDigestPath(disc)) && s.contains(hashedSaltPath(disc));
}

void migratePlainTextIfNeeded(const QString& disc)
{
    // If a plaintext key exists and no hashed key exists, migrate and remove plaintext
    QSettings s;
    const QString keyPath = QString("license/keys/%1").arg(disc);
    const bool hasPlain = s.contains(keyPath);
    if (!hasPlain || hasHashedFor(disc)) return;

    const QString key = s.value(keyPath).toString().trimmed();
    if (key.isEmpty()) return;

    const QByteArray salt = generateSalt();
    const QByteArray digest = computeDigest(salt, key);
    s.setValue(hashedSaltPath(disc), salt.toBase64());
    s.setValue(hashedDigestPath(disc), digest.toBase64());
    s.remove(keyPath); // remove plaintext
}
}

QStringList AppSettings::recentFiles()
{
    QSettings s;
    QStringList list = s.value(recentKey()).toStringList();
    // De-duplicate and drop empties
    QStringList out;
    QSet<QString> seen;
    for (const QString& p : list) {
        const QString t = p.trimmed();
        if (t.isEmpty()) continue;
        if (seen.contains(t)) continue;
        seen.insert(t);
        out.append(t);
    }
    return out;
}

void AppSettings::addRecentFile(const QString& path, int maxCount)
{
    if (path.trimmed().isEmpty()) return;
    QSettings s;
    QStringList list = s.value(recentKey()).toStringList();
    list.removeAll(path);
    list.prepend(path);
    while (list.size() > maxCount) list.removeLast();
    s.setValue(recentKey(), list);
}

void AppSettings::clearRecentFiles()
{
    QSettings s;
    s.remove(recentKey());
}

void AppSettings::setRecentFiles(const QStringList& files)
{
    QSettings s;
    QStringList out;
    QSet<QString> seen;
    for (const QString& p : files) {
        const QString t = p.trimmed();
        if (t.isEmpty()) continue;
        if (seen.contains(t)) continue;
        seen.insert(t);
        out.append(t);
    }
    s.setValue(recentKey(), out);
}

QStringList AppSettings::pinnedFiles()
{
    QSettings s;
    QStringList list = s.value(pinnedKey()).toStringList();
    // Cleanup
    QStringList out;
    QSet<QString> seen;
    for (const QString& p : list) {
        const QString t = p.trimmed();
        if (t.isEmpty()) continue;
        if (seen.contains(t)) continue;
        seen.insert(t);
        out.append(t);
    }
    return out;
}

void AppSettings::setPinnedFiles(const QStringList& files)
{
    QSettings s;
    QStringList out;
    QSet<QString> seen;
    for (const QString& p : files) {
        const QString t = p.trimmed();
        if (t.isEmpty()) continue;
        if (seen.contains(t)) continue;
        seen.insert(t);
        out.append(t);
    }
    s.setValue(pinnedKey(), out);
}

void AppSettings::pinFile(const QString& path, bool pin)
{
    if (path.trimmed().isEmpty()) return;
    QStringList pins = pinnedFiles();
    if (pin) {
        if (!pins.contains(path)) pins.prepend(path);
    } else {
        pins.removeAll(path);
    }
    setPinnedFiles(pins);
}

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

bool AppSettings::darkMode()
{
    QSettings s;
    return s.value("ui/darkMode", false).toBool();
}

void AppSettings::setDarkMode(bool enabled)
{
    QSettings s;
    s.setValue("ui/darkMode", enabled);
}

QStringList AppSettings::availableDisciplines()
{
    // Extendable list of disciplines; first item is the default
    return QStringList{
        QStringLiteral("Engineering Surveying"),
        QStringLiteral("Cadastral Surveying"),
        QStringLiteral("Remote Sensing"),
        QStringLiteral("GIS & Mapping")
    };
}

QString AppSettings::discipline()
{
    QSettings s;
    const QStringList discs = availableDisciplines();
    const QString def = discs.value(0, QStringLiteral("Engineering Surveying"));
    QString d = s.value("license/discipline", def).toString();
    if (!discs.contains(d)) {
        d = def;
        s.setValue("license/discipline", d);
    }
    return d;
}

void AppSettings::setDiscipline(const QString& name)
{
    // Only accept known disciplines; otherwise store anyway to allow future additions
    QSettings s;
    s.setValue("license/discipline", name);
}

QString AppSettings::licenseKeyFor(const QString& disc)
{
    // Legacy accessor: return empty; keys are not exposed anymore.
    // Attempt migration of any existing plaintext key, then do not reveal content.
    migratePlainTextIfNeeded(disc);
    return QString();
}

void AppSettings::setLicenseKeyFor(const QString& disc, const QString& key)
{
    QSettings s;
    const QString trimmed = key.trimmed();
    // Create a fresh salt and digest for this discipline
    const QByteArray salt = generateSalt();
    const QByteArray digest = computeDigest(salt, trimmed);
    s.setValue(hashedSaltPath(disc), salt.toBase64());
    s.setValue(hashedDigestPath(disc), digest.toBase64());
    // Ensure any legacy plaintext is removed
    s.remove(QString("license/keys/%1").arg(disc));
}

bool AppSettings::hasLicenseFor(const QString& disc)
{
    // Migrate any plaintext first
    migratePlainTextIfNeeded(disc);
    return hasHashedFor(disc);
}

// Backwards-compatible helpers resolve to current discipline
QString AppSettings::licenseKey()
{
    // Do not expose license key
    return QString();
}

void AppSettings::setLicenseKey(const QString& key)
{
    setLicenseKeyFor(discipline(), key);
}

bool AppSettings::hasLicense()
{
    return hasLicenseFor(discipline());
}

QString AppSettings::licensePrefixFor(const QString& disc)
{
    const QString d = disc.trimmed();
    if (d.compare(QStringLiteral("Engineering Surveying"), Qt::CaseInsensitive) == 0) return QStringLiteral("ES");
    if (d.compare(QStringLiteral("Cadastral Surveying"), Qt::CaseInsensitive) == 0) return QStringLiteral("CS");
    if (d.compare(QStringLiteral("Remote Sensing"), Qt::CaseInsensitive) == 0) return QStringLiteral("RS");
    if (d.compare(QStringLiteral("GIS & Mapping"), Qt::CaseInsensitive) == 0) return QStringLiteral("GM");
    return QString();
}

bool AppSettings::verifyLicenseFor(const QString& disc, const QString& key)
{
    QSettings s;
    migratePlainTextIfNeeded(disc);
    const QByteArray saltB64 = s.value(hashedSaltPath(disc)).toByteArray();
    const QByteArray digB64  = s.value(hashedDigestPath(disc)).toByteArray();
    if (saltB64.isEmpty() || digB64.isEmpty()) return false;
    const QByteArray salt = QByteArray::fromBase64(saltB64);
    const QByteArray stored = QByteArray::fromBase64(digB64);
    const QByteArray candidate = computeDigest(salt, key.trimmed());
    return stored == candidate;
}

void AppSettings::clearLicenseFor(const QString& disc)
{
    QSettings s;
    s.remove(QString("license/keys/%1").arg(disc));            // legacy
    s.remove(hashedSaltPath(disc));
    s.remove(hashedDigestPath(disc));
}

// --- Engineering discipline preferences ---
QString AppSettings::measurementUnits()
{
    QSettings s;
    // Default to metric for engineering surveying
    return s.value("eng/units", QStringLiteral("metric")).toString();
}

void AppSettings::setMeasurementUnits(const QString& units)
{
    QSettings s;
    s.setValue("eng/units", units);
}

QString AppSettings::angleFormat()
{
    QSettings s;
    // Default to DMS for engineering surveying
    return s.value("eng/angleFormat", QStringLiteral("dms")).toString();
}

void AppSettings::setAngleFormat(const QString& fmt)
{
    QSettings s;
    s.setValue("eng/angleFormat", fmt);
}

QString AppSettings::crs()
{
    QSettings s;
    // Leave configurable; default to a common geographic CRS
    return s.value("eng/crs", QStringLiteral("EPSG:4326")).toString();
}

void AppSettings::setCrs(const QString& code)
{
    QSettings s;
    s.setValue("eng/crs", code);
}

bool AppSettings::engineeringPresetApplied()
{
    QSettings s;
    return s.value("eng/presetApplied", false).toBool();
}

void AppSettings::setEngineeringPresetApplied(bool applied)
{
    QSettings s;
    s.setValue("eng/presetApplied", applied);
}
