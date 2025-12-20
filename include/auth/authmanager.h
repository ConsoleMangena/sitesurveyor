#ifndef AUTHMANAGER_H
#define AUTHMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStringList>
#include <QDateTime>
#include <QTimer>

// User profile data structure (from account.prefs)
struct UserProfile {
    QString id;           // From account.$id
    QString name;         // From account.name or prefs.fullName
    QString username;     // From prefs.username
    QString email;        // From account.email
    QString organization; // From prefs.organization
    QString userType;     // From prefs.userType
    QString city;         // From prefs.city
    QString country;      // From prefs.country
};

// License data structure
struct License {
    QString id;              // Document $id
    QString userId;          // User ID
    QString plan;            // free / professional / enterprise
    QDateTime expiresAt;     // Expiration date
    QStringList features;    // Enabled feature flags
    QStringList deviceIds;   // Registered device UUIDs
    int maxDevices = 1;      // Max simultaneous devices
    bool isActive = false;   // License active status
    
    bool isValid() const {
        if (!isActive) return false;
        if (plan == "free") return true;  // Free never expires
        return expiresAt.isValid() && expiresAt > QDateTime::currentDateTime();
    }
    
    bool hasFeature(const QString& feature) const {
        return features.contains(feature);
    }
};

class AuthManager : public QObject
{
    Q_OBJECT

public:
    explicit AuthManager(QObject *parent = nullptr);
    
    // Core actions
    void login(const QString& email, const QString& password);
    void checkSession();
    void logout();
    

    
    // Status
    bool isAuthenticated() const { return m_isAuthenticated; }
    QString currentUser() const { return m_profile.name.isEmpty() ? m_profile.email : m_profile.name; }
    
    // Profile access
    const UserProfile& userProfile() const { return m_profile; }
    
    // License access
    const License& license() const { return m_license; }
    bool hasFeature(const QString& feature) const { return m_license.hasFeature(feature); }
    bool isLicenseValid() const { return m_license.isValid(); }
    QString licensePlan() const { return m_license.plan; }

signals:
    void loginSuccess();
    void loginError(const QString& message);
    void sessionVerified();
    void sessionInvalid();
    void licenseLoaded();
    void licenseError(const QString& message);
    void licenseExpired();


private slots:
    void onLoginFinished();
    void onCheckSessionFinished();
    void onLicenseFetched();
    void onDeviceRegistered();

private:
    QNetworkAccessManager* m_network;
    bool m_isAuthenticated;
    UserProfile m_profile;
    License m_license;
    QString m_sessionId;
    QString m_deviceId;
    
    // Config
    const QString API_ENDPOINT = "https://nyc.cloud.appwrite.io/v1";
    const QString PROJECT_ID = "690f708900139eaa58f4";
    const QString DATABASE_ID = "sitesurveyor";
    const QString LICENSES_COLLECTION = "licenses";
    const QString API_KEY = "standard_b432bca7313523e8e09f74151f265876552f4ff92daa46960442685084484972150d85e4c74c949f4c0e4a1ad93f4ff7f6968832ba66f1c36b0d615bf4144645719724d4f17e88364438c856fc510045463c350f671a07406725ba012c165c0cdeb9fcd645d5f6059d788e2e0315081366f4065753f93204f943382270a0d355";
    
    void saveSession(const QString& sessionId);
    void loadSession();
    void clearSession();
    void parseProfileFromPrefs(const QJsonObject& prefs);
    void fetchLicense();
    void registerDevice();
    void saveLicense();
    void loadLicense();
    void saveProfile();
    void loadProfile();
    QString getDeviceId();
private slots:
    void checkLicenseExpiration();

private:
   QTimer* m_licenseCheckTimer;
};

#endif // AUTHMANAGER_H
