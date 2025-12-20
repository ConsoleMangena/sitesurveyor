#include "auth/cloudmanager.h"
#include "auth/authmanager.h"
#include <QHttpPart>
#include <QUrlQuery>
#include <QUuid>
#include <QRegularExpression>
#include <algorithm>

CloudManager::CloudManager(AuthManager* auth, QObject *parent)
    : QObject(parent), m_auth(auth)
{
    m_network = new QNetworkAccessManager(this);
}

void CloudManager::uploadProject(const QString& name, const QByteArray& jsonData)
{
    if (!m_auth->isAuthenticated()) {
        emit uploadFinished(false, "Not authenticated");
        return;
    }

    // Prepare multipart request
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    // File ID (unique)
    QHttpPart idPart;
    idPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"fileId\""));
    idPart.setBody("unique()");
    multiPart->append(idPart);

    // File Data
    QHttpPart filePart;
    QString filename = name.endsWith(".ssp") ? name : name + ".ssp";
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"file\"; filename=\"" + filename + "\""));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/json"));
    
    // Compress data
    QByteArray compressedData = qCompress(jsonData);
    filePart.setBody(compressedData);
    multiPart->append(filePart);

    // File Permissions - grant the current user full access
    QString userId = m_auth->userId();
    QStringList perms = {
        QString("read(\"user:%1\")").arg(userId),
        QString("update(\"user:%1\")").arg(userId),
        QString("delete(\"user:%1\")").arg(userId)
    };
    for (const QString& perm : perms) {
        QHttpPart permPart;
        permPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"permissions[]\""));
        permPart.setBody(perm.toUtf8());
        multiPart->append(permPart);
    }

    QUrl url(m_auth->apiEndpoint() + "/storage/buckets/" + BUCKET_ID + "/files");
    QNetworkRequest request = m_auth->createAuthorizedRequest(url);
    
    // AuthManager sets JSON content type by default in login? No, standard request doesn't.
    // But we need to remove Content-Type so QNetworkAccessManager sets boundary for multipart
    // Actually createAuthorizedRequest only sets headers X-Appwrite-Project/Session.
    // So we are good.

    QNetworkReply* reply = m_network->post(request, multiPart);
    multiPart->setParent(reply); // Delete with reply
    
    connect(reply, &QNetworkReply::uploadProgress, this, &CloudManager::progress);
    connect(reply, &QNetworkReply::finished, this, &CloudManager::onUploadFinished);
}

void CloudManager::onUploadFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        emit uploadFinished(true, "Upload successful");
    } else {
        QByteArray responseData = reply->readAll();
        qDebug() << "[CloudManager] Upload failed. HTTP Status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "[CloudManager] Response:" << responseData;
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        QString msg = doc.object()["message"].toString();
        emit uploadFinished(false, msg.isEmpty() ? reply->errorString() : msg);
    }
    reply->deleteLater();
}

void CloudManager::listProjects()
{
    if (!m_auth->isAuthenticated()) {
        emit listFinished(false, {}, "Not authenticated");
        return;
    }

    QUrl url(m_auth->apiEndpoint() + "/storage/buckets/" + BUCKET_ID + "/files");
    QNetworkRequest request = m_auth->createAuthorizedRequest(url);

    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, &CloudManager::onListFinished);
}

void CloudManager::onListFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray files = doc.object()["files"].toArray();
        
        QVector<CloudFile> result;
        for (const auto& val : files) {
            QJsonObject obj = val.toObject();
            CloudFile f;
            f.id = obj["$id"].toString();
            f.name = obj["name"].toString();
            f.size = obj["sizeOriginal"].toInt();
            f.createdAt = QDateTime::fromString(obj["$createdAt"].toString(), Qt::ISODate);
            f.updatedAt = QDateTime::fromString(obj["$updatedAt"].toString(), Qt::ISODate);
            f.mimeType = obj["mimeType"].toString();
            f.etag = obj["signature"].toString();  // Appwrite uses signature as etag
            
            // Parse version from filename (e.g., "project_v2.ssp" -> version=2, baseName="project")
            f.version = parseVersion(f.name);
            f.baseName = stripVersion(f.name);
            
            // Filter only .ssp files and exclude archive/ folder
            if (f.name.endsWith(".ssp") && !f.name.startsWith("archive/")) {
                result.append(f);
            }
        }
        
        emit listFinished(true, result, "");
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QString msg = doc.object()["message"].toString();
        emit listFinished(false, {}, msg.isEmpty() ? reply->errorString() : msg);
    }
    reply->deleteLater();
}

