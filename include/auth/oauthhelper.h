#ifndef OAUTHHELPER_H
#define OAUTHHELPER_H

#include <QObject>
#include <QTcpServer>

class OAuthHelper : public QObject
{
    Q_OBJECT

public:
    explicit OAuthHelper(QObject *parent = nullptr);
    ~OAuthHelper();
    
    // Starts server on specific port (5173 is Appwrite's expected Flutter port)
    bool startListener(quint16 port = 5173);
    void stopListener();
    
    quint16 serverPort() const;

signals:
    void tokenReceived(const QString& secret);
    void failure(const QString& message);

private slots:
    void onNewConnection();

private:
    QTcpServer* m_server;
    
    void handleRequest(class QTcpSocket* socket);
};

#endif // OAUTHHELPER_H
