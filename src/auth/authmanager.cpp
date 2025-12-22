#include "auth/authmanager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QSysInfo>
#include <QCryptographicHash>
#include <QUrlQuery>
#include <QDebug>

AuthManager::AuthManager(QObject *parent)
    : QObject(parent), m_isAuthenticated(false)
{
    m_network = new QNetworkAccessManager(this);
    m_licenseCheckTimer = new QTimer(this);
    connect(m_licenseCheckTimer, &QTimer::timeout, this, &AuthManager::checkLicenseExpiration);
    // Check every 30 minutes
    m_licenseCheckTimer->setInterval(30 * 60 * 1000);

    // JWT refresh timer - refresh 2 minutes before 15-minute expiration = 13 minutes
    m_jwtRefreshTimer = new QTimer(this);
    connect(m_jwtRefreshTimer, &QTimer::timeout, this, &AuthManager::refreshJwt);
    m_jwtRefreshTimer->setInterval(13 * 60 * 1000);  // 13 minutes

    m_deviceId = getDeviceId();
    loadSession();
    loadLicense();  // Load cached license
}

QString AuthManager::getDeviceId()
{
    // Generate unique device ID based on machine info
    QSettings settings("SiteSurveyor", "Device");
    QString deviceId = settings.value("deviceId").toString();

    if (deviceId.isEmpty()) {
        // Generate new device ID from machine unique ID
        QString machineInfo = QSysInfo::machineHostName() +
                              QSysInfo::productType() +
                              QSysInfo::currentCpuArchitecture();
        QByteArray hash = QCryptographicHash::hash(machineInfo.toUtf8(), QCryptographicHash::Sha256);
        deviceId = hash.toHex().left(32);  // Use first 32 chars
        settings.setValue("deviceId", deviceId);
    }

    return deviceId;
}

QNetworkRequest AuthManager::createAuthorizedRequest(const QUrl& url) const
{
    QNetworkRequest request(url);
    request.setRawHeader("X-Appwrite-Project", PROJECT_ID.toUtf8());
    if (!m_jwt.isEmpty()) {
        // Use JWT for native desktop app authentication
        request.setRawHeader("X-Appwrite-JWT", m_jwt.toUtf8());
        qDebug() << "[AuthManager] Request to" << url.path() << "with JWT";
    } else if (!m_sessionId.isEmpty()) {
        // Fallback to session cookie
        QString cookieName = QString("a_session_%1").arg(PROJECT_ID);
        QString cookieValue = QString("%1=%2").arg(cookieName, m_sessionId);
        request.setRawHeader("Cookie", cookieValue.toUtf8());
        qDebug() << "[AuthManager] Request to" << url.path() << "with session cookie (no JWT)";
    } else {
        qDebug() << "[AuthManager] WARNING: Request to" << url.path() << "WITHOUT auth!";
    }
    return request;
}

void AuthManager::login(const QString& email, const QString& password)
{
    // Store credentials temporarily for offline save after successful login
    m_pendingEmail = email;
    m_pendingPassword = password;

    QUrl url(API_ENDPOINT + "/account/sessions/email");
    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("X-Appwrite-Project", PROJECT_ID.toUtf8());

    QJsonObject json;
    json["email"] = email;
    json["password"] = password;

    QNetworkReply* reply = m_network->post(request, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, &AuthManager::onLoginFinished);
}

