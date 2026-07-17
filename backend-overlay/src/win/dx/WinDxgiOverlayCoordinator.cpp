/////////////////////////////////////////////////////////
// File: WinDxgiOverlayCoordinator.cpp
// Date: 2026-07-17
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Owns one DXGI swap-chain hook chain per
//              process and routes frames to the active
//              D3D10/11/12 renderer DLL.
/////////////////////////////////////////////////////////

#ifndef NOMINMAX
    #define NOMINMAX
#endif

#include "WinDxgiOverlayCoordinator.h"

#include "WinDxgiOverlayRouter.h"
#include "WinLogger.h"
#include "MinHook.h"

#include <windows.h>
#include <d3d10_1.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

namespace
{
// Function pointer types for the DXGI methods owned by this coordinator
// Saved originals are the path back into the game/driver after our hook runs
using PFN_Present = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
using PFN_Present1 = HRESULT(WINAPI*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
using PFN_ResizeBuffers = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using PFN_ResizeBuffers1 = HRESULT(WINAPI*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT*, IUnknown* const*);
using PFN_D3D10CreateDeviceAndSwapChain = HRESULT(WINAPI*)(
    IDXGIAdapter*,
    D3D10_DRIVER_TYPE,
    HMODULE,
    UINT,
    UINT,
    DXGI_SWAP_CHAIN_DESC*,
    IDXGISwapChain**,
    ID3D10Device**);
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
using PFN_D3D12CreateDevice = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
using PFN_CreateDXGIFactory2 = HRESULT(WINAPI*)(UINT, REFIID, void**);

constexpr int VTable_Present = 8;
constexpr int VTable_ResizeBuffers = 13;
constexpr int VTable_Present1 = 22;
constexpr int VTable_ResizeBuffers1 = 39;

// Original DXGI functions returned by MinHook - Coordinator hooks call these after routing to the selected renderer backend
static PFN_Present s_realPresent = nullptr;
static PFN_Present1 s_realPresent1 = nullptr;
static PFN_ResizeBuffers s_realResizeBuffers = nullptr;
static PFN_ResizeBuffers1 s_realResizeBuffers1 = nullptr;

// Named event elects one DX DLL as the shared DXGI hook owner - Other injected DX DLLs compile this code too, but become observers when the event already exists
static HANDLE s_ownerEvent = nullptr;
static std::atomic_bool s_hooksReady{false};
static bool s_ownsMinHook = false;

// One-shot log guards
static std::atomic_bool s_loggedOwner{false};
static std::atomic_bool s_loggedObserver{false};
static std::atomic_bool s_loggedUnknownRenderer{false};
static std::atomic_bool s_loggedMissingDx10{false};
static std::atomic_bool s_loggedMissingDx11{false};
static std::atomic_bool s_loggedMissingDx12{false};
static std::atomic_bool s_loggedRouteDx10{false};
static std::atomic_bool s_loggedRouteDx11{false};
static std::atomic_bool s_loggedRouteDx12{false};

/////////////////////////////////////////////////////////////////////

std::string HookStatus(const char* name, MH_STATUS status)
{
    return std::string(name) + " status=" + std::to_string(static_cast<int>(status));
}

/////////////////////////////////////////////////////////////////////

template <typename T>
void ReleaseCom(T*& value)
{
    if (value)
    {
        value->Release();
        value = nullptr;
    }
}

/////////////////////////////////////////////////////////////////////

bool QueryReadableMemory(uintptr_t address, MEMORY_BASIC_INFORMATION& info)
{
    if (VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) == 0)
    {
        return false;
    }
    if (info.State != MEM_COMMIT)
    {
        return false;
    }
    if ((info.Protect & PAGE_GUARD) || (info.Protect & PAGE_NOACCESS))
    {
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////

bool IsReadableExecutableAddress(uintptr_t address)
{
    MEMORY_BASIC_INFORMATION info{};
    if (!QueryReadableMemory(address, info))
    {
        return false;
    }
    constexpr DWORD executable = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (info.Protect & executable) != 0;
}

/////////////////////////////////////////////////////////////////////

template <typename T>
bool ReadValue(uintptr_t address, T& value)
{
    MEMORY_BASIC_INFORMATION info{};
    if (!QueryReadableMemory(address, info))
    {
        return false;
    }
    const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    if (address + sizeof(T) > regionEnd)
    {
        return false;
    }
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
    return true;
}

/////////////////////////////////////////////////////////////////////

bool TryResolveJumpTarget(uintptr_t address, uintptr_t& target)
{
    // Follow common jump stubs already placed by RTSS/Steam/other overlays - Hooking the resolved destination to keep their hook chain intact
    uint8_t bytes[14]{};
    if (!ReadValue(address, bytes))
    {
        return false;
    }

    if (bytes[0] == 0xE9)
    {
        int32_t relative = 0;
        std::memcpy(&relative, bytes + 1, sizeof(relative));
        target = address + 5 + relative;
        return target != address;
    }

    if (bytes[0] == 0xEB)
    {
        const int8_t relative = static_cast<int8_t>(bytes[1]);
        target = address + 2 + relative;
        return target != address;
    }

#if defined(_WIN64)
    if (bytes[0] == 0xFF && bytes[1] == 0x25)
    {
        int32_t relative = 0;
        std::memcpy(&relative, bytes + 2, sizeof(relative));
        const uintptr_t pointerAddress = address + 6 + relative;
        uintptr_t resolved = 0;
        if (!ReadValue(pointerAddress, resolved))
        {
            return false;
        }
        target = resolved;
        return target != address;
    }

    if (bytes[0] == 0x48 && bytes[1] == 0xFF && bytes[2] == 0x25)
    {
        int32_t relative = 0;
        std::memcpy(&relative, bytes + 3, sizeof(relative));
        const uintptr_t pointerAddress = address + 7 + relative;
        uintptr_t resolved = 0;
        if (!ReadValue(pointerAddress, resolved))
        {
            return false;
        }
        target = resolved;
        return target != address;
    }

    if (bytes[0] == 0x48 && bytes[1] == 0xB8 && bytes[10] == 0xFF && bytes[11] == 0xE0)
    {
        std::memcpy(&target, bytes + 2, sizeof(target));
        return target != address;
    }
#else
    if (bytes[0] == 0xFF && bytes[1] == 0x25)
    {
        uint32_t pointerAddress = 0;
        std::memcpy(&pointerAddress, bytes + 2, sizeof(pointerAddress));
        uint32_t resolved = 0;
        if (!ReadValue(static_cast<uintptr_t>(pointerAddress), resolved))
        {
            return false;
        }
        target = static_cast<uintptr_t>(resolved);
        return target != address;
    }
#endif

    return false;
}

/////////////////////////////////////////////////////////////////////

void* ResolveExistingHookTarget(void* target, const char* name)
{
    // MinHook is installed after following a short chain of existing jumps - Prevent replacing a third-party overlay's front stub directly
    uintptr_t current = reinterpret_cast<uintptr_t>(target);
    uintptr_t next = 0;
    int jumpsFollowed = 0;
    constexpr int MaxJumpsToFollow = 16;
    while (jumpsFollowed < MaxJumpsToFollow && TryResolveJumpTarget(current, next))
    {
        if (!IsReadableExecutableAddress(next))
        {
            break;
        }
        current = next;
        ++jumpsFollowed;
    }

    if (jumpsFollowed > 0)
    {
        LYMALINK_LOG("[WinDxgiOverlayCoordinator][CreateHook] followed existing jump chain for " + std::string(name) + " hops=" + std::to_string(jumpsFollowed));
    }

    return reinterpret_cast<void*>(current);
}

/////////////////////////////////////////////////////////////////////

bool CreateHook(void* target, void* detour, void** original, const char* name)
{
    // All DX10/11/12 DLLs may try to start coordinator, but only event owner reaches here
    // MinHook target is process-wide DXGI code
    if (!target)
    {
        LYMALINK_LOG(std::string("[WinDxgiOverlayCoordinator][CreateHook] missing target ") + name);
        return false;
    }

    void* resolvedTarget = ResolveExistingHookTarget(target, name);
    if (!resolvedTarget)
    {
        LYMALINK_LOG(std::string("[WinDxgiOverlayCoordinator][CreateHook] unresolved target ") + name);
        return false;
    }

    MH_STATUS status = MH_CreateHook(resolvedTarget, detour, original);
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED)
    {
        LYMALINK_LOG("[WinDxgiOverlayCoordinator][CreateHook] " + HookStatus(name, status));
        return false;
    }
    if (status == MH_ERROR_ALREADY_CREATED && (!original || !*original))
    {
        LYMALINK_LOG(std::string("[WinDxgiOverlayCoordinator][CreateHook] already created without original trampoline for ") + name);
        return false;
    }

    status = MH_EnableHook(resolvedTarget);
    if (status != MH_OK && status != MH_ERROR_ENABLED)
    {
        LYMALINK_LOG("[WinDxgiOverlayCoordinator][EnableHook] " + HookStatus(name, status));
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////

bool RegisterDummyWindowClass(HINSTANCE instance, const wchar_t* className)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = instance;
    wc.lpszClassName = className;
    if (RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS)
    {
        return true;
    }
    LYMALINK_LOG("[WinDxgiOverlayCoordinator][CreateDummyWindow] RegisterClassExW failed error=" + std::to_string(GetLastError()));
    return false;
}

/////////////////////////////////////////////////////////////////////

HWND CreateDummyWindow()
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* className = L"LymalinkDxgiCoordinatorDummyWindow";
    if (!RegisterDummyWindowClass(instance, className))
    {
        return nullptr;
    }

    HWND window = CreateWindowExW(0, className, L"Lymalink DXGI Coordinator", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, instance, nullptr);
    if (!window)
    {
        LYMALINK_LOG("[WinDxgiOverlayCoordinator][CreateDummyWindow] CreateWindowExW failed error=" + std::to_string(GetLastError()));
    }
    return window;
}

/////////////////////////////////////////////////////////////////////

DXGI_SWAP_CHAIN_DESC DummySwapChainDesc(HWND window)
{
    // Dummy swap chain is used to read stable DXGI vtable method addresses
    // It is destroyed before Start returns and does not become the game swap chain
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width = 64;
    desc.BufferDesc.Height = 64;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.OutputWindow = window;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    return desc;
}

/////////////////////////////////////////////////////////////////////

bool CreateD3D10DummySwapChain(HWND window, IDXGISwapChain** swapChain)
{
    // Do not LoadLibrary - If game/runtime has not loaded D3D10, this backend cannot own startup
    HMODULE d3d10 = GetModuleHandleW(L"d3d10.dll");
    if (!d3d10)
    {
        return false;
    }

    auto createDeviceAndSwapChain = reinterpret_cast<PFN_D3D10CreateDeviceAndSwapChain>(GetProcAddress(d3d10, "D3D10CreateDeviceAndSwapChain"));
    if (!createDeviceAndSwapChain)
    {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC desc = DummySwapChainDesc(window);
    ID3D10Device* device = nullptr;
    HRESULT result = createDeviceAndSwapChain(nullptr, D3D10_DRIVER_TYPE_HARDWARE, nullptr, 0, D3D10_SDK_VERSION, &desc, swapChain, &device);
    if (FAILED(result) || !*swapChain)
    {
        ReleaseCom(device);
        ReleaseCom(*swapChain);
        result = createDeviceAndSwapChain(nullptr, D3D10_DRIVER_TYPE_WARP, nullptr, 0, D3D10_SDK_VERSION, &desc, swapChain, &device);
    }
    ReleaseCom(device);
    return SUCCEEDED(result) && *swapChain;
}

/////////////////////////////////////////////////////////////////////

bool CreateD3D11DummySwapChain(HWND window, IDXGISwapChain** swapChain)
{
    // Prefer DX11 dummy first because most DXGI swap-chain tables are available through it
    // Fallbacks below cover processes where only DX10 or DX12 is currently loaded
    HMODULE d3d11 = GetModuleHandleW(L"d3d11.dll");
    if (!d3d11)
    {
        return false;
    }

    auto createDeviceAndSwapChain = reinterpret_cast<PFN_D3D11CreateDeviceAndSwapChain>(
        GetProcAddress(d3d11, "D3D11CreateDeviceAndSwapChain"));
    if (!createDeviceAndSwapChain)
    {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC desc = DummySwapChainDesc(window);
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL createdLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT result = createDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, &desc, swapChain, &device, &createdLevel, &context);
    if (FAILED(result) || !*swapChain)
    {
        ReleaseCom(context);
        ReleaseCom(device);
        ReleaseCom(*swapChain);
        result = createDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, &desc, swapChain, &device, &createdLevel, &context);
    }
    ReleaseCom(context);
    ReleaseCom(device);
    return SUCCEEDED(result) && *swapChain;
}

/////////////////////////////////////////////////////////////////////

bool CreateD3D12DummySwapChain(HWND window, IDXGISwapChain** swapChain)
{
    // DX12 dummy needs both d3d12.dll and dxgi.dll already loaded
    // It creates a temporary queue only for factory swap-chain creation
    HMODULE d3d12 = GetModuleHandleW(L"d3d12.dll");
    HMODULE dxgi = GetModuleHandleW(L"dxgi.dll");
    if (!d3d12 || !dxgi)
    {
        return false;
    }

    auto createDevice = reinterpret_cast<PFN_D3D12CreateDevice>(GetProcAddress(d3d12, "D3D12CreateDevice"));
    auto createFactory = reinterpret_cast<PFN_CreateDXGIFactory2>(GetProcAddress(dxgi, "CreateDXGIFactory2"));
    if (!createDevice || !createFactory)
    {
        return false;
    }

    ID3D12Device* device = nullptr;
    ID3D12CommandQueue* queue = nullptr;
    IDXGIFactory2* factory = nullptr;
    IDXGISwapChain1* swapChain1 = nullptr;

    HRESULT result = createDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), reinterpret_cast<void**>(&device));
    if (FAILED(result) || !device)
    {
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    result = device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&queue));
    if (FAILED(result) || !queue)
    {
        ReleaseCom(device);
        return false;
    }

    result = createFactory(0, __uuidof(IDXGIFactory2), reinterpret_cast<void**>(&factory));
    if (FAILED(result) || !factory)
    {
        ReleaseCom(queue);
        ReleaseCom(device);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = 64;
    desc.Height = 64;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SampleDesc.Count = 1;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    result = factory->CreateSwapChainForHwnd(queue, window, &desc, nullptr, nullptr, &swapChain1);
    if (SUCCEEDED(result) && swapChain1)
    {
        result = swapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(swapChain));
    }

    ReleaseCom(swapChain1);
    ReleaseCom(factory);
    ReleaseCom(queue);
    ReleaseCom(device);
    return SUCCEEDED(result) && *swapChain;
}

