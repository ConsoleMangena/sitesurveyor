#include "appwriteclient.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkCookieJar>
#include <QAuthenticator>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QSettings>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTimer>
#include <QSslError>
#include <QUuid>
#include <QInputDialog>
#include <QLineEdit>

static QByteArray methodToByteArray(const QString& m) {
    return m.toUtf8();
}

AppwriteClient::AppwriteClient(QObject* parent)
    : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);
    // Enable cookies so login session persists across requests
    QNetworkCookieJar* jar = new QNetworkCookieJar(m_nam);
    m_nam->setCookieJar(jar);
    // Disable system proxy by default; allow opt-in via settings
    {
        QSettings s;
        const bool useSystemProxy = s.value("network/useSystemProxy", false).toBool() || qEnvironmentVariableIsSet("APPWRITE_USE_SYSTEM_PROXY");
        if (useSystemProxy) {
            QNetworkProxyFactory::setUseSystemConfiguration(true);
        } else {
            QNetworkProxy noProxy(QNetworkProxy::NoProxy);
            m_nam->setProxy(noProxy);
        }
    }
    QObject::connect(m_nam, &QNetworkAccessManager::authenticationRequired, this, [this](QNetworkReply*, QAuthenticator* auth){
        QSettings s;
        QString u = s.value("cloud/httpAuthUser").toString();
        QString p = s.value("cloud/httpAuthPass").toString();
        if (u.isEmpty() || p.isEmpty()) {
            bool ok = false;
            const QString promptUser = QInputDialog::getText(nullptr, QStringLiteral("HTTP Authentication"),
                                                            QStringLiteral("Username:"), QLineEdit::Normal,
                                                            QString(), &ok);
            if (ok) {
                const QString promptPass = QInputDialog::getText(nullptr, QStringLiteral("HTTP Authentication"),
                                                                QStringLiteral("Password:"), QLineEdit::Password,
                                                                QString(), &ok);
                if (ok) { u = promptUser; p = promptPass; s.setValue("cloud/httpAuthUser", u); s.setValue("cloud/httpAuthPass", p); }
            }
        }
        if (!u.isEmpty()) auth->setUser(u);
        if (!p.isEmpty()) auth->setPassword(p);
    });
    QObject::connect(m_nam, &QNetworkAccessManager::proxyAuthenticationRequired, this, [this](const QNetworkProxy&, QAuthenticator* auth){
        QSettings s;
        QString u = s.value("cloud/proxyUser").toString();
        QString p = s.value("cloud/proxyPass").toString();
        if (u.isEmpty() || p.isEmpty()) {
            bool ok = false;
            const QString promptUser = QInputDialog::getText(nullptr, QStringLiteral("Proxy Authentication"),
                                                            QStringLiteral("Username:"), QLineEdit::Normal,
                                                            QString(), &ok);
            if (ok) {
                const QString promptPass = QInputDialog::getText(nullptr, QStringLiteral("Proxy Authentication"),
                                                                QStringLiteral("Password:"), QLineEdit::Password,
                                                                QString(), &ok);
                if (ok) { u = promptUser; p = promptPass; s.setValue("cloud/proxyUser", u); s.setValue("cloud/proxyPass", p); }
            }
        }
        if (!u.isEmpty()) auth->setUser(u);
        if (!p.isEmpty()) auth->setPassword(p);
    });
}

void AppwriteClient::setEndpoint(const QString& endpoint)
{
    m_endpoint = endpoint.trimmed();
    while (m_endpoint.endsWith('/')) m_endpoint.chop(1);
    if (m_endpoint.endsWith("/v1")) m_endpoint.chop(3);
}

void AppwriteClient::setProjectId(const QString& projectId)
{
    m_projectId = projectId.trimmed();
}

void AppwriteClient::setSelfSigned(bool allow)
{
    m_selfSigned = allow;
}