void AuthManager::onLoginFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject obj = doc.object();

        m_sessionId = obj["$id"].toString();
        m_isAuthenticated = true;
        m_isOfflineMode = false;

        saveSession(m_sessionId);

        // Save credentials for offline login
        if (!m_pendingEmail.isEmpty() && !m_pendingPassword.isEmpty()) {
            saveOfflineCredentials(m_pendingEmail, m_pendingPassword);
            m_pendingEmail.clear();
            m_pendingPassword.clear();
        }

        // Fetch profile data before emitting success
        QUrl url(API_ENDPOINT + "/account");
        QNetworkRequest request(url);
        request.setRawHeader("X-Appwrite-Project", PROJECT_ID.toUtf8());
        request.setRawHeader("X-Appwrite-Session", m_sessionId.toUtf8());

        QNetworkReply* accountReply = m_network->get(request);
        connect(accountReply, &QNetworkReply::finished, this, [this, accountReply]() {
            if (accountReply->error() == QNetworkReply::NoError) {
                QJsonDocument doc = QJsonDocument::fromJson(accountReply->readAll());
                QJsonObject obj = doc.object();

                m_profile.id = obj["$id"].toString();
                m_profile.name = obj["name"].toString();
                m_profile.email = obj["email"].toString();

                if (obj.contains("prefs") && obj["prefs"].isObject()) {
                    parseProfileFromPrefs(obj["prefs"].toObject());
                }

                saveProfile();  // Cache profile for offline mode

                // Fetch JWT for native desktop app authentication
                fetchJwt();
            }
            accountReply->deleteLater();
            emit loginSuccess();
        });
    } else {
        QString errApi;
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isNull()) {
            errApi = doc.object()["message"].toString();
        }
        m_pendingEmail.clear();
        m_pendingPassword.clear();
        emit loginError(errApi.isEmpty() ? reply->errorString() : errApi);
    }
    reply->deleteLater();
}

void AuthManager::checkSession()
{
    if (m_sessionId.isEmpty()) {
        emit sessionInvalid();
        return;
    }

    QUrl url(API_ENDPOINT + "/account");
    QNetworkRequest request(url);
    request.setRawHeader("X-Appwrite-Project", PROJECT_ID.toUtf8());
    if (!m_sessionId.isEmpty()) {
        request.setRawHeader("X-Appwrite-Session", m_sessionId.toUtf8());
    }

    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, &AuthManager::onCheckSessionFinished);
}

void AuthManager::onCheckSessionFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject obj = doc.object();

        m_profile.id = obj["$id"].toString();
        m_profile.name = obj["name"].toString();
        m_profile.email = obj["email"].toString();

        if (obj.contains("prefs") && obj["prefs"].isObject()) {
            parseProfileFromPrefs(obj["prefs"].toObject());
        }

        m_isAuthenticated = true;
        saveProfile();  // Cache profile for offline mode

        // Fetch license after profile
        // License check disabled for bucket-only mode
        // fetchLicense();

        emit sessionVerified();
    } else {
        m_isAuthenticated = false;
        clearSession();
        emit sessionInvalid();
    }
    reply->deleteLater();
}

void AuthManager::parseProfileFromPrefs(const QJsonObject& prefs)
{
    QString fullName = prefs["fullName"].toString();
    if (!fullName.isEmpty()) {
        m_profile.name = fullName;
    }

    m_profile.username = prefs["username"].toString();
    m_profile.organization = prefs["organization"].toString();
    m_profile.userType = prefs["userType"].toString();
    m_profile.city = prefs["city"].toString();
    m_profile.country = prefs["country"].toString();
}

void AuthManager::fetchLicense()
{
    // Query license document for this user
    QString queryUrl = QString("%1/databases/%2/collections/%3/documents")
        .arg(API_ENDPOINT)
        .arg(DATABASE_ID)
        .arg(LICENSES_COLLECTION);

    QUrl url(queryUrl);
    QUrlQuery query;
    query.addQueryItem("queries[]", QString("equal(\"userId\", \"%1\")").arg(m_profile.id));
    url.setQuery(query);

    url.setQuery(query);

    QNetworkRequest request = createAuthorizedRequest(url);

    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, &AuthManager::onLicenseFetched);
}

