#include "appsettings.h"
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QSysInfo>
#include <QByteArray>
#include <QSet>
#include <QMap>

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

// Compile-time secret (provide via -DSS_LICENSE_SECRET or env SS_LICENSE_SECRET at build)
QByteArray licenseSecret()
{
#ifdef SS_LICENSE_SECRET_STR
    static const QByteArray k = QByteArray(SS_LICENSE_SECRET_STR);
    return k;
#else
    // Dev fallback secret; DO NOT SHIP BUILDS WITH THIS
    static const QByteArray k = QByteArray("DEV-SECRET-CHANGE-ME");
    return k;
#endif
}

// RFC 4648 Base32 (uppercase)
QString base32(const QByteArray& in)
{
    static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    QString out;
    int bits = 0;
    int value = 0;
    for (unsigned char c : in) {
        value = (value << 8) | c;
        bits += 8;
        while (bits >= 5) {
            int idx = (value >> (bits - 5)) & 0x1F;
            out.append(QChar(alphabet[idx]));
            bits -= 5;
        }
    }
    if (bits > 0) {
        int idx = (value << (5 - bits)) & 0x1F;
        out.append(QChar(alphabet[idx]));
    }
    return out;
}

QByteArray hmacSha256(const QByteArray& key, const QByteArray& data)
{
    const int blockSize = 64; // SHA-256 block size
    QByteArray k = key;
    if (k.size() > blockSize) k = QCryptographicHash::hash(k, QCryptographicHash::Sha256);
    if (k.size() < blockSize) k.append(QByteArray(blockSize - k.size(), '\0'));
    QByteArray o_key_pad(blockSize, '\x5c');
    QByteArray i_key_pad(blockSize, '\x36');
    for (int i = 0; i < blockSize; ++i) {
        o_key_pad[i] = o_key_pad[i] ^ k[i];
        i_key_pad[i] = i_key_pad[i] ^ k[i];
    }
    QByteArray inner = i_key_pad + data;
    QByteArray innerHash = QCryptographicHash::hash(inner, QCryptographicHash::Sha256);
    QByteArray outer = o_key_pad + innerHash;
    return QCryptographicHash::hash(outer, QCryptographicHash::Sha256);
}

// --- Recent / Pinned files (Welcome Start) ---
static QString recentKey() { return QStringLiteral("ui/recentFiles"); }
static QString pinnedKey() { return QStringLiteral("ui/pinnedFiles"); }
// User profile keys
static QString userFirstKey() { return QStringLiteral("user/first"); }
static QString userLastKey()  { return QStringLiteral("user/last"); }
static QString userNameKey()  { return QStringLiteral("user/name"); }
static QString userEmailKey() { return QStringLiteral("user/email"); }

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

QString hashedSigPath(const QString& disc)
{
    return QString("license/hashed/%1/sig").arg(disc);
}

QString normalizeKey(const QString& key)
{
    QString k;
    k.reserve(key.size());
    for (QChar ch : key) {
        if (ch.isLetterOrNumber()) k.append(ch);
    }
    return k.toUpper();
}

