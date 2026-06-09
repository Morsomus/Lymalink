/////////////////////////////////////////////////////////
// File: OverlayReceiver.cpp
// Date: 2026-05-26
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements in-process receiver API and
//              overlay notification renderer for Vulkan
//              and OpenGL targets.
/////////////////////////////////////////////////////////

#include "OverlayReceiver.h"
#include "Logger.h"
#include "FontEmbedded.h"
#include "tools/Utils.h"

#include <fstream>
// POSIX shared memory
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
// Timing
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <vector>
#include <cmath>
// ImGui, backend headers are included conditionally
#ifdef LYMALINK_OVERLAY_OPENGL_TEXTURES
#include <GL/gl.h>
#endif
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "imgui.h"
#ifndef LYMALINK_OVERLAY_OPENGL_TEXTURES
#include "imgui_impl_vulkan.h"
#endif

/////////////////////////////////////////////////////////////////////

OverlayReceiver::OverlayReceiver()
{
    // Check if we should use Socket to communicate with lymalinkd OverlayNotifier - Otherwise use Shared Memory
    m_socketPath = SocketDetectFlatpakPath();
    m_useSocket = !m_socketPath.empty();
}

OverlayReceiver::~OverlayReceiver()
{
    Shutdown();
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool OverlayReceiver::InitConnection()
{
    // Try connections on init, failing is not fatal
    if (m_useSocket)
    {
        return SocketConnect();
    }
    else
    {
        return SharedMemoryOpen();
    }
}

/////////////////////////////////////////////////////////////////////

void OverlayReceiver::Shutdown()
{
    DestroyOpenGLIconTexture();
    DestroyVulkanIconTexture();
    SocketClose();
    SharedMemoryClose();

    // Only destroy a context that this service created. Some hook paths create
    // the context before handing control here so renderer backends can attach.
    if (m_imguiReady && m_ownsImguiContext)
    {
        ImGui::DestroyContext();
    }

    m_imguiReady = false;
    m_ownsImguiContext = false;
    m_vulkanReady = false;
    m_openGLReady = false;
}

/////////////////////////////////////////////////////////////////////

void OverlayReceiver::InvalidateVulkanResources()
{
    DestroyVulkanIconTexture();
    m_vkDevice = VK_NULL_HANDLE;
    m_vkPhysicalDevice = VK_NULL_HANDLE;
    m_vkQueue = VK_NULL_HANDLE;
    m_vkCommandPool = VK_NULL_HANDLE;
    m_vkGetPhysicalDeviceMemoryProperties = nullptr;
    m_vulkanReady = false;
}

/////////////////////////////////////////////////////////////////////

void OverlayReceiver::RenderNotificationFrame(uint32_t fbWidth, uint32_t fbHeight)
{
    m_fbWidth = fbWidth;
    m_fbHeight = fbHeight;

    if (m_useSocket)    // Using Socket method
    {
        // Maintain socket connection and process incoming data stream
        if (m_socketFd == -1)
        {
            SocketConnect();
        }
        SocketDrain();
        SocketClaimPendingNotification();
    }
    else                // Using Shared Memory method
    {
        // Establish shared memory mapping if not already connected
        if (!m_shm)
        {
            SharedMemoryOpen();
            return;
        }

        // Handle daemon shutdown
        if (!m_shm->daemonActive.load())
        {
            m_currentActiveNotification = ActiveNotification{};
            m_fadingOut = false;
            m_alpha = 0.0f;
            m_slideOffset = 0.0f;
            SharedMemoryClose();
            return;
        }

        SharedMemoryClaimPendingNotification();
    }

    // Early exit if no active notification
    if (!m_currentActiveNotification.visible && !m_fadingOut)
    {
        return;
    }

    const uint64_t now = Utils::NowMs();
    const uint64_t elapsed = now - m_currentActiveNotification.shownAtMs;

    // Trigger exit animation if the display duration threshold has been reached
    if (!m_fadingOut && elapsed >= m_currentActiveNotification.durationMs)
    {
        m_fadingOut = true;
    }

    // Compute delta for smooth animation regardless of frame rate
    static uint64_t s_lastFrameMs = Utils::NowMs();
    const float delta = static_cast<float>(now - s_lastFrameMs) / 1000.0f;
    s_lastFrameMs = now;

    // Fade/slide animation calculations
    UpdateNotificationAnimation(delta);

    if (m_imguiReady)
    {
        DrawNotificationWindow();
    }
}

/////////////////////////////////////////////////////////////////////


void OverlayReceiver::EnsureVulkanImGuiContext(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue graphicsQueue, uint32_t graphicsQueueFamily, VkCommandPool commandPool, PFN_vkGetPhysicalDeviceMemoryProperties getMemProps)
{
    // Cache Vulkan execution context handles and device interfaces
    m_vkDevice = device;
    m_vkPhysicalDevice = physicalDevice;
    m_vkQueue = graphicsQueue;
    m_vkCommandPool = commandPool;
    m_vkGetPhysicalDeviceMemoryProperties = getMemProps;
    (void)graphicsQueueFamily;

    if (!m_imguiReady)
    {
        // Setup ImGui context
        if (!ImGui::GetCurrentContext())
        {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            m_ownsImguiContext = true;
            OverlayFonts::EnsureEmbeddedFontLoaded();
        }
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(m_fbWidth), static_cast<float>(m_fbHeight));
        io.IniFilename = nullptr;
        // Style applied once here - shared between Vulkan and OpenGL paths
        ImGui::StyleColorsDark();
        m_imguiReady = true;
    }

    m_vulkanReady = true;
}

/////////////////////////////////////////////////////////////////////

