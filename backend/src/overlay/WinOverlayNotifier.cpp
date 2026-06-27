/////////////////////////////////////////////////////////
// File: WinOverlayNotifier.cpp
// Date: 2026-06-21
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements Windows shared-memory producer
//              for injected overlay processes.
/////////////////////////////////////////////////////////

#include "WinOverlayNotifier.h"

#include "Defines.h"
#include "tools/Logger.h"
#include "tools/Utils.h"

#define NOMINMAX
#include <windows.h>
#include <wincodec.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#define COMPONENT "WinOverlayNotifier"

/////////////////////////////////////////////////////////////////////

WinOverlayNotifier::WinOverlayNotifier() :
    m_mutex{}
{
    m_initialised = false;
    m_activeTimeoutMs = 10000;
}

WinOverlayNotifier::~WinOverlayNotifier()
{
    Shutdown();
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool WinOverlayNotifier::Init()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Enable Vulkan layer loading while daemon owns notification transport
    m_initialised = true;
    SetVulkanLayersEnabled(true);
    return true;
}

/////////////////////////////////////////////////////////////////////

void WinOverlayNotifier::Shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialised)
    {
        return;
    }

    // Signal every injected overlay before removing its shared-memory mapping
    for (auto& [targetId, mapping] : m_mappings)
    {
        (void)targetId;
        CloseMapping(mapping);
    }
    m_mappings.clear();
    SetVulkanLayersEnabled(false);
    m_initialised = false;
}

/////////////////////////////////////////////////////////////////////

bool WinOverlayNotifier::RegisterProcess(int targetId, uint32_t pid)
{
    if (targetId <= 0 || pid == 0)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialised)
    {
        return false;
    }

    // Replace old mapping if target process restarted with a new PID
    auto existing = m_mappings.find(targetId);
    if (existing != m_mappings.end())
    {
        CloseMapping(existing->second);
        m_mappings.erase(existing);
    }

    // Mapping name includes PID, preventing cross-process notification delivery
    const std::wstring name = WinOverlaySharedMemoryName(pid);
    HANDLE handle = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(sizeof(WinOverlaySharedMemoryState)), name.c_str());
    if (!handle || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (handle)
        {
            CloseHandle(handle);
        }

        LOG_BE(Urgency::Warning, "Could not create unique overlay SHM for targetId=%d pid=%u.", targetId, pid);
        return false;
    }

    auto* state = static_cast<WinOverlaySharedMemoryState*>(MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(WinOverlaySharedMemoryState)));
    if (!state)
    {
        CloseHandle(handle);
        LOG_BE(Urgency::Warning, "MapViewOfFile failed for targetId=%d pid=%u.", targetId, pid);
        return false;
    }

    // Initialise ABI fields before exposing mapping to injected overlay process
    std::memset(state, 0, sizeof(*state));
    state->version = WIN_OVERLAY_SHM_VERSION;
    state->structSize = sizeof(*state);
    InterlockedExchange(&state->daemonActive, 1);
    m_mappings.emplace(targetId, Mapping{pid, handle, state});
    LOG_BE(Urgency::Debug, "Overlay SHM ready for targetId=%d pid=%u.", targetId, pid);
    return true;
}

/////////////////////////////////////////////////////////////////////

void WinOverlayNotifier::UnregisterProcess(int targetId)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Target exit may race registration; missing mapping is already clean
    const auto it = m_mappings.find(targetId);
    if (it == m_mappings.end())
    {
        return;
    }
    CloseMapping(it->second);
    m_mappings.erase(it);
}

/////////////////////////////////////////////////////////////////////

void WinOverlayNotifier::ClearSharedMemoryNotification()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Clear only active flags; next writer replaces complete payload
    for (auto& [targetId, mapping] : m_mappings)
    {
        (void)targetId;
        if (mapping.state)
        {
            InterlockedExchange(&mapping.state->active, 0);
        }
    }
}

/////////////////////////////////////////////////////////////////////