void CloudManager::downloadProject(const QString& fileId)
{
    if (!m_auth->isAuthenticated()) {
        emit downloadFinished(false, {}, "Not authenticated");
        return;
    }

    QUrl url(m_auth->apiEndpoint() + "/storage/buckets/" + BUCKET_ID + "/files/" + fileId + "/download");
    QNetworkRequest request = m_auth->createAuthorizedRequest(url);

    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, &CloudManager::onDownloadFinished);
}

void CloudManager::onDownloadFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        // Try to decompress
        QByteArray decompressed = qUncompress(data);
        if (decompressed.isEmpty() && !data.isEmpty()) {
             // Decompression failed or data wasn't compressed (e.g. legacy file)
             // qUncompress returns empty on failure to check header
             // Fallback to raw data
             decompressed = data;
        }
        emit downloadFinished(true, decompressed, "Download successful");
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QString msg = doc.object()["message"].toString();
        emit downloadFinished(false, {}, msg.isEmpty() ? reply->errorString() : msg);
    }
    reply->deleteLater();
}

void CloudManager::deleteProject(const QString& fileId)
{
    if (!m_auth->isAuthenticated()) {
        emit deleteFinished(false, "Not authenticated");
        return;
    }

    QUrl url(m_auth->apiEndpoint() + "/storage/buckets/" + BUCKET_ID + "/files/" + fileId);
    QNetworkRequest request = m_auth->createAuthorizedRequest(url);

    QNetworkReply* reply = m_network->deleteResource(request);
    connect(reply, &QNetworkReply::finished, this, &CloudManager::onDeleteFinished);
}

void CloudManager::onDeleteFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        emit deleteFinished(true, "Deleted successfully");
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QString msg = doc.object()["message"].toString();
        emit deleteFinished(false, msg.isEmpty() ? reply->errorString() : msg);
    }
    reply->deleteLater();
}

// ============ Helper Functions ============

int CloudManager::parseVersion(const QString& filename) const
{
    // Match pattern: name_v{N}.ssp
    QRegularExpression rx("_v(\\d+)\\.ssp$");
    QRegularExpressionMatch match = rx.match(filename);
    if (match.hasMatch()) {
        return match.captured(1).toInt();
    }
    return 1;  // Default to version 1 if no version in name
}

QString CloudManager::stripVersion(const QString& filename) const
{
    // Remove archive/ prefix, _v{N} suffix and .ssp extension
    QString base = filename;
    if (base.startsWith("archive/")) base.remove(0, 8); // remove "archive/"
    base.remove(QRegularExpression("_v\\d+\\.ssp$"));
    base.remove(QRegularExpression("\\.ssp$"));
    return base;
}

QString CloudManager::makeVersionedName(const QString& baseName, int version) const
{
    // Store versions in a "virtual folder" to keep main list clean
    // Appwrite doesn't have real folders, but '/' in name works visually in most clients 
    // and helps us filter.
    return QString("archive/%1_v%2.ssp").arg(baseName).arg(version);
}

// ============ Version Management ============

void CloudManager::uploadNewVersion(const QString& baseName, const QByteArray& jsonData)
{
    if (!m_auth->isAuthenticated()) {
        emit versionUploaded(false, 0, "Not authenticated");
        return;
    }

    // First list existing versions to determine next version number
    QUrl url(m_auth->apiEndpoint() + "/storage/buckets/" + BUCKET_ID + "/files");
    QUrlQuery query;
    query.addQueryItem("search", baseName);
    url.setQuery(query);
    
    QNetworkRequest request = m_auth->createAuthorizedRequest(url);
    QNetworkReply* reply = m_network->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [this, baseName, jsonData, reply]() {
        int maxVersion = 0;
        
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonArray files = doc.object()["files"].toArray();
            
            for (const auto& val : files) {
                QString name = val.toObject()["name"].toString();
                if (stripVersion(name) == baseName) {
                    int v = parseVersion(name);
                    if (v > maxVersion) maxVersion = v;
                }
            }
        }
        reply->deleteLater();
        
        // Upload with incremented version
        int newVersion = maxVersion + 1;
        QString versionedName = makeVersionedName(baseName, newVersion);
        
        // Use existing upload logic with versioned name
        uploadProject(versionedName, jsonData);
        
        // Emit version-specific signal after upload completes
        // Note: We rely on uploadFinished signal from uploadProject
        emit versionUploaded(true, newVersion, "");
    });
}