void OverlayReceiver::EnsureOpenGLImGuiContext()
{
    if (!m_imguiReady)
    {
        // Setup ImGui context
        if (!ImGui::GetCurrentContext())
        {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            m_ownsImguiContext = true;
            OverlayFonts::EnsureEmbeddedFontLoaded();
        }
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(m_fbWidth), static_cast<float>(m_fbHeight));
        io.IniFilename = nullptr;
        ImGui::StyleColorsDark();
        m_imguiReady = true;
    }

    m_openGLReady = true;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool OverlayReceiver::SharedMemoryOpen()
{
    static bool s_loggedMissing = false;

    // Attempt to open the shared memory block with read/write permissions
    m_shmFd = shm_open(OVERLAY_SHM_NAME, O_RDWR, 0600);
    if (m_shmFd == -1)
    {
        // Daemon not running yet, silent failure, retry next frame
        if (!s_loggedMissing)
        {
            LYMALINK_LOG("[OverlayReceiver][SharedMemoryOpen] shm_open failed for " + std::string(OVERLAY_SHM_NAME) + ": " + std::strerror(errno));
            s_loggedMissing = true;
        }
        return false;
    }

    // Map the shared memory into the process's address space
    m_shm = static_cast<OverlaySharedMemoryState*>(mmap(nullptr, sizeof(OverlaySharedMemoryState), PROT_READ | PROT_WRITE, MAP_SHARED, m_shmFd, 0));
    if (m_shm == MAP_FAILED)
    {
        LYMALINK_LOG("[OverlayReceiver][SharedMemoryOpen] mmap failed: " + std::string(std::strerror(errno)));
        m_shm = nullptr;
        close(m_shmFd);
        m_shmFd = -1;
        return false;
    }

    // Ensure API compatibility between the daemon and receiver
    if (m_shm->version != OVERLAY_SHM_VERSION)
    {
        // Version mismatch between daemon and overlay, unmap and bail
        LYMALINK_LOG("[OverlayReceiver][SharedMemoryOpen] version mismatch got=" + std::to_string(m_shm->version) + " expected=" + std::to_string(OVERLAY_SHM_VERSION));
        SharedMemoryClose();
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////

void OverlayReceiver::SharedMemoryClose()
{
    // Safely unmap memory if it was successfully mapped
    if (m_shm && m_shm != MAP_FAILED)
    {
        if (munmap(m_shm, sizeof(OverlaySharedMemoryState)) == -1)
        {
            LYMALINK_LOG("[OverlayReceiver][SharedMemoryClose] munmap failed: " + std::string(std::strerror(errno)));
        }
        m_shm = nullptr;
    }

    // Close the file descriptor if it is still open
    if (m_shmFd != -1)
    {
        if (close(m_shmFd) == -1)
        {
            LYMALINK_LOG("[OverlayReceiver][SharedMemoryClose] close failed: " + std::string(std::strerror(errno)));
        }
        m_shmFd = -1;
    }
}

/////////////////////////////////////////////////////////////////////

bool OverlayReceiver::SharedMemoryClaimPendingNotification()
{
    if (!m_shm)
    {
        return false;
    }

    // Already showing, don't overwrite until current one finishes
    if (m_currentActiveNotification.visible)
    {
        return false;
    }

    bool expected = true;
    // Atomically claim the notification so other overlays (multi-GPU etc.) don't double-show
    if (!m_shm->active.compare_exchange_strong(expected, false))
    {
        return false;
    }

    m_currentActiveNotification.visible = true;
    m_currentActiveNotification.shownAtMs = Utils::NowMs();
    m_currentActiveNotification.durationMs = m_shm->durationMs > 0 ? m_shm->durationMs : 4000;
    m_currentActiveNotification.title = m_shm->title;
    m_currentActiveNotification.description = m_shm->description;
    m_currentActiveNotification.iconPath = m_shm->iconPath;
    m_currentActiveNotification.appIconPath = m_shm->appIconPath;
    m_currentActiveNotification.position = static_cast<OverlayNotificationPosition>(m_shm->notificationPosition);

    if (m_shm->hasIconPixels == 1)
    {
        m_currentActiveNotification.iconPixels.assign(m_shm->iconPixels, m_shm->iconPixels + OVERLAY_ICON_DATA_SIZE);
    }

    // Reset animation state for new notification
    m_alpha = 0.0f;
    m_slideOffset = 40.0f;
    m_fadingOut = false;
    m_iconAlpha = 0.0f;
    m_iconAnimProgress = 0.0f;

    return true;
}

/////////////////////////////////////////////////////////////////////

bool OverlayReceiver::SocketConnect()
{
    // Check if a socket already exists
    if (m_socketFd != -1)
    {
        return !m_socketConnecting || SocketIsConnected();
    }

    // Enforce a 1-second rate limit between connection attempts
    const uint64_t now = Utils::NowMs();
    if (now < m_nextSocketConnectAttemptMs)
    {
        return false;
    }
    m_nextSocketConnectAttemptMs = now + 1000;

    // Validate that the Unix domain socket path fits in the buffer
    if (m_socketPath.size() >= sizeof(sockaddr_un::sun_path))
    {
        static bool s_loggedPathTooLong = false;
        if (!s_loggedPathTooLong)
        {
            LYMALINK_LOG("[OverlayReceiver][SocketConnect] socket path too long: " + m_socketPath);
            s_loggedPathTooLong = true;
        }
        return false;
    }

    // Create a non-blocking Unix domain socket (with fallback for older kernels)
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

    // Handle socket creation failure
    if (fd == -1)
    {
        if (!m_loggedSocketFailed)
        {
            LYMALINK_LOG("[OverlayReceiver][SocketConnect] socket failed: " + std::string(std::strerror(errno)));
            m_loggedSocketFailed = true;
        }
        return false;
    }

    // Attempt to connect
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, m_socketPath.c_str(), sizeof(addr.sun_path) - 1);
    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0)
    {
        m_socketFd = fd;
        m_socketConnecting = false;
        LYMALINK_LOG("[OverlayReceiver][SocketConnect] connected.");
        return true;
    }

    // Handle non-blocking connection still in progress
    if (errno == EINPROGRESS || errno == EAGAIN || errno == EALREADY)
    {
        m_socketFd = fd;
        m_socketConnecting = true;
        return false;
    }

    close(fd);
    return false;
}

