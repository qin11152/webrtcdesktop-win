#pragma once

#include <QObject>
#include <QWebSocket>

class WebsocketClient : public QObject
{
    Q_OBJECT
public:
    explicit WebsocketClient(const QUrl &url, QObject *parent = nullptr);

    void sendMessage(const QString &message);

Q_SIGNALS:
    void connected();
    void disconnected();
    void messageReceived(const QString &message);

private Q_SLOTS:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);

private:
    QWebSocket m_webSocket;
    QUrl m_url;
};