void AuthManager::onLicenseFetched()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject obj = doc.object();
        QJsonArray documents = obj["documents"].toArray();

        if (!documents.isEmpty()) {
            QJsonObject licenseDoc = documents[0].toObject();

            m_license.id = licenseDoc["$id"].toString();
            m_license.userId = licenseDoc["userId"].toString();
            m_license.plan = licenseDoc["plan"].toString();
            m_license.isActive = licenseDoc["isActive"].toBool();
            m_license.maxDevices = licenseDoc["maxDevices"].toInt(1);

            // Parse expiration date
            QString expiresStr = licenseDoc["expiresAt"].toString();
            if (!expiresStr.isEmpty()) {
                m_license.expiresAt = QDateTime::fromString(expiresStr, Qt::ISODate);
            }

            // Parse features array
            m_license.features.clear();
            QJsonArray featuresArr = licenseDoc["features"].toArray();
            for (const auto& f : featuresArr) {
                m_license.features.append(f.toString());
            }

            // Parse device IDs array
            m_license.deviceIds.clear();
            QJsonArray devicesArr = licenseDoc["deviceIds"].toArray();
            for (const auto& d : devicesArr) {
                m_license.deviceIds.append(d.toString());
            }

            // Check if license is valid (active and not expired)
            if (!m_license.isValid()) {
                emit licenseExpired();
                reply->deleteLater();
                return;
            }

            // Check if current device is registered
            if (!m_license.deviceIds.contains(m_deviceId)) {
                if (m_license.deviceIds.count() < m_license.maxDevices) {
                    // Register this device
                    registerDevice();
                } else {
                    emit licenseError("Maximum devices reached. Please remove a device from your account.");
                }
            } else {
                saveLicense();  // Cache the license locally

                // Start runtime check
                if (!m_licenseCheckTimer->isActive()) {
                    m_licenseCheckTimer->start();
                }

                emit licenseLoaded();
            }
        } else {
            // No license found - user must activate a license key
            m_license = License(); // Reset to default (invalid)
            emit licenseExpired();
        }
    } else {
        emit licenseError("Failed to fetch license: " + reply->errorString());
    }
    reply->deleteLater();
}

void AuthManager::registerDevice()
{
    // Update license document with new device ID
    QString updateUrl = QString("%1/databases/%2/collections/%3/documents/%4")
        .arg(API_ENDPOINT)
        .arg(DATABASE_ID)
        .arg(LICENSES_COLLECTION)
        .arg(m_license.id);

    QUrl url(updateUrl);
    QNetworkRequest request = createAuthorizedRequest(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Add current device to list
    QStringList newDevices = m_license.deviceIds;
    newDevices.append(m_deviceId);

    QJsonObject data;
    QJsonArray devicesArr;
    for (const QString& d : newDevices) {
        devicesArr.append(d);
    }
    data["deviceIds"] = devicesArr;

    QJsonObject body;
    body["data"] = data;

    QNetworkReply* reply = m_network->sendCustomRequest(request, "PATCH", QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, &AuthManager::onDeviceRegistered);
}

void AuthManager::onDeviceRegistered()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        m_license.deviceIds.append(m_deviceId);
        saveLicense();  // Cache the license locally
        emit licenseLoaded();
    } else {
        emit licenseError("Failed to register device: " + reply->errorString());
    }
    reply->deleteLater();
}

void AuthManager::logout()
{
    if (!m_isAuthenticated) return;

    QUrl url(API_ENDPOINT + "/account/sessions/current");
    QNetworkRequest request(url);
    request.setRawHeader("X-Appwrite-Project", PROJECT_ID.toUtf8());
    if (!m_sessionId.isEmpty()) {
        request.setRawHeader("X-Appwrite-Session", m_sessionId.toUtf8());
    }

    m_network->deleteResource(request);
    clearSession();
}

void AuthManager::saveSession(const QString& sessionId)
{
    QSettings settings("SiteSurveyor", "Auth");
    settings.setValue("sessionId", sessionId);
    m_sessionId = sessionId;
}

void AuthManager::loadSession()
{
    QSettings settings("SiteSurveyor", "Auth");
    m_sessionId = settings.value("sessionId").toString();
}

void AuthManager::clearSession()
{
    QSettings settings("SiteSurveyor", "Auth");
    settings.remove("sessionId");
    // Clear license cache
    settings.remove("license/id");
    settings.remove("license/userId");
    settings.remove("license/plan");
    settings.remove("license/expiresAt");
    settings.remove("license/features");
    settings.remove("license/deviceIds");
    settings.remove("license/maxDevices");
    settings.remove("license/isActive");
    settings.remove("license/licenseKey");
    // Clear profile cache
    settings.remove("profile/id");
    settings.remove("profile/name");
    settings.remove("profile/email");
    settings.remove("profile/username");
    settings.remove("profile/organization");
    settings.remove("profile/userType");
    settings.remove("profile/city");
    settings.remove("profile/country");

    m_sessionId.clear();
    m_profile = UserProfile();
    m_license = License();
    m_isAuthenticated = false;
    m_licenseCheckTimer->stop();
}

