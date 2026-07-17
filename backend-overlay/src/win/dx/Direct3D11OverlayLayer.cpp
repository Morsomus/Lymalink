/////////////////////////////////////////////////////////
// File: Direct3D11OverlayLayer.cpp
// Date: 2026-07-05
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Windows Direct3D 11 overlay renderer driven
//              by shared DXGI coordinator and Dear ImGui.
/////////////////////////////////////////////////////////

#ifndef NOMINMAX
    #define NOMINMAX
#endif

#include "FontEmbedded.h"
#include "OverlaySharedMemoryState.h"
#include "WinLogger.h"
#include "WinOverlayReceiver.h"
#include "WinDxgiOverlayCoordinator.h"
#include "WinDxgiOverlayRouter.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_4.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace
{
// One receiver and one ImGui context are shared by every DX11 hook in this process
static WinOverlayReceiver s_overlay;
static std::mutex s_renderMutex;
static std::atomic_bool s_shuttingDown{false};
static thread_local bool s_rendering = false;   // Prevents recursive interception while ImGui renders

static IDXGISwapChain* s_swapChain = nullptr;
static ID3D11Device* s_device = nullptr;
static ID3D11DeviceContext* s_context = nullptr;
static ID3D11RenderTargetView* s_renderTargetView = nullptr;
static ID3D11ShaderResourceView* s_iconView = nullptr;
static ID3D11Texture2D* s_iconTexture = nullptr;
static bool s_imguiReady = false;
static uint64_t s_iconGeneration = 0;

static std::atomic_bool s_loggedRoutedSwapChain{false};

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

bool RenderOverlay(IDXGISwapChain* swapChain, const char* presentPath)
{
    // Present and Present1 are the main DX11 render paths
    if (s_rendering || s_shuttingDown.load() || !swapChain)
    {
        return false;
    }

    // DX11 hooks can intercept shared DXGI swap chains - Non-DX11 chains are pass-through
    const WinDxgiOverlayRouter::Renderer renderer = WinDxgiOverlayRouter::DetectSwapChainRenderer(swapChain);
    if (renderer != WinDxgiOverlayRouter::Renderer::Direct3D11)
    {
        if (renderer != WinDxgiOverlayRouter::Renderer::Unknown && !s_loggedRoutedSwapChain.exchange(true))
        {
            LYMALINK_LOG("[Direct3D11OverlayLayer][RenderOverlay] pass-through non-DX11 swap chain.");
        }
        return false;
    }

    // Resolve the D3D11 device from the intercepted swap chain
    ID3D11Device* device = nullptr;
    if (FAILED(swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device))) || !device)
    {
        return false;
    }

    ID3D11DeviceContext* context = nullptr;
    device->GetImmediateContext(&context);
    if (!context)
    {
        ReleaseCom(device);
        return true;
    }

    // Need real framebuffer size before creating ImGui frame
    uint32_t width = 0;
    uint32_t height = 0;
    if (!QueryFramebufferSize(swapChain, width, height) || width == 0 || height == 0)
    {
        ReleaseCom(context);
        ReleaseCom(device);
        return true;
    }

    std::lock_guard<std::mutex> lock(s_renderMutex);
    s_rendering = true;

    if (InitImGuiLocked(swapChain, device, context, width, height) && EnsureRenderTargetLocked(swapChain, device))
    {
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
    return true;
}

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

DWORD WINAPI InitThread(LPVOID)
{
    // Do not force D3D11 into non-DX11 processes - Watch briefly for late runtime loads
    HMODULE d3d11 = nullptr;
    constexpr int MaxRuntimeWaitMs = 30000;
    constexpr int RuntimePollMs = 100;
    for (int waitedMs = 0; waitedMs <= MaxRuntimeWaitMs && !s_shuttingDown.load(); waitedMs += RuntimePollMs)
    {
        d3d11 = GetModuleHandleW(L"d3d11.dll");
        if (d3d11)
        {
            break;
        }
        Sleep(RuntimePollMs);
    }
    LYMALINK_LOG(std::string("[Direct3D11OverlayLayer][Identity] I am DX11; process uses DX11=") + (d3d11 ? "yes; active" : "no; inactive"));
    if (s_shuttingDown.load())
    {
        return 1;
    }
    if (!d3d11)
    {
        LYMALINK_LOG("[Direct3D11OverlayLayer][InitThread] d3d11.dll not loaded; layer inactive.");
        return 1;
    }

    if (!WinDxgiOverlayCoordinator::Start())
    {
        LYMALINK_LOG("[Direct3D11OverlayLayer][InitThread] DXGI coordinator unavailable.");
        return 1;
    }

    LYMALINK_LOG("[Direct3D11OverlayLayer][InitThread] renderer ready; DXGI coordinator active.");
    return 0;
}
}

/////////////////////////////////////////////////////////////////////
// Called from WinOverlayEntrypoint.cpp DllMain when this target defines
// LYMALINK_OVERLAY_ATTACH_HOOKS.
/////////////////////////////////////////////////////////////////////

extern "C" __declspec(dllexport) BOOL WINAPI LymalinkDirect3D11RenderSwapChain(IUnknown* swapChainObject, const char* presentPath)
{
    if (s_shuttingDown.load() || !swapChainObject || !presentPath)
    {
        return FALSE;
    }

    IDXGISwapChain* swapChain = nullptr;
    if (FAILED(swapChainObject->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&swapChain))) || !swapChain)
    {
        return FALSE;
    }

    const bool rendered = RenderOverlay(swapChain, presentPath);
    ReleaseCom(swapChain);
    return rendered ? TRUE : FALSE;
}

/////////////////////////////////////////////////////////////////////

extern "C" __declspec(dllexport) BOOL WINAPI LymalinkDirect3D11BeforeResize(IUnknown* swapChainObject)
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

    BeforeResizeBuffers(swapChain);
    ReleaseCom(swapChain);
    return TRUE;
}

/////////////////////////////////////////////////////////////////////

extern "C" __declspec(dllexport) HRESULT WINAPI LymalinkDirect3D11AfterResize(IUnknown* swapChainObject, const char* apiName, HRESULT result, IUnknown* const* presentQueue)
{
    (void)apiName;
    (void)presentQueue;
    if (s_shuttingDown.load() || !swapChainObject)
    {
        return result;
    }

    IDXGISwapChain* swapChain = nullptr;
    if (FAILED(swapChainObject->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&swapChain))) || !swapChain)
    {
        return result;
    }

    AfterResizeBuffers(swapChain, result);
    ReleaseCom(swapChain);
    return result;
}

/////////////////////////////////////////////////////////////////////

// Export for ABI compatibility; coordinator uses render/resize exports for routing
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

    ReleaseCom(swapChain);
    return TRUE;
}

/////////////////////////////////////////////////////////////////////

extern "C" void LymalinkOverlayOnProcessAttach(HINSTANCE instance)
{
    // Keep attach lightweight; hook setup runs on worker thread
    LYMALINK_LOG("[Direct3D11OverlayLayer][Identity] I am DX11; process attach.");
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
    WinDxgiOverlayCoordinator::Shutdown();
    {
        std::lock_guard<std::mutex> lock(s_renderMutex);
        ShutdownImGuiLocked();
    }
    s_overlay.Shutdown();
}