/////////////////////////////////////////////////////////////////////

void OverlayReceiver::SocketClose()
{
    // Close the socket descriptor and reset state
    if (m_socketFd != -1)
    {
        close(m_socketFd);
        m_socketFd = -1;
    }
    m_socketConnecting = false;
}

/////////////////////////////////////////////////////////////////////

bool OverlayReceiver::SocketIsConnected()
{
    if (m_socketFd == -1)
    {
        return false;
    }

    // Check the pending error status of the non-blocking socket
    int error = 0;
    socklen_t errorSize = sizeof(error);
    if (getsockopt(m_socketFd, SOL_SOCKET, SO_ERROR, &error, &errorSize) == -1)
    {
        SocketClose();
        return false;
    }

    // No error -> the connection attempt was successful
    if (error == 0)
    {
        if (m_socketConnecting)
        {
            LYMALINK_LOG("[OverlayReceiver][SocketIsConnected] connected.");
        }
        m_socketConnecting = false;
        return true;
    }

    // False if the asynchronous connection is still in progress
    if (error == EINPROGRESS || error == EALREADY || error == EAGAIN)
    {
        return false;
    }

    // Close the socket and fail if any other error occurred
    SocketClose();
    return false;
}

/////////////////////////////////////////////////////////////////////

void OverlayReceiver::SocketDrain()
{
    if (m_socketFd == -1)
    {
        return;
    }

    if (m_socketConnecting && !SocketIsConnected())
    {
        return;
    }

    // Read available packets from the incoming buffer
    while (true)
    {
        // Handle read packet
        OverlaySocketPacket packet{};
        const ssize_t bytes = recv(m_socketFd, &packet, sizeof(packet), MSG_DONTWAIT);
        if (bytes == static_cast<ssize_t>(sizeof(packet)))
        {
            if (packet.version == OVERLAY_SOCKET_VERSION)
            {
                m_socketPending = SocketPacketToNotification(packet);
                m_hasSocketPending = true;
                LYMALINK_LOG("[OverlayReceiver][SocketDrain] packet received: " + m_socketPending.title);
            }
            continue;
        }

        // Handle system interrupt - retry the read
        if (bytes == -1 && errno == EINTR)
        {
            continue;
        }

        // Handle empty buffer
        if (bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            break;
        }

        // Handle partial/malformed packets
        if (bytes > 0)
        {
            LYMALINK_LOG("[OverlayReceiver][SocketDrain] invalid packet size: " + std::to_string(bytes));
            continue;
        }

        // Handle socket disconnect or unrecoverable error (bytes == 0 or other errno)
        SocketClose();
        break;
    }
}

/////////////////////////////////////////////////////////////////////

bool OverlayReceiver::SocketClaimPendingNotification()
{
    if (!m_hasSocketPending || m_currentActiveNotification.visible || m_fadingOut)
    {
        return false;
    }

    // Promote the pending notification to active and timestamp its start time
    m_currentActiveNotification = m_socketPending;
    m_currentActiveNotification.visible = true;
    m_currentActiveNotification.shownAtMs = Utils::NowMs();
    m_hasSocketPending = false;

    // Reset animation state variables
    m_alpha = 0.0f;
    m_slideOffset = 40.0f;
    m_fadingOut = false;
    m_iconAlpha = 0.0f;
    m_iconAnimProgress = 0.0f;
    LYMALINK_LOG("[OverlayReceiver][SocketClaimPendingNotification] claimed: " + m_currentActiveNotification.title);
    return true;
}

/////////////////////////////////////////////////////////////////////

std::string OverlayReceiver::SocketDetectFlatpakPath() const
{
    // Try to open the Flatpak environment metadata file
    std::ifstream flatpakInfo("/.flatpak-info");
    if (!flatpakInfo.is_open())
    {
        return {};
    }

    // Parse the file line-by-line to find the application ID (e.g., "name=com.usebottles.bottles")
    std::string appId;
    std::string line;
    while (std::getline(flatpakInfo, line))
    {
        constexpr const char* NAME_PREFIX = "name=";
        if (line.rfind(NAME_PREFIX, 0) == 0)
        {
            appId = line.substr(std::strlen(NAME_PREFIX));
            break;
        }
    }

    if (appId.empty())
    {
        return {};
    }

    // Check if this specific Flatpak process/launcher should be ignored/blocked
    if (SocketIsBlockedFlatpakLauncherProcess(appId))
    {
        LYMALINK_LOG("[OverlayReceiver] Flatpak process ignored for app-id: " + appId);
        return {};
    }

    // Determine the base runtime directory (prefer XDG_RUNTIME_DIR, fallback to default local path)
    const char* runtimeDir = std::getenv("XDG_RUNTIME_DIR");
    std::string basePath;
    if (runtimeDir && runtimeDir[0] != '\0')
    {
        basePath = runtimeDir;
    }
    else
    {
        basePath = "/run/user/" + std::to_string(getuid());
    }

    // Construct full non-isolated path to the Flatpak application's IPC socket
    const std::string socketPath = basePath + "/app/" + appId + "/" + OVERLAY_SOCKET_FILENAME;

    return socketPath;
}

/////////////////////////////////////////////////////////////////////

bool OverlayReceiver::SocketIsBlockedFlatpakLauncherProcess(const std::string& appId) const
{
    if (appId != "com.usebottles.bottles")
    {
        return false;
    }

    // Check the short process name via /proc/self/comm
    const std::string processName = Utils::ReadProcessComm();
    if (processName == "bottles" || processName == "bottles-cli")
    {
        return true;
    }

    // Inspect the full command line arguments
    const std::string cmdline = Utils::ReadProcessCmdline();
    if (cmdline.find("/app/bin/bottles") != std::string::npos)
    {
        return true;
    }

    const bool cmdlineIncludesPython = cmdline.find("python") != std::string::npos;
    const bool cmdlineIncludesBottles = cmdline.find("bottles") != std::string::npos;

    return cmdlineIncludesPython && cmdlineIncludesBottles;
}