void AuthManager::saveLicense()
{
    QSettings settings("SiteSurveyor", "Auth");
    settings.setValue("license/id", m_license.id);
    settings.setValue("license/userId", m_license.userId);
    settings.setValue("license/plan", m_license.plan);
    settings.setValue("license/expiresAt", m_license.expiresAt.toString(Qt::ISODate));
    settings.setValue("license/features", m_license.features);
    settings.setValue("license/deviceIds", m_license.deviceIds);
    settings.setValue("license/maxDevices", m_license.maxDevices);
    settings.setValue("license/isActive", m_license.isActive);
    qDebug() << "[License] Saved license to cache:" << m_license.id << "plan:" << m_license.plan;
}

void AuthManager::loadLicense()
{
    QSettings settings("SiteSurveyor", "Auth");
    QString cachedId = settings.value("license/id").toString();

    if (!cachedId.isEmpty()) {
        m_license.id = cachedId;
        m_license.userId = settings.value("license/userId").toString();
        m_license.plan = settings.value("license/plan").toString();
        QString expiresStr = settings.value("license/expiresAt").toString();
        if (!expiresStr.isEmpty()) {
            m_license.expiresAt = QDateTime::fromString(expiresStr, Qt::ISODate);
        }
        m_license.features = settings.value("license/features").toStringList();
        m_license.deviceIds = settings.value("license/deviceIds").toStringList();
        m_license.maxDevices = settings.value("license/maxDevices", 1).toInt();
        m_license.isActive = settings.value("license/isActive", false).toBool();
        qDebug() << "[License] Loaded cached license:" << m_license.id << "plan:" << m_license.plan << "valid:" << m_license.isValid();
    }

    // Also load cached profile for offline mode
    loadProfile();
}

void AuthManager::saveProfile()
{
    QSettings settings("SiteSurveyor", "Auth");
    settings.setValue("profile/id", m_profile.id);
    settings.setValue("profile/name", m_profile.name);
    settings.setValue("profile/email", m_profile.email);
    settings.setValue("profile/username", m_profile.username);
    settings.setValue("profile/organization", m_profile.organization);
    settings.setValue("profile/userType", m_profile.userType);
    settings.setValue("profile/city", m_profile.city);
    settings.setValue("profile/country", m_profile.country);
    qDebug() << "[Profile] Saved profile to cache:" << m_profile.email;
}

void AuthManager::loadProfile()
{
    QSettings settings("SiteSurveyor", "Auth");
    QString cachedEmail = settings.value("profile/email").toString();

    if (!cachedEmail.isEmpty()) {
        m_profile.id = settings.value("profile/id").toString();
        m_profile.name = settings.value("profile/name").toString();
        m_profile.email = cachedEmail;
        m_profile.username = settings.value("profile/username").toString();
        m_profile.organization = settings.value("profile/organization").toString();
        m_profile.userType = settings.value("profile/userType").toString();
        m_profile.city = settings.value("profile/city").toString();
        m_profile.country = settings.value("profile/country").toString();

        // If we have a cached session and profile, consider authenticated for offline mode
        if (!m_sessionId.isEmpty() && m_license.isValid()) {
            m_isAuthenticated = true;
            qDebug() << "[Profile] Loaded cached profile (offline mode):" << m_profile.email;
        }
    }
}



void AuthManager::checkLicenseExpiration()
{
    if (!m_license.isValid()) {
        qDebug() << "[License] Runtime check: License expired or invalid";
        m_licenseCheckTimer->stop();
        emit licenseExpired();
    }
}

void AuthManager::fetchJwt()
{
    if (m_sessionId.isEmpty()) {
        qDebug() << "[AuthManager] Cannot fetch JWT: no session";
        return;
    }

    QUrl url(API_ENDPOINT + "/account/jwts");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("X-Appwrite-Project", PROJECT_ID.toUtf8());
    // For JWT creation, we need the session cookie since we don't have JWT yet
    QString cookieName = QString("a_session_%1").arg(PROJECT_ID);
    QString cookieValue = QString("%1=%2").arg(cookieName, m_sessionId);
    request.setRawHeader("Cookie", cookieValue.toUtf8());

    QNetworkReply* reply = m_network->post(request, QByteArray("{}"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject obj = doc.object();
            m_jwt = obj["jwt"].toString();
            qDebug() << "[AuthManager] JWT fetched successfully, expires in 15 minutes";

            // Start auto-refresh timer
            if (!m_jwtRefreshTimer->isActive()) {
                m_jwtRefreshTimer->start();
                qDebug() << "[AuthManager] JWT auto-refresh timer started (13 min interval)";
            }
        } else {
            qDebug() << "[AuthManager] Failed to fetch JWT:" << reply->errorString();
            QByteArray response = reply->readAll();
            qDebug() << "[AuthManager] Response:" << response;
        }
        reply->deleteLater();
    });
}