QNetworkReply* AppwriteClient::send(const QString& method, const QString& path, const QByteArray& body, const QByteArray& contentType)
{
    QUrl url(QString("%1%2").arg(m_endpoint, path));
    QNetworkRequest req(url);
    req.setRawHeader("X-Appwrite-Project", m_projectId.toUtf8());
    if (!contentType.isEmpty()) req.setHeader(QNetworkRequest::ContentTypeHeader, QString::fromUtf8(contentType));
    req.setRawHeader("Accept", "application/json");
    // Appwrite response format header for compatibility
    req.setRawHeader("X-Appwrite-Response-Format", "1.0");
    {
        QSettings s;
        QString u = s.value("cloud/httpAuthUser").toString();
        QString p = s.value("cloud/httpAuthPass").toString();
        if (!u.isEmpty() && !p.isEmpty()) {
            const QByteArray token = (u + ":" + p).toUtf8().toBase64();
            req.setRawHeader("Authorization", QByteArray("Basic ") + token);
        }
    }

    // Do NOT send an Origin header. Appwrite validates Origin against project platforms;
    // sending the server's own origin can trigger 401. Desktop apps should omit it.
    if (m_selfSigned) req.setRawHeader("X-Appwrite-Self-Signed", "true");


    QNetworkReply* reply = nullptr;
    const QByteArray verb = methodToByteArray(method);
    if (verb == "GET") reply = m_nam->get(req);
    else if (verb == "POST") reply = m_nam->post(req, body);
    else if (verb == "PUT") reply = m_nam->put(req, body);
    else if (verb == "PATCH") reply = m_nam->sendCustomRequest(req, verb, body);
    else if (verb == "DELETE") reply = m_nam->deleteResource(req);
    else reply = m_nam->sendCustomRequest(req, verb, body);

#ifndef QT_NO_SSL
    if (m_selfSigned && reply) {
        QObject::connect(reply, &QNetworkReply::sslErrors, reply, [reply](const QList<QSslError>&){
            reply->ignoreSslErrors();
        });
    }
#endif
    return reply;
}

QNetworkReply* AppwriteClient::sendJson(const QString& method, const QString& path, const QJsonObject& obj)
{
    QByteArray body;
    if (!obj.isEmpty()) body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    return send(method, path, body, QByteArray("application/json"));
}

QNetworkReply* AppwriteClient::signUp(const QString& email, const QString& password, const QString& name)
{
    QJsonObject body;
    // Per Appwrite API, clients should pass the literal string "unique()" to have the server create an ID.
    body.insert("userId", QStringLiteral("unique()"));
    body.insert("email", email);
    body.insert("password", password);
    if (!name.isEmpty()) body.insert("name", name);
    return sendJson("POST", "/v1/account", body);
}

QNetworkReply* AppwriteClient::login(const QString& email, const QString& password)
{
    QJsonObject body; body.insert("email", email); body.insert("password", password);
    QNetworkReply* r = sendJson("POST", "/v1/account/sessions/email", body);
    connect(r, &QNetworkReply::finished, this, [this, r](){
        if (r->error() == QNetworkReply::NoError) {
            m_loggedIn = true;
        } else {
            m_loggedIn = false;
        }
    });
    return r;
}

QNetworkReply* AppwriteClient::logout()
{
    QNetworkReply* r = send("DELETE", "/v1/account/sessions/current");
    connect(r, &QNetworkReply::finished, this, [this, r](){ Q_UNUSED(r); m_loggedIn = false; });
    // Clear cookies so subsequent calls are anonymous
    if (m_nam && m_nam->cookieJar()) {
        // Replace with a fresh jar
        QNetworkCookieJar* jar = new QNetworkCookieJar(m_nam);
        m_nam->setCookieJar(jar);
    }
    return r;
}

QNetworkReply* AppwriteClient::oauth2CreateToken(const QString& provider, const QString& userId, const QString& secret)
{
    QJsonObject body; body.insert("userId", userId); body.insert("secret", secret);
    const QString path = QString("/v1/account/sessions/oauth2/%1/token").arg(provider);
    QNetworkReply* r = sendJson("POST", path, body);
    connect(r, &QNetworkReply::finished, this, [this, r](){
        if (r->error() == QNetworkReply::NoError) m_loggedIn = true; else m_loggedIn = false;
    });
    return r;
}

QNetworkReply* AppwriteClient::getAccount()
{
    return send("GET", "/v1/account");
}

QNetworkReply* AppwriteClient::getPrefs()
{
    return send("GET", "/v1/account/prefs");
}

QNetworkReply* AppwriteClient::updatePrefs(const QJsonObject& prefs)
{
    QJsonObject body; body.insert("prefs", prefs);
    return sendJson("PUT", "/v1/account/prefs", body);
}

QNetworkReply* AppwriteClient::requestVerification(const QString& redirectUrl)
{
    QJsonObject body;
    const QString url = redirectUrl.isEmpty() ? QStringLiteral("https://sitesurveyor.app/verify") : redirectUrl;
    body.insert("url", url);
    return sendJson("POST", "/v1/account/verification", body);
}