/////////////////////////////////////////////////////////////////////

bool CreateDummySwapChain(IDXGISwapChain** swapChain)
{
    // Any loaded D3D10/11/12 runtime can provide DXGI vtable addresses - Start only needs one successful dummy chain to install shared hooks
    HWND window = CreateDummyWindow();
    if (!window)
    {
        return false;
    }

    bool created = CreateD3D11DummySwapChain(window, swapChain) || CreateD3D10DummySwapChain(window, swapChain) || CreateD3D12DummySwapChain(window, swapChain);
    DestroyWindow(window);
    return created;
}

/////////////////////////////////////////////////////////////////////

void** VTable(void* object)
{
    return object ? *reinterpret_cast<void***>(object) : nullptr;
}

/////////////////////////////////////////////////////////////////////

std::wstring OwnerEventName()
{
    // Event is per target process, not global to all games - Local namespace avoids cross-session collisions
    return L"Local\\LymalinkDxgiOverlayCoordinator-" + std::to_wstring(GetCurrentProcessId());
}

/////////////////////////////////////////////////////////////////////

std::atomic_bool& MissingLogFlag(WinDxgiOverlayRouter::Renderer renderer)
{
    switch (renderer)
    {
        case WinDxgiOverlayRouter::Renderer::Direct3D10:
            return s_loggedMissingDx10;
        case WinDxgiOverlayRouter::Renderer::Direct3D11:
            return s_loggedMissingDx11;
        default:
            return s_loggedMissingDx12;
    }
}

