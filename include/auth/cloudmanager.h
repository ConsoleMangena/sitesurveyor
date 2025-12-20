#ifndef CLOUDMANAGER_H
#define CLOUDMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QHttpMultiPart>
#include <QFile>
#include <QFileInfo>

class AuthManager;

struct CloudFile {
    QString id;
    QString name;
    QString baseName;      // Name without version suffix
    int version;           // Version number extracted from name
    QDateTime createdAt;
    QDateTime updatedAt;
    int size;
    QString mimeType;
    QString authorId;
    QString etag;          // For conflict detection
};

struct ProjectShare {
    QString userId;
    QString email;
    QString permission;    // "read" or "write"
};

class CloudManager : public QObject
{
    Q_OBJECT

public:
    explicit CloudManager(AuthManager* auth, QObject *parent = nullptr);

    // Core actions
    void uploadProject(const QString& name, const QByteArray& jsonData);
    void listProjects();
    void downloadProject(const QString& fileId);
    void deleteProject(const QString& fileId);
    
    // Version management
    void uploadNewVersion(const QString& baseName, const QByteArray& jsonData);
    void listVersions(const QString& baseName);
    void downloadVersion(const QString& fileId);
    void deleteVersion(const QString& fileId);
    
    // Sharing
    void shareProject(const QString& fileId, const QString& userEmail, const QString& permission);
    void unshareProject(const QString& fileId, const QString& userId);
    void listShares(const QString& fileId);
    
    // Conflict detection
    void checkForConflict(const QString& fileId, const QString& localEtag);

signals:
    void uploadFinished(bool success, const QString& message);
    void listFinished(bool success, const QVector<CloudFile>& files, const QString& message);
    void downloadFinished(bool success, const QByteArray& data, const QString& message);
    void deleteFinished(bool success, const QString& message);
    void progress(qint64 bytesSent, qint64 bytesTotal);
    
    // Version signals
    void versionsListed(bool success, const QVector<CloudFile>& versions, const QString& message);
    void versionUploaded(bool success, int newVersion, const QString& message);
    
    // Sharing signals
    void shareFinished(bool success, const QString& message);
    void unshareFinished(bool success, const QString& message);
    void sharesListed(bool success, const QVector<ProjectShare>& shares, const QString& message);
    
    // Conflict signal
    void conflictDetected(const QString& fileId, const CloudFile& remoteFile);
    void noConflict(const QString& fileId);

private slots:
    void onUploadFinished();
    void onListFinished();
    void onDownloadFinished();
    void onDeleteFinished();

private:
    AuthManager* m_auth;
    QNetworkAccessManager* m_network;
    
    // Helper to parse version from filename
    int parseVersion(const QString& filename) const;
    QString stripVersion(const QString& filename) const;
    QString makeVersionedName(const QString& baseName, int version) const;
    
    // Config
    const QString BUCKET_ID = "projects"; 
};

#endif // CLOUDMANAGER_H