/////////////////////////////////////////////////////////////////////

OverlayReceiver::ActiveNotification OverlayReceiver::SocketPacketToNotification(const OverlaySocketPacket& packet) const
{
    // Parse raw packet data into a populated UI notification object
    ActiveNotification notification;
    notification.visible = true;
    notification.shownAtMs = Utils::NowMs();
    notification.durationMs = packet.durationMs > 0 ? packet.durationMs : 4000;
    notification.title = packet.title;
    notification.description = packet.description;
    notification.iconPath = packet.iconPath;
    notification.appIconPath = packet.appIconPath;
    notification.position = static_cast<OverlayNotificationPosition>(packet.notificationPosition);
    
    if (packet.hasIconPixels)
    {
        notification.iconPixels.assign(packet.iconPixels, packet.iconPixels + OVERLAY_ICON_DATA_SIZE);
    }

    return notification;
}

/////////////////////////////////////////////////////////////////////

void OverlayReceiver::DrawNotificationWindow()
{
    // LYMALINK_LOG("alpha=" + std::to_string(m_alpha) + " openGLReady=" + std::to_string(m_openGLReady ? 1 : 0) + " vulkanReady=" + std::to_string(m_vulkanReady ? 1 : 0));

    // Layout boundaries for the notification window
    constexpr float MARGIN = 40.0f;
    constexpr float NOTIF_WIDTH  = 480.0f;
    constexpr float NOTIF_HEIGHT = 100.0f;
    constexpr float ICON_SIZE = 84.0f;
    constexpr float ICON_GAP = 10.0f;
    constexpr float PAD_X = 16.0f;

    // Calculate baseline safe zones for screen placement
    const float fbWidth = static_cast<float>(m_fbWidth);
    const float fbHeight = static_cast<float>(m_fbHeight);
    float x = fbWidth - NOTIF_WIDTH - MARGIN;
    float y = fbHeight - NOTIF_HEIGHT - MARGIN;
    float slideX = m_slideOffset;
    float slideY = 0.0f;

    // Determine viewport coordinates and slide vectors based on placement anchor
    switch (m_currentActiveNotification.position)
    {
        case OverlayNotificationPosition::TopLeft:
            x = MARGIN;
            y = MARGIN;
            slideX = -m_slideOffset;
            break;
        case OverlayNotificationPosition::TopCenter:
            x = (fbWidth - NOTIF_WIDTH) * 0.5f;
            y = MARGIN;
            slideX = 0.0f;
            slideY = -m_slideOffset;
            break;
        case OverlayNotificationPosition::TopRight:
            x = fbWidth - NOTIF_WIDTH - MARGIN;
            y = MARGIN;
            slideX = m_slideOffset;
            break;
        case OverlayNotificationPosition::BottomLeft:
            x = MARGIN;
            y = fbHeight - NOTIF_HEIGHT - MARGIN;
            slideX = -m_slideOffset;
            break;
        case OverlayNotificationPosition::BottomCenter:
            x = (fbWidth - NOTIF_WIDTH) * 0.5f;
            y = fbHeight - NOTIF_HEIGHT - MARGIN;
            slideX = 0.0f;
            slideY = m_slideOffset;
            break;
        case OverlayNotificationPosition::BottomRight:
        default:
            x = fbWidth - NOTIF_WIDTH - MARGIN;
            y = fbHeight - NOTIF_HEIGHT - MARGIN;
            slideX = m_slideOffset;
            break;
    }

    // Set layout dimensions and sync window transparency with animation alpha
    ImGui::SetNextWindowPos(ImVec2(x + slideX, y + slideY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(NOTIF_WIDTH, NOTIF_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.82f * m_alpha);

    // Disable all window interactions, decorations, and focus-stealing
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    // Apply aesthetics and bind text transparency to animation alpha
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, m_alpha));

    // Render the notification content container
    if (ImGui::Begin("##lymalink_overlay", nullptr, flags))
    {
        // Resolve texture backing via active graphics API
        const bool hasIconTexture = m_openGLReady
            ? EnsureOpenGLIconTexture(m_currentActiveNotification.iconPath)
            : EnsureVulkanIconTexture(m_currentActiveNotification.iconPath);            
        const ImVec2 iconCursor = ImGui::GetCursorScreenPos();
        const ImVec2 targetSize = ImVec2(ICON_SIZE, ICON_SIZE);

        // Render graphics API native texture descriptor if valid
        if (hasIconTexture)
        {
#if UINTPTR_MAX == UINT64_MAX
            const ImTextureID vulkanTexId = static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(m_vkIconDescSet));
#else
            const ImTextureID vulkanTexId = static_cast<ImTextureID>(m_vkIconDescSet);
#endif
            const ImTextureID texId = m_openGLReady
                ? static_cast<ImTextureID>(static_cast<uintptr_t>(m_iconTextureId))
                : vulkanTexId;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, m_iconAlpha));
            ImGui::Image(texId, targetSize, ImVec2(0,0), ImVec2(1,1));
            ImGui::PopStyleColor();
        }
        else
        {
            // Placeholder: blue rounded rectangle
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(iconCursor, ImVec2(iconCursor.x + ICON_SIZE, iconCursor.y + ICON_SIZE), ImGui::GetColorU32(ImVec4(0.18f, 0.54f, 0.72f, 0.75f * m_alpha)), 6.0f);
            drawList->AddRect(iconCursor, ImVec2(iconCursor.x + ICON_SIZE, iconCursor.y + ICON_SIZE), ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.18f * m_alpha)), 6.0f);
            ImGui::Dummy(ImVec2(ICON_SIZE, ICON_SIZE));
        }

        // Arrange elements horizontally alongside the icon slot
        ImGui::SameLine(0.0f, ICON_GAP);
        ImGui::BeginGroup();

        // Render the main notification header, elide right
        float text_max_width = NOTIF_WIDTH - ICON_SIZE - ICON_GAP - PAD_X;
        std::string title_display = ImElideRight(m_currentActiveNotification.title.c_str(), text_max_width);
        ImGui::TextUnformatted(title_display.c_str());
        ImGui::Spacing();

        // Render the secondary body text with a slightly muted color
        // Wrap + max 3 rows + elide right
        ImGui::PushTextWrapPos(NOTIF_WIDTH - PAD_X); // Use same max width than the main notification header
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, m_alpha));
        std::string desc_display = ImLimitLines(m_currentActiveNotification.description.c_str(), NOTIF_WIDTH - ICON_SIZE - ICON_GAP - PAD_X, 3);
        ImGui::TextUnformatted(desc_display.c_str());
        ImGui::PopStyleColor();
        // Pops the text wrapping width pushed earlier
        // Essential to restore the default layout flow: without it, all subsequent widgets would be unintentionally constrained to this narrow width, breaking the UI structure
        ImGui::PopTextWrapPos();

        ImGui::EndGroup();
    }
    ImGui::End();

    // Clean up pushed style mutations
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