/////////////////////////////////////////////////////////////////////

std::atomic_bool& RouteLogFlag(WinDxgiOverlayRouter::Renderer renderer)
{
    switch (renderer)
    {
        case WinDxgiOverlayRouter::Renderer::Direct3D10:
            return s_loggedRouteDx10;
        case WinDxgiOverlayRouter::Renderer::Direct3D11:
            return s_loggedRouteDx11;
        default:
            return s_loggedRouteDx12;
    }
}

/////////////////////////////////////////////////////////////////////

const char* RendererName(WinDxgiOverlayRouter::Renderer renderer)
{
    switch (renderer)
    {
        case WinDxgiOverlayRouter::Renderer::Direct3D10:
            return "dx10";
        case WinDxgiOverlayRouter::Renderer::Direct3D11:
            return "dx11";
        case WinDxgiOverlayRouter::Renderer::Direct3D12:
            return "dx12";
        default:
            return "unknown";
    }
}

/////////////////////////////////////////////////////////////////////

bool DispatchRender(IDXGISwapChain* swapChain, const char* presentPath)
{
    // Detect the real device behind the presented swap chain every frame, then call that renderer DLL
    // Loaded-but-wrong DX DLLs will not touch overlay shared memory
    const WinDxgiOverlayRouter::Renderer renderer = WinDxgiOverlayRouter::DetectSwapChainRenderer(swapChain);
    if (renderer == WinDxgiOverlayRouter::Renderer::Unknown)
    {
        if (!s_loggedUnknownRenderer.exchange(true))
        {
            LYMALINK_LOG("[WinDxgiOverlayCoordinator][Render] renderer unknown; pass-through.");
        }
        return false;
    }

    if (!WinDxgiOverlayRouter::RenderSwapChain(renderer, swapChain, presentPath))
    {
        if (!MissingLogFlag(renderer).exchange(true))
        {
            LYMALINK_LOG("[WinDxgiOverlayCoordinator][Render] backend missing renderer=" + std::string(RendererName(renderer)));
        }
        return false;
    }

    if (!RouteLogFlag(renderer).exchange(true))
    {
        LYMALINK_LOG("[WinDxgiOverlayCoordinator][Render] routed renderer=" + std::string(RendererName(renderer)) + " path=" + presentPath);
    }
    return true;
}

