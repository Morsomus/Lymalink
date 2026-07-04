/////////////////////////////////////////////////////////
// File: Direct3D10OverlayLayer.cpp
// Date: 2026-07-04
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Windows Direct3D 10 overlay hooks driven by
//              MinHook and Dear ImGui DX10 backend.
/////////////////////////////////////////////////////////

#ifndef NOMINMAX
    #define NOMINMAX
#endif

#include "FontEmbedded.h"
#include "OverlaySharedMemoryState.h"
#include "WinLogger.h"
#include "WinOverlayReceiver.h"
#include "imgui.h"
#include "imgui_impl_dx10.h"
#include "MinHook.h"

#include <windows.h>
#include <d3d10.h>
#include <dxgi.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace
{
// Function pointer types for DXGI/D3D10 methods we intercept
using PFN_Present = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
using PFN_ResizeBuffers = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using PFN_D3D10CreateDeviceAndSwapChain = HRESULT(WINAPI*)(
    IDXGIAdapter*,
    D3D10_DRIVER_TYPE,
    HMODULE,
    UINT,
    UINT,
    DXGI_SWAP_CHAIN_DESC*,
    IDXGISwapChain**,
    ID3D10Device**);

// Known IDXGISwapChain vtable slots used by the hook setup
constexpr int VTable_Present = 8;
constexpr int VTable_ResizeBuffers = 13;
constexpr UINT MaxSavedViewports = 16;

// Original DXGI methods saved by MinHook
static PFN_Present s_realPresent = nullptr;
static PFN_ResizeBuffers s_realResizeBuffers = nullptr;

// One receiver and one ImGui context are shared by every DX10 hook in this process
static WinOverlayReceiver s_overlay;
static std::mutex s_renderMutex;
static std::atomic_bool s_hooksReady{false};
static std::atomic_bool s_shuttingDown{false};
static thread_local bool s_rendering = false;   // Prevents recursive interception while ImGui renders

static IDXGISwapChain* s_swapChain = nullptr;
static ID3D10Device* s_device = nullptr;
static ID3D10RenderTargetView* s_renderTargetView = nullptr;
static ID3D10ShaderResourceView* s_iconView = nullptr;
static ID3D10Texture2D* s_iconTexture = nullptr;
static bool s_imguiReady = false;
static uint64_t s_iconGeneration = 0;

static std::atomic_bool s_loggedPresentHit{false};

/////////////////////////////////////////////////////////////////////

std::string HookStatus(const char* name, MH_STATUS status)
{
    // Keep MinHook logs compact and readable
    return std::string(name) + " status=" + std::to_string(static_cast<int>(status));
}

/////////////////////////////////////////////////////////////////////

template <typename T>
void ReleaseCom(T*& value)
{
    // Release optional COM pointer and clear caller storage
    if (value)
    {
        value->Release();
        value = nullptr;
    }
}

/////////////////////////////////////////////////////////////////////

void AddRefSwapChain(IDXGISwapChain* swapChain)
{
    // Store the active swap chain with an owned reference
    if (swapChain)
    {
        swapChain->AddRef();
    }
    ReleaseCom(s_swapChain);
    s_swapChain = swapChain;
}

/////////////////////////////////////////////////////////////////////

void AddRefDevice(ID3D10Device* device)
{
    // Store the active device with an owned reference
    if (device)
    {
        device->AddRef();
    }
    ReleaseCom(s_device);
    s_device = device;
}

/////////////////////////////////////////////////////////////////////

void ReleaseIconResources()
{
    // Icon texture/view are rebuilt when shared icon pixels change
    ReleaseCom(s_iconView);
    ReleaseCom(s_iconTexture);
    s_iconGeneration = 0;
}

/////////////////////////////////////////////////////////////////////

void ReleaseSwapChainResources()
{
    // Backbuffer-dependent resources must be dropped before resize
    ReleaseCom(s_renderTargetView);
    ReleaseIconResources();
}

/////////////////////////////////////////////////////////////////////

void ShutdownImGuiLocked()
{
    // Caller holds render mutex while tearing down ImGui
    ReleaseSwapChainResources();
    if (s_imguiReady)
    {
        ImGui_ImplDX10_Shutdown();
        s_imguiReady = false;
    }
    if (ImGui::GetCurrentContext())
    {
        ImGui::DestroyContext();
    }
    ReleaseCom(s_device);
    ReleaseCom(s_swapChain);
}

/////////////////////////////////////////////////////////////////////

bool QueryFramebufferSize(IDXGISwapChain* swapChain, uint32_t& outWidth, uint32_t& outHeight)
{
    // Clear outputs until a valid size is found
    outWidth = 0;
    outHeight = 0;
    if (!swapChain)
    {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    // Prefer DXGI swap-chain description when it has a real buffer size
    if (SUCCEEDED(swapChain->GetDesc(&desc)) && desc.BufferDesc.Width > 0 && desc.BufferDesc.Height > 0)
    {
        outWidth = desc.BufferDesc.Width;
        outHeight = desc.BufferDesc.Height;
        return true;
    }

    ID3D10Texture2D* backBuffer = nullptr;
    if (SUCCEEDED(swapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), reinterpret_cast<void**>(&backBuffer))) && backBuffer)
    {
        // Backbuffer desc is a fallback when swap-chain desc is incomplete
        D3D10_TEXTURE2D_DESC textureDesc{};
        backBuffer->GetDesc(&textureDesc);
        ReleaseCom(backBuffer);
        if (textureDesc.Width > 0 && textureDesc.Height > 0)
        {
            outWidth = textureDesc.Width;
            outHeight = textureDesc.Height;
            return true;
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////

bool EnsureRenderTargetLocked(IDXGISwapChain* swapChain, ID3D10Device* device)
{
    // Existing render target view remains valid until ResizeBuffers
    if (s_renderTargetView)
    {
        return true;
    }

    ID3D10Texture2D* backBuffer = nullptr;
    // Create an RTV over the current backbuffer so ImGui can draw
    HRESULT result = swapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), reinterpret_cast<void**>(&backBuffer));
    if (FAILED(result) || !backBuffer)
    {
        LYMALINK_LOG("[Direct3D10OverlayLayer][EnsureRenderTarget] GetBuffer failed hr=" + std::to_string(result));
        return false;
    }

    result = device->CreateRenderTargetView(backBuffer, nullptr, &s_renderTargetView);
    ReleaseCom(backBuffer);
    if (FAILED(result) || !s_renderTargetView)
    {
        LYMALINK_LOG("[Direct3D10OverlayLayer][EnsureRenderTarget] CreateRenderTargetView failed hr=" + std::to_string(result));
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////

bool InitImGuiLocked(IDXGISwapChain* swapChain, ID3D10Device* device, uint32_t width, uint32_t height)
{
    // Need a valid swap chain and device before touching ImGui
    if (!swapChain || !device)
    {
        return false;
    }
    // Reuse existing ImGui backend when swap chain and device did not change
    if (s_imguiReady && s_swapChain == swapChain && s_device == device)
    {
        return true;
    }

    // Swap chain or device changed, so rebuild ImGui from scratch
    ShutdownImGuiLocked();
    AddRefSwapChain(swapChain);
    AddRefDevice(device);

    // Bind ImGui to the current D3D10 device
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    OverlayFonts::EnsureEmbeddedFontLoaded();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();

    // Start Dear ImGui DX10 renderer backend
    if (!ImGui_ImplDX10_Init(device))
    {
        LYMALINK_LOG("[Direct3D10OverlayLayer][InitImGui] ImGui_ImplDX10_Init failed.");
        ShutdownImGuiLocked();
        return false;
    }

    s_imguiReady = true;
    LYMALINK_LOG("[Direct3D10OverlayLayer][InitImGui] ready " + std::to_string(width) + "x" + std::to_string(height));
    return true;
}

/////////////////////////////////////////////////////////////////////

ImTextureID EnsureDirect3D10IconTexture(ID3D10Device* device, const std::vector<uint8_t>& rgbaPixels, uint64_t generation)
{
    // Reject missing device or incomplete icon buffer
    if (!device || rgbaPixels.size() != OVERLAY_ICON_DATA_SIZE)
    {
        return ImTextureID_Invalid;
    }
    // Same generation means existing shader resource view is still current
    if (s_iconView && s_iconGeneration == generation)
    {
        return reinterpret_cast<ImTextureID>(s_iconView);
    }

    // Old icon resources are stale
    ReleaseIconResources();

    // D3D10 texture accepts the shared RGBA icon bytes directly
    D3D10_TEXTURE2D_DESC textureDesc{};
    textureDesc.Width = OVERLAY_ICON_SIZE;
    textureDesc.Height = OVERLAY_ICON_SIZE;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D10_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE;

    D3D10_SUBRESOURCE_DATA initialData{};
    initialData.pSysMem = rgbaPixels.data();
    initialData.SysMemPitch = OVERLAY_ICON_STRIDE;

    HRESULT result = device->CreateTexture2D(&textureDesc, &initialData, &s_iconTexture);
    if (FAILED(result) || !s_iconTexture)
    {
        LYMALINK_LOG("[Direct3D10OverlayLayer][EnsureIconTexture] CreateTexture2D failed hr=" + std::to_string(result));
        return ImTextureID_Invalid;
    }

    D3D10_SHADER_RESOURCE_VIEW_DESC viewDesc{};
    viewDesc.Format = textureDesc.Format;
    viewDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    viewDesc.Texture2D.MipLevels = 1;

    // ImGui uses the shader resource view as ImTextureID
    result = device->CreateShaderResourceView(s_iconTexture, &viewDesc, &s_iconView);
    if (FAILED(result) || !s_iconView)
    {
        LYMALINK_LOG("[Direct3D10OverlayLayer][EnsureIconTexture] CreateShaderResourceView failed hr=" + std::to_string(result));
        ReleaseIconResources();
        return ImTextureID_Invalid;
    }

    // Store generation so next frame can reuse these resources
    s_iconGeneration = generation;
    return reinterpret_cast<ImTextureID>(s_iconView);
}

/////////////////////////////////////////////////////////////////////

void RenderOverlay(IDXGISwapChain* swapChain)
{
    // Present is the main DX10 render path
    if (s_rendering || s_shuttingDown.load() || !swapChain)
    {
        return;
    }

    // Resolve the D3D10 device from the intercepted swap chain
    ID3D10Device* device = nullptr;
    if (FAILED(swapChain->GetDevice(__uuidof(ID3D10Device), reinterpret_cast<void**>(&device))) || !device)
    {
        return;
    }

    // Need real framebuffer size before creating ImGui frame
    uint32_t width = 0;
    uint32_t height = 0;
    if (!QueryFramebufferSize(swapChain, width, height) || width == 0 || height == 0)
    {
        ReleaseCom(device);
        return;
    }

    std::lock_guard<std::mutex> lock(s_renderMutex);
    s_rendering = true;

    if (InitImGuiLocked(swapChain, device, width, height) && EnsureRenderTargetLocked(swapChain, device))
    {
        // Save host render state that this overlay temporarily changes
        ID3D10RenderTargetView* previousRenderTarget = nullptr;
        ID3D10DepthStencilView* previousDepthStencil = nullptr;
        D3D10_VIEWPORT previousViewports[MaxSavedViewports]{};
        UINT previousViewportCount = MaxSavedViewports;
        device->OMGetRenderTargets(1, &previousRenderTarget, &previousDepthStencil);
        device->RSGetViewports(&previousViewportCount, previousViewports);

        device->OMSetRenderTargets(1, &s_renderTargetView, nullptr);

        // Pull newest shared-memory state and render overlay widgets
        ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
        ImGui_ImplDX10_NewFrame();
        ImGui::NewFrame();
        s_overlay.BeginFrame();
        ImTextureID icon = EnsureDirect3D10IconTexture(device, s_overlay.IconPixels(), s_overlay.IconGeneration());
        s_overlay.Draw(width, height, icon);
        ImGui::Render();
        ImGui_ImplDX10_RenderDrawData(ImGui::GetDrawData());

        // Restore host render state after ImGui draw calls
        device->OMSetRenderTargets(1, &previousRenderTarget, previousDepthStencil);
        if (previousViewportCount > 0)
        {
            device->RSSetViewports(previousViewportCount, previousViewports);
        }
        ReleaseCom(previousRenderTarget);
        ReleaseCom(previousDepthStencil);
    }

    s_rendering = false;
    ReleaseCom(device);
}

/////////////////////////////////////////////////////////////////////

HRESULT WINAPI Hook_Present(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
{
    // Main render path for DX10 apps
    if (!s_loggedPresentHit.exchange(true))
    {
        LYMALINK_LOG("[Direct3D10OverlayLayer][Hook_Present] first Present intercepted.");
    }
    RenderOverlay(swapChain);
    return s_realPresent ? s_realPresent(swapChain, syncInterval, flags) : DXGI_ERROR_INVALID_CALL;
}

/////////////////////////////////////////////////////////////////////

void BeforeResizeBuffers(IDXGISwapChain* swapChain)
{
    std::lock_guard<std::mutex> lock(s_renderMutex);
    if (s_imguiReady && s_swapChain == swapChain)
    {
        // Backbuffer resources must be released before ResizeBuffers
        ReleaseSwapChainResources();
        ImGui_ImplDX10_InvalidateDeviceObjects();
    }
}

/////////////////////////////////////////////////////////////////////

void AfterResizeBuffers(IDXGISwapChain* swapChain, HRESULT result)
{
    std::lock_guard<std::mutex> lock(s_renderMutex);
    if (SUCCEEDED(result) && s_imguiReady && s_swapChain == swapChain)
    {
        // Recreate ImGui device objects after successful ResizeBuffers
        if (!ImGui_ImplDX10_CreateDeviceObjects())
        {
            LYMALINK_LOG("[Direct3D10OverlayLayer][AfterResizeBuffers] ImGui_ImplDX10_CreateDeviceObjects failed.");
        }
    }
}

/////////////////////////////////////////////////////////////////////

HRESULT WINAPI Hook_ResizeBuffers(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags)
{
    // ResizeBuffers invalidates backbuffer-dependent D3D resources
    BeforeResizeBuffers(swapChain);
    const HRESULT result = s_realResizeBuffers ?
        s_realResizeBuffers(swapChain, bufferCount, width, height, newFormat, swapChainFlags) :
        DXGI_ERROR_INVALID_CALL;
    AfterResizeBuffers(swapChain, result);
    return result;
}

/////////////////////////////////////////////////////////////////////

void CreateHook(void* target, void* detour, void** original, const char* name)
{
    // Create and enable one MinHook detour
    if (!target)
    {
        LYMALINK_LOG(std::string("[Direct3D10OverlayLayer][CreateHook] missing target ") + name);
        return;
    }

    MH_STATUS status = MH_CreateHook(target, detour, original);
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED)
    {
        LYMALINK_LOG("[Direct3D10OverlayLayer][CreateHook] " + HookStatus(name, status));
        return;
    }

    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED)
    {
        LYMALINK_LOG("[Direct3D10OverlayLayer][EnableHook] " + HookStatus(name, status));
    }
}

/////////////////////////////////////////////////////////////////////

bool RegisterDummyWindowClass(HINSTANCE instance, const wchar_t* className)
{
    // Dummy swap chain needs a real window handle
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
    // Hidden tiny window exists only for dummy swap-chain creation
    HINSTANCE instance = GetModuleHandleW(nullptr);
    constexpr const wchar_t* className = L"LymalinkDx10DummyWindow";
    if (!RegisterDummyWindowClass(instance, className))
    {
        LYMALINK_LOG("[Direct3D10OverlayLayer][CreateDummyWindow] RegisterClassExW failed error=" + std::to_string(GetLastError()));
        return nullptr;
    }

    HWND window = CreateWindowExW(
        0,
        className,
        L"Lymalink DX10 Dummy",
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
        LYMALINK_LOG("[Direct3D10OverlayLayer][CreateDummyWindow] CreateWindowExW failed error=" + std::to_string(GetLastError()));
    }
    return window;
}

/////////////////////////////////////////////////////////////////////

bool CreateDummyDeviceAndSwapChain(PFN_D3D10CreateDeviceAndSwapChain createDeviceAndSwapChain, HWND window, IDXGISwapChain** swapChain, ID3D10Device** device)
{
    // Need factory function, hidden window, and output storage
    if (!createDeviceAndSwapChain || !window || !swapChain || !device)
    {
        return false;
    }

    // Minimal windowed parameters are enough for vtable discovery
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width = 1;
    desc.BufferDesc.Height = 1;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 1;
    desc.OutputWindow = window;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D10_DRIVER_TYPE driverTypes[] = {
        D3D10_DRIVER_TYPE_HARDWARE,
        D3D10_DRIVER_TYPE_WARP,
        D3D10_DRIVER_TYPE_REFERENCE
    };

    HRESULT result = DXGI_ERROR_INVALID_CALL;
    for (D3D10_DRIVER_TYPE driverType : driverTypes)
    {
        // Try hardware first, then software fallbacks
        result = createDeviceAndSwapChain(
            nullptr,
            driverType,
            nullptr,
            0,
            D3D10_SDK_VERSION,
            &desc,
            swapChain,
            device);
        if (SUCCEEDED(result))
        {
            return true;
        }
        *swapChain = nullptr;
        *device = nullptr;
    }

    LYMALINK_LOG("[Direct3D10OverlayLayer][CreateDummyDeviceAndSwapChain] failed hr=" + std::to_string(result));
    // Keep caller from using failed partial output
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
    // Load d3d10.dll if target process has not loaded it yet
    HMODULE d3d10 = GetModuleHandleW(L"d3d10.dll");
    if (!d3d10)
    {
        d3d10 = LoadLibraryW(L"d3d10.dll");
    }
    if (!d3d10)
    {
        LYMALINK_LOG("[Direct3D10OverlayLayer][InitThread] d3d10.dll load failed error=" + std::to_string(GetLastError()));
        return 1;
    }

    // D3D10CreateDeviceAndSwapChain is required for hook discovery
    // Dummy swap chain exposes vtable entries for MinHook targets
    auto createDeviceAndSwapChain = reinterpret_cast<PFN_D3D10CreateDeviceAndSwapChain>(GetProcAddress(d3d10, "D3D10CreateDeviceAndSwapChain"));
    if (!createDeviceAndSwapChain)
    {
        LYMALINK_LOG("[Direct3D10OverlayLayer][InitThread] D3D10CreateDeviceAndSwapChain missing.");
        return 1;
    }

    // Hidden window backs the temporary dummy swap chain
    HWND window = CreateDummyWindow();
    if (!window)
    {
        return 1;
    }

    // Build dummy D3D10 device/swap chain so its vtable can be hooked
    IDXGISwapChain* swapChain = nullptr;
    ID3D10Device* device = nullptr;
    if (!CreateDummyDeviceAndSwapChain(createDeviceAndSwapChain, window, &swapChain, &device))
    {
        ReleaseCom(device);
        ReleaseCom(swapChain);
        DestroyWindow(window);
        return 1;
    }

    // MinHook must be initialized before creating detours
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
    {
        LYMALINK_LOG("[Direct3D10OverlayLayer][InitThread] " + HookStatus("MH_Initialize", status));
        ReleaseCom(device);
        ReleaseCom(swapChain);
        DestroyWindow(window);
        return 1;
    }

    void** table = VTable(swapChain);
    // Hook swap-chain calls used by DX10 presentation and resize
    CreateHook(table[VTable_Present], reinterpret_cast<void*>(&Hook_Present), reinterpret_cast<void**>(&s_realPresent), "IDXGISwapChain::Present");
    CreateHook(table[VTable_ResizeBuffers], reinterpret_cast<void*>(&Hook_ResizeBuffers), reinterpret_cast<void**>(&s_realResizeBuffers), "IDXGISwapChain::ResizeBuffers");

    // Dummy objects are no longer needed once hooks are installed
    ReleaseCom(device);
    ReleaseCom(swapChain);
    DestroyWindow(window);

    // Mark install complete for detach cleanup
    s_hooksReady.store(true);
    LYMALINK_LOG("[Direct3D10OverlayLayer][InitThread] hooks installed.");
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
        LYMALINK_LOG("[Direct3D10OverlayLayer][Attach] CreateThread failed error=" + std::to_string(GetLastError()));
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
