/////////////////////////////////////////////////////////
// File: Direct3D11OverlayLayer.cpp
// Date: 2026-07-05
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Windows Direct3D 11 overlay hooks driven by
//              MinHook and Dear ImGui DX11 backend.
/////////////////////////////////////////////////////////

#ifndef NOMINMAX
    #define NOMINMAX
#endif

#include "FontEmbedded.h"
#include "OverlaySharedMemoryState.h"
#include "WinLogger.h"
#include "WinOverlayReceiver.h"
#include "WinDxgiOverlayRouter.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "MinHook.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>

#include <atomic>
#include <cstring>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace
{
// Function pointer types for DXGI/D3D11 methods we intercept
using PFN_Present = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
using PFN_ResizeBuffers = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using PFN_Present1 = HRESULT(WINAPI*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
using PFN_D3D11CreateDeviceAndSwapChain = HRESULT(WINAPI*)(
    IDXGIAdapter*,
    D3D_DRIVER_TYPE,
    HMODULE,
    UINT,
    const D3D_FEATURE_LEVEL*,
    UINT,
    UINT,
    const DXGI_SWAP_CHAIN_DESC*,
    IDXGISwapChain**,
    ID3D11Device**,
    D3D_FEATURE_LEVEL*,
    ID3D11DeviceContext**);

// Known DXGI swap-chain vtable slots used by the hook setup
constexpr int VTable_Present = 8;
constexpr int VTable_ResizeBuffers = 13;
constexpr int VTable_Present1 = 22;

// Original DXGI methods saved by MinHook
static PFN_Present s_realPresent = nullptr;
static PFN_ResizeBuffers s_realResizeBuffers = nullptr;
static PFN_Present1 s_realPresent1 = nullptr;

static void* s_vtableObject = nullptr;
static void** s_originalVTable = nullptr;
static void** s_patchedVTable = nullptr;
static PFN_Present s_vtablePresent = nullptr;
static PFN_Present1 s_vtablePresent1 = nullptr;

// One receiver and one ImGui context are shared by every DX11 hook in this process
static WinOverlayReceiver s_overlay;
static std::mutex s_renderMutex;
static std::atomic_bool s_hooksReady{false};
static std::atomic_bool s_shuttingDown{false};
static thread_local bool s_rendering = false;   // Prevents recursive interception while ImGui renders
static thread_local bool s_callingVTableOriginal = false;

static IDXGISwapChain* s_swapChain = nullptr;
static ID3D11Device* s_device = nullptr;
static ID3D11DeviceContext* s_context = nullptr;
static ID3D11RenderTargetView* s_renderTargetView = nullptr;
static ID3D11ShaderResourceView* s_iconView = nullptr;
static ID3D11Texture2D* s_iconTexture = nullptr;
static bool s_imguiReady = false;
static uint64_t s_iconGeneration = 0;

static std::atomic_bool s_loggedPresentHit{false};
static std::atomic_bool s_loggedPresent1Hit{false};
static std::atomic_bool s_loggedVTablePresentHit{false};
static std::atomic_bool s_loggedVTablePresent1Hit{false};
static std::atomic_bool s_loggedRoutedSwapChain{false};

HRESULT WINAPI Hook_Present(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);
HRESULT WINAPI Hook_ResizeBuffers(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags);
HRESULT WINAPI Hook_Present1(IDXGISwapChain1* swapChain, UINT syncInterval, UINT flags, const DXGI_PRESENT_PARAMETERS* presentParameters);
HRESULT WINAPI Hook_VTablePresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);
HRESULT WINAPI Hook_VTablePresent1(IDXGISwapChain1* swapChain, UINT syncInterval, UINT flags, const DXGI_PRESENT_PARAMETERS* presentParameters);

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

void AddRefDevice(ID3D11Device* device)
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

void AddRefContext(ID3D11DeviceContext* context)
{
    // Store the active immediate context with an owned reference
    if (context)
    {
        context->AddRef();
    }
    ReleaseCom(s_context);
    s_context = context;
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
        ImGui_ImplDX11_Shutdown();
        s_imguiReady = false;
    }
    if (ImGui::GetCurrentContext())
    {
        ImGui::DestroyContext();
    }
    ReleaseCom(s_context);
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

    ID3D11Texture2D* backBuffer = nullptr;
    if (SUCCEEDED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer))) && backBuffer)
    {
        // Backbuffer desc is a fallback when swap-chain desc is incomplete
        D3D11_TEXTURE2D_DESC textureDesc{};
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

bool EnsureRenderTargetLocked(IDXGISwapChain* swapChain, ID3D11Device* device)
{
    // Existing render target view remains valid until ResizeBuffers
    if (s_renderTargetView)
    {
        return true;
    }

    ID3D11Texture2D* backBuffer = nullptr;
    // Create an RTV over the current backbuffer so ImGui can draw
    HRESULT result = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
    if (FAILED(result) || !backBuffer)
    {
        LYMALINK_LOG("[Direct3D11OverlayLayer][EnsureRenderTarget] GetBuffer failed hr=" + std::to_string(result));
        return false;
    }

    result = device->CreateRenderTargetView(backBuffer, nullptr, &s_renderTargetView);
    ReleaseCom(backBuffer);
    if (FAILED(result) || !s_renderTargetView)
    {
        LYMALINK_LOG("[Direct3D11OverlayLayer][EnsureRenderTarget] CreateRenderTargetView failed hr=" + std::to_string(result));
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////

bool InitImGuiLocked(IDXGISwapChain* swapChain, ID3D11Device* device, ID3D11DeviceContext* context, uint32_t width, uint32_t height)
{
    // Need a valid swap chain, device, and immediate context before touching ImGui
    if (!swapChain || !device || !context)
    {
        return false;
    }
    // Reuse existing ImGui backend when swap chain and device did not change
    if (s_imguiReady && s_swapChain == swapChain && s_device == device && s_context == context)
    {
        return true;
    }

    // Swap chain, device, or context changed, so rebuild ImGui from scratch
    ShutdownImGuiLocked();
    AddRefSwapChain(swapChain);
    AddRefDevice(device);
    AddRefContext(context);

    // Bind ImGui to the current D3D11 device/context pair
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    OverlayFonts::EnsureEmbeddedFontLoaded();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();

    // Start Dear ImGui DX11 renderer backend
    if (!ImGui_ImplDX11_Init(device, context))
    {
        LYMALINK_LOG("[Direct3D11OverlayLayer][InitImGui] ImGui_ImplDX11_Init failed.");
        ShutdownImGuiLocked();
        return false;
    }

    s_imguiReady = true;
    LYMALINK_LOG("[Direct3D11OverlayLayer][InitImGui] ready " + std::to_string(width) + "x" + std::to_string(height));
    return true;
}

/////////////////////////////////////////////////////////////////////

ImTextureID EnsureDirect3D11IconTexture(ID3D11Device* device, const std::vector<uint8_t>& rgbaPixels, uint64_t generation)
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

    // D3D11 texture accepts the shared RGBA icon bytes directly
    D3D11_TEXTURE2D_DESC textureDesc{};
    textureDesc.Width = OVERLAY_ICON_SIZE;
    textureDesc.Height = OVERLAY_ICON_SIZE;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initialData{};
    initialData.pSysMem = rgbaPixels.data();
    initialData.SysMemPitch = OVERLAY_ICON_STRIDE;

    HRESULT result = device->CreateTexture2D(&textureDesc, &initialData, &s_iconTexture);
    if (FAILED(result) || !s_iconTexture)
    {
        LYMALINK_LOG("[Direct3D11OverlayLayer][EnsureIconTexture] CreateTexture2D failed hr=" + std::to_string(result));
        return ImTextureID_Invalid;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
    viewDesc.Format = textureDesc.Format;
    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    viewDesc.Texture2D.MipLevels = 1;

    // ImGui uses the shader resource view as ImTextureID
    result = device->CreateShaderResourceView(s_iconTexture, &viewDesc, &s_iconView);
    if (FAILED(result) || !s_iconView)
    {
        LYMALINK_LOG("[Direct3D11OverlayLayer][EnsureIconTexture] CreateShaderResourceView failed hr=" + std::to_string(result));
        ReleaseIconResources();
        return ImTextureID_Invalid;
    }

    // Store generation so next frame can reuse these resources
    s_iconGeneration = generation;
    return reinterpret_cast<ImTextureID>(s_iconView);
}

/////////////////////////////////////////////////////////////////////

bool InstallSwapChainVTableHookLocked(IDXGISwapChain* swapChain)
{
    // Install object-level Present hooks for a routed swap chain
    if (!swapChain)
    {
        return false;
    }

    // Same swap chain is already patched
    if (s_vtableObject == swapChain && s_patchedVTable)
    {
        return true;
    }

    // Present1 is available only when the object exposes IDXGISwapChain1 on the same pointer
    IDXGISwapChain1* swapChain1 = nullptr;
    if (FAILED(swapChain->QueryInterface(__uuidof(IDXGISwapChain1), reinterpret_cast<void**>(&swapChain1))))
    {
        swapChain1 = nullptr;
    }

    const bool sameInterface = swapChain1 && reinterpret_cast<void*>(swapChain1) == reinterpret_cast<void*>(swapChain);
    // Read the COM object's current vtable pointer
    void*** objectVTable = reinterpret_cast<void***>(swapChain);
    void** table = *objectVTable;
    if (!table)
    {
        ReleaseCom(swapChain1);
        return false;
    }

    // Clone the slots we need so only this object is affected
    const size_t slotCount = sameInterface ? VTable_Present1 + 1 : VTable_ResizeBuffers + 1;
    void** patched = static_cast<void**>(VirtualAlloc(nullptr, sizeof(void*) * slotCount, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!patched)
    {
        LYMALINK_LOG("[Direct3D11OverlayLayer][VTableHook] VirtualAlloc failed error=" + std::to_string(GetLastError()));
        ReleaseCom(swapChain1);
        return false;
    }

    std::memcpy(patched, table, sizeof(void*) * slotCount);
    s_vtablePresent = reinterpret_cast<PFN_Present>(table[VTable_Present]);
    patched[VTable_Present] = reinterpret_cast<void*>(&Hook_VTablePresent);
    if (sameInterface)
    {
        s_vtablePresent1 = reinterpret_cast<PFN_Present1>(table[VTable_Present1]);
        patched[VTable_Present1] = reinterpret_cast<void*>(&Hook_VTablePresent1);
    }

    // Replace the object's vtable pointer with the patched clone
    DWORD oldProtect = 0;
    if (!VirtualProtect(objectVTable, sizeof(void**), PAGE_READWRITE, &oldProtect))
    {
        LYMALINK_LOG("[Direct3D11OverlayLayer][VTableHook] VirtualProtect failed error=" + std::to_string(GetLastError()));
        VirtualFree(patched, 0, MEM_RELEASE);
        s_vtablePresent = nullptr;
        s_vtablePresent1 = nullptr;
        ReleaseCom(swapChain1);
        return false;
    }

    *objectVTable = patched;
    VirtualProtect(objectVTable, sizeof(void**), oldProtect, &oldProtect);

    // Keep original state so the vtable hooks can call through
    s_vtableObject = swapChain;
    s_originalVTable = table;
    s_patchedVTable = patched;

    ReleaseCom(swapChain1);
    LYMALINK_LOG("[Direct3D11OverlayLayer][VTableHook] installed swap-chain vtable hook.");
    return true;
}

/////////////////////////////////////////////////////////////////////

void RenderOverlay(IDXGISwapChain* swapChain, const char* presentPath)
{
    // Present and Present1 are the main DX11 render paths
    if (s_rendering || s_shuttingDown.load() || !swapChain)
    {
        return;
    }

    const WinDxgiOverlayRouter::Renderer renderer = WinDxgiOverlayRouter::DetectSwapChainRenderer(swapChain);
    if (renderer != WinDxgiOverlayRouter::Renderer::Direct3D11)
    {
        if (WinDxgiOverlayRouter::InstallSwapChainHook(renderer, swapChain) && !s_loggedRoutedSwapChain.exchange(true))
        {
            LYMALINK_LOG("[Direct3D11OverlayLayer][RenderOverlay] routed swap chain to owning DX layer.");
        }
        return;
    }

    // Resolve the D3D11 device from the intercepted swap chain
    ID3D11Device* device = nullptr;
    if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device))) || !device)
    {
        return;
    }

    ID3D11DeviceContext* context = nullptr;
    device->GetImmediateContext(&context);
    if (!context)
    {
        ReleaseCom(device);
        return;
    }

    // Need real framebuffer size before creating ImGui frame
    uint32_t width = 0;
    uint32_t height = 0;
    if (!QueryFramebufferSize(swapChain, width, height) || width == 0 || height == 0)
    {
        ReleaseCom(context);
        ReleaseCom(device);
        return;
    }

    std::lock_guard<std::mutex> lock(s_renderMutex);
    s_rendering = true;

    if (InitImGuiLocked(swapChain, device, context, width, height) && EnsureRenderTargetLocked(swapChain, device))
    {
        InstallSwapChainVTableHookLocked(swapChain);
        
        // Save host render targets that this overlay temporarily changes
        ID3D11RenderTargetView* previousRenderTarget = nullptr;
        ID3D11DepthStencilView* previousDepthStencil = nullptr;
        context->OMGetRenderTargets(1, &previousRenderTarget, &previousDepthStencil);

        context->OMSetRenderTargets(1, &s_renderTargetView, nullptr);

        // Pull newest shared-memory state and render overlay widgets
        ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();
        const bool claimedNotification = s_overlay.BeginFrame();
        ImTextureID icon = EnsureDirect3D11IconTexture(device, s_overlay.IconPixels(), s_overlay.IconGeneration());
        s_overlay.Draw(width, height, icon);
        if (claimedNotification)
        {
            LYMALINK_LOG("[Direct3D11OverlayLayer][RenderOverlay] claimed notification; renderer=dx11 path=" +
                std::string(presentPath) + " size=" +
                std::to_string(width) + "x" + std::to_string(height));
        }
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Restore host render targets after ImGui draw calls
        context->OMSetRenderTargets(1, &previousRenderTarget, previousDepthStencil);
        ReleaseCom(previousRenderTarget);
        ReleaseCom(previousDepthStencil);
    }

    s_rendering = false;
    ReleaseCom(context);
    ReleaseCom(device);
}