/////////////////////////////////////////////////////////////////////

HRESULT DispatchBeforeResize(IDXGISwapChain* swapChain)
{
    // Resize invalidates renderer-owned backbuffer resources
    // Capture renderer before real ResizeBuffers because device/swap-chain state may change during call
    const WinDxgiOverlayRouter::Renderer renderer = WinDxgiOverlayRouter::DetectSwapChainRenderer(swapChain);
    if (renderer == WinDxgiOverlayRouter::Renderer::Unknown)
    {
        return DXGI_ERROR_NOT_FOUND;
    }
    WinDxgiOverlayRouter::BeforeResize(renderer, swapChain);
    return static_cast<HRESULT>(renderer);
}

/////////////////////////////////////////////////////////////////////

HRESULT DispatchAfterResize(HRESULT rendererToken, IDXGISwapChain* swapChain, const char* apiName, HRESULT result, IUnknown* const* presentQueue)
{
    // Pair with DispatchBeforeResize - DX10/11 rebuild ImGui device objects - DX12 can also recover
    // DXGI_ERROR_INVALID_CALL and capture ResizeBuffers1 present queue
    if (rendererToken == DXGI_ERROR_NOT_FOUND)
    {
        return result;
    }
    const auto renderer = static_cast<WinDxgiOverlayRouter::Renderer>(rendererToken);
    if (renderer == WinDxgiOverlayRouter::Renderer::Unknown)
    {
        return result;
    }
    return WinDxgiOverlayRouter::AfterResize(renderer, swapChain, apiName, result, presentQueue);
}

