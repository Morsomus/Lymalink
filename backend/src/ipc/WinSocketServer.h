/////////////////////////////////////////////////////////
// File: WinSocketServer.h
// Date: 2026-06-20
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Windows local-socket IPC server
/////////////////////////////////////////////////////////

#pragma once

#include <QObject>
#include <QLocalServer>
#include <QSet>

#include <functional>

class QLocalSocket;
class QJsonObject;

class WinSocketServer : public QObject
{
    Q_OBJECT

public:
    explicit WinSocketServer(QObject *parent = nullptr);
    ~WinSocketServer() override;

    bool Start();
    void Stop();
    void EmitAchievementUnlocked(int targetId, const std::string& key);
    void EmitGameStateChanged(const std::vector<int>& targetIds, const std::string& state);
    void EmitTargetDataChanged(int targetId);
    void EmitManualAchievementDataScanFinished(int targetId, bool found, const std::string& reason);

    std::function<void(int)> onReloadTarget;
    std::function<void()> onRequestActiveTargets;
    std::function<void()> onReloadAllTargets;
    std::function<void()> onReloadConfig;
    std::function<void(int)> onStartManualAchievementDataScan;
    std::function<void(int)> onCancelManualAchievementDataScan;
    std::function<void()> onTestToast;
    std::function<void()> onTestSound;
    std::function<void()> onShutdown;

private:
    QLocalServer m_server;
    QSet<QLocalSocket *> m_clients;

    void HandleConnection();
    void HandleReadyRead(QLocalSocket* socket);
    void HandleRequest(QLocalSocket* socket, const QJsonObject& request);
    void Send(QLocalSocket* socket, const QJsonObject& message);
    void Broadcast(const QJsonObject& message);
};