/////////////////////////////////////////////////////////////////////

HRESULT WINAPI Hook_Present(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
{
    // Main render path for DX11 apps using IDXGISwapChain::Present
    if (s_callingVTableOriginal)
    {
        return s_realPresent ? s_realPresent(swapChain, syncInterval, flags) : DXGI_ERROR_INVALID_CALL;
    }
    if (!s_loggedPresentHit.exchange(true))
    {
        LYMALINK_LOG("[Direct3D11OverlayLayer][Hook_Present] first Present intercepted.");
    }
    RenderOverlay(swapChain, "Present");
    return s_realPresent ? s_realPresent(swapChain, syncInterval, flags) : DXGI_ERROR_INVALID_CALL;
}

/////////////////////////////////////////////////////////////////////

HRESULT WINAPI Hook_Present1(IDXGISwapChain1* swapChain, UINT syncInterval, UINT flags, const DXGI_PRESENT_PARAMETERS* presentParameters)
{
    // Main render path for DX11 apps using IDXGISwapChain1::Present1
    if (s_callingVTableOriginal)
    {
        return s_realPresent1 ? s_realPresent1(swapChain, syncInterval, flags, presentParameters) : DXGI_ERROR_INVALID_CALL;
    }
    if (!s_loggedPresent1Hit.exchange(true))
    {
        LYMALINK_LOG("[Direct3D11OverlayLayer][Hook_Present1] first Present1 intercepted.");
    }
    RenderOverlay(swapChain, "Present1");
    return s_realPresent1 ? s_realPresent1(swapChain, syncInterval, flags, presentParameters) : DXGI_ERROR_INVALID_CALL;
}

/////////////////////////////////////////////////////////////////////

HRESULT WINAPI Hook_VTablePresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
{
    // Render through this layer, then call the swap chain's original Present
    if (!s_loggedVTablePresentHit.exchange(true))
    {
        LYMALINK_LOG("[Direct3D11OverlayLayer][Hook_VTablePresent] first vtable Present intercepted.");
    }
    RenderOverlay(swapChain, "VTablePresent");

    // Avoid re-entering this layer's MinHook Present hook while forwarding
    s_callingVTableOriginal = true;
    const HRESULT result = s_vtablePresent ? s_vtablePresent(swapChain, syncInterval, flags) :
        (s_realPresent ? s_realPresent(swapChain, syncInterval, flags) : DXGI_ERROR_INVALID_CALL);
    s_callingVTableOriginal = false;
    return result;
}

/////////////////////////////////////////////////////////////////////

HRESULT WINAPI Hook_VTablePresent1(IDXGISwapChain1* swapChain, UINT syncInterval, UINT flags, const DXGI_PRESENT_PARAMETERS* presentParameters)
{
    // Render through this layer, then call the swap chain's original Present1.
    if (!s_loggedVTablePresent1Hit.exchange(true))
    {
        LYMALINK_LOG("[Direct3D11OverlayLayer][Hook_VTablePresent1] first vtable Present1 intercepted.");
    }
    RenderOverlay(swapChain, "VTablePresent1");

    // Avoid re-entering this layer's MinHook Present1 hook while forwarding.
    s_callingVTableOriginal = true;
    const HRESULT result = s_vtablePresent1 ? s_vtablePresent1(swapChain, syncInterval, flags, presentParameters) :
        (s_realPresent1 ? s_realPresent1(swapChain, syncInterval, flags, presentParameters) : DXGI_ERROR_INVALID_CALL);
    s_callingVTableOriginal = false;
    return result;
}

/////////////////////////////////////////////////////////////////////

void BeforeResizeBuffers(IDXGISwapChain* swapChain)
{
    std::lock_guard<std::mutex> lock(s_renderMutex);
    if (s_imguiReady && s_swapChain == swapChain)
    {
        // Backbuffer resources must be released before ResizeBuffers
        ReleaseSwapChainResources();
        ImGui_ImplDX11_InvalidateDeviceObjects();
    }
}

/////////////////////////////////////////////////////////////////////

void AfterResizeBuffers(IDXGISwapChain* swapChain, HRESULT result)
{
    std::lock_guard<std::mutex> lock(s_renderMutex);
    if (SUCCEEDED(result) && s_imguiReady && s_swapChain == swapChain)
    {
        // Recreate ImGui device objects after successful ResizeBuffers
        if (!ImGui_ImplDX11_CreateDeviceObjects())
        {
            LYMALINK_LOG("[Direct3D11OverlayLayer][AfterResizeBuffers] ImGui_ImplDX11_CreateDeviceObjects failed.");
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
        LYMALINK_LOG(std::string("[Direct3D11OverlayLayer][CreateHook] missing target ") + name);
        return;
    }

    MH_STATUS status = MH_CreateHook(target, detour, original);
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED)
    {
        LYMALINK_LOG("[Direct3D11OverlayLayer][CreateHook] " + HookStatus(name, status));
        return;
    }

    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED)
    {
        LYMALINK_LOG("[Direct3D11OverlayLayer][EnableHook] " + HookStatus(name, status));
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
    constexpr const wchar_t* className = L"LymalinkDx11DummyWindow";
    if (!RegisterDummyWindowClass(instance, className))
    {
        LYMALINK_LOG("[Direct3D11OverlayLayer][CreateDummyWindow] RegisterClassExW failed error=" + std::to_string(GetLastError()));
        return nullptr;
    }

    HWND window = CreateWindowExW(
        0,
        className,
        L"Lymalink DX11 Dummy",
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
        LYMALINK_LOG("[Direct3D11OverlayLayer][CreateDummyWindow] CreateWindowExW failed error=" + std::to_string(GetLastError()));
    }
    return window;
}

/////////////////////////////////////////////////////////////////////

bool CreateDummyDeviceAndSwapChain(
    PFN_D3D11CreateDeviceAndSwapChain createDeviceAndSwapChain,
    HWND window,
    IDXGISwapChain** swapChain,
    ID3D11Device** device,
    ID3D11DeviceContext** context)
{
    // Need factory function, hidden window, and output storage
    if (!createDeviceAndSwapChain || !window || !swapChain || !device || !context)
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

    const D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    const D3D_DRIVER_TYPE driverTypes[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE
    };

    HRESULT result = DXGI_ERROR_INVALID_CALL;
    for (D3D_DRIVER_TYPE driverType : driverTypes)
    {
        // Try hardware first, then software fallbacks
        result = createDeviceAndSwapChain(
            nullptr,
            driverType,
            nullptr,
            0,
            featureLevels,
            static_cast<UINT>(sizeof(featureLevels) / sizeof(featureLevels[0])),
            D3D11_SDK_VERSION,
            &desc,
            swapChain,
            device,
            nullptr,
            context);
        if (SUCCEEDED(result))
        {
            return true;
        }
        *swapChain = nullptr;
        *device = nullptr;
        *context = nullptr;
    }

    LYMALINK_LOG("[Direct3D11OverlayLayer][CreateDummyDeviceAndSwapChain] failed hr=" + std::to_string(result));
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
    // Load d3d11.dll if target process has not loaded it yet
    HMODULE d3d11 = GetModuleHandleW(L"d3d11.dll");
    if (!d3d11)
    {
        d3d11 = LoadLibraryW(L"d3d11.dll");
    }
    if (!d3d11)
    {
        LYMALINK_LOG("[Direct3D11OverlayLayer][InitThread] d3d11.dll load failed error=" + std::to_string(GetLastError()));
        return 1;
    }

    // D3D11CreateDeviceAndSwapChain is required for hook discovery
    // Dummy swap chain exposes vtable entries for MinHook targets
    auto createDeviceAndSwapChain = reinterpret_cast<PFN_D3D11CreateDeviceAndSwapChain>(GetProcAddress(d3d11, "D3D11CreateDeviceAndSwapChain"));
    if (!createDeviceAndSwapChain)
    {
        LYMALINK_LOG("[Direct3D11OverlayLayer][InitThread] D3D11CreateDeviceAndSwapChain missing.");
        return 1;
    }

    // Hidden window backs the temporary dummy swap chain
    HWND window = CreateDummyWindow();
    if (!window)
    {
        return 1;
    }

    // Build dummy D3D11 device/swap chain so its vtable can be hooked
    IDXGISwapChain* swapChain = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    if (!CreateDummyDeviceAndSwapChain(createDeviceAndSwapChain, window, &swapChain, &device, &context))
    {
        ReleaseCom(context);
        ReleaseCom(device);
        ReleaseCom(swapChain);
        DestroyWindow(window);
        return 1;
    }

    // MinHook must be initialized before creating detours
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
    {
        LYMALINK_LOG("[Direct3D11OverlayLayer][InitThread] " + HookStatus("MH_Initialize", status));
        ReleaseCom(context);
        ReleaseCom(device);
        ReleaseCom(swapChain);
        DestroyWindow(window);
        return 1;
    }

    void** table = VTable(swapChain);
    // Hook swap-chain calls used by DX11 presentation and resize
    CreateHook(table[VTable_Present], reinterpret_cast<void*>(&Hook_Present), reinterpret_cast<void**>(&s_realPresent), "IDXGISwapChain::Present");
    CreateHook(table[VTable_ResizeBuffers], reinterpret_cast<void*>(&Hook_ResizeBuffers), reinterpret_cast<void**>(&s_realResizeBuffers), "IDXGISwapChain::ResizeBuffers");

    // Present1 exists on newer DXGI swap chains
    IDXGISwapChain1* swapChain1 = nullptr;
    if (SUCCEEDED(swapChain->QueryInterface(__uuidof(IDXGISwapChain1), reinterpret_cast<void**>(&swapChain1))) && swapChain1)
    {
        void** table1 = VTable(swapChain1);
        CreateHook(table1[VTable_Present1], reinterpret_cast<void*>(&Hook_Present1), reinterpret_cast<void**>(&s_realPresent1), "IDXGISwapChain1::Present1");
        ReleaseCom(swapChain1);
    }

    // Dummy objects are no longer needed once hooks are installed
    ReleaseCom(context);
    ReleaseCom(device);
    ReleaseCom(swapChain);
    DestroyWindow(window);

    // Mark install complete for detach cleanup
    s_hooksReady.store(true);
    LYMALINK_LOG("[Direct3D11OverlayLayer][InitThread] hooks installed.");
    return 0;
}
}