/////////////////////////////////////////////////////////////////////

void OverlayReceiver::UpdateNotificationAnimation(float delta)
{
    constexpr float FADE_SPEED = 4.0f;      // alpha units per second
    constexpr float SLIDE_SPEED = 200.0f;   // pixels per second

    delta = std::min(delta, 0.1f); 

    if (!m_fadingOut)
    {
        // Animate entrance: fade in and slide to the target position
        m_alpha = std::min(1.0f, m_alpha + delta * FADE_SPEED);
        m_slideOffset = std::max(0.0f, m_slideOffset - delta * SLIDE_SPEED);
        if (m_iconAnimationDuration > 0.0f)
        {
            m_iconAnimProgress = std::min(1.0f, m_iconAnimProgress + delta / m_iconAnimationDuration);
        }
        else
        {
            m_iconAnimProgress = 1.0f;
        }

        // EaseOutCubic: fast start, slow end
        float t = m_iconAnimProgress;
        float ease = 1.0f - std::pow(1.0f - t, 3.0f);
        m_iconAlpha = ease;
    }
    else
    {
        // Animate exit: fade out and slide away
        m_alpha = std::max(0.0f, m_alpha - delta * FADE_SPEED);
        m_slideOffset = std::min(40.0f, m_slideOffset + delta * SLIDE_SPEED);

        // Image animation exit
        m_iconAlpha = std::max(0.0f, m_alpha);
        m_iconAnimProgress = 0.0f;

        if (m_alpha <= 0.0f)
        {
            // Animation complete, clear notification
            m_currentActiveNotification.visible = false;
            m_fadingOut = false;
            m_iconAlpha = 0.0f;
            m_slideOffset = 40.0f;
        }
    }
}

/////////////////////////////////////////////////////////////////////

