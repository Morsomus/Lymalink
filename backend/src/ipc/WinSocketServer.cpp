/////////////////////////////////////////////////////////
// File: WinSocketServer.cpp
// Date: 2026-06-20
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements Windows local-socket IPC server
/////////////////////////////////////////////////////////

#include "WinSocketServer.h"
#include "Defines.h"
#include "tools/Logger.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>

#define COMPONENT "WinSocketServer"

/////////////////////////////////////////////////////////////////////

WinSocketServer::WinSocketServer(QObject *parent) : QObject(parent)
{
    connect(&m_server, &QLocalServer::newConnection, this, &WinSocketServer::HandleConnection);
}

WinSocketServer::~WinSocketServer()
{
    Stop();
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool WinSocketServer::Start()
{
    const QString serverName = QString::fromLatin1(WIN_SOCKET_SERVER_NAME);
    if (!m_server.listen(serverName))
    {
        // A previous crash can leave a stale endpoint - Never remove it before listening: doing so could disconnect a live daemon
        QLocalServer::removeServer(serverName);
        if (!m_server.listen(serverName))
        {
            LOG_BE(Urgency::Critical, "Windows IPC listen failed: %s", m_server.errorString().toUtf8().constData());
            return false;
        }
    }
    LOG_BE(Urgency::Info, "Windows IPC ready: %s", WIN_SOCKET_SERVER_NAME);
    return true;
}

/////////////////////////////////////////////////////////////////////

void WinSocketServer::Stop()
{
    for (QLocalSocket *client : m_clients)
    {
        client->disconnectFromServer();
        client->deleteLater();
    }
    m_clients.clear();
    m_server.close();
    QLocalServer::removeServer(QString::fromLatin1(WIN_SOCKET_SERVER_NAME));
}

/////////////////////////////////////////////////////////////////////

void WinSocketServer::EmitAchievementUnlocked(int targetId, const std::string& key)
{
    Broadcast({{"type", "event"}, {"event", "AchievementUnlocked"}, {"targetId", targetId}, {"key", QString::fromStdString(key)}});
}

/////////////////////////////////////////////////////////////////////

void WinSocketServer::EmitGameStateChanged(const std::vector<int>& targetIds, const std::string& state)
{
    QJsonArray ids;
    for (int id : targetIds)
    {
        ids.append(id);
    }
    Broadcast({{"type", "event"}, {"event", "GameStateChanged"}, {"targetIds", ids}, {"state", QString::fromStdString(state)}});
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void WinSocketServer::HandleConnection()
{
    while (QLocalSocket *socket = m_server.nextPendingConnection())
    {
        m_clients.insert(socket);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            HandleReadyRead(socket);
        });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket]() {
            m_clients.remove(socket);
            socket->deleteLater();
        });
    }
}

/////////////////////////////////////////////////////////////////////

void WinSocketServer::HandleReadyRead(QLocalSocket* socket)
{
    while (socket->canReadLine())
    {
        const QJsonDocument document = QJsonDocument::fromJson(socket->readLine().trimmed());
        if (!document.isObject())
        {
            Send(socket, {{"type", "response"}, {"ok", false}, {"error", "Invalid JSON request."}});
            continue;
        }
        HandleRequest(socket, document.object());
    }
}

/////////////////////////////////////////////////////////////////////

void WinSocketServer::HandleRequest(QLocalSocket* socket, const QJsonObject& request)
{
    const QJsonValue id = request.value("id");
    const QString method = request.value("method").toString();
    QJsonObject response{{"type", "response"}, {"id", id}, {"ok", true}};

    if (method == "Ping")
    {
        response.insert("result", "pong");
    }
    else if (method == "ReloadTarget" && onReloadTarget)
    {
        onReloadTarget(request.value("targetId").toInt());
    }
    else if (method == "RequestActiveTargets" && onRequestActiveTargets)
    {
        onRequestActiveTargets();
    }
    else if (method == "ReloadAllTargets" && onReloadAllTargets)
    {
        onReloadAllTargets();
    }
    else if (method == "ReloadConfig" && onReloadConfig)
    {
        onReloadConfig();
    }
    else if (method == "TestToast" && onTestToast)
    {
        onTestToast();
    }
    else if (method == "TestSound" && onTestSound)
    {
        onTestSound();
    }
    else if (method == "Shutdown" && onShutdown)
    {
        onShutdown();
    }
    else if (method != "Ping")
    {
        response.insert("ok", false);
        response.insert("error", "Unknown or unavailable method.");
    }

    Send(socket, response);
}

/////////////////////////////////////////////////////////////////////

void WinSocketServer::Send(QLocalSocket* socket, const QJsonObject& message)
{
    socket->write(QJsonDocument(message).toJson(QJsonDocument::Compact));
    socket->write("\n");
    socket->flush();
}

/////////////////////////////////////////////////////////////////////

void WinSocketServer::Broadcast(const QJsonObject& message)
{
    for (QLocalSocket *client : m_clients)
    {
        Send(client, message);
    }
}