/////////////////////////////////////////////////////////////////////
// Called from WinOverlayEntrypoint.cpp DllMain when this target defines
// LYMALINK_OVERLAY_ATTACH_HOOKS.
/////////////////////////////////////////////////////////////////////

// Export used by other DX overlay DLLs to attach this DX11 layer to a real swap chain.
extern "C" __declspec(dllexport) BOOL WINAPI LymalinkDirect3D11InstallSwapChainHook(IUnknown* swapChainObject)
{
    if (s_shuttingDown.load() || !swapChainObject)
    {
        return FALSE;
    }

    IDXGISwapChain* swapChain = nullptr;
    if (FAILED(swapChainObject->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&swapChain))) || !swapChain)
    {
        return FALSE;
    }

    ID3D11Device* device = nullptr;
    if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device))) || !device)
    {
        ReleaseCom(swapChain);
        return FALSE;
    }
    ReleaseCom(device);

    std::lock_guard<std::mutex> lock(s_renderMutex);
    const bool installed = InstallSwapChainVTableHookLocked(swapChain);
    ReleaseCom(swapChain);
    return installed ? TRUE : FALSE;
}

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
        LYMALINK_LOG("[Direct3D11OverlayLayer][Attach] CreateThread failed error=" + std::to_string(GetLastError()));
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