bool OverlayReceiver::EnsureVulkanIconTexture(const std::string& iconPath)
{
#ifndef LYMALINK_OVERLAY_OPENGL_TEXTURES
    // Validate state and device handles
    if (iconPath.empty() || !m_vulkanReady || m_vkDevice == VK_NULL_HANDLE
        || m_vkPhysicalDevice == VK_NULL_HANDLE || m_vkQueue == VK_NULL_HANDLE
        || m_vkCommandPool == VK_NULL_HANDLE)
    {
        LYMALINK_LOG("[OverlayReceiver][EnsureVulkanIconTexture] invalid Vulkan state, skipping icon upload.");
        DestroyVulkanIconTexture();
        return false;
    }

    if (m_vkIconDescSet != VK_NULL_HANDLE && m_loadedIconPath == iconPath)
    {
        return true;
    }

    // Reset current texture states
    DestroyVulkanIconTexture();

    // Pixels: from socket packet or from file fallback
    std::vector<uint8_t> pixels;
    int w = 0, h = 0;

    if (!m_currentActiveNotification.iconPixels.empty())
    {
        // Use RGBA pixels embedded in the socket packet
        pixels = m_currentActiveNotification.iconPixels;
        w = static_cast<int>(OVERLAY_ICON_SIZE);
        h = static_cast<int>(OVERLAY_ICON_SIZE);
    }
    else
    {
        // Fallback: load from icon file path (works outside sandbox)
        GError* error = nullptr;
        GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file_at_scale(
            iconPath.c_str(), 64, 64, TRUE, &error);
        if (!pixbuf)
        {
            if (error)
            {
                LYMALINK_LOG("[OverlayReceiver][EnsureVulkanIconTexture] failed to load icon: " + iconPath + ": " + error->message);
                g_error_free(error);
            }
            return false;
        }

        // Ensure image format has alpha channel
        GdkPixbuf* rgba = gdk_pixbuf_get_has_alpha(pixbuf) ? pixbuf : gdk_pixbuf_add_alpha(pixbuf, FALSE, 0, 0, 0);
        if (rgba != pixbuf)
        {
            g_object_unref(pixbuf);
        }
        if (!rgba)
        {
            return false;
        }

        w  = gdk_pixbuf_get_width(rgba);
        h  = gdk_pixbuf_get_height(rgba);
        const int rs = gdk_pixbuf_get_rowstride(rgba);

        // Validate image dimensions and channels
        if (w <= 0 || h <= 0 || gdk_pixbuf_get_n_channels(rgba) != 4)
        {
            g_object_unref(rgba);
            return false;
        }

        // Copy source pixels to contiguous pixel buffer
        pixels.resize(static_cast<size_t>(w * h * 4));
        const guchar* src = gdk_pixbuf_get_pixels(rgba);
        for (int row = 0; row < h; ++row)
        {
            std::memcpy(pixels.data() + static_cast<size_t>(row * w * 4), src + static_cast<size_t>(row * rs), static_cast<size_t>(w * 4));
        }
        g_object_unref(rgba);
    }

    const size_t imgBytes = static_cast<size_t>(w * h * 4);

    // Create staging buffer allocation
    VkBufferCreateInfo bufCI{};
    bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufCI.size = imgBytes;
    bufCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer stagingBuf = VK_NULL_HANDLE;
    if (vkCreateBuffer(m_vkDevice, &bufCI, nullptr, &stagingBuf) != VK_SUCCESS)
    {
        LYMALINK_LOG("[OverlayReceiver][EnsureVulkanIconTexture] vkCreateBuffer failed.");
        return false;
    }

    // Allocate host visible memory for staging buffer
    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(m_vkDevice, stagingBuf, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = VulkanFindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    if (vkAllocateMemory(m_vkDevice, &allocInfo, nullptr, &stagingMem) != VK_SUCCESS)
    {
        LYMALINK_LOG("[OverlayReceiver][EnsureVulkanIconTexture] vkAllocateMemory (staging) failed.");
        vkDestroyBuffer(m_vkDevice, stagingBuf, nullptr);
        return false;
    }

    vkBindBufferMemory(m_vkDevice, stagingBuf, stagingMem, 0);

    // Map memory and copy pixels to staging buffer
    void* mapped = nullptr;
    vkMapMemory(m_vkDevice, stagingMem, 0, imgBytes, 0, &mapped);
    std::memcpy(mapped, pixels.data(), imgBytes);
    vkUnmapMemory(m_vkDevice, stagingMem);

    // Create target device optimal image
    VkImageCreateInfo imgCI{};
    imgCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgCI.imageType = VK_IMAGE_TYPE_2D;
    imgCI.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgCI.extent = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1 };
    imgCI.mipLevels = 1;
    imgCI.arrayLayers = 1;
    imgCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imgCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imgCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_vkDevice, &imgCI, nullptr, &m_vkIconImage) != VK_SUCCESS)
    {
        LYMALINK_LOG("[OverlayReceiver][EnsureVulkanIconTexture] vkCreateImage failed.");
        vkDestroyBuffer(m_vkDevice, stagingBuf, nullptr);
        vkFreeMemory(m_vkDevice, stagingMem, nullptr);
        return false;
    }

    // Allocate device local memory for image
    vkGetImageMemoryRequirements(m_vkDevice, m_vkIconImage, &memReq);
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = VulkanFindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_vkDevice, &allocInfo, nullptr, &m_vkIconMemory) != VK_SUCCESS)
    {
        LYMALINK_LOG("[OverlayReceiver][EnsureVulkanIconTexture] vkAllocateMemory (image) failed.");
        vkDestroyBuffer(m_vkDevice, stagingBuf, nullptr);
        vkFreeMemory(m_vkDevice, stagingMem, nullptr);
        DestroyVulkanIconTexture();
        return false;
    }

    vkBindImageMemory(m_vkDevice, m_vkIconImage, m_vkIconMemory, 0);

    // Allocate one-shot command buffer for transfer operations
    VkCommandBufferAllocateInfo cbAlloc{};
    cbAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAlloc.commandPool = m_vkCommandPool;
    cbAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAlloc.commandBufferCount = 1;

    VkCommandBuffer cb = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(m_vkDevice, &cbAlloc, &cb) != VK_SUCCESS)
    {
        LYMALINK_LOG("[OverlayReceiver][EnsureVulkanIconTexture] vkAllocateCommandBuffers failed.");
        vkDestroyBuffer(m_vkDevice, stagingBuf, nullptr);
        vkFreeMemory(m_vkDevice, stagingMem, nullptr);
        DestroyVulkanIconTexture();
        return false;
    }

    VkCommandBufferBeginInfo cbBegin{};
    cbBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &cbBegin);

    // Transition image layout from undefined to transfer destination
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_vkIconImage;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Record copy region from buffer to image
    VkBufferImageCopy region{};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageExtent = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1 };
    vkCmdCopyBufferToImage(cb, stagingBuf, m_vkIconImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition image layout from transfer destination to shader read optimal
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cb);

    // Create execution fence
    VkFenceCreateInfo fenceCI{};
    fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(m_vkDevice, &fenceCI, nullptr, &fence) != VK_SUCCESS)
    {
        LYMALINK_LOG("[OverlayReceiver][EnsureVulkanIconTexture] vkCreateFence failed.");
        vkFreeCommandBuffers(m_vkDevice, m_vkCommandPool, 1, &cb);
        vkDestroyBuffer(m_vkDevice, stagingBuf, nullptr);
        vkFreeMemory(m_vkDevice, stagingMem, nullptr);
        DestroyVulkanIconTexture();
        return false;
    }

    // Submit copy commands to hardware queue
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cb;
    const VkResult submitResult = vkQueueSubmit(m_vkQueue, 1, &submitInfo, fence);
    if (submitResult != VK_SUCCESS)
    {
        LYMALINK_LOG("[OverlayReceiver][EnsureVulkanIconTexture] vkQueueSubmit failed: " + std::to_string(submitResult));
        vkDestroyFence(m_vkDevice, fence, nullptr);
        vkFreeCommandBuffers(m_vkDevice, m_vkCommandPool, 1, &cb);
        vkDestroyBuffer(m_vkDevice, stagingBuf, nullptr);
        vkFreeMemory(m_vkDevice, stagingMem, nullptr);
        DestroyVulkanIconTexture();
        return false;
    }

    // Block CPU until GPU transfer completes or times out
    constexpr uint64_t ICON_UPLOAD_WAIT_NS = 200000000ULL; // 200 ms safety cap
    const VkResult waitResult = vkWaitForFences(m_vkDevice, 1, &fence, VK_TRUE, ICON_UPLOAD_WAIT_NS);
    if (waitResult != VK_SUCCESS)
    {
        LYMALINK_LOG("[OverlayReceiver][EnsureVulkanIconTexture] vkWaitForFences failed/timeout: " + std::to_string(waitResult));
        vkDestroyFence(m_vkDevice, fence, nullptr);
        vkFreeCommandBuffers(m_vkDevice, m_vkCommandPool, 1, &cb);
        vkDestroyBuffer(m_vkDevice, stagingBuf, nullptr);
        vkFreeMemory(m_vkDevice, stagingMem, nullptr);
        DestroyVulkanIconTexture();
        return false;
    }

    // Clean up temporary copy infrastructure resources
    vkDestroyFence(m_vkDevice, fence, nullptr);
    vkFreeCommandBuffers(m_vkDevice, m_vkCommandPool, 1, &cb);
    vkDestroyBuffer(m_vkDevice, stagingBuf, nullptr);
    vkFreeMemory(m_vkDevice, stagingMem, nullptr);

    // Create target image view object
    VkImageViewCreateInfo viewCI{};
    viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCI.image = m_vkIconImage;
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCI.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewCI.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    if (vkCreateImageView(m_vkDevice, &viewCI, nullptr, &m_vkIconImageView) != VK_SUCCESS)
    {
        LYMALINK_LOG("[OverlayReceiver][EnsureVulkanIconTexture] vkCreateImageView failed.");
        DestroyVulkanIconTexture();
        return false;
    }

    // Create target texture sampler object
    VkSamplerCreateInfo samplerCI{};
    samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.maxLod = 1.0f;
    if (vkCreateSampler(m_vkDevice, &samplerCI, nullptr, &m_vkIconSampler) != VK_SUCCESS)
    {
        LYMALINK_LOG("[OverlayReceiver][EnsureVulkanIconTexture] vkCreateSampler failed.");
        DestroyVulkanIconTexture();
        return false;
    }

    // DescriptorSet (imgui_impl_vulkan helper)
    m_vkIconDescSet = ImGui_ImplVulkan_AddTexture(m_vkIconSampler, m_vkIconImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (m_vkIconDescSet == VK_NULL_HANDLE)
    {
        LYMALINK_LOG("[OverlayReceiver][EnsureVulkanIconTexture] ImGui_ImplVulkan_AddTexture failed.");
        DestroyVulkanIconTexture();
        return false;
    }

    // Cache texture metadata
    m_loadedIconPath = iconPath;
    LYMALINK_LOG("[OverlayReceiver][EnsureVulkanIconTexture] loaded: " + iconPath);
    return true;
#else
    // Handle disabled Vulkan feature fallback
    (void)iconPath;
    return false;
#endif
}