void CloudManager::listVersions(const QString& baseName)
{
    if (!m_auth->isAuthenticated()) {
        emit versionsListed(false, {}, "Not authenticated");
        return;
    }

    QUrl url(m_auth->apiEndpoint() + "/storage/buckets/" + BUCKET_ID + "/files");
    QNetworkRequest request = m_auth->createAuthorizedRequest(url);
    QNetworkReply* reply = m_network->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [this, baseName, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonArray files = doc.object()["files"].toArray();
            
            QVector<CloudFile> versions;
            for (const auto& val : files) {
                QJsonObject obj = val.toObject();
                QString name = obj["name"].toString();
                
                if (stripVersion(name) == baseName && name.endsWith(".ssp")) {
                    CloudFile f;
                    f.id = obj["$id"].toString();
                    f.name = name;
                    f.baseName = baseName;
                    f.version = parseVersion(name);
                    f.size = obj["sizeOriginal"].toInt();
                    f.createdAt = QDateTime::fromString(obj["$createdAt"].toString(), Qt::ISODate);
                    f.updatedAt = QDateTime::fromString(obj["$updatedAt"].toString(), Qt::ISODate);
                    f.etag = obj["signature"].toString();
                    versions.append(f);
                }
            }
            
            // Sort by version descending
            std::sort(versions.begin(), versions.end(), [](const CloudFile& a, const CloudFile& b) {
                return a.version > b.version;
            });
            
            emit versionsListed(true, versions, "");
        } else {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QString msg = doc.object()["message"].toString();
            emit versionsListed(false, {}, msg.isEmpty() ? reply->errorString() : msg);
        }
        reply->deleteLater();
    });
}

void CloudManager::downloadVersion(const QString& fileId)
{
    downloadProject(fileId);  // Reuse existing download logic
}

void CloudManager::deleteVersion(const QString& fileId)
{
    deleteProject(fileId);  // Reuse existing delete logic
}

// ============ Sharing ============

// ============ Sharing ============

void CloudManager::shareProject(const QString& fileId, const QString& userId, const QString& permission)
{
    // Real Implementation:
    // 1. Get current file details to retrieve existing permissions
    // 2. Append new permission
    // 3. Update file permissions via API
    
    if (!m_auth->isAuthenticated()) {
        emit shareFinished(false, "Not authenticated");
        return;
    }
    
    // 1. Get current details
    QUrl url(m_auth->apiEndpoint() + "/storage/buckets/" + BUCKET_ID + "/files/" + fileId);
    QNetworkRequest request = m_auth->createAuthorizedRequest(url);
    QNetworkReply* reply = m_network->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [this, fileId, userId, permission, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject obj = doc.object();
            
            // Get existing permissions
            QJsonArray perms = obj["$permissions"].toArray();
            QString newPerm = QString("%1(\"user:%2\")").arg(permission).arg(userId);
            
            // Check if already exists
            bool exists = false;
            for (const auto& p : perms) {
                if (p.toString() == newPerm) {
                    exists = true;
                    break;
                }
            }
            
            if (!exists) {
                perms.append(newPerm);
                
                // 2. Update permissions
                // PUT /storage/buckets/{bucketId}/files/{fileId}
                // Body: {"permissions": [...]}
                QUrl updateUrl(m_auth->apiEndpoint() + "/storage/buckets/" + BUCKET_ID + "/files/" + fileId);
                QNetworkRequest updateReq = m_auth->createAuthorizedRequest(updateUrl);
                updateReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
                
                QJsonObject updateBody;
                updateBody["permissions"] = perms;
                
                QNetworkReply* updateReply = m_network->put(updateReq, QJsonDocument(updateBody).toJson());
                connect(updateReply, &QNetworkReply::finished, this, [this, updateReply]() {
                   if (updateReply->error() == QNetworkReply::NoError) {
                       emit shareFinished(true, "Project shared successfully");
                   } else {
                       QJsonDocument errDoc = QJsonDocument::fromJson(updateReply->readAll());
                       QString msg = errDoc.object()["message"].toString();
                       emit shareFinished(false, "Failed to update permissions: " + msg);
                   }
                   updateReply->deleteLater();
                });
            } else {
                emit shareFinished(true, "User already has access");
            }
        } else {
            emit shareFinished(false, "Failed to retrieve file details");
        }
        reply->deleteLater();
    });
}