/////////////////////////////////////////////////////////////////////

HRESULT WINAPI Hook_Present(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
{
    // Present path: draw overlay first, then pass control to original DXGI Present
    DispatchRender(swapChain, "Present");
    return s_realPresent ? s_realPresent(swapChain, syncInterval, flags) : DXGI_ERROR_INVALID_CALL;
}

/////////////////////////////////////////////////////////////////////

HRESULT WINAPI Hook_Present1(IDXGISwapChain1* swapChain, UINT syncInterval, UINT flags, const DXGI_PRESENT_PARAMETERS* presentParameters)
{
    // Present1 path is used by newer flip-model swap chains
    DispatchRender(swapChain, "Present1");
    return s_realPresent1 ? s_realPresent1(swapChain, syncInterval, flags, presentParameters) : DXGI_ERROR_INVALID_CALL;
}

/////////////////////////////////////////////////////////////////////

HRESULT WINAPI Hook_ResizeBuffers(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags)
{
    // Notify backend before/after the real resize so renderer resources follow swap-chain lifetime
    const HRESULT rendererToken = DispatchBeforeResize(swapChain);
    const HRESULT result = s_realResizeBuffers ? s_realResizeBuffers(swapChain, bufferCount, width, height, newFormat, swapChainFlags) : DXGI_ERROR_INVALID_CALL;
    return DispatchAfterResize(rendererToken, swapChain, "ResizeBuffers", result, nullptr);
}

/////////////////////////////////////////////////////////////////////

HRESULT WINAPI Hook_ResizeBuffers1(IDXGISwapChain3* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags, const UINT* creationNodeMask, IUnknown* const* presentQueue)
{
    // ResizeBuffers1 can pass a new present queue - DX12 backend uses it after successful resize
    const HRESULT rendererToken = DispatchBeforeResize(swapChain);
    const HRESULT result = s_realResizeBuffers1 ? s_realResizeBuffers1(swapChain, bufferCount, width, height, newFormat, swapChainFlags, creationNodeMask, presentQueue) : DXGI_ERROR_INVALID_CALL;
    return DispatchAfterResize(rendererToken, swapChain, "ResizeBuffers1", result, presentQueue);
}
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool WinDxgiOverlayCoordinator::Start()
{
    // Called by every active DX10/11/12 DLL - Named event lets only the first active DLL install hooks
    if (s_hooksReady.load())
    {
        return true;
    }

    IDXGISwapChain* swapChain = nullptr;
    if (!CreateDummySwapChain(&swapChain))
    {
        LYMALINK_LOG("[WinDxgiOverlayCoordinator][Start] dummy swap chain unavailable.");
        return false;
    }

    HANDLE ownerEvent = CreateEventW(nullptr, TRUE, TRUE, OwnerEventName().c_str());
    const DWORD createError = GetLastError();
    if (!ownerEvent)
    {
        LYMALINK_LOG("[WinDxgiOverlayCoordinator][Start] CreateEventW failed error=" + std::to_string(createError));
        ReleaseCom(swapChain);
        return false;
    }
    if (createError == ERROR_ALREADY_EXISTS)
    {
        // Another DX DLL already owns DXGI hooks - This DLL still exports renderer callbacks for routing
        if (!s_loggedObserver.exchange(true))
        {
            LYMALINK_LOG("[WinDxgiOverlayCoordinator][Start] existing owner; this DLL is observer.");
        }
        CloseHandle(ownerEvent);
        ReleaseCom(swapChain);
        return true;
    }
    s_ownerEvent = ownerEvent;

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
    {
        LYMALINK_LOG("[WinDxgiOverlayCoordinator][Start] " + HookStatus("MH_Initialize", status));
        CloseHandle(s_ownerEvent);
        s_ownerEvent = nullptr;
        ReleaseCom(swapChain);
        return false;
    }
    s_ownsMinHook = status == MH_OK;

    void** table = VTable(swapChain);
    if (!table)
    {
        LYMALINK_LOG("[WinDxgiOverlayCoordinator][Start] swap-chain vtable missing.");
        Shutdown();
        ReleaseCom(swapChain);
        return false;
    }

    const bool presentReady = CreateHook(table[VTable_Present], reinterpret_cast<void*>(&Hook_Present), reinterpret_cast<void**>(&s_realPresent), "IDXGISwapChain::Present");
    const bool resizeReady = CreateHook(table[VTable_ResizeBuffers], reinterpret_cast<void*>(&Hook_ResizeBuffers), reinterpret_cast<void**>(&s_realResizeBuffers), "IDXGISwapChain::ResizeBuffers");

    // These methods may not exist on older DXGI objects - they are best-effort optional hooks
    IDXGISwapChain1* swapChain1 = nullptr;
    if (SUCCEEDED(swapChain->QueryInterface(__uuidof(IDXGISwapChain1), reinterpret_cast<void**>(&swapChain1))) && swapChain1)
    {
        void** table1 = VTable(swapChain1);
        if (table1)
        {
            CreateHook(table1[VTable_Present1], reinterpret_cast<void*>(&Hook_Present1), reinterpret_cast<void**>(&s_realPresent1), "IDXGISwapChain1::Present1");
        }
        ReleaseCom(swapChain1);
    }

    IDXGISwapChain3* swapChain3 = nullptr;
    if (SUCCEEDED(swapChain->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&swapChain3))) && swapChain3)
    {
        void** table3 = VTable(swapChain3);
        if (table3)
        {
            CreateHook(table3[VTable_ResizeBuffers1], reinterpret_cast<void*>(&Hook_ResizeBuffers1), reinterpret_cast<void**>(&s_realResizeBuffers1), "IDXGISwapChain3::ResizeBuffers1");
        }
        ReleaseCom(swapChain3);
    }

    ReleaseCom(swapChain);

    if (!presentReady || !resizeReady)
    {
        LYMALINK_LOG("[WinDxgiOverlayCoordinator][Start] required hooks incomplete present=" + std::to_string(presentReady ? 1 : 0) + " resize=" + std::to_string(resizeReady ? 1 : 0));
        Shutdown();
        return false;
    }

    s_hooksReady.store(true);
    if (!s_loggedOwner.exchange(true))
    {
        LYMALINK_LOG("[WinDxgiOverlayCoordinator][Start] hooks installed; owner pid=" + std::to_string(GetCurrentProcessId()));
    }
    return true;
}

/////////////////////////////////////////////////////////////////////

void WinDxgiOverlayCoordinator::Shutdown()
{
    // Only the owner has s_hooksReady/s_ownerEvent set - Observer DLLs call this safely as no-op
    if (s_hooksReady.load())
    {
        MH_DisableHook(MH_ALL_HOOKS);
        s_hooksReady.store(false);
    }
    if (s_ownsMinHook)
    {
        MH_Uninitialize();
        s_ownsMinHook = false;
    }
    if (s_ownerEvent)
    {
        CloseHandle(s_ownerEvent);
        s_ownerEvent = nullptr;
    }
}
