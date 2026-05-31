/////////////////////////////////////////////////////////
// File: OverlayNotifier.h
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares API used by lymalinkd to signal
//              the overlay process.
/////////////////////////////////////////////////////////

#pragma once

#include "OverlaySocketProtocol.h"
#include "OverlaySharedMemoryState.h"
#include "../notification/AchievementNotificationService.h" // AchievementNotification

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class OverlayNotifier : public IDesktopNotificationService
{
public:
    OverlayNotifier();
    ~OverlayNotifier();

    // Creates and maps the shared memory segment.
    // Must be called once during daemon startup before any game launches.
    bool Init();
    void Shutdown();
    void SetSocketPaused(bool paused);

    bool ShowAchievementToast(const AchievementNotification& notification) override;

private:
    struct SocketServer
    {
        int fd = -1;
        std::string appId;
        std::string path;
    };

    int m_shmFd = -1;
    OverlaySharedMemoryState* m_shm;
    std::atomic_bool m_socketRunning;
    std::atomic_bool m_socketPaused;
    std::thread m_socketThread;
    std::mutex m_socketMutex;
    std::mutex m_socketStateMutex;
    std::condition_variable m_socketWakeCv;
    std::vector<SocketServer> m_socketServers;
    std::vector<int> m_socketClients;

    // SHM based methods - Writes notification payload to SHM
    bool WriteNotification(const AchievementNotification& notification, uint32_t durationMs = 6000);
    bool CreateSharedMemory();
    void DestroySharedMemory();

    // Socket based methods
    void SocketThread(); // Socket Thread
    bool StartSocketServer();
    void StopSocketServer();
    void RefreshSocketServers();
    bool BindSocketForApp(const std::string& appId);
    void CloseSocketServer(SocketServer& server);
    bool BroadcastSocketNotification(const AchievementNotification& notification, uint32_t durationMs = 6000);
    OverlaySocketPacket BuildSocketPacket(const AchievementNotification& notification, uint32_t durationMs) const;
    bool EmbedIconIntoPacket(OverlaySocketPacket& packet, const std::string& iconPath) const;
    OverlayNotificationPosition ResolveNotificationPosition() const;
    OverlayNotificationPosition ParseNotificationPosition(const std::string& value) const;
    std::string ResolveConfigPath() const;
    std::string ResolveRuntimeDir() const;
};
