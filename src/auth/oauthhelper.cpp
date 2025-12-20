#include "auth/oauthhelper.h"
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>
#include <QTextStream>
#include <QDebug>

OAuthHelper::OAuthHelper(QObject *parent)
    : QObject(parent)
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &OAuthHelper::onNewConnection);
}

OAuthHelper::~OAuthHelper()
{
    stopListener();
}

bool OAuthHelper::startListener(quint16 port)
{
    if (m_server->isListening()) m_server->close();
    return m_server->listen(QHostAddress::LocalHost, port);
}

void OAuthHelper::stopListener()
{
    if (m_server->isListening()) m_server->close();
}

quint16 OAuthHelper::serverPort() const
{
    return m_server->serverPort();
}

void OAuthHelper::onNewConnection()
{
    QTcpSocket* socket = m_server->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        handleRequest(socket);
    });
}

void OAuthHelper::handleRequest(QTcpSocket* socket)
{
    if (!socket->canReadLine()) return;
    
    // Read all headers
    QByteArray requestData = socket->readAll();
    QString requestStr = QString::fromUtf8(requestData);
    
    // Parse first line
    QStringList lines = requestStr.split("\r\n");
    if (lines.isEmpty()) {
        socket->close();
        socket->deleteLater();
        return;
    }
    
    QString requestLine = lines.first();
    qDebug() << "OAuthHelper received:" << requestLine.trimmed();
    
    // Log all headers
    qDebug() << "=== Full HTTP Request ===";
    for (const QString& line : lines) {
        if (!line.isEmpty()) {
            qDebug() << line;
        }
    }
    qDebug() << "=========================";
    
    QStringList tokens = requestLine.split(' ');
    
    if (tokens.size() < 2 || tokens[0] != "GET") {
        socket->close();
        socket->deleteLater();
        return;
    }
    
    // Parse URL (e.g. "/success?secret=xyz&project=...")
    QUrl url = QUrl::fromUserInput("http://localhost" + tokens[1]);
    QString path = url.path();
    
    qDebug() << "Parsed Path:" << path;
    
    // 1. Handle SUCCESS (Serve the JS Bridge)
    if (path == "/success") {
        QString html = "<html><head><title>Verifying...</title></head>"
                       "<body>"
                       "<h1 id='msg'>Verifying Login...</h1>"
                       "<script>"
                       "  // Debug: Send full URL to server\n"
                       "  fetch('/debug?url=' + encodeURIComponent(window.location.href));\n"
                       "  // Check both Hash and Query for secret\n"
                       "  var s = '';\n"
                       "  var q = window.location.search;\n"
                       "  if(q.includes('secret=')) s = new URLSearchParams(q).get('secret');\n"
                       "  if(!s && window.location.hash.includes('secret=')) {\n"
                       "      s = new URLSearchParams(window.location.hash.substring(1)).get('secret');\n"
                       "  }\n"
                       "  if(s) {\n"
                       "      fetch('/token?secret=' + s).then(r => window.close());\n"
                       "      document.getElementById('msg').innerText = 'Login successful! Closing...';\n"
                       "  } else {\n"
                       "      document.getElementById('msg').innerText = 'Login failed. No secret found.';\n"
                       "      fetch('/failure?reason=nosecret');\n"
                       "  }\n"
                       "</script>"
                       "</body></html>";

        QTextStream os(socket);
        // Use HTTP/1.1 and explicit close header
        os << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: text/html\r\n"
           << "Connection: close\r\n"
           << "\r\n"
           << html;
        os.flush();
        
        // Wait for bytes to be written before closing
        socket->flush();
        socket->disconnectFromHost();
        if (socket->state() != QAbstractSocket::UnconnectedState) {
            socket->waitForDisconnected(1000);
        }
        socket->deleteLater();
        return;
    }

    // DEBUG ENDPOINT
    if (path == "/debug") {
        QUrlQuery query(url);
        qDebug() << "Reflected Client URL:" << query.queryItemValue("url");
        QTextStream os(socket);
        os << "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nOK";
        os.flush();
        socket->flush();
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }
    
    // 2. Handle TOKEN (Received from JS)
    if (path == "/token") {
        QUrlQuery query(url);
        QString secret = query.queryItemValue("secret");
        qDebug() << "Parsed Secret from JS:" << secret;

        if (!secret.isEmpty()) {
            emit tokenReceived(secret);
            
            // Acknowledge
            QTextStream os(socket);
            os << "HTTP/1.1 200 OK\r\n"
               << "Connection: close\r\n\r\n" 
               << "Done";
            os.flush();
            socket->flush();
            socket->disconnectFromHost();
            if (socket->state() != QAbstractSocket::UnconnectedState) {
                socket->waitForDisconnected(1000);
            }
            socket->deleteLater();
            m_server->close();
            return;
        }
    }

    // 3. Handle FAILURE
    if (path == "/failure") {
        emit failure("Login failed or cancelled.");
        QTextStream os(socket);
        os << "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nFailed";
        os.flush();
        socket->flush();
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }
    
    // 4. Ignore others (favicon)
    QTextStream os(socket);
    os << "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
    os.flush();
    socket->flush();
    socket->disconnectFromHost();
    socket->deleteLater();
}