bool WinOverlayNotifier::ShowAchievementToast(const AchievementNotification& notification)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_mappings.find(notification.targetId);
    if (it == m_mappings.end() || !it->second.state)
    {
        LOG_BE(Urgency::Debug, "Overlay target not mapped, skipping notification: targetId=%d.", notification.targetId);
        return false;
    }

    WinOverlaySharedMemoryState& state = *it->second.state;
    const uint64_t nowMs = Utils::NowMs();

    // Do not overwrite an active notification unless its state is stale
    if (InterlockedCompareExchange(&state.active, 0, 0) != 0)
    {
        const uint64_t ageMs = nowMs > state.timestamp ? nowMs - state.timestamp : 0;
        if (ageMs < m_activeTimeoutMs)
        {
            LOG_BE(Urgency::Debug, "Overlay busy, skipping notification: targetId=%d.", notification.targetId);
            return false;
        }
        InterlockedExchange(&state.active, 0);
    }

    // Reset fixed buffers so shorter next payload cannot retain stale bytes
    std::memset(state.title, 0, sizeof(state.title));
    std::memset(state.description, 0, sizeof(state.description));
    std::memset(state.iconPath, 0, sizeof(state.iconPath));
    std::memset(state.appIconPath, 0, sizeof(state.appIconPath));
    std::memset(state.iconPixels, 0, sizeof(state.iconPixels));
    CopyText(state.title, sizeof(state.title), notification.achievementName);
    CopyText(state.description, sizeof(state.description), notification.achievementDescription);
    CopyText(state.iconPath, sizeof(state.iconPath), notification.iconPath);
    CopyText(state.appIconPath, sizeof(state.appIconPath), notification.appIconPath);
    state.timestamp = nowMs;
    state.durationMs = 6000;
    state.notificationPosition = static_cast<uint32_t>(ResolveNotificationPosition());
    state.hasIconPixels = LoadIconPixels(notification, state.iconPixels) ? 1U : 0U;

    // Publish only after all payload bytes are visible to the injected reader
    MemoryBarrier();
    InterlockedExchange(&state.active, 1);
    return true;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

OverlayNotificationPosition WinOverlayNotifier::ResolveNotificationPosition()
{
    const std::string configPath = ResolveConfigPath();
    if (configPath.empty())
    {
        return OverlayNotificationPosition::BottomRight;
    }

    // Keep overlay placement in daemon config so injected renderers stay stateless
    const std::string value = Utils::ReadIniValue(configPath, GROUP_BACKGROUND_SERVICE, "OverlayNotificationPosition");
    if (!value.empty())
    {
        return ParseNotificationPosition(value);
    }

    return OverlayNotificationPosition::BottomRight;
}

/////////////////////////////////////////////////////////////////////

