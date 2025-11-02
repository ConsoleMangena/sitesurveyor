#ifndef APPWRITECLIENT_H
#define APPWRITECLIENT_H

#include <QObject>
#include <QUrl>
#include <QJsonObject>

class QNetworkAccessManager;
class QNetworkReply;

class AppwriteClient : public QObject {
    Q_OBJECT
public:
    explicit AppwriteClient(QObject* parent = nullptr);

    void setEndpoint(const QString& endpoint);
    void setProjectId(const QString& projectId);
    void setSelfSigned(bool allow);

    QString endpoint() const { return m_endpoint; }
    QString projectId() const { return m_projectId; }

    bool isConfigured() const { return !m_endpoint.isEmpty() && !m_projectId.isEmpty(); }
    bool isLoggedIn() const { return m_loggedIn; }

    // Account
    QNetworkReply* signUp(const QString& email, const QString& password, const QString& name = QString());
    QNetworkReply* login(const QString& email, const QString& password);
    QNetworkReply* logout();

    // Account info and preferences
    QNetworkReply* getAccount();
    QNetworkReply* getPrefs();
    QNetworkReply* updatePrefs(const QJsonObject& prefs);

    // Verification
    QNetworkReply* requestVerification(const QString& redirectUrl = QString());

signals:
    void sslIssue(const QString& message);

private:
    QNetworkReply* sendJson(const QString& method, const QString& path, const QJsonObject& obj = QJsonObject());
    QNetworkReply* send(const QString& method, const QString& path, const QByteArray& body = QByteArray(), const QByteArray& contentType = QByteArray("application/json"));

    QString m_endpoint;
    QString m_projectId;
    bool m_selfSigned{false};
    bool m_loggedIn{false};
    QNetworkAccessManager* m_nam{nullptr};
};

#endif // APPWRITECLIENT_H