/////////////////////////////////////////////////////////////////////

void OverlayReceiver::DestroyVulkanIconTexture()
{
#ifndef LYMALINK_OVERLAY_OPENGL_TEXTURES
    if (m_vkDevice == VK_NULL_HANDLE)
    {
        m_vkIconDescSet = VK_NULL_HANDLE;
        m_vkIconSampler = VK_NULL_HANDLE;
        m_vkIconImageView = VK_NULL_HANDLE;
        m_vkIconImage = VK_NULL_HANDLE;
        m_vkIconMemory = VK_NULL_HANDLE;
        m_loadedIconPath.clear();
        return;
    }

    // Unregister texture descriptor from ImGui
    if (m_vkIconDescSet != VK_NULL_HANDLE)
    {
        ImGui_ImplVulkan_RemoveTexture(m_vkIconDescSet);
        m_vkIconDescSet = VK_NULL_HANDLE;
    }
    // Destroy Vulkan sampler
    if (m_vkIconSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(m_vkDevice, m_vkIconSampler, nullptr);
        m_vkIconSampler = VK_NULL_HANDLE;
    }
    // Destroy Vulkan image view
    if (m_vkIconImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(m_vkDevice, m_vkIconImageView, nullptr);
        m_vkIconImageView = VK_NULL_HANDLE;
    }
    // Destroy Vulkan image object
    if (m_vkIconImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(m_vkDevice, m_vkIconImage, nullptr);
        m_vkIconImage = VK_NULL_HANDLE;
    }
    // Free GPU device memory backing the image
    if (m_vkIconMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(m_vkDevice, m_vkIconMemory, nullptr);
        m_vkIconMemory = VK_NULL_HANDLE;
    }

    m_loadedIconPath.clear();
#endif
}

/////////////////////////////////////////////////////////////////////

uint32_t OverlayReceiver::VulkanFindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const
{
#ifndef LYMALINK_OVERLAY_OPENGL_TEXTURES
    // Validate physical device and function pointer availability
    if (m_vkPhysicalDevice == VK_NULL_HANDLE || !m_vkGetPhysicalDeviceMemoryProperties)
    {
        LYMALINK_LOG("[OverlayReceiver][VulkanFindMemoryType] physical device or fn ptr null.");
        return 0;
    }

    // Query hardware memory properties from device
    VkPhysicalDeviceMemoryProperties memProps{};
    m_vkGetPhysicalDeviceMemoryProperties(m_vkPhysicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
    {
        // Check suitability based on requirements mask and property flags
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & props) == props)
        {
            return i;
        }
    }

    LYMALINK_LOG("[OverlayReceiver][VulkanFindMemoryType] no suitable memory type found.");
    return 0;
#else
    // Handle disabled Vulkan feature fallback
    (void)typeFilter;
    (void)props;
    return 0;
#endif
}


/////////////////////////////////////////////////////////////////////