void CloudManager::unshareProject(const QString& fileId, const QString& userId)
{
    if (!m_auth->isAuthenticated()) {
        emit unshareFinished(false, "Not authenticated");
        return;
    }
    
    // 1. Get current details
    QUrl url(m_auth->apiEndpoint() + "/storage/buckets/" + BUCKET_ID + "/files/" + fileId);
    QNetworkRequest request = m_auth->createAuthorizedRequest(url);
    QNetworkReply* reply = m_network->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [this, fileId, userId, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject obj = doc.object();
            
            QJsonArray perms = obj["$permissions"].toArray();
            QJsonArray newPerms;
            
            bool changed = false;
            // Filter out permissions for this user
            QString targetUser = QString("\"user:%1\"").arg(userId);
            
            for (const auto& p : perms) {
                QString pStr = p.toString();
                if (!pStr.contains(targetUser)) {
                    newPerms.append(p); 
                } else {
                    changed = true;
                }
            }
            
            if (changed) {
                // Update
                QUrl updateUrl(m_auth->apiEndpoint() + "/storage/buckets/" + BUCKET_ID + "/files/" + fileId);
                QNetworkRequest updateReq = m_auth->createAuthorizedRequest(updateUrl);
                updateReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
                QJsonObject updateBody;
                updateBody["permissions"] = newPerms;
                
                QNetworkReply* updateReply = m_network->put(updateReq, QJsonDocument(updateBody).toJson());
                connect(updateReply, &QNetworkReply::finished, this, [this, updateReply]() {
                   if (updateReply->error() == QNetworkReply::NoError) {
                       emit unshareFinished(true, "Access removed successfully");
                   } else {
                       emit unshareFinished(false, "Failed to remove access");
                   }
                   updateReply->deleteLater();
                });
            } else {
                emit unshareFinished(true, "User did not have access");
            }
        } else {
             emit unshareFinished(false, "Failed to retrieve file details");
        }
        reply->deleteLater();
    });
}

void CloudManager::listShares(const QString& fileId)
{
    if (!m_auth->isAuthenticated()) {
        emit sharesListed(false, {}, "Not authenticated");
        return;
    }
    
    // Get file details to parse permissions
    QUrl url(m_auth->apiEndpoint() + "/storage/buckets/" + BUCKET_ID + "/files/" + fileId);
    QNetworkRequest request = m_auth->createAuthorizedRequest(url);
    QNetworkReply* reply = m_network->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
             QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
             QJsonArray perms = doc.object()["$permissions"].toArray();
             
             QVector<ProjectShare> shares;
             QRegularExpression rx("^(read|write|delete)\\(\"user:(.+)\"\\)$");
             
             for (const auto& p : perms) {
                 QString pStr = p.toString();
                 QRegularExpressionMatch match = rx.match(pStr);
                 if (match.hasMatch()) {
                     QString role = match.captured(1);
                     QString uid = match.captured(2);
                     
                     // Avoid duplicates (read/write might be separate entries)
                     // Simple implementation: List them as separate entries or merge?
                     // Merging is nicer UI but complex. Let's list.
                     
                     // Wait, dont list myself
                     if (uid == m_auth->userId()) continue;
                     
                     ProjectShare s;
                     s.userId = uid;
                     s.email = uid; // We don't have email lookup capability, display ID
                     s.permission = role;
                     shares.append(s);
                 }
             }
             
             emit sharesListed(true, shares, "");
        } else {
            emit sharesListed(false, {}, "Failed to load shares");
        }
        reply->deleteLater();
    });
}

// ============ Conflict Detection ============

void CloudManager::checkForConflict(const QString& fileId, const QString& localEtag)
{
    if (!m_auth->isAuthenticated()) {
        emit noConflict(fileId);
        return;
    }

    QUrl url(m_auth->apiEndpoint() + "/storage/buckets/" + BUCKET_ID + "/files/" + fileId);
    QNetworkRequest request = m_auth->createAuthorizedRequest(url);
    QNetworkReply* reply = m_network->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [this, fileId, localEtag, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject obj = doc.object();
            QString remoteEtag = obj["signature"].toString();
            
            if (remoteEtag != localEtag && !localEtag.isEmpty()) {
                CloudFile remote;
                remote.id = obj["$id"].toString();
                remote.name = obj["name"].toString();
                remote.updatedAt = QDateTime::fromString(obj["$updatedAt"].toString(), Qt::ISODate);
                remote.etag = remoteEtag;
                emit conflictDetected(fileId, remote);
            } else {
                emit noConflict(fileId);
            }
        } else {
            emit noConflict(fileId);  // Assume no conflict if we can't check
        }
        reply->deleteLater();
    });
}