OverlayNotificationPosition WinOverlayNotifier::ParseNotificationPosition(const std::string& value)
{
    // Accept common user/config spellings such as "top-left", "top_left", or "top left"
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
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

std::string WinOverlayNotifier::ResolveConfigPath()
{
    const char* appData = std::getenv("APPDATA");
    if (!appData || *appData == '\0')
    {
        return {};
    }

    return (std::filesystem::path(appData) / ORGANIZATION / (std::string(APPLICATION) + ".ini")).string();
}

/////////////////////////////////////////////////////////////////////

template <typename T>
void WinOverlayNotifier::ReleaseCom(T*& value)
{
    // Release each partially constructed WIC object during cleanup
    if (value)
    {
        value->Release();
        value = nullptr;
    }
}

/////////////////////////////////////////////////////////////////////

bool WinOverlayNotifier::LoadIconFile(const std::filesystem::path& path, uint8_t* destination)
{
    if (path.empty() || !std::filesystem::exists(path))
    {
        return false;
    }

    // CoInitializeEx may return S_FALSE; balance every successful call
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = SUCCEEDED(comResult);
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapScaler* scaler = nullptr;
    IWICFormatConverter* converter = nullptr;
    bool loaded = false;

    // Decode, scale, and convert source image to overlay's RGBA pixel layout
    // Each step is guarded so failure can fall through to shared COM cleanup
    HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (SUCCEEDED(result))
    {
        result = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    }
    if (SUCCEEDED(result))
    {
        result = decoder->GetFrame(0, &frame);
    }
    if (SUCCEEDED(result))
    {
        result = factory->CreateBitmapScaler(&scaler);
    }
    if (SUCCEEDED(result))
    {
        result = scaler->Initialize(frame, OVERLAY_ICON_SIZE, OVERLAY_ICON_SIZE, WICBitmapInterpolationModeFant);
    }
    if (SUCCEEDED(result))
    {
        result = factory->CreateFormatConverter(&converter);
    }
    if (SUCCEEDED(result))
    {
        result = converter->Initialize(scaler, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    }
    if (SUCCEEDED(result))
    {
        result = converter->CopyPixels(nullptr, OVERLAY_ICON_STRIDE, OVERLAY_ICON_DATA_SIZE, destination);
        loaded = SUCCEEDED(result);
    }

    ReleaseCom(converter);
    ReleaseCom(scaler);
    ReleaseCom(frame);
    ReleaseCom(decoder);
    ReleaseCom(factory);
    if (uninitialize)
    {
        CoUninitialize();
    }
    return loaded;
}

/////////////////////////////////////////////////////////////////////

void WinOverlayNotifier::SetVulkanLayersEnabled(bool enabled)
{
    // Register or disable architecture-matched Vulkan implicit-layer manifests
    // Vulkan loader treats registry DWORD 0 as enabled and non-zero as disabled
    const DWORD registryValue = enabled ? 0 : 1;
    const char* action = enabled ? "enabled" : "disabled";

    std::vector<std::filesystem::path> overlayDirs;
    const auto addOverlayDir = [&overlayDirs](std::filesystem::path path) {
        path = path.lexically_normal();
        if (std::find(overlayDirs.begin(), overlayDirs.end(), path) == overlayDirs.end())
        {
            overlayDirs.push_back(std::move(path));
        }
    };

    wchar_t executablePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
    {
        // Portable/build tree layout: overlay manifests live next to daemon executable
        addOverlayDir(std::filesystem::path(executablePath).parent_path() / "overlay");
    }

    char localAppData[MAX_PATH]{};
    const DWORD localAppDataLength = GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH);
    if (localAppDataLength > 0 && localAppDataLength < MAX_PATH)
    {
        // Installer layout: manifests are deployed under the per-user Programs folder
        addOverlayDir(std::filesystem::path(localAppData) / "Programs" / "Lymalink" / "overlay");
    }

    const std::array<std::pair<const wchar_t*, REGSAM>, 2> registrations{{
        {L"x64", KEY_WOW64_64KEY},
        {L"x86", KEY_WOW64_32KEY},
    }};

    int updated = 0;
    for (const std::filesystem::path& overlayDir : overlayDirs)
    {
        for (const auto& registration : registrations)
        {
            // Write architecture-specific manifest in corresponding registry view
            const std::filesystem::path manifest = overlayDir / (std::wstring(L"lymalink-overlay-vulkan-") + registration.first + L".json");
            if (!std::filesystem::exists(manifest))
            {
                LOG_BE(Urgency::Debug, "Vulkan overlay manifest not found: %ls", manifest.c_str());
                continue;
            }

            HKEY key = nullptr;
            const LSTATUS openStatus = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Khronos\\Vulkan\\ImplicitLayers", 0, nullptr, 0, KEY_SET_VALUE | registration.second, nullptr, &key, nullptr);
            if (openStatus != ERROR_SUCCESS)
            {
                LOG_BE(Urgency::Warning, "Could not open Vulkan implicit-layer registry key for %ls manifest: status=%ld.", registration.first, static_cast<long>(openStatus));
                continue;
            }

            const LSTATUS setStatus = RegSetValueExW(key, manifest.c_str(), 0, REG_DWORD, reinterpret_cast<const BYTE*>(&registryValue), sizeof(registryValue));
            RegCloseKey(key);
            if (setStatus != ERROR_SUCCESS)
            {
                LOG_BE(Urgency::Warning, "Could not set Vulkan implicit-layer registry value for %ls manifest: status=%ld path=%ls.", registration.first, static_cast<long>(setStatus), manifest.c_str());
                continue;
            }

            ++updated;
            LOG_BE(Urgency::Debug, "Vulkan overlay layer %s for %ls: %ls.", action, registration.first, manifest.c_str());
        }
    }

    if (updated == 0)
    {
        LOG_BE(Urgency::Warning, "No Vulkan overlay manifests were %s.", action);
    }
}

/////////////////////////////////////////////////////////////////////

bool WinOverlayNotifier::LoadIconPixels(const AchievementNotification& notification, uint8_t* destination)
{
    // Prefer achievement icon; app icon provides fallback when it is unavailable
    return LoadIconFile(std::filesystem::u8path(notification.iconPath), destination) || LoadIconFile(std::filesystem::u8path(notification.appIconPath), destination);
}

/////////////////////////////////////////////////////////////////////

void WinOverlayNotifier::CopyText(char* destination, size_t destinationSize, const std::string& source)
{
    if (destinationSize == 0)
    {
        return;
    }

    // Reserve final byte for null terminator when source exceeds destination
    const size_t copied = (std::min)(destinationSize - 1, source.size());
    std::memcpy(destination, source.data(), copied);
    destination[copied] = '\0';
}

/////////////////////////////////////////////////////////////////////

void WinOverlayNotifier::CloseMapping(Mapping& mapping)
{
    // Mark mapping inactive before releasing resources visible to injected process
    if (mapping.state)
    {
        // Readers poll these atomics; clear them before unmapping to avoid stale toasts
        InterlockedExchange(&mapping.state->daemonActive, 0);
        InterlockedExchange(&mapping.state->active, 0);
        UnmapViewOfFile(mapping.state);
        mapping.state = nullptr;
    }
    if (mapping.handle)
    {
        CloseHandle(mapping.handle);
        mapping.handle = nullptr;
    }
}
