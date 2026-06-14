/////////////////////////////////////////////////////////
// File: OverlayNotifier.cpp
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements API used by lymalinkd to signal
//              the overlay process.
/////////////////////////////////////////////////////////

#include "OverlayNotifier.h"
#include "Defines.h"
#include "tools/Logger.h"
#include "tools/Utils.h"

#include <algorithm>
#include <chrono>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <memory>
#include <poll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define COMPONENT "OverlayNotifier"

namespace fs = std::filesystem;

OverlayNotifier::OverlayNotifier() :
    m_socketThread(),
    m_socketMutex(),
    m_socketStateMutex(),
    m_socketWakeCv(),
    m_socketServers(),
    m_socketClients()
{
    m_shmFd = -1;
    m_shm = nullptr;
    m_socketRunning.store(false);
    m_socketPaused.store(false);
}

OverlayNotifier::~OverlayNotifier()
{
    Shutdown();
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool OverlayNotifier::Init()
{
    const bool sharedMemoryReady = CreateSharedMemory();
    const bool socketReady = StartSocketServer();
    return sharedMemoryReady && socketReady;
}

/////////////////////////////////////////////////////////////////////

void OverlayNotifier::Shutdown()
{
    StopSocketServer();
    DestroySharedMemory();
}

/////////////////////////////////////////////////////////////////////

void OverlayNotifier::SetSocketPaused(bool paused)
{
    m_socketPaused.store(paused);
    m_socketWakeCv.notify_one();

    if (paused)
    {
        LOG_BE(Urgency::Debug, "Pausing Flatpak socket transport, closing active sockets.");
        CloseAllSocketEndpoints();
    }
}

/////////////////////////////////////////////////////////////////////

bool OverlayNotifier::ShowAchievementToast(const AchievementNotification& notification)
{
    // Send via transport, succeed if at least one succeeds.
    const bool sharedMemoryWritten = WriteNotification(notification);
    const bool socketWritten = BroadcastSocketNotification(notification);
    return sharedMemoryWritten || socketWritten;
    return socketWritten;
}

/////////////////////////////////////////////////////////////////////

bool OverlayNotifier::ShowAchievementToastSharedMemory(const AchievementNotification& notification)
{
    return WriteNotification(notification);
}

/////////////////////////////////////////////////////////////////////

bool OverlayNotifier::ShowAchievementToastSocket(const AchievementNotification& notification)
{
    return BroadcastSocketNotification(notification);
}

/////////////////////////////////////////////////////////////////////

bool OverlayNotifier::HasSocketClient()
{
    std::lock_guard<std::mutex> lock(m_socketMutex);
    return !m_socketClients.empty();
}

/////////////////////////////////////////////////////////////////////

void OverlayNotifier::ClearSharedMemoryNotification()
{
    std::lock_guard<std::mutex> lock(m_shmMutex);
    if (!m_shm)
    {
        return;
    }

    m_shm->active.store(false);
    m_shm->timestamp = 0;
    m_shm->durationMs = 0;
    m_shm->notificationPosition = static_cast<uint32_t>(OverlayNotificationPosition::BottomRight);
    m_shm->hasIconPixels = 0;
    std::memset(m_shm->title, 0, sizeof(m_shm->title));
    std::memset(m_shm->description, 0, sizeof(m_shm->description));
    std::memset(m_shm->iconPath, 0, sizeof(m_shm->iconPath));
    std::memset(m_shm->appIconPath, 0, sizeof(m_shm->appIconPath));
    std::memset(m_shm->iconPixels, 0, sizeof(m_shm->iconPixels));

    LOG_BE(Urgency::Debug, "Shared memory notification cleared.");
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool OverlayNotifier::WriteNotification(const AchievementNotification& notification, uint32_t durationMs)
{
    constexpr uint64_t ACTIVE_TIMEOUT_MS = 10000;
    std::lock_guard<std::mutex> lock(m_shmMutex);

    if (!m_shm)
    {
        LOG_BE(Urgency::Critical, "Shared memory not initialised.");
        return false;
    }

    if (!m_shm->daemonActive.load())
    {
        LOG_BE(Urgency::Warning, "Daemon shared memory marked inactive.");
        return false;
    }

    const uint64_t nowMs = Utils::NowMs();

    // Skip if overlay is already displaying a notification, unless stale for too long
    if (m_shm->active.load())
    {
        const uint64_t activeAgeMs = nowMs > m_shm->timestamp ? (nowMs - m_shm->timestamp) : 0;
        if (activeAgeMs >= ACTIVE_TIMEOUT_MS)
        {
            LOG_BE(Urgency::Debug, "Overlay active flag stale, forcing reset.");
            m_shm->active.store(false);
        }
        else
        {
            LOG_BE(Urgency::Debug, "Overlay busy, skipping write.");
            return false;
        }
    }

    LOG_BE(Urgency::Debug, "Writing to SHM.");

    // Load and scale directly to overlay icon target size
    int kIconSize = 64;
    int kIconStride = kIconSize * 4; // 256 bytes per row
    m_shm->hasIconPixels = 0;
    // Load primary icon, fallback to secondary
    const char* candidatePaths[] = { notification.iconPath.c_str(), notification.appIconPath.c_str() };
    for (const char* path : candidatePaths)
    {
        if (path && path[0] != '\0')
        {
            GError* error = nullptr;
            GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file_at_scale(path, kIconSize, kIconSize, TRUE, &error);       
            if (pixbuf)
            {
                GdkPixbuf* rgba = gdk_pixbuf_get_has_alpha(pixbuf) ? pixbuf : gdk_pixbuf_add_alpha(pixbuf, FALSE, 0, 0, 0);
                if (rgba != pixbuf) g_object_unref(pixbuf);
                
                if (rgba)
                {
                    const guchar* src = gdk_pixbuf_get_pixels(rgba);
                    const int stride = gdk_pixbuf_get_rowstride(rgba);

                    for (int i = 0; i < kIconSize; ++i)
                    {
                        std::memcpy(m_shm->iconPixels + (i * kIconStride), src + (i * stride), kIconStride);
                    }
                    g_object_unref(rgba);
                    m_shm->hasIconPixels = 1;
                    LOG_BE(Urgency::Debug, "Embedded icon '%s' into SHM.", path);

                    break;
                }
            }
            else if (error)
            {
                g_error_free(error);
            }
        }
    }
    
    // Write data fields before setting active so the reader never sees a partially written state
    m_shm->version = OVERLAY_SHM_VERSION;
    m_shm->timestamp = nowMs;
    m_shm->durationMs = durationMs;
    m_shm->notificationPosition = static_cast<uint32_t>(ResolveNotificationPosition());

    std::strncpy(m_shm->title, notification.achievementName.c_str(), sizeof(m_shm->title) - 1);
    std::strncpy(m_shm->description, notification.achievementDescription.c_str(), sizeof(m_shm->description) - 1);
    std::strncpy(m_shm->iconPath, notification.iconPath.c_str(), sizeof(m_shm->iconPath) - 1);
    std::strncpy(m_shm->appIconPath, notification.appIconPath.c_str(), sizeof(m_shm->appIconPath) - 1);

    // Null-terminate in case the source string filled the buffer exactly
    m_shm->title[sizeof(m_shm->title) - 1] = '\0';
    m_shm->description[sizeof(m_shm->description) - 1] = '\0';
    m_shm->iconPath[sizeof(m_shm->iconPath) - 1] = '\0';
    m_shm->appIconPath[sizeof(m_shm->appIconPath) - 1] = '\0';

    // Release store ensures overlay sees all prior writes before active = true
    m_shm->active.store(true);

    LOG_BE(Urgency::Debug, "Written: %s", notification.achievementName.c_str());
    return true;
}

/////////////////////////////////////////////////////////////////////

bool OverlayNotifier::CreateSharedMemory()
{
    // O_EXCL fails if the segment already exists (e.g. from a previous crash)
    // Unlink first to guarantee a clean segment at startup.
    shm_unlink(OVERLAY_SHM_NAME);

    m_shmFd = shm_open(OVERLAY_SHM_NAME, O_CREAT | O_RDWR, 0600);
    if (m_shmFd == -1)
    {
        LOG_BE(Urgency::Critical, "shm_open failed: %s", strerror(errno));
        return false;
    }

    if (ftruncate(m_shmFd, sizeof(OverlaySharedMemoryState)) == -1)
    {
        LOG_BE(Urgency::Critical, "ftruncate failed: %s", strerror(errno));
        close(m_shmFd);
        m_shmFd = -1;
        return false;
    }

    m_shm = static_cast<OverlaySharedMemoryState*>(mmap(nullptr, sizeof(OverlaySharedMemoryState), PROT_READ | PROT_WRITE, MAP_SHARED, m_shmFd, 0));
    if (m_shm == MAP_FAILED)
    {
        LOG_BE(Urgency::Critical, "mmap failed: %s", strerror(errno));
        m_shm = nullptr;
        close(m_shmFd);
        m_shmFd = -1;
        return false;
    }

    // Construct the OverlaySharedMemoryState structure in-place within the mapped region
    std::construct_at(m_shm);
    m_shm->daemonActive.store(true);

    LOG_BE(Urgency::Debug, "Shared memory ready.");
    return true;
}

/////////////////////////////////////////////////////////////////////

void OverlayNotifier::DestroySharedMemory()
{
    // Signal overlay and daemon inactive, then unmap
    if (m_shm && m_shm != MAP_FAILED)
    {
        m_shm->active.store(false);
        m_shm->daemonActive.store(false);
        munmap(m_shm, sizeof(OverlaySharedMemoryState));
        m_shm = nullptr;
    }

    // Close the file descriptor
    if (m_shmFd != -1)
    {
        close(m_shmFd);
        m_shmFd = -1;
    }

    // Unlink so the segment disappears on clean daemon exit
    shm_unlink(OVERLAY_SHM_NAME);

    LOG_BE(Urgency::Debug, "Shared memory released.");
}

/////////////////////////////////////////////////////////////////////

bool OverlayNotifier::StartSocketServer()
{
    if (m_socketRunning.load())
    {
        return true;
    }

    m_socketRunning.store(true);
    m_socketPaused.store(false);
    m_socketThread = std::thread(&OverlayNotifier::SocketThread, this);
    
    LOG_BE(Urgency::Debug, "Flatpak socket server started.");
    return true;
}

/////////////////////////////////////////////////////////////////////

void OverlayNotifier::StopSocketServer()
{
    if (!m_socketRunning.exchange(false))
    {
        return;
    }
    m_socketWakeCv.notify_one();

    // Wait for the socket thread to finish
    if (m_socketThread.joinable())
    {
        m_socketThread.join();
    }

    CloseAllSocketEndpoints();
    
    LOG_BE(Urgency::Debug, "Flatpak socket server stopped.");
}

/////////////////////////////////////////////////////////////////////

void OverlayNotifier::SocketThread()
{
    while (m_socketRunning.load())
    {
        {
            std::unique_lock<std::mutex> stateLock(m_socketStateMutex);
            m_socketWakeCv.wait(stateLock, [this]() {
                return !m_socketRunning.load() || !m_socketPaused.load();
            });
        }

        if (!m_socketRunning.load())
        {
            break;
        }

        // Pick up new Flatpak runtimes that may have started
        RefreshSocketServers();

        // Build pollfd list for all servers and clients.
        std::vector<pollfd> pollFds;
        {
            std::lock_guard<std::mutex> lock(m_socketMutex);
            pollFds.reserve(m_socketServers.size() + m_socketClients.size());
            for (const SocketServer& server : m_socketServers)
            {
                pollFds.push_back({server.fd, POLLIN, 0});
            }
            for (int clientFd : m_socketClients)
            {
                pollFds.push_back({clientFd, POLLIN | POLLHUP | POLLERR, 0});
            }
        }

        // No sockets to poll, sleep briefly to avoid busy-waiting
        if (pollFds.empty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        const int pollResult = poll(pollFds.data(), pollFds.size(), 500);
        if (pollResult <= 0)
        {
            continue;
        }

        // Return to prevent accidentally reopening closed sockets
        if (m_socketPaused.load())
        {
            continue;
        }

        std::lock_guard<std::mutex> lock(m_socketMutex);
        const size_t serverCount = m_socketServers.size();
        for (size_t i = 0; i < pollFds.size(); ++i)
        {
            if (pollFds[i].revents == 0)
            {
                continue;
            }

            // Handle new client connections on server sockets
            if (i < serverCount)
            {
                while (true)
                {
                    const int clientFd = accept4(pollFds[i].fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
                    if (clientFd != -1)
                    {
                        m_socketClients.push_back(clientFd);
                        LOG_BE(Urgency::Debug, "Overlay client connected.");
                        continue;
                    }

                    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                    {
                        break;
                    }

                    LOG_BE(Urgency::Critical, "accept failed: %s", std::strerror(errno));
                    break;
                }
            }
            else
            {
                // Check if a client has disconnected
                const int clientFd = pollFds[i].fd;
                char discard = 0;
                const ssize_t bytes = recv(clientFd, &discard, sizeof(discard), MSG_PEEK | MSG_DONTWAIT);
                if (bytes == 0 || (bytes == -1 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
                {
                    close(clientFd);
                    m_socketClients.erase(std::remove(m_socketClients.begin(), m_socketClients.end(), clientFd), m_socketClients.end());
                    LOG_BE(Urgency::Debug, "Overlay client disconnected.");
                }
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////

void OverlayNotifier::CloseAllSocketEndpoints()
{
    std::lock_guard<std::mutex> lock(m_socketMutex);

    for (SocketServer& server : m_socketServers)
    {
        CloseSocketServer(server);
    }
    m_socketServers.clear();

    // Close remaining client file descriptors
    for (int clientFd : m_socketClients)
    {
        close(clientFd);
    }
    m_socketClients.clear();
}

/////////////////////////////////////////////////////////////////////

void OverlayNotifier::RefreshSocketServers()
{
    const fs::path flatpakRuntimeDir = fs::path(ResolveRuntimeDir()) / "app";
    const std::unordered_set<std::string> activeAppIds = ResolveActiveFlatpakAppIds();

    std::lock_guard<std::mutex> lock(m_socketMutex);

    // Clean up stale socket servers
    for (auto it = m_socketServers.begin(); it != m_socketServers.end();)
    {
        const bool runtimeDirExists = fs::exists(fs::path(it->path).parent_path());
        const bool appIsActive = activeAppIds.contains(it->appId);
        // If either the runtime directory is gone or the app isn't active anymore, close it
        if (!runtimeDirExists || !appIsActive)
        {
            LOG_BE(Urgency::Debug, "Flatpak target inactive or runtime directory removed, closing socket server for appId: %s", it->appId.c_str());
            CloseSocketServer(*it);
            it = m_socketServers.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Exit if there are no active Flatpaks to process
    if (activeAppIds.empty())
    {
        static bool s_loggedNoFlatpaks = false;
        if (!s_loggedNoFlatpaks)
        {
            LOG_BE(Urgency::Debug, "No active Flatpak applications reported by flatpak ps.");
            s_loggedNoFlatpaks = true;
        }
        return;
    }

    if (m_socketPaused.load())
    {
        return;
    }

    // Look for new Flatpaks and spin up socket servers for them - requires: active app-id and visible per-app runtime dir
    for (const std::string& appId : activeAppIds)
    {
        const fs::path appRuntimeDir = flatpakRuntimeDir / appId;
        if (!fs::exists(appRuntimeDir))
        {
            continue;
        }

        // Check if we are already managing a socket for this appId
        const auto existing = std::find_if(m_socketServers.begin(), m_socketServers.end(), [&appId](const SocketServer& server) {
            return server.appId == appId;
        });

        // For new active target, create a new socket server
        if (existing == m_socketServers.end())
        {
            LOG_BE(Urgency::Debug, "New active Flatpak target detected, attempting to bind socket for appId: %s", appId.c_str());
            BindSocketForApp(appId);

            // TODO: Remove
            LOG_BE(Urgency::Debug, "Currently bound Flatpak targets (%zu):", m_socketServers.size());
            for (const auto& server : m_socketServers) {
                LOG_BE(Urgency::Debug, "  - %s", server.appId.c_str());
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////

std::unordered_set<std::string> OverlayNotifier::ResolveActiveFlatpakAppIds() const
{
    std::unordered_set<std::string> appIds;

    // One app-id per line; duplicates can occur when an app has multiple running sandboxes
    FILE* pipe = popen("flatpak ps --columns=application 2>/dev/null", "r");
    if (!pipe)
    {
        // Log the failure only once to prevent log spamming
        static bool s_loggedFlatpakPsFailed = false;
        if (!s_loggedFlatpakPsFailed)
        {
            LOG_BE(Urgency::Debug, "Failed to run flatpak ps for active Flatpak discovery.");
            s_loggedFlatpakPsFailed = true;
        }
        return appIds;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe))
    {
        const std::string appId = Utils::TrimWhitespace(buffer);

        if (appId.empty())
        {
            continue;
        }

        appIds.insert(appId);
    }

    // Close the pipe and get the command's exit status
    const int exitCode = pclose(pipe);
    if (exitCode != 0)
    {
        // Log the bad exit status once and clear results to ensure data validity
        static bool s_loggedFlatpakPsExit = false;
        if (!s_loggedFlatpakPsExit)
        {
            LOG_BE(Urgency::Debug, "flatpak ps exited with status %d during active Flatpak discovery.", exitCode);
            s_loggedFlatpakPsExit = true;
        }
        appIds.clear();
    }

    return appIds;
}

/////////////////////////////////////////////////////////////////////

bool OverlayNotifier::BindSocketForApp(const std::string& appId)
{
    const fs::path appRuntimeDir = fs::path(ResolveRuntimeDir()) / "app" / appId;
    if (!fs::exists(appRuntimeDir))
    {
        return false;
    }

    const std::string socketPath = (appRuntimeDir / OVERLAY_SOCKET_FILENAME).string();
    // Reject paths that would overflow the sockaddr_un structure
    if (socketPath.size() >= sizeof(sockaddr_un::sun_path))
    {
        LOG_BE(Urgency::Critical, "Socket path too long: %s", socketPath.c_str());
        return false;
    }

    // Prefer non-blocking socket; fall back to setfd if kernel doesn't support flags in socket()
    int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd == -1 && errno == EINVAL)
    {
        fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
        if (fd != -1)
        {
            fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
            fcntl(fd, F_SETFD, FD_CLOEXEC);
        }
    }

    if (fd == -1)
    {
        LOG_BE(Urgency::Critical, "Socket creation failed: %s", std::strerror(errno));
        return false;
    }

    // Remove stale socket file if it exists
    unlink(socketPath.c_str());

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);
    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
    {
        LOG_BE(Urgency::Critical, "Bind failed for %s: %s", socketPath.c_str(), std::strerror(errno));
        close(fd);
        return false;
    }

    // Backlog of 8 connections
    if (listen(fd, 8) == -1)
    {
        LOG_BE(Urgency::Critical, "Listen failed for %s: %s", socketPath.c_str(), std::strerror(errno));
        close(fd);
        unlink(socketPath.c_str());
        return false;
    }

    m_socketServers.push_back(SocketServer{fd, appId, socketPath});
    LOG_BE(Urgency::Debug, "Listening: %s", socketPath.c_str());
    return true;
}

/////////////////////////////////////////////////////////////////////

void OverlayNotifier::CloseSocketServer(SocketServer& server)
{
    // Close and invalidate the server file descriptor
    if (server.fd != -1)
    {
        close(server.fd);
        server.fd = -1;
    }
    // Remove the associated socket file from the filesystem
    if (!server.path.empty())
    {
        unlink(server.path.c_str());
    }
}

/////////////////////////////////////////////////////////////////////

bool OverlayNotifier::BroadcastSocketNotification(const AchievementNotification& notification, uint32_t durationMs)
{
    const OverlaySocketPacket packet = BuildSocketPacket(notification, durationMs);

    std::lock_guard<std::mutex> lock(m_socketMutex);
    bool sentToAnyClient = false;
    for (auto it = m_socketClients.begin(); it != m_socketClients.end();)
    {
        const ssize_t bytes = send(*it, &packet, sizeof(packet), MSG_DONTWAIT | MSG_NOSIGNAL);
        // Successfully sent, move to the next client
        if (bytes == static_cast<ssize_t>(sizeof(packet)))
        {
            sentToAnyClient = true;
            ++it;
            continue;
        }

        // Would block, client buffer full, leave it for now
        if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
        {
            ++it;
            continue;
        }

        // Send failed, remove the dead connection
        close(*it);
        it = m_socketClients.erase(it);
    }

    if (sentToAnyClient)
    {
        LOG_BE(Urgency::Debug, "Sent: %s", notification.achievementName.c_str());
    }
    return sentToAnyClient;
}

/////////////////////////////////////////////////////////////////////

OverlaySocketPacket OverlayNotifier::BuildSocketPacket(const AchievementNotification& notification, uint32_t durationMs) const
{
    OverlaySocketPacket packet{};
    // Populate the socket packet header
    packet.version = OVERLAY_SOCKET_VERSION;
    packet.timestamp = Utils::NowMs();
    packet.durationMs = durationMs;
    packet.notificationPosition = static_cast<uint32_t>(ResolveNotificationPosition());

    // Copy string fields into fixed-size buffers
    std::strncpy(packet.title, notification.achievementName.c_str(), sizeof(packet.title) - 1);
    std::strncpy(packet.description, notification.achievementDescription.c_str(), sizeof(packet.description) - 1);
    std::strncpy(packet.iconPath, notification.iconPath.c_str(), sizeof(packet.iconPath) - 1);
    std::strncpy(packet.appIconPath, notification.appIconPath.c_str(), sizeof(packet.appIconPath) - 1);

    // Ensure null-termination
    packet.title[sizeof(packet.title) - 1] = '\0';
    packet.description[sizeof(packet.description) - 1] = '\0';
    packet.iconPath[sizeof(packet.iconPath) - 1] = '\0';
    packet.appIconPath[sizeof(packet.appIconPath) - 1] = '\0';

    // Embed 64x64 RGBA icon bytes for socket transport.
    // Prefer achievement icon, fall back to app icon if missing/unloadable.
    if (!EmbedIconIntoPacket(packet, notification.iconPath))
    {
        EmbedIconIntoPacket(packet, notification.appIconPath);
        LOG_BE(Urgency::Info, "Fallback to appIconPath used: %s", notification.appIconPath.c_str());
    }
    else
    {
        LOG_BE(Urgency::Debug, "Primary iconPath successfully embedded: %s", notification.iconPath.c_str());
    }

    return packet;
}

/////////////////////////////////////////////////////////////////////

bool OverlayNotifier::EmbedIconIntoPacket(OverlaySocketPacket& packet, const std::string& iconPath) const
{
    if (iconPath.empty())
    {
        return false;
    }

    // Load and scale directly to overlay icon target size
    GError* error = nullptr;
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file_at_scale(iconPath.c_str(), static_cast<int>(OVERLAY_ICON_SIZE), static_cast<int>(OVERLAY_ICON_SIZE), TRUE, &error);
    if (!pixbuf)
    {
        if (error)
        {
            LOG_BE(Urgency::Warning, "Failed to load icon from '%s': %s", iconPath.c_str(), error->message);
            g_error_free(error);
        }
        return false;
    }

    // Ensure RGBA output (add alpha channel if source is RGB)
    GdkPixbuf* rgba = gdk_pixbuf_get_has_alpha(pixbuf) ? pixbuf : gdk_pixbuf_add_alpha(pixbuf, FALSE, 0, 0, 0);
    if (rgba != pixbuf)
    {
        // If we created a new pixbuf above, drop original RGB one
        g_object_unref(pixbuf);
    }
    if (!rgba)
    {
        LOG_BE(Urgency::Critical, "Failed to ensure alpha channel for icon: %s", iconPath.c_str());
        return false;
    }

    const int w = gdk_pixbuf_get_width(rgba);
    const int h = gdk_pixbuf_get_height(rgba);
    const int rs = gdk_pixbuf_get_rowstride(rgba);
    const int channels = gdk_pixbuf_get_n_channels(rgba);
    // Guard against unexpected pixbuf layouts
    if (w <= 0 || h <= 0 || channels != 4)
    {
        LOG_BE(Urgency::Warning, "Unexpected pixbuf geometry/channels for icon '%s' (%dx%d, channels: %d)", iconPath.c_str(), w, h, channels);
        g_object_unref(rgba);
        return false;
    }

    // Copy row-by-row into fixed packet storage (clamp to packet bounds)
    const guchar* src = gdk_pixbuf_get_pixels(rgba);
    const int copyRows = std::min(h, static_cast<int>(OVERLAY_ICON_SIZE));
    const int copyCols = std::min(w, static_cast<int>(OVERLAY_ICON_SIZE)) * 4;
    for (int row = 0; row < copyRows; ++row)
    {
        std::memcpy(packet.iconPixels + static_cast<size_t>(row) * OVERLAY_ICON_STRIDE, src + static_cast<size_t>(row) * static_cast<size_t>(rs), static_cast<size_t>(copyCols));
    }

    g_object_unref(rgba);
    packet.hasIconPixels = 1;

    LOG_BE(Urgency::Debug, "Successfully embedded %dx%d icon pixels into packet.", w, h);
    return true;
}

/////////////////////////////////////////////////////////////////////

OverlayNotificationPosition OverlayNotifier::ResolveNotificationPosition() const
{
    // Check for user set override
    if (const char* value = std::getenv("LYMALINK_OVERLAY_NOTIFICATION_POSITION"))
    {
        if (*value != '\0')
        {
            OverlayNotificationPosition position = ParseNotificationPosition(value);
            LOG_BE(Urgency::Info, "Overlay notification position overridden via environment variable: %s", value);
            return position;
        }
    }

    // Check overlay notification position
    std::string configuredPosition = "";
    const std::string configPath = ResolveConfigPath();
    if (!configPath.empty())
    {
        configuredPosition = Utils::ReadIniValue(configPath, GROUP_BACKGROUND_SERVICE, "OverlayNotificationPosition");
    }

    if (!configuredPosition.empty())
    {
        OverlayNotificationPosition position = ParseNotificationPosition(configuredPosition);
        LOG_BE(Urgency::Debug, "Overlay notification position loaded from config: %s", configuredPosition.c_str());
        return position;
    }

    LOG_BE(Urgency::Debug, "No overlay notification position configured. Using default: BottomRight");
    return OverlayNotificationPosition::BottomRight;
}

/////////////////////////////////////////////////////////////////////

OverlayNotificationPosition OverlayNotifier::ParseNotificationPosition(const std::string& value) const
{
    std::string normalized = value;
    // Case-insensitive compare
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    // Accept separators by stripping them before matching
    normalized.erase(std::remove(normalized.begin(), normalized.end(), '_'), normalized.end());
    normalized.erase(std::remove(normalized.begin(), normalized.end(), '-'), normalized.end());
    normalized.erase(std::remove(normalized.begin(), normalized.end(), ' '), normalized.end());

    if (normalized == "topleft")
    {
        return OverlayNotificationPosition::TopLeft;
    }
    if (normalized == "topcenter")
    {
        return OverlayNotificationPosition::TopCenter;
    }
    if (normalized == "topright")
    {
        return OverlayNotificationPosition::TopRight;
    }
    if (normalized == "bottomleft")
    {
        return OverlayNotificationPosition::BottomLeft;
    }
    if (normalized == "bottomcenter")
    {
        return OverlayNotificationPosition::BottomCenter;
    }

    if (normalized != "bottomright")
    {
        LOG_BE(Urgency::Warning, "Unknown notification position value '%s'. Falling back to BottomRight.", value.c_str());
    }

    return OverlayNotificationPosition::BottomRight;
}

/////////////////////////////////////////////////////////////////////

std::string OverlayNotifier::ResolveConfigPath() const
{
    std::filesystem::path configHome;
    // Preferred config root from XDG spec
    if (const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME"))
    {
        if (*xdgConfigHome != '\0')
        {
            configHome = xdgConfigHome;
            LOG_BE(Urgency::Debug, "XDG_CONFIG_HOME detected: %s", xdgConfigHome);
        }
    }

    if (configHome.empty())
    {
        // Fallback for setups without XDG_CONFIG_HOME
        const char* home = std::getenv("HOME");
        if (!home || *home == '\0')
        {
            LOG_BE(Urgency::Critical, "HOME environment variable not set or empty. Cannot resolve config path.");
            return {};
        }
        configHome = std::filesystem::path(home) / ".config";
    }

    std::string configPath = (configHome / ORGANIZATION / (std::string(APPLICATION) + ".ini")).string();
    return configPath;
}

/////////////////////////////////////////////////////////////////////

std::string OverlayNotifier::ResolveRuntimeDir() const
{
    // Use the XDG runtime directory if available
    if (const char* runtimeDir = std::getenv("XDG_RUNTIME_DIR"))
    {
        if (runtimeDir[0] != '\0')
        {
            // LOG_BE(Urgency::Debug, "XDG_RUNTIME_DIR detected: %s", runtimeDir);
            return runtimeDir;
        }
    }

    // Fall back to a heuristic based on the current UID
    std::string fallbackDir = "/run/user/" + std::to_string(getuid());
    LOG_BE(Urgency::Debug, "XDG_RUNTIME_DIR not available, using fallback: %s", fallbackDir.c_str());
    return fallbackDir;
}
