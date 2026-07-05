/////////////////////////////////////////////////////////
// File: Direct3D9OverlayLayer.cpp
// Date: 2026-07-03
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Windows Direct3D 9 overlay hooks driven by
//              MinHook and Dear ImGui DX9 backend.
/////////////////////////////////////////////////////////

#ifndef NOMINMAX
    #define NOMINMAX
#endif

#include "FontEmbedded.h"
#include "OverlaySharedMemoryState.h"
#include "WinLogger.h"
#include "WinOverlayReceiver.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "MinHook.h"

#include <windows.h>
#include <d3d9.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace
{
// Function pointer types for D3D9 methods we intercept
using PFN_EndScene = HRESULT(WINAPI*)(IDirect3DDevice9*);
using PFN_Present = HRESULT(WINAPI*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using PFN_Reset = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using PFN_PresentEx = HRESULT(WINAPI*)(IDirect3DDevice9Ex*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD);
using PFN_ResetEx = HRESULT(WINAPI*)(IDirect3DDevice9Ex*, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*);
using PFN_Direct3DCreate9 = IDirect3D9*(WINAPI*)(UINT);
using PFN_Direct3DCreate9Ex = HRESULT(WINAPI*)(UINT, IDirect3D9Ex**);

// Known D3D9 vtable slots used by the hook setup
constexpr int VTable_Reset = 16;
constexpr int VTable_Present = 17;
constexpr int VTable_EndScene = 42;
constexpr int VTable_PresentEx = 121;
constexpr int VTable_ResetEx = 132;

// Original D3D9 methods saved by MinHook
static PFN_EndScene s_realEndScene = nullptr;
static PFN_Present s_realPresent = nullptr;
static PFN_Reset s_realReset = nullptr;
static PFN_PresentEx s_realPresentEx = nullptr;
static PFN_ResetEx s_realResetEx = nullptr;

// One receiver and one ImGui context are shared by every DX9 hook in this process
static WinOverlayReceiver s_overlay;
static std::mutex s_renderMutex;
static std::atomic_bool s_hooksReady{false};
static std::atomic_bool s_shuttingDown{false};
static std::atomic_bool s_endSceneRendered{false};
static thread_local bool s_rendering = false;   // Prevents recursive interception while ImGui renders

static IDirect3DDevice9* s_imguiDevice = nullptr;
static bool s_imguiReady = false;
static IDirect3DTexture9* s_iconTexture = nullptr;
static uint64_t s_iconGeneration = 0;

static std::atomic_bool s_loggedEndSceneHit{false};
static std::atomic_bool s_loggedPresentFallback{false};
static std::atomic_bool s_loggedDeviceLost{false};

/////////////////////////////////////////////////////////////////////

std::string HookStatus(const char* name, MH_STATUS status)
{
    // Keep MinHook logs compact and readable
    return std::string(name) + " status=" + std::to_string(static_cast<int>(status));
}

/////////////////////////////////////////////////////////////////////

void ReleaseCom(IUnknown*& value)
{
    // Release optional COM pointer and clear caller storage
    if (value)
    {
        value->Release();
        value = nullptr;
    }
}

/////////////////////////////////////////////////////////////////////

template <typename T>
void ReleaseCom(T*& value)
{
    // Typed overload avoids casts at call sites
    if (value)
    {
        value->Release();
        value = nullptr;
    }
}

/////////////////////////////////////////////////////////////////////

void ReleaseIconTexture()
{
    // Icon texture is rebuilt when shared icon pixels change
    ReleaseCom(s_iconTexture);
    s_iconGeneration = 0;
}

/////////////////////////////////////////////////////////////////////

void ShutdownImGuiLocked()
{
    // Caller holds render mutex while tearing down ImGui
    ReleaseIconTexture();
    if (s_imguiReady)
    {
        ImGui_ImplDX9_Shutdown();
        s_imguiReady = false;
    }
    if (ImGui::GetCurrentContext())
    {
        ImGui::DestroyContext();
    }
    s_imguiDevice = nullptr;
}

/////////////////////////////////////////////////////////////////////

bool InitImGuiLocked(IDirect3DDevice9* device, uint32_t width, uint32_t height)
{
    // Need a valid device before touching ImGui
    if (!device)
    {
        return false;
    }
    // Reuse existing ImGui backend when device did not change
    if (s_imguiReady && s_imguiDevice == device)
    {
        return true;
    }

    // Device changed, so rebuild ImGui from scratch
    ShutdownImGuiLocked();

    // Bind ImGui to the current D3D9 device
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    OverlayFonts::EnsureEmbeddedFontLoaded();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();

    // Start Dear ImGui DX9 renderer backend
    if (!ImGui_ImplDX9_Init(device))
    {
        LYMALINK_LOG("[Direct3D9OverlayLayer][InitImGui] ImGui_ImplDX9_Init failed.");
        ShutdownImGuiLocked();
        return false;
    }

    // Remember active device for later reset/shutdown checks
    s_imguiDevice = device;
    s_imguiReady = true;
    LYMALINK_LOG("[Direct3D9OverlayLayer][InitImGui] ready " + std::to_string(width) + "x" + std::to_string(height));
    return true;
}

/////////////////////////////////////////////////////////////////////

bool QueryFramebufferSize(IDirect3DDevice9* device, uint32_t& outWidth, uint32_t& outHeight)
{
    // Clear outputs until a valid size is found
    outWidth = 0;
    outHeight = 0;

    // Prefer viewport size; fall back to render target size
    D3DVIEWPORT9 viewport{};
    if (SUCCEEDED(device->GetViewport(&viewport)) && viewport.Width > 0 && viewport.Height > 0)
    {
        outWidth = viewport.Width;
        outHeight = viewport.Height;
        return true;
    }

    IDirect3DSurface9* target = nullptr;
    if (SUCCEEDED(device->GetRenderTarget(0, &target)) && target)
    {
        // Render target gives backbuffer size when viewport is unusable
        D3DSURFACE_DESC desc{};
        const bool ok = SUCCEEDED(target->GetDesc(&desc)) && desc.Width > 0 && desc.Height > 0;
        if (ok)
        {
            outWidth = desc.Width;
            outHeight = desc.Height;
        }
        // GetRenderTarget returns an owned reference
        target->Release();
        return ok;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////

ImTextureID EnsureDirect3D9IconTexture(IDirect3DDevice9* device, const std::vector<uint8_t>& rgbaPixels, uint64_t generation)
{
    // Reject missing device or incomplete icon buffer
    if (!device || rgbaPixels.size() != OVERLAY_ICON_DATA_SIZE)
    {
        return ImTextureID_Invalid;
    }
    // Same generation means existing texture is still current
    if (s_iconTexture && s_iconGeneration == generation)
    {
        return reinterpret_cast<ImTextureID>(s_iconTexture);
    }

    // Old texture is stale
    ReleaseIconTexture();

    // D3D9 texture expects BGRA byte order
    HRESULT result = device->CreateTexture(
        OVERLAY_ICON_SIZE,
        OVERLAY_ICON_SIZE,
        1,
        D3DUSAGE_DYNAMIC,
        D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT,
        &s_iconTexture,
        nullptr);
    if (FAILED(result) || !s_iconTexture)
    {
        LYMALINK_LOG("[Direct3D9OverlayLayer][EnsureIconTexture] CreateTexture failed hr=" + std::to_string(result));
        return ImTextureID_Invalid;
    }

    D3DLOCKED_RECT locked{};
    result = s_iconTexture->LockRect(0, &locked, nullptr, D3DLOCK_DISCARD);
    if (FAILED(result))
    {
        LYMALINK_LOG("[Direct3D9OverlayLayer][EnsureIconTexture] LockRect failed hr=" + std::to_string(result));
        ReleaseIconTexture();
        return ImTextureID_Invalid;
    }

    // Copy row by row because D3D pitch can be wider than icon stride
    for (uint32_t y = 0; y < OVERLAY_ICON_SIZE; ++y)
    {
        const uint8_t* src = rgbaPixels.data() + y * OVERLAY_ICON_STRIDE;
        auto* dst = static_cast<uint8_t*>(locked.pBits) + y * locked.Pitch;
        for (uint32_t x = 0; x < OVERLAY_ICON_SIZE; ++x)
        {
            // Convert RGBA source into D3D9 BGRA texture bytes
            dst[x * 4 + 0] = src[x * 4 + 2];
            dst[x * 4 + 1] = src[x * 4 + 1];
            dst[x * 4 + 2] = src[x * 4 + 0];
            dst[x * 4 + 3] = src[x * 4 + 3];
        }
    }

    s_iconTexture->UnlockRect(0);
    // Store generation so next frame can reuse this texture
    s_iconGeneration = generation;
    return reinterpret_cast<ImTextureID>(s_iconTexture);
}

/////////////////////////////////////////////////////////////////////

void RenderOverlay(IDirect3DDevice9* device, bool presentFallback)
{
    if (s_rendering || s_shuttingDown.load() || !device)
    {
        return;
    }

    // Present fallback draws only if EndScene did not draw this frame
    if (presentFallback)
    {
        if (s_endSceneRendered.exchange(false))
        {
            return;
        }
        if (!s_loggedPresentFallback.exchange(true))
        {
            LYMALINK_LOG("[Direct3D9OverlayLayer][RenderOverlay] using present fallback.");
        }
    }

    const HRESULT cooperative = device->TestCooperativeLevel();
    if (cooperative == D3DERR_DEVICELOST || cooperative == D3DERR_DEVICENOTRESET)
    {
        if (!s_loggedDeviceLost.exchange(true))
        {
            LYMALINK_LOG("[Direct3D9OverlayLayer][RenderOverlay] skipped: device lost.");
        }
        return;
    }
    s_loggedDeviceLost.store(false);

    // Need real framebuffer size before creating ImGui frame
    uint32_t width = 0;
    uint32_t height = 0;
    if (!QueryFramebufferSize(device, width, height) || width == 0 || height == 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(s_renderMutex);
    s_rendering = true;
    bool sceneBegun = false;

    if (presentFallback)
    {
        // Present path may be outside a scene, so open one for ImGui
        const HRESULT sceneResult = device->BeginScene();
        if (FAILED(sceneResult))
        {
            s_rendering = false;
            return;
        }
        sceneBegun = true;
    }

    if (InitImGuiLocked(device, width, height))
    {
        // Pull newest shared-memory state and render overlay widgets
        ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
        ImGui_ImplDX9_NewFrame();
        ImGui::NewFrame();
        const bool claimedNotification = s_overlay.BeginFrame();
        ImTextureID icon = EnsureDirect3D9IconTexture(device, s_overlay.IconPixels(), s_overlay.IconGeneration());
        s_overlay.Draw(width, height, icon);
        if (claimedNotification)
        {
            LYMALINK_LOG("[Direct3D9OverlayLayer][RenderOverlay] claimed notification; renderer=dx9 path=" +
                std::string(presentFallback ? "Present" : "EndScene") +
                " size=" + std::to_string(width) + "x" + std::to_string(height));
        }
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }

    if (sceneBegun && s_realEndScene)
    {
        s_realEndScene(device);
    }

    s_rendering = false;
}

/////////////////////////////////////////////////////////////////////

HRESULT WINAPI Hook_EndScene(IDirect3DDevice9* device)
{
    // Main render path for most D3D9 apps
    if (!s_loggedEndSceneHit.exchange(true))
    {
        LYMALINK_LOG("[Direct3D9OverlayLayer][Hook_EndScene] first EndScene intercepted.");
    }
    RenderOverlay(device, false);
    s_endSceneRendered.store(true);
    return s_realEndScene ? s_realEndScene(device) : D3DERR_INVALIDCALL;
}

/////////////////////////////////////////////////////////////////////

HRESULT WINAPI Hook_Present(IDirect3DDevice9* device, const RECT* sourceRect, const RECT* destRect, HWND overrideWindow, const RGNDATA* dirtyRegion)
{
    // Fallback path for apps that skip or hide EndScene
    RenderOverlay(device, true);
    return s_realPresent ? s_realPresent(device, sourceRect, destRect, overrideWindow, dirtyRegion) : D3DERR_INVALIDCALL;
}

/////////////////////////////////////////////////////////////////////

HRESULT WINAPI Hook_PresentEx(IDirect3DDevice9Ex* device, const RECT* sourceRect, const RECT* destRect, HWND overrideWindow, const RGNDATA* dirtyRegion, DWORD flags)
{
    // Same fallback for D3D9Ex devices
    RenderOverlay(device, true);
    return s_realPresentEx ? s_realPresentEx(device, sourceRect, destRect, overrideWindow, dirtyRegion, flags) : D3DERR_INVALIDCALL;
}

/////////////////////////////////////////////////////////////////////

void BeforeReset(IDirect3DDevice9* device)
{
    std::lock_guard<std::mutex> lock(s_renderMutex);
    if (s_imguiReady && s_imguiDevice == device)
    {
        // Default-pool resources must be released before Reset
        ReleaseIconTexture();
        ImGui_ImplDX9_InvalidateDeviceObjects();
    }
}

/////////////////////////////////////////////////////////////////////

void AfterReset(IDirect3DDevice9* device, HRESULT result)
{
    std::lock_guard<std::mutex> lock(s_renderMutex);
    if (SUCCEEDED(result) && s_imguiReady && s_imguiDevice == device)
    {
        // Recreate ImGui device objects after successful Reset
        if (!ImGui_ImplDX9_CreateDeviceObjects())
        {
            LYMALINK_LOG("[Direct3D9OverlayLayer][AfterReset] ImGui_ImplDX9_CreateDeviceObjects failed.");
        }
    }
}

/////////////////////////////////////////////////////////////////////

HRESULT WINAPI Hook_Reset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* presentationParameters)
{
    // Reset invalidates default-pool D3D resources
    BeforeReset(device);
    const HRESULT result = s_realReset ? s_realReset(device, presentationParameters) : D3DERR_INVALIDCALL;
    AfterReset(device, result);
    return result;
}

/////////////////////////////////////////////////////////////////////

HRESULT WINAPI Hook_ResetEx(IDirect3DDevice9Ex* device, D3DPRESENT_PARAMETERS* presentationParameters, D3DDISPLAYMODEEX* fullscreenDisplayMode)
{
    // D3D9Ex reset uses the same ImGui resource lifecycle
    BeforeReset(device);
    const HRESULT result = s_realResetEx ? s_realResetEx(device, presentationParameters, fullscreenDisplayMode) : D3DERR_INVALIDCALL;
    AfterReset(device, result);
    return result;
}

/////////////////////////////////////////////////////////////////////

void CreateHook(void* target, void* detour, void** original, const char* name)
{
    // Create and enable one MinHook detour
    if (!target)
    {
        LYMALINK_LOG(std::string("[Direct3D9OverlayLayer][CreateHook] missing target ") + name);
        return;
    }

    MH_STATUS status = MH_CreateHook(target, detour, original);
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED)
    {
        LYMALINK_LOG("[Direct3D9OverlayLayer][CreateHook] " + HookStatus(name, status));
        return;
    }

    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED)
    {
        LYMALINK_LOG("[Direct3D9OverlayLayer][EnableHook] " + HookStatus(name, status));
    }
}

/////////////////////////////////////////////////////////////////////

bool RegisterDummyWindowClass(HINSTANCE instance, const wchar_t* className)
{
    // Dummy device needs a real window handle
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = instance;
    wc.lpszClassName = className;
    return RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

/////////////////////////////////////////////////////////////////////

HWND CreateDummyWindow()
{
    // Hidden tiny window exists only for dummy device creation
    HINSTANCE instance = GetModuleHandleW(nullptr);
    constexpr const wchar_t* className = L"LymalinkDx9DummyWindow";
    if (!RegisterDummyWindowClass(instance, className))
    {
        LYMALINK_LOG("[Direct3D9OverlayLayer][CreateDummyWindow] RegisterClassExW failed error=" + std::to_string(GetLastError()));
        return nullptr;
    }

    HWND window = CreateWindowExW(
        0,
        className,
        L"Lymalink DX9 Dummy",
        WS_POPUP,
        0,
        0,
        64,
        64,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (!window)
    {
        LYMALINK_LOG("[Direct3D9OverlayLayer][CreateDummyWindow] CreateWindowExW failed error=" + std::to_string(GetLastError()));
    }
    return window;
}

/////////////////////////////////////////////////////////////////////

bool CreateDummyDevice(IDirect3D9* d3d, HWND window, IDirect3DDevice9** device)
{
    // Need D3D object, hidden window, and output storage
    if (!d3d || !window || !device)
    {
        return false;
    }

    // Minimal windowed parameters are enough for vtable discovery
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth = 1;
    pp.BackBufferHeight = 1;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferCount = 1;
    pp.hDeviceWindow = window;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    const DWORD behaviorFlags[] = {
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        D3DCREATE_MIXED_VERTEXPROCESSING,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING
    };

    HRESULT result = D3DERR_INVALIDCALL;
    for (DWORD behavior : behaviorFlags)
    {
        // Try common HAL modes before slower reference device
        result = d3d->CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            window,
            behavior | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
            &pp,
            device);
        if (SUCCEEDED(result))
        {
            return true;
        }
        *device = nullptr;
    }

    // Last resort: reference device works without hardware acceleration
    result = d3d->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_REF,
        window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
        &pp,
        device);
    if (SUCCEEDED(result))
    {
        return true;
    }

    // NULLREF is enough for vtable discovery and can work while a game owns exclusive fullscreen
    result = d3d->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_NULLREF,
        window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
        &pp,
        device);
    if (SUCCEEDED(result))
    {
        LYMALINK_LOG("[Direct3D9OverlayLayer][CreateDummyDevice] using NULLREF dummy device.");
        return true;
    }

    LYMALINK_LOG("[Direct3D9OverlayLayer][CreateDummyDevice] CreateDevice failed hr=" + std::to_string(result));
    // Keep caller from using failed partial output
    *device = nullptr;
    return false;
}

/////////////////////////////////////////////////////////////////////

bool CreateDummyDeviceEx(IDirect3D9Ex* d3d, HWND window, IDirect3DDevice9Ex** device)
{
    // Need D3DEx object, hidden window, and output storage
    if (!d3d || !window || !device)
    {
        return false;
    }

    // Minimal windowed parameters are enough for vtable discovery
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth = 1;
    pp.BackBufferHeight = 1;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferCount = 1;
    pp.hDeviceWindow = window;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    const DWORD behaviorFlags[] = {
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        D3DCREATE_MIXED_VERTEXPROCESSING,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING
    };

    HRESULT result = D3DERR_INVALIDCALL;
    for (DWORD behavior : behaviorFlags)
    {
        // Try common HAL modes before slower reference device
        result = d3d->CreateDeviceEx(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            window,
            behavior | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
            &pp,
            nullptr,
            device);
        if (SUCCEEDED(result))
        {
            return true;
        }
        *device = nullptr;
    }

    // Last resort: reference device works without hardware acceleration
    result = d3d->CreateDeviceEx(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_REF,
        window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
        &pp,
        nullptr,
        device);
    if (SUCCEEDED(result))
    {
        return true;
    }

    // NULLREF is enough for vtable discovery and can work while a game owns exclusive fullscreen
    result = d3d->CreateDeviceEx(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_NULLREF,
        window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
        &pp,
        nullptr,
        device);
    if (SUCCEEDED(result))
    {
        LYMALINK_LOG("[Direct3D9OverlayLayer][CreateDummyDeviceEx] using NULLREF dummy device.");
        return true;
    }

    LYMALINK_LOG("[Direct3D9OverlayLayer][CreateDummyDeviceEx] CreateDeviceEx failed hr=" + std::to_string(result));
    // Keep caller from using failed partial output
    *device = nullptr;
    return false;
}

/////////////////////////////////////////////////////////////////////

void** VTable(void* object)
{
    // COM objects store vtable pointer at object start
    return object ? *reinterpret_cast<void***>(object) : nullptr;
}

/////////////////////////////////////////////////////////////////////

DWORD WINAPI InitThread(LPVOID)
{
    // Load d3d9.dll if target process has not loaded it yet
    HMODULE d3d9 = GetModuleHandleW(L"d3d9.dll");
    if (!d3d9)
    {
        d3d9 = LoadLibraryW(L"d3d9.dll");
    }
    if (!d3d9)
    {
        LYMALINK_LOG("[Direct3D9OverlayLayer][InitThread] d3d9.dll load failed error=" + std::to_string(GetLastError()));
        return 1;
    }

    // Direct3DCreate9 is required for standard D3D9 hook discovery
    // Dummy device exposes vtable entries for MinHook targets
    auto direct3DCreate9 = reinterpret_cast<PFN_Direct3DCreate9>(GetProcAddress(d3d9, "Direct3DCreate9"));
    if (!direct3DCreate9)
    {
        LYMALINK_LOG("[Direct3D9OverlayLayer][InitThread] Direct3DCreate9 missing.");
        return 1;
    }

    // Hidden window backs the temporary dummy device
    HWND window = CreateDummyWindow();
    if (!window)
    {
        return 1;
    }

    // Build dummy D3D9 device so its vtable can be hooked
    IDirect3D9* d3d = direct3DCreate9(D3D_SDK_VERSION);
    IDirect3DDevice9* device = nullptr;
    if (!d3d || !CreateDummyDevice(d3d, window, &device))
    {
        ReleaseCom(device);
        ReleaseCom(d3d);
        DestroyWindow(window);
        return 1;
    }

    // MinHook must be initialized before creating detours
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
    {
        LYMALINK_LOG("[Direct3D9OverlayLayer][InitThread] " + HookStatus("MH_Initialize", status));
        ReleaseCom(device);
        ReleaseCom(d3d);
        DestroyWindow(window);
        return 1;
    }

    void** table = VTable(device);
    // Hook classic IDirect3DDevice9 calls.
    CreateHook(table[VTable_EndScene], reinterpret_cast<void*>(&Hook_EndScene), reinterpret_cast<void**>(&s_realEndScene), "IDirect3DDevice9::EndScene");
    CreateHook(table[VTable_Present], reinterpret_cast<void*>(&Hook_Present), reinterpret_cast<void**>(&s_realPresent), "IDirect3DDevice9::Present");
    CreateHook(table[VTable_Reset], reinterpret_cast<void*>(&Hook_Reset), reinterpret_cast<void**>(&s_realReset), "IDirect3DDevice9::Reset");

    // Direct3DCreate9Ex exists only on systems with D3D9Ex support
    auto direct3DCreate9Ex = reinterpret_cast<PFN_Direct3DCreate9Ex>(GetProcAddress(d3d9, "Direct3DCreate9Ex"));
    if (direct3DCreate9Ex)
    {
        IDirect3D9Ex* d3dEx = nullptr;
        IDirect3DDevice9Ex* deviceEx = nullptr;
        if (SUCCEEDED(direct3DCreate9Ex(D3D_SDK_VERSION, &d3dEx)) && d3dEx && CreateDummyDeviceEx(d3dEx, window, &deviceEx))
        {
            void** exTable = VTable(deviceEx);
            // Hook D3D9Ex-specific calls when available
            CreateHook(exTable[VTable_PresentEx], reinterpret_cast<void*>(&Hook_PresentEx), reinterpret_cast<void**>(&s_realPresentEx), "IDirect3DDevice9Ex::PresentEx");
            CreateHook(exTable[VTable_ResetEx], reinterpret_cast<void*>(&Hook_ResetEx), reinterpret_cast<void**>(&s_realResetEx), "IDirect3DDevice9Ex::ResetEx");
        }
        ReleaseCom(deviceEx);
        ReleaseCom(d3dEx);
    }

    // Dummy objects are no longer needed once hooks are installed
    ReleaseCom(device);
    ReleaseCom(d3d);
    DestroyWindow(window);

    // Mark install complete for detach cleanup
    s_hooksReady.store(true);
    LYMALINK_LOG("[Direct3D9OverlayLayer][InitThread] hooks installed.");
    return 0;
}
}

/////////////////////////////////////////////////////////////////////
// Called from WinOverlayEntrypoint.cpp DllMain when this target defines
// LYMALINK_OVERLAY_ATTACH_HOOKS.
/////////////////////////////////////////////////////////////////////

extern "C" void LymalinkOverlayOnProcessAttach(HINSTANCE instance)
{
    // Keep attach lightweight; hook setup runs on worker thread
    DisableThreadLibraryCalls(instance);

    HANDLE thread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    if (thread)
    {
        CloseHandle(thread);
    }
    else
    {
        LYMALINK_LOG("[Direct3D9OverlayLayer][Attach] CreateThread failed error=" + std::to_string(GetLastError()));
    }
}

/////////////////////////////////////////////////////////////////////

extern "C" void LymalinkOverlayOnProcessDetach()
{
    // Stop hooks first, then release ImGui and shared overlay state
    s_shuttingDown.store(true);
    if (s_hooksReady.load())
    {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }
    {
        std::lock_guard<std::mutex> lock(s_renderMutex);
        ShutdownImGuiLocked();
    }
    s_overlay.Shutdown();
}