QString computeStoredSig(const QByteArray& salt, const QByteArray& digest)
{
    QByteArray msg;
    msg.reserve(licensePepper().size() + 1 + salt.size() + 1 + digest.size());
    msg.append(licensePepper()); msg.append(':'); msg.append(salt); msg.append(':'); msg.append(digest);
    QByteArray mac = hmacSha256(licenseSecret(), msg);
    return base32(mac).left(20);
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
    // Also set a signature so tampering is detectable
    s.setValue(hashedSigPath(disc), computeStoredSig(salt, digest));
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

// --- User profile (cached) ---
QString AppSettings::userFirstName()
{
    QSettings s; return s.value(userFirstKey()).toString();
}
void AppSettings::setUserFirstName(const QString& first)
{
    QSettings s; s.setValue(userFirstKey(), first.trimmed());
}
QString AppSettings::userLastName()
{
    QSettings s; return s.value(userLastKey()).toString();
}
void AppSettings::setUserLastName(const QString& last)
{
    QSettings s; s.setValue(userLastKey(), last.trimmed());
}
QString AppSettings::userName()
{
    QSettings s; return s.value(userNameKey()).toString();
}
void AppSettings::setUserName(const QString& name)
{
    QSettings s; s.setValue(userNameKey(), name.trimmed());
}
QString AppSettings::userEmail()
{
    QSettings s; return s.value(userEmailKey()).toString();
}
void AppSettings::setUserEmail(const QString& email)
{
    QSettings s; s.setValue(userEmailKey(), email.trimmed());
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

int AppSettings::rightPanelWidth()
{
    QSettings s;
    return s.value("ui/rightPanelWidth", 44).toInt();
}

void AppSettings::setRightPanelWidth(int w)
{
    QSettings s;
    if (w < 24) w = 24;
    if (w > 800) w = 800;
    s.setValue("ui/rightPanelWidth", w);
}

bool AppSettings::autosaveEnabled()
{
    QSettings s;
    return s.value("autosave/enabled", true).toBool();
}

void AppSettings::setAutosaveEnabled(bool on)
{
    QSettings s;
    s.setValue("autosave/enabled", on);
}

int AppSettings::autosaveIntervalMinutes()
{
    QSettings s;
    int m = s.value("autosave/intervalMin", 3).toInt();
    if (m < 1) m = 1; if (m > 60) m = 60;
    return m;
}

void AppSettings::setAutosaveIntervalMinutes(int minutes)
{
    QSettings s;
    int m = minutes;
    if (m < 1) m = 1; if (m > 60) m = 60;
    s.setValue("autosave/intervalMin", m);
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
    const QByteArray salt = generateSalt();
    const QByteArray digest = computeDigest(salt, trimmed);
    s.setValue(hashedSaltPath(disc), salt.toBase64());
    s.setValue(hashedDigestPath(disc), digest.toBase64());
    // Store tamper-evident signature bound to this machine
    s.setValue(hashedSigPath(disc), computeStoredSig(salt, digest));
    s.remove(QString("license/keys/%1").arg(disc));
}

bool AppSettings::hasLicenseFor(const QString& disc)
{
    migratePlainTextIfNeeded(disc);
    if (!hasHashedFor(disc)) return false;
    QSettings s;
    const QByteArray salt = QByteArray::fromBase64(s.value(hashedSaltPath(disc)).toByteArray());
    const QByteArray digest = QByteArray::fromBase64(s.value(hashedDigestPath(disc)).toByteArray());
    const QString sig = s.value(hashedSigPath(disc)).toString();
#ifdef SS_LICENSE_SECRET_DEV
    // In dev builds, fall back to accepting unsigned hashed licenses
    if (sig.isEmpty()) return true;
#endif
    if (salt.isEmpty() || digest.isEmpty() || sig.isEmpty()) return false;
    const QString expected = computeStoredSig(salt, digest);
    return sig == expected;
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

bool AppSettings::activateLicense(const QString& disc, const QString& key, bool bindToMachine)
{
    const QString pref = licensePrefixFor(disc).toUpper();
    if (pref.isEmpty()) return false;
    const QString trimmed = key.trimmed().toUpper();
    if (!trimmed.startsWith(pref + QLatin1Char('-'))) return false;

#ifndef SS_LICENSE_SECRET_STR
    // Dev builds (no release secret provided): allow DEV- keys unconditionally
    if (trimmed.startsWith(QStringLiteral("DEV-"))) {
        setLicenseKeyFor(disc, key);
        return true;
    }
#endif

    // Validate structure: PREFIX-<BODY>-<SIG>, where SIG = base32(HMAC(secret, PREFIX|disc|BODY|device))[:10]
    QString norm = normalizeKey(trimmed);
    // Remove PREFIX
    if (!norm.startsWith(pref)) return false;
    norm.remove(0, pref.size());
    if (norm.size() < 18) return false; // require at least 8 body + 10 sig
    const int SIG_LEN = 10;
    const QString sig = norm.right(SIG_LEN);
    const QString body = norm.left(norm.size() - SIG_LEN);

    auto makeSig = [&](bool bind)->QString{
        QByteArray msg;
        msg.append(pref.toUtf8()); msg.append('|');
        msg.append(disc.toUtf8()); msg.append('|');
        msg.append(body.toUtf8()); msg.append('|');
        if (bind) msg.append(licensePepper()); else msg.append('*');
        return base32(hmacSha256(licenseSecret(), msg)).left(SIG_LEN);
    };

    bool ok = (sig == makeSig(true));
    // Allow universal (unbound) keys only for Engineering Surveying
    const bool allowUnbound = disc.compare(QStringLiteral("Engineering Surveying"), Qt::CaseInsensitive) == 0;
    if (!ok && allowUnbound) ok = (sig == makeSig(false));
    if (!ok) return false;

    // Store hashed+signed state for offline checks (machine-bound)
    setLicenseKeyFor(disc, key);
    return true;
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
    const QString trimmed = key.trimmed();
    const QByteArray saltB64 = s.value(hashedSaltPath(disc)).toByteArray();
    const QByteArray digB64  = s.value(hashedDigestPath(disc)).toByteArray();
    if (saltB64.isEmpty() || digB64.isEmpty()) return false;
    const QByteArray salt = QByteArray::fromBase64(saltB64);
    const QByteArray stored = QByteArray::fromBase64(digB64);
    const QByteArray candidate = computeDigest(salt, trimmed);
    const bool match = (stored == candidate);
    if (!match) return false;
    // Also ensure signature matches (tamper detection)
    const QString sig = s.value(hashedSigPath(disc)).toString();
#ifdef SS_LICENSE_SECRET_DEV
    if (sig.isEmpty()) return true;
#endif
    return sig == computeStoredSig(salt, stored);
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

bool AppSettings::osnapEnd()
{
    QSettings s; return s.value("snap/end", true).toBool();
}

void AppSettings::setOsnapEnd(bool on)
{
    QSettings s; s.setValue("snap/end", on);
}

bool AppSettings::osnapMid()
{
    QSettings s; return s.value("snap/mid", true).toBool();
}

void AppSettings::setOsnapMid(bool on)
{
    QSettings s; s.setValue("snap/mid", on);
}

bool AppSettings::osnapNearest()
{
    QSettings s; return s.value("snap/nearest", true).toBool();
}

void AppSettings::setOsnapNearest(bool on)
{
    QSettings s; s.setValue("snap/nearest", on);
}

bool AppSettings::osnapIntersect()
{
    QSettings s; return s.value("snap/intersect", true).toBool();
}

void AppSettings::setOsnapIntersect(bool on)
{
    QSettings s; s.setValue("snap/intersect", on);
}

bool AppSettings::osnapPerp()
{
    QSettings s; return s.value("snap/perp", false).toBool();
}

void AppSettings::setOsnapPerp(bool on)
{
    QSettings s; s.setValue("snap/perp", on);
}

bool AppSettings::osnapTangent()
{
    QSettings s; return s.value("snap/tangent", false).toBool();
}

void AppSettings::setOsnapTangent(bool on)
{
    QSettings s; s.setValue("snap/tangent", on);
}

bool AppSettings::osnapCenter()
{
    QSettings s; return s.value("snap/center", false).toBool();
}

void AppSettings::setOsnapCenter(bool on)
{
    QSettings s; s.setValue("snap/center", on);
}

bool AppSettings::osnapQuadrant()
{
    QSettings s; return s.value("snap/quadrant", false).toBool();
}

void AppSettings::setOsnapQuadrant(bool on)
{
    QSettings s; s.setValue("snap/quadrant", on);
}

bool AppSettings::polarMode()
{
    QSettings s; return s.value("polar/mode", false).toBool();
}

void AppSettings::setPolarMode(bool on)
{
    QSettings s; s.setValue("polar/mode", on);
}

double AppSettings::polarIncrementDeg()
{
    QSettings s; return s.value("polar/incrementDeg", 15.0).toDouble();
}

void AppSettings::setPolarIncrementDeg(double deg)
{
    QSettings s; s.setValue("polar/incrementDeg", deg);
}

QColor AppSettings::osnapGlyphColor()
{
    QSettings s; QColor c = s.value("snap/glyphColor", QColor(255,255,0)).value<QColor>(); return c;
}

void AppSettings::setOsnapGlyphColor(const QColor& color)
{
    QSettings s; s.setValue("snap/glyphColor", color);
}