bool OverlayReceiver::EnsureOpenGLIconTexture(const std::string& iconPath)
{
#ifdef LYMALINK_OVERLAY_OPENGL_TEXTURES
    // Validate path and state
    if (iconPath.empty() || !m_openGLReady)
    {
        DestroyOpenGLIconTexture();
        return false;
    }

    // Reuse existing matching texture
    if (m_iconTextureId != 0 && m_loadedIconPath == iconPath)
    {
        return true;
    }

    // Reset current texture before loading new one
    DestroyOpenGLIconTexture();

    std::vector<unsigned char> contiguous;
    int width = 0, height = 0;

    // Load raw pixel data from received notification payload if available
    if (!m_currentActiveNotification.iconPixels.empty())
    {
        contiguous.assign(m_currentActiveNotification.iconPixels.begin(), m_currentActiveNotification.iconPixels.end());
        width = static_cast<int>(OVERLAY_ICON_SIZE);
        height = static_cast<int>(OVERLAY_ICON_SIZE);
    }
    else
    {
        // Load image file from disk
        GError* error = nullptr;
        GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file_at_scale(iconPath.c_str(), 64, 64, TRUE, &error);
        if (!pixbuf)
        {
            if (error)
            {
                LYMALINK_LOG("[OverlayReceiver][EnsureOpenGLIconTexture] failed to load icon: " + iconPath + ": " + error->message);
                g_error_free(error);
            }
            return false;
        }

        // Ensure image format has alpha channel
        GdkPixbuf* rgbaPixbuf = gdk_pixbuf_get_has_alpha(pixbuf) ? pixbuf : gdk_pixbuf_add_alpha(pixbuf, FALSE, 0, 0, 0);
        if (rgbaPixbuf != pixbuf)
        {
            g_object_unref(pixbuf);
        }
        if (!rgbaPixbuf)
        {
            return false;
        }

        width = gdk_pixbuf_get_width(rgbaPixbuf);
        height = gdk_pixbuf_get_height(rgbaPixbuf);
        const int rowStride = gdk_pixbuf_get_rowstride(rgbaPixbuf);
        const int channels = gdk_pixbuf_get_n_channels(rgbaPixbuf);
        const guchar* pixels_src = gdk_pixbuf_get_pixels(rgbaPixbuf);

        // Validate image dimensions and data
        if (width <= 0 || height <= 0 || channels != 4 || !pixels_src)
        {
            g_object_unref(rgbaPixbuf);
            return false;
        }

        // Copy source pixels to contiguous pixel buffer
        contiguous.resize(static_cast<size_t>(width * height * 4));
        for (int row = 0; row < height; ++row)
        {
            std::memcpy(contiguous.data() + static_cast<size_t>(row * width * 4), pixels_src + static_cast<size_t>(row * rowStride), static_cast<size_t>(width * 4));
        }
        g_object_unref(rgbaPixbuf);
    }

    // Generate and configure OpenGL texture
    GLuint textureId = 0;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, contiguous.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    // Cache texture metadata
    m_iconTextureId = textureId;
    m_loadedIconPath = iconPath;
    return m_iconTextureId != 0;
#else
    // Handle disabled texture feature fallback
    (void)iconPath;
    return false;
#endif
}

/////////////////////////////////////////////////////////////////////

void OverlayReceiver::DestroyOpenGLIconTexture()
{
#ifdef LYMALINK_OVERLAY_OPENGL_TEXTURES
    // Delete existing texture from GPU memory
    if (m_iconTextureId != 0 && m_openGLReady)
    {
        const GLuint textureId = static_cast<GLuint>(m_iconTextureId);
        glDeleteTextures(1, &textureId);
    }
#endif
    m_iconTextureId = 0;
    m_loadedIconPath.clear();
}

/////////////////////////////////////////////////////////////////////

std::string OverlayReceiver::ImElideRight(const char* text, float max_width)
{
    if (!text || !*text)
    {
        return "";
    }

    ImFont* font = ImGui::GetFont();
    float fs = ImGui::GetFontSize();

    std::string ellipsis = "...";
    // Measure the pixel width of the ellipsis characters themselves
    float ew = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, ellipsis.c_str(), ellipsis.c_str() + 3).x;

    // Check if the full text fits within the allowed width (reserving space for ellipsis)
    ImVec2 full_sz = font->CalcTextSizeA(fs, max_width, 0.0f, text, nullptr);
    if (full_sz.x <= max_width - ew)
    {
        return text;
    }
    
    // Measure characters incrementally from the left until adding the next one exceeds the limit
    float cur_w = 0.0f;
    const char* p = text;
    const char* end = text + strlen(text);
    while (p < end)
    {
        float cw = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, p, p + 1).x;
        if (cur_w + cw > max_width - ew)
        {
            break;  // Width limit reached
        }
        cur_w += cw;
        ++p;
    }

    // Return truncated string + ellipsis
    return std::string(text, p - text).append(ellipsis);
}

/////////////////////////////////////////////////////////////////////

std::string OverlayReceiver::ImLimitLines(const char* text, float wrap_width, int max_lines)
{
    if (!text || !*text)
    {
        return "";
    }

    ImFont* font = ImGui::GetFont();
    float fs = ImGui::GetFontSize();
    float line_h = ImGui::GetTextLineHeight();
    float max_h = max_lines * line_h;

    int len = strlen(text);
    int lo = 0, hi = len;

    // Binary search: finds the longest prefix whose wrapped height fits within max_h
    while (lo < hi)
    {
        int mid = lo + (hi - lo + 1) / 2;   // Bias right to prevent infinite loops
        ImVec2 sz = font->CalcTextSizeA(fs, FLT_MAX, wrap_width, text, text + mid);
        if (sz.y <= max_h + 0.5f)
        {
            lo = mid; // Fits, try longer
        }
        else
        {
            hi = mid - 1; // Too tall, try shorter
        }
    }

    if (lo < len)
    {
        // Step back to the nearest safe word/rule break to avoid mid-word clipping
        for (int i = 0; i < 2; ++i)
        {
            --lo;
            while (lo > 0 && text[lo] != ' ' && text[lo] != '\n')
            {
                --lo;
            }
        }
        return std::string(text, lo).append("...");
    }

    // Full text fits within the height limit
    return text;
}