void AuthManager::refreshJwt()
{
    if (m_sessionId.isEmpty()) {
        qDebug() << "[AuthManager] Cannot refresh JWT: no session";
        m_jwtRefreshTimer->stop();
        return;
    }

    qDebug() << "[AuthManager] Auto-refreshing JWT...";

    QUrl url(API_ENDPOINT + "/account/jwts");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("X-Appwrite-Project", PROJECT_ID.toUtf8());
    QString cookieName = QString("a_session_%1").arg(PROJECT_ID);
    QString cookieValue = QString("%1=%2").arg(cookieName, m_sessionId);
    request.setRawHeader("Cookie", cookieValue.toUtf8());

    QNetworkReply* reply = m_network->post(request, QByteArray("{}"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject obj = doc.object();
            m_jwt = obj["jwt"].toString();
            qDebug() << "[AuthManager] JWT refreshed successfully";
            emit jwtRefreshed();
        } else {
            qDebug() << "[AuthManager] JWT refresh failed:" << reply->errorString();
            // Don't stop timer - will retry on next interval
        }
        reply->deleteLater();
    });
}

// =========================================================================
// OFFLINE LOGIN SUPPORT
// =========================================================================

QString AuthManager::hashCredentials(const QString& email, const QString& password) const
{
    // Create a secure hash of email + password + device-specific salt
    QString combined = email.toLower() + ":" + password + ":" + m_deviceId;
    QByteArray hash = QCryptographicHash::hash(combined.toUtf8(), QCryptographicHash::Sha256);
    return hash.toHex();
}

void AuthManager::saveOfflineCredentials(const QString& email, const QString& password)
{
    QSettings settings("SiteSurveyor", "Auth");
    QString credHash = hashCredentials(email, password);
    settings.setValue("offline/credentialHash", credHash);
    settings.setValue("offline/email", email.toLower());
    settings.setValue("offline/enabled", true);
    qDebug() << "[AuthManager] Offline credentials saved for:" << email;
}

bool AuthManager::hasOfflineCredentials() const
{
    QSettings settings("SiteSurveyor", "Auth");
    return settings.value("offline/enabled", false).toBool() &&
           !settings.value("offline/credentialHash").toString().isEmpty() &&
           !settings.value("profile/email").toString().isEmpty();
}

bool AuthManager::tryOfflineLogin(const QString& email, const QString& password)
{
    QSettings settings("SiteSurveyor", "Auth");

    // Check if offline login is enabled and credentials exist
    if (!settings.value("offline/enabled", false).toBool()) {
        qDebug() << "[AuthManager] Offline login not enabled";
        return false;
    }

    QString storedHash = settings.value("offline/credentialHash").toString();
    QString storedEmail = settings.value("offline/email").toString();

    if (storedHash.isEmpty() || storedEmail.isEmpty()) {
        qDebug() << "[AuthManager] No stored offline credentials";
        return false;
    }

    // Verify email matches
    if (email.toLower() != storedEmail.toLower()) {
        qDebug() << "[AuthManager] Offline login: email mismatch";
        return false;
    }

    // Verify password hash
    QString providedHash = hashCredentials(email, password);
    if (providedHash != storedHash) {
        qDebug() << "[AuthManager] Offline login: password mismatch";
        return false;
    }

    // Credentials match - load cached profile and authenticate
    loadProfile();
    loadLicense();

    // Verify we have cached profile data
    if (m_profile.email.isEmpty()) {
        qDebug() << "[AuthManager] Offline login: no cached profile";
        return false;
    }

    m_isAuthenticated = true;
    m_isOfflineMode = true;

    qDebug() << "[AuthManager] Offline login successful for:" << m_profile.email;
    emit offlineLoginSuccess();

    return true;
}
