#include "WebsocketClient.h"

#include <QDebug>

WebsocketClient::WebsocketClient(const QUrl &url, QObject *parent)
: QObject(parent), m_url(url)
{
    connect(&m_webSocket, &QWebSocket::connected, this, &WebsocketClient::onConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &WebsocketClient::onDisconnected);
    connect(&m_webSocket, &QWebSocket::textMessageReceived, this, &WebsocketClient::onTextMessageReceived);

    m_webSocket.open(m_url);
}

void WebsocketClient::sendMessage(const QString &message)
{
    m_webSocket.sendTextMessage(message);
}

void WebsocketClient::onConnected()
{
    qDebug() << "WebSocket connected to" << m_url.toString();
    Q_EMIT connected();
}

void WebsocketClient::onDisconnected()
{
    qDebug() << "WebSocket disconnected";
    Q_EMIT disconnected();
}

void WebsocketClient::onTextMessageReceived(const QString &message)
{
    qDebug() << "Message received:" << message;
    Q_EMIT messageReceived(message);
}