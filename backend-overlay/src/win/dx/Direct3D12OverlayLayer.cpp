/////////////////////////////////////////////////////////
// File: Direct3D12OverlayLayer.cpp
// Date: 2026-07-05
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Windows Direct3D 12 overlay hooks driven by
//              MinHook and Dear ImGui DX12 backend.
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
#include "imgui_impl_dx12.h"
#include "MinHook.h"

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace
{
// Function pointer types for DX12 queue/factory methods we intercept.
// DXGI Present/Resize hooks are owned by WinDxgiOverlayCoordinator.
using PFN_CreateSwapChain = HRESULT(WINAPI*)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
using PFN_CreateSwapChainForHwnd = HRESULT(WINAPI*)(IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
using PFN_CreateSwapChainForCoreWindow = HRESULT(WINAPI*)(IDXGIFactory2*, IUnknown*, IUnknown*, const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**);
using PFN_CreateSwapChainForComposition = HRESULT(WINAPI*)(IDXGIFactory2*, IUnknown*, const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**);
using PFN_ExecuteCommandLists = void(WINAPI*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
using PFN_D3D12CreateDevice = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
using PFN_CreateDXGIFactory2 = HRESULT(WINAPI*)(UINT, REFIID, void**);

// Known DXGI/D3D12 vtable slots used by the queue/factory hook setup
constexpr int VTable_CreateSwapChain = 10;
constexpr int VTable_CreateSwapChainForHwnd = 15;
constexpr int VTable_CreateSwapChainForCoreWindow = 16;
constexpr int VTable_CreateSwapChainForComposition = 24;
constexpr int VTable_ExecuteCommandLists = 10;

constexpr int ResizeFailureRecoveryPresentFrames = 180;
constexpr UINT MaxSwapChainBuffers = DXGI_MAX_SWAP_CHAIN_BUFFERS;
constexpr UINT SrvDescriptorCount = 64;

struct FrameContext
{
    ID3D12CommandAllocator* allocator = nullptr;
    ID3D12Resource* renderTarget = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
    UINT64 fenceValue = 0;
};

struct QueueMapping
{
    IUnknown* swapChainIdentity = nullptr;
    ID3D12CommandQueue* queue = nullptr;
};

struct DescriptorSlot
{
    UINT index = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
};

// Original queue/factory methods saved by MinHook
static PFN_CreateSwapChain s_realCreateSwapChain = nullptr;
static PFN_CreateSwapChainForHwnd s_realCreateSwapChainForHwnd = nullptr;
static PFN_CreateSwapChainForCoreWindow s_realCreateSwapChainForCoreWindow = nullptr;
static PFN_CreateSwapChainForComposition s_realCreateSwapChainForComposition = nullptr;
static PFN_ExecuteCommandLists s_realExecuteCommandLists = nullptr;

// One receiver and one ImGui context are shared by every DX12 hook in this process
static WinOverlayReceiver s_overlay;
static std::mutex s_renderMutex;
static std::mutex s_queueMutex;
static std::vector<QueueMapping> s_queueMappings;
static std::atomic_bool s_hooksReady{false};
static std::atomic_bool s_shuttingDown{false};
static std::atomic_int s_resizeFailureRecoveryFrames{0};
static thread_local bool s_rendering = false;   // Prevents recursive interception while ImGui renders
static bool s_ownsMinHook = false;

static IDXGISwapChain* s_swapChain = nullptr;
static ID3D12Device* s_device = nullptr;
static ID3D12CommandQueue* s_commandQueue = nullptr;
static ID3D12DescriptorHeap* s_rtvHeap = nullptr;
static ID3D12DescriptorHeap* s_srvHeap = nullptr;
static ID3D12GraphicsCommandList* s_commandList = nullptr;
static ID3D12Fence* s_fence = nullptr;
static HANDLE s_fenceEvent = nullptr;
static UINT64 s_nextFenceValue = 1;
static UINT s_rtvDescriptorSize = 0;
static UINT s_srvDescriptorSize = 0;
static UINT s_bufferCount = 0;
static UINT s_frameIndexFallback = 0;
static DXGI_FORMAT s_rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
static bool s_imguiReady = false;
static std::array<FrameContext, MaxSwapChainBuffers> s_frames{};
static std::array<bool, SrvDescriptorCount> s_srvSlots{};

static ID3D12Resource* s_iconTexture = nullptr;
static ID3D12Resource* s_iconUpload = nullptr;
static DescriptorSlot s_iconDescriptor{};
static bool s_iconDescriptorValid = false;
static uint64_t s_iconGeneration = 0;

static ID3D12CommandQueue* s_lastDirectQueue = nullptr;

static std::atomic_bool s_loggedQueueCapture{false};
static std::atomic_bool s_loggedFallbackQueue{false};
static std::atomic_bool s_loggedUsingFallbackQueue{false};
static std::atomic_bool s_loggedMissingQueue{false};
static std::atomic_bool s_loggedRoutedSwapChain{false};
static std::atomic_bool s_loggedResizeFailure{false};
static std::atomic_bool s_loggedResizeRecoveryWait{false};

/////////////////////////////////////////////////////////////////////

std::string HookStatus(const char* name, MH_STATUS status)
{
    // Keep MinHook logs compact and readable
    return std::string(name) + " status=" + std::to_string(static_cast<int>(status));
}

/////////////////////////////////////////////////////////////////////

bool QueryReadableMemory(uintptr_t address, MEMORY_BASIC_INFORMATION& info)
{
    // Validate the page before reading instruction bytes or indirect jump targets
    if (VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) == 0)
    {
        return false;
    }

    if (info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) != 0 || (info.Protect & PAGE_NOACCESS) != 0)
    {
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////

bool IsReadableAddress(uintptr_t address)
{
    // Data slots used by indirect jumps may be non-executable but still readable.
    MEMORY_BASIC_INFORMATION info{};
    return QueryReadableMemory(address, info);
}

/////////////////////////////////////////////////////////////////////

bool IsReadableExecutableAddress(uintptr_t address)
{
    // Final hook destinations must be executable code, not just readable memory
    MEMORY_BASIC_INFORMATION info{};
    if (!QueryReadableMemory(address, info))
    {
        return false;
    }

    const DWORD protect = info.Protect & 0xff;
    return protect == PAGE_EXECUTE ||
        protect == PAGE_EXECUTE_READ ||
        protect == PAGE_EXECUTE_READWRITE ||
        protect == PAGE_EXECUTE_WRITECOPY;
}

/////////////////////////////////////////////////////////////////////

template <typename T>
bool ReadValue(uintptr_t address, T& value)
{
    // Copy through memcpy so unaligned instruction/immediate reads stay well-defined
    if (!IsReadableAddress(address))
    {
        return false;
    }

    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
    return true;
}

/////////////////////////////////////////////////////////////////////

bool TryResolveJumpTarget(uintptr_t address, uintptr_t& target)
{
    // Resolve common JMP stubs so RTSS/MSI Afterburner-style hooks stay in front
    unsigned char bytes[14]{};
    if (!ReadValue(address, bytes))
    {
        return false;
    }

    // E9 rel32
    if (bytes[0] == 0xe9)
    {
        int32_t displacement = 0;
        std::memcpy(&displacement, bytes + 1, sizeof(displacement));
        target = address + 5 + displacement;
        return target != address;
    }

    // EB rel8
    if (bytes[0] == 0xeb)
    {
        const int8_t displacement = static_cast<int8_t>(bytes[1]);
        target = address + 2 + displacement;
        return target != address;
    }

#if defined(_WIN64)
    // FF 25 rel32: x64 absolute indirect jump via RIP-relative pointer
    if (bytes[0] == 0xff && bytes[1] == 0x25)
    {
        int32_t displacement = 0;
        std::memcpy(&displacement, bytes + 2, sizeof(displacement));
        const uintptr_t pointerAddress = address + 6 + displacement;
        return ReadValue(pointerAddress, target) && target != address;
    }

    // 48 FF 25 rel32: REX-prefixed absolute indirect jump
    if (bytes[0] == 0x48 && bytes[1] == 0xff && bytes[2] == 0x25)
    {
        int32_t displacement = 0;
        std::memcpy(&displacement, bytes + 3, sizeof(displacement));
        const uintptr_t pointerAddress = address + 7 + displacement;
        return ReadValue(pointerAddress, target) && target != address;
    }
#else
    // FF 25 imm32: x86 absolute indirect jump via absolute pointer
    if (bytes[0] == 0xff && bytes[1] == 0x25)
    {
        uint32_t pointerAddress = 0;
        std::memcpy(&pointerAddress, bytes + 2, sizeof(pointerAddress));
        uint32_t target32 = 0;
        if (!ReadValue(static_cast<uintptr_t>(pointerAddress), target32))
        {
            return false;
        }
        target = static_cast<uintptr_t>(target32);
        return target != address;
    }
#endif

    return false;
}

/////////////////////////////////////////////////////////////////////

void* ResolveExistingHookTarget(void* target, const char* name)
{
    // Follow a short chain of pre-existing jump stubs, then let MinHook hook the resolved executable destination
    // Keep RTSS/other-overlay chaining intact
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
        LYMALINK_LOG("[Direct3D12OverlayLayer][CreateHook] followed existing jump chain for " +
            std::string(name) + " hops=" + std::to_string(jumpsFollowed));
    }

    return reinterpret_cast<void*>(current);
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

void AddRefDevice(ID3D12Device* device)
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

void AddRefCommandQueue(ID3D12CommandQueue* queue)
{
    // Store the active direct queue with an owned reference
    if (queue)
    {
        queue->AddRef();
    }
    ReleaseCom(s_commandQueue);
    s_commandQueue = queue;
}

/////////////////////////////////////////////////////////////////////

void AddRefLastDirectQueue(ID3D12CommandQueue* queue)
{
    if (!queue)
    {
        return;
    }

    D3D12_COMMAND_QUEUE_DESC desc = queue->GetDesc();
    if (desc.Type != D3D12_COMMAND_LIST_TYPE_DIRECT)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(s_queueMutex);
    if (s_lastDirectQueue == queue)
    {
        return;
    }
    queue->AddRef();
    ReleaseCom(s_lastDirectQueue);
    s_lastDirectQueue = queue;
    if (!s_loggedFallbackQueue.exchange(true))
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][ExecuteCommandLists] captured fallback direct queue.");
    }
}

/////////////////////////////////////////////////////////////////////

IUnknown* QueryIdentity(IUnknown* object)
{
    if (!object)
    {
        return nullptr;
    }

    IUnknown* identity = nullptr;
    if (FAILED(object->QueryInterface(__uuidof(IUnknown), reinterpret_cast<void**>(&identity))))
    {
        return nullptr;
    }
    return identity;
}

/////////////////////////////////////////////////////////////////////

ID3D12CommandQueue* QueryDirectQueue(IUnknown* object)
{
    if (!object)
    {
        return nullptr;
    }

    ID3D12CommandQueue* queue = nullptr;
    if (FAILED(object->QueryInterface(__uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&queue))) || !queue)
    {
        return nullptr;
    }

    D3D12_COMMAND_QUEUE_DESC desc = queue->GetDesc();
    if (desc.Type != D3D12_COMMAND_LIST_TYPE_DIRECT)
    {
        ReleaseCom(queue);
        return nullptr;
    }

    return queue;
}

/////////////////////////////////////////////////////////////////////

void CaptureSwapChainQueue(IUnknown* swapChainObject, IUnknown* queueObject, const char* source)
{
    ID3D12CommandQueue* queue = QueryDirectQueue(queueObject);
    if (!queue)
    {
        return;
    }

    IUnknown* identity = QueryIdentity(swapChainObject);
    if (!identity)
    {
        ReleaseCom(queue);
        return;
    }

    std::lock_guard<std::mutex> lock(s_queueMutex);
    for (QueueMapping& mapping : s_queueMappings)
    {
        if (mapping.swapChainIdentity == identity)
        {
            ReleaseCom(mapping.queue);
            mapping.queue = queue;
            ReleaseCom(identity);
            if (!s_loggedQueueCapture.exchange(true))
            {
                LYMALINK_LOG(std::string("[Direct3D12OverlayLayer][CaptureSwapChainQueue] first queue captured from ") + source + ".");
            }
            return;
        }
    }

    s_queueMappings.push_back(QueueMapping{identity, queue});
    if (!s_loggedQueueCapture.exchange(true))
    {
        LYMALINK_LOG(std::string("[Direct3D12OverlayLayer][CaptureSwapChainQueue] first queue captured from ") + source + ".");
    }
}

/////////////////////////////////////////////////////////////////////

ID3D12CommandQueue* FindCapturedQueue(IUnknown* swapChainObject)
{
    IUnknown* identity = QueryIdentity(swapChainObject);
    if (!identity)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(s_queueMutex);
    for (const QueueMapping& mapping : s_queueMappings)
    {
        if (mapping.swapChainIdentity == identity && mapping.queue)
        {
            mapping.queue->AddRef();
            ReleaseCom(identity);
            return mapping.queue;
        }
    }

    ReleaseCom(identity);
    return nullptr;
}

/////////////////////////////////////////////////////////////////////

ID3D12CommandQueue* FindFallbackQueue()
{
    std::lock_guard<std::mutex> lock(s_queueMutex);
    if (!s_lastDirectQueue)
    {
        return nullptr;
    }
    s_lastDirectQueue->AddRef();
    return s_lastDirectQueue;
}

/////////////////////////////////////////////////////////////////////

bool IsActiveSwapChain(IUnknown* swapChainObject)
{
    IUnknown* activeIdentity = QueryIdentity(s_swapChain);
    IUnknown* currentIdentity = QueryIdentity(swapChainObject);
    const bool active = activeIdentity && currentIdentity && activeIdentity == currentIdentity;
    ReleaseCom(activeIdentity);
    ReleaseCom(currentIdentity);
    return active;
}

/////////////////////////////////////////////////////////////////////

void ReleaseQueueMappings()
{
    std::lock_guard<std::mutex> lock(s_queueMutex);
    for (QueueMapping& mapping : s_queueMappings)
    {
        ReleaseCom(mapping.queue);
        ReleaseCom(mapping.swapChainIdentity);
    }
    s_queueMappings.clear();
    ReleaseCom(s_lastDirectQueue);
}

/////////////////////////////////////////////////////////////////////

void WaitForFenceValueLocked(UINT64 value)
{
    if (!s_fence || !s_fenceEvent || value == 0)
    {
        return;
    }

    if (s_fence->GetCompletedValue() >= value)
    {
        return;
    }

    if (SUCCEEDED(s_fence->SetEventOnCompletion(value, s_fenceEvent)))
    {
        WaitForSingleObject(s_fenceEvent, INFINITE);
    }
}

/////////////////////////////////////////////////////////////////////

void WaitForOverlayGpuLocked()
{
    UINT64 maxFence = 0;
    for (UINT i = 0; i < s_bufferCount && i < MaxSwapChainBuffers; ++i)
    {
        maxFence = (std::max)(maxFence, s_frames[i].fenceValue);
    }
    WaitForFenceValueLocked(maxFence);
}

/////////////////////////////////////////////////////////////////////

void ReleaseSrvDescriptor(const DescriptorSlot& slot)
{
    if (slot.index < SrvDescriptorCount)
    {
        s_srvSlots[slot.index] = false;
    }
}

/////////////////////////////////////////////////////////////////////

void ReleaseIconResources()
{
    // Icon texture/view are rebuilt when shared icon pixels change
    if (s_iconDescriptorValid)
    {
        ReleaseSrvDescriptor(s_iconDescriptor);
        s_iconDescriptorValid = false;
    }
    ReleaseCom(s_iconTexture);
    ReleaseCom(s_iconUpload);
    s_iconGeneration = 0;
}

/////////////////////////////////////////////////////////////////////

void ReleaseFrameResources()
{
    // Backbuffer-dependent resources must be dropped before resize
    for (FrameContext& frame : s_frames)
    {
        ReleaseCom(frame.renderTarget);
        ReleaseCom(frame.allocator);
        frame.rtv = {};
        frame.fenceValue = 0;
    }
    s_bufferCount = 0;
    s_frameIndexFallback = 0;
    ReleaseCom(s_commandList);
    ReleaseCom(s_rtvHeap);
}

/////////////////////////////////////////////////////////////////////

void ShutdownImGuiLocked()
{
    // Caller holds render mutex while tearing down ImGui
    WaitForOverlayGpuLocked();
    ReleaseFrameResources();
    ReleaseIconResources();
    if (s_imguiReady)
    {
        ImGui_ImplDX12_Shutdown();
        s_imguiReady = false;
    }
    if (ImGui::GetCurrentContext())
    {
        ImGui::DestroyContext();
    }
    ReleaseCom(s_fence);
    if (s_fenceEvent)
    {
        CloseHandle(s_fenceEvent);
        s_fenceEvent = nullptr;
    }
    ReleaseCom(s_srvHeap);
    s_srvSlots.fill(false);
    s_nextFenceValue = 1;
    ReleaseCom(s_commandQueue);
    ReleaseCom(s_device);
    ReleaseCom(s_swapChain);
}

/////////////////////////////////////////////////////////////////////

D3D12_CPU_DESCRIPTOR_HANDLE CpuSrvHandle(UINT index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = s_srvHeap ? s_srvHeap->GetCPUDescriptorHandleForHeapStart() : D3D12_CPU_DESCRIPTOR_HANDLE{};
    handle.ptr += static_cast<SIZE_T>(index) * s_srvDescriptorSize;
    return handle;
}

/////////////////////////////////////////////////////////////////////

D3D12_GPU_DESCRIPTOR_HANDLE GpuSrvHandle(UINT index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle = s_srvHeap ? s_srvHeap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{};
    handle.ptr += static_cast<UINT64>(index) * s_srvDescriptorSize;
    return handle;
}

/////////////////////////////////////////////////////////////////////

bool AllocateSrvDescriptor(DescriptorSlot& outSlot)
{
    if (!s_srvHeap)
    {
        return false;
    }

    for (UINT i = 0; i < SrvDescriptorCount; ++i)
    {
        if (!s_srvSlots[i])
        {
            s_srvSlots[i] = true;
            outSlot.index = i;
            outSlot.cpu = CpuSrvHandle(i);
            outSlot.gpu = GpuSrvHandle(i);
            return true;
        }
    }

    LYMALINK_LOG("[Direct3D12OverlayLayer][AllocateSrvDescriptor] descriptor heap full.");
    return false;
}

/////////////////////////////////////////////////////////////////////

void ImGuiAllocateSrv(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle)
{
    DescriptorSlot slot{};
    if (!AllocateSrvDescriptor(slot))
    {
        *outCpuHandle = {};
        *outGpuHandle = {};
        return;
    }

    *outCpuHandle = slot.cpu;
    *outGpuHandle = slot.gpu;
}

/////////////////////////////////////////////////////////////////////

void ImGuiFreeSrv(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE)
{
    if (!s_srvHeap || s_srvDescriptorSize == 0 || cpuHandle.ptr == 0)
    {
        return;
    }

    const SIZE_T start = s_srvHeap->GetCPUDescriptorHandleForHeapStart().ptr;
    if (cpuHandle.ptr < start)
    {
        return;
    }

    const SIZE_T offset = cpuHandle.ptr - start;
    const UINT index = static_cast<UINT>(offset / s_srvDescriptorSize);
    if (index < SrvDescriptorCount && offset == static_cast<SIZE_T>(index) * s_srvDescriptorSize)
    {
        s_srvSlots[index] = false;
    }
}

/////////////////////////////////////////////////////////////////////

bool QueryFramebufferSize(IDXGISwapChain* swapChain, uint32_t& outWidth, uint32_t& outHeight, DXGI_FORMAT& outFormat, UINT& outBufferCount)
{
    // Clear outputs until a valid size is found
    outWidth = 0;
    outHeight = 0;
    outFormat = DXGI_FORMAT_UNKNOWN;
    outBufferCount = 0;
    if (!swapChain)
    {
        return false;
    }

    IDXGISwapChain1* swapChain1 = nullptr;
    if (SUCCEEDED(swapChain->QueryInterface(__uuidof(IDXGISwapChain1), reinterpret_cast<void**>(&swapChain1))) && swapChain1)
    {
        DXGI_SWAP_CHAIN_DESC1 desc1{};
        if (SUCCEEDED(swapChain1->GetDesc1(&desc1)))
        {
            outWidth = desc1.Width;
            outHeight = desc1.Height;
            outFormat = desc1.Format == DXGI_FORMAT_UNKNOWN ? outFormat : desc1.Format;
            outBufferCount = desc1.BufferCount;
        }
        ReleaseCom(swapChain1);
    }

    if (outWidth == 0 || outHeight == 0 || outBufferCount == 0)
    {
        DXGI_SWAP_CHAIN_DESC desc{};
        // Prefer DXGI swap-chain description when it has a real buffer size
        if (SUCCEEDED(swapChain->GetDesc(&desc)))
        {
            if (outWidth == 0)
            {
                outWidth = desc.BufferDesc.Width;
            }
            if (outHeight == 0)
            {
                outHeight = desc.BufferDesc.Height;
            }
            if (outFormat == DXGI_FORMAT_UNKNOWN)
            {
                outFormat = desc.BufferDesc.Format;
            }
            if (outBufferCount == 0)
            {
                outBufferCount = desc.BufferCount;
            }
        }
    }

    ID3D12Resource* backBuffer = nullptr;
    if ((outWidth == 0 || outHeight == 0 || outFormat == DXGI_FORMAT_UNKNOWN) &&
        SUCCEEDED(swapChain->GetBuffer(0, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&backBuffer))) && backBuffer)
    {
        // Backbuffer desc is a fallback when swap-chain desc is incomplete
        D3D12_RESOURCE_DESC textureDesc = backBuffer->GetDesc();
        if (outWidth == 0)
        {
            outWidth = static_cast<uint32_t>(textureDesc.Width);
        }
        if (outHeight == 0)
        {
            outHeight = textureDesc.Height;
        }
        if (outFormat == DXGI_FORMAT_UNKNOWN)
        {
            outFormat = textureDesc.Format;
        }
        ReleaseCom(backBuffer);
    }

    outBufferCount = (std::max)(1U, (std::min)(outBufferCount == 0 ? 2U : outBufferCount, MaxSwapChainBuffers));
    if (outFormat == DXGI_FORMAT_UNKNOWN)
    {
        outFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    return outWidth > 0 && outHeight > 0;
}

/////////////////////////////////////////////////////////////////////

UINT CurrentBackBufferIndex(IDXGISwapChain* swapChain)
{
    IDXGISwapChain3* swapChain3 = nullptr;
    if (SUCCEEDED(swapChain->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&swapChain3))) && swapChain3)
    {
        const UINT index = swapChain3->GetCurrentBackBufferIndex();
        ReleaseCom(swapChain3);
        return s_bufferCount == 0 ? 0 : index % s_bufferCount;
    }

    const UINT index = s_bufferCount == 0 ? 0 : s_frameIndexFallback % s_bufferCount;
    ++s_frameIndexFallback;
    return index;
}

/////////////////////////////////////////////////////////////////////

D3D12_RESOURCE_BARRIER TransitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    return barrier;
}

/////////////////////////////////////////////////////////////////////

bool EnsureFrameResourcesLocked(IDXGISwapChain* swapChain, ID3D12Device* device, UINT bufferCount, DXGI_FORMAT format)
{
    // Existing backbuffer resources remain valid until ResizeBuffers
    if (s_rtvHeap && s_bufferCount == bufferCount && s_rtvFormat == format)
    {
        return true;
    }

    WaitForOverlayGpuLocked();
    ReleaseFrameResources();
    s_bufferCount = bufferCount;
    s_rtvFormat = format;
    s_rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.NumDescriptors = bufferCount;
    HRESULT result = device->CreateDescriptorHeap(&rtvDesc, __uuidof(ID3D12DescriptorHeap), reinterpret_cast<void**>(&s_rtvHeap));
    if (FAILED(result) || !s_rtvHeap)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][EnsureFrameResources] CreateDescriptorHeap(RTV) failed hr=" + std::to_string(result));
        return false;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = s_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < bufferCount; ++i)
    {
        result = swapChain->GetBuffer(i, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&s_frames[i].renderTarget));
        if (FAILED(result) || !s_frames[i].renderTarget)
        {
            LYMALINK_LOG("[Direct3D12OverlayLayer][EnsureFrameResources] GetBuffer failed hr=" + std::to_string(result));
            return false;
        }

        s_frames[i].rtv = rtvHandle;
        device->CreateRenderTargetView(s_frames[i].renderTarget, nullptr, rtvHandle);
        rtvHandle.ptr += s_rtvDescriptorSize;

        result = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), reinterpret_cast<void**>(&s_frames[i].allocator));
        if (FAILED(result) || !s_frames[i].allocator)
        {
            LYMALINK_LOG("[Direct3D12OverlayLayer][EnsureFrameResources] CreateCommandAllocator failed hr=" + std::to_string(result));
            return false;
        }
    }

    result = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, s_frames[0].allocator, nullptr, __uuidof(ID3D12GraphicsCommandList), reinterpret_cast<void**>(&s_commandList));
    if (FAILED(result) || !s_commandList)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][EnsureFrameResources] CreateCommandList failed hr=" + std::to_string(result));
        return false;
    }
    s_commandList->Close();
    return true;
}

/////////////////////////////////////////////////////////////////////

bool InitImGuiLocked(IDXGISwapChain* swapChain, ID3D12Device* device, ID3D12CommandQueue* queue, uint32_t width, uint32_t height, UINT bufferCount, DXGI_FORMAT format)
{
    // Need a valid swap chain, device, and direct command queue before touching ImGui
    if (!swapChain || !device || !queue)
    {
        return false;
    }
    // Reuse existing ImGui backend when swap chain, device, and queue did not change
    if (s_imguiReady && IsActiveSwapChain(swapChain) && s_device == device && s_commandQueue == queue)
    {
        return EnsureFrameResourcesLocked(swapChain, device, bufferCount, format);
    }

    // Swap chain, device, or command queue changed, so rebuild ImGui from scratch
    ShutdownImGuiLocked();
    AddRefSwapChain(swapChain);
    AddRefDevice(device);
    AddRefCommandQueue(queue);

    s_srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_DESCRIPTOR_HEAP_DESC srvDesc{};
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.NumDescriptors = SrvDescriptorCount;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    HRESULT result = device->CreateDescriptorHeap(&srvDesc, __uuidof(ID3D12DescriptorHeap), reinterpret_cast<void**>(&s_srvHeap));
    if (FAILED(result) || !s_srvHeap)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][InitImGui] CreateDescriptorHeap(SRV) failed hr=" + std::to_string(result));
        ShutdownImGuiLocked();
        return false;
    }

    result = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), reinterpret_cast<void**>(&s_fence));
    if (FAILED(result) || !s_fence)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][InitImGui] CreateFence failed hr=" + std::to_string(result));
        ShutdownImGuiLocked();
        return false;
    }

    s_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!s_fenceEvent)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][InitImGui] CreateEventW failed error=" + std::to_string(GetLastError()));
        ShutdownImGuiLocked();
        return false;
    }

    if (!EnsureFrameResourcesLocked(swapChain, device, bufferCount, format))
    {
        ShutdownImGuiLocked();
        return false;
    }

    // Bind ImGui to the current D3D12 device/queue pair
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    OverlayFonts::EnsureEmbeddedFontLoaded();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();

    ImGui_ImplDX12_InitInfo initInfo{};
    initInfo.Device = device;
    initInfo.CommandQueue = queue;
    initInfo.NumFramesInFlight = static_cast<int>(bufferCount);
    initInfo.RTVFormat = format;
    initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    initInfo.SrvDescriptorHeap = s_srvHeap;
    initInfo.SrvDescriptorAllocFn = ImGuiAllocateSrv;
    initInfo.SrvDescriptorFreeFn = ImGuiFreeSrv;
    if (!ImGui_ImplDX12_Init(&initInfo))
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][InitImGui] ImGui_ImplDX12_Init failed.");
        ShutdownImGuiLocked();
        return false;
    }

    s_imguiReady = true;
    LYMALINK_LOG("[Direct3D12OverlayLayer][InitImGui] ready " + std::to_string(width) + "x" + std::to_string(height));
    return true;
}

/////////////////////////////////////////////////////////////////////

bool UploadIconTextureLocked(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const std::vector<uint8_t>& rgbaPixels)
{
    D3D12_RESOURCE_DESC textureDesc{};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = OVERLAY_ICON_SIZE;
    textureDesc.Height = OVERLAY_ICON_SIZE;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    HRESULT result = device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        __uuidof(ID3D12Resource),
        reinterpret_cast<void**>(&s_iconTexture));
    if (FAILED(result) || !s_iconTexture)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][EnsureIconTexture] CreateCommittedResource(texture) failed hr=" + std::to_string(result));
        return false;
    }

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rowCount = 0;
    UINT64 rowSize = 0;
    UINT64 uploadSize = 0;
    device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &rowCount, &rowSize, &uploadSize);

    D3D12_RESOURCE_DESC uploadDesc{};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = uploadSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    result = device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        __uuidof(ID3D12Resource),
        reinterpret_cast<void**>(&s_iconUpload));
    if (FAILED(result) || !s_iconUpload)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][EnsureIconTexture] CreateCommittedResource(upload) failed hr=" + std::to_string(result));
        return false;
    }

    uint8_t* mapped = nullptr;
    D3D12_RANGE readRange{};
    result = s_iconUpload->Map(0, &readRange, reinterpret_cast<void**>(&mapped));
    if (FAILED(result) || !mapped)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][EnsureIconTexture] Map failed hr=" + std::to_string(result));
        return false;
    }

    // Copy row by row because D3D12 upload pitch is aligned wider than icon stride
    for (UINT y = 0; y < OVERLAY_ICON_SIZE; ++y)
    {
        std::memcpy(mapped + footprint.Offset + static_cast<SIZE_T>(y) * footprint.Footprint.RowPitch, rgbaPixels.data() + y * OVERLAY_ICON_STRIDE, OVERLAY_ICON_STRIDE);
    }
    s_iconUpload->Unmap(0, nullptr);

    D3D12_TEXTURE_COPY_LOCATION destination{};
    destination.pResource = s_iconTexture;
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destination.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource = s_iconUpload;
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint = footprint;
    commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

    D3D12_RESOURCE_BARRIER barrier = TransitionBarrier(s_iconTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &barrier);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    device->CreateShaderResourceView(s_iconTexture, &srvDesc, s_iconDescriptor.cpu);
    return true;
}

/////////////////////////////////////////////////////////////////////

ImTextureID EnsureDirect3D12IconTexture(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const std::vector<uint8_t>& rgbaPixels, uint64_t generation)
{
    // Reject missing device or incomplete icon buffer
    if (!device || !commandList || rgbaPixels.size() != OVERLAY_ICON_DATA_SIZE)
    {
        return ImTextureID_Invalid;
    }
    // Same generation means existing shader resource view is still current
    if (s_iconTexture && s_iconDescriptorValid && s_iconGeneration == generation)
    {
        return static_cast<ImTextureID>(s_iconDescriptor.gpu.ptr);
    }

    // Old icon resources may still be in use by a previous overlay frame
    WaitForOverlayGpuLocked();
    ReleaseIconResources();
    if (!AllocateSrvDescriptor(s_iconDescriptor))
    {
        return ImTextureID_Invalid;
    }
    s_iconDescriptorValid = true;

    if (!UploadIconTextureLocked(device, commandList, rgbaPixels))
    {
        ReleaseIconResources();
        return ImTextureID_Invalid;
    }

    // Store generation so next frame can reuse these resources
    s_iconGeneration = generation;
    return static_cast<ImTextureID>(s_iconDescriptor.gpu.ptr);
}

/////////////////////////////////////////////////////////////////////

bool RenderOverlay(IDXGISwapChain* swapChain, const char* presentPath)
{
    // Present and Present1 are the main DX12 render paths
    if (s_rendering || s_shuttingDown.load() || !swapChain)
    {
        return false;
    }

    // DX12 hooks can intercept shared DXGI swap chains - Non-DX12 chains are pass-through
    const WinDxgiOverlayRouter::Renderer renderer = WinDxgiOverlayRouter::DetectSwapChainRenderer(swapChain);
    if (renderer != WinDxgiOverlayRouter::Renderer::Direct3D12)
    {
        if (renderer != WinDxgiOverlayRouter::Renderer::Unknown && !s_loggedRoutedSwapChain.exchange(true))
        {
            LYMALINK_LOG("[Direct3D12OverlayLayer][RenderOverlay] pass-through non-DX12 swap chain.");
        }
        return false;
    }

    const int recoveryFrames = s_resizeFailureRecoveryFrames.load();
    if (recoveryFrames > 0)
    {
        // Some games leave the swap chain in a bad transient state after invalid resize
        // Let Present pass through for a short window, then rebuild ImGui on a clean frame
        if (!s_loggedResizeRecoveryWait.exchange(true))
        {
            LYMALINK_LOG("[Direct3D12OverlayLayer][RenderOverlay] resize recovery active; overlay pass-through.");
        }
        if (s_resizeFailureRecoveryFrames.fetch_sub(1) == 1)
        {
            s_loggedResizeRecoveryWait.store(false);
            LYMALINK_LOG("[Direct3D12OverlayLayer][RenderOverlay] resize recovery complete; overlay will reinitialize.");
        }
        return true;
    }

    // Resolve the D3D12 device from the intercepted swap chain
    ID3D12Device* device = nullptr;
    if (FAILED(swapChain->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void**>(&device))) || !device)
    {
        return false;
    }

    ID3D12CommandQueue* queue = FindCapturedQueue(swapChain);
    if (!queue)
    {
        queue = FindFallbackQueue();
        if (queue && !s_loggedUsingFallbackQueue.exchange(true))
        {
            LYMALINK_LOG("[Direct3D12OverlayLayer][RenderOverlay] using fallback direct queue; swap-chain queue was not captured.");
        }
        if (!queue && !s_loggedMissingQueue.exchange(true))
        {
            LYMALINK_LOG("[Direct3D12OverlayLayer][RenderOverlay] skipped: command queue unavailable.");
        }
    }
    if (!queue)
    {
        ReleaseCom(device);
        return true;
    }

    // Need real framebuffer size before creating ImGui frame
    uint32_t width = 0;
    uint32_t height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    UINT bufferCount = 0;
    if (!QueryFramebufferSize(swapChain, width, height, format, bufferCount) || width == 0 || height == 0)
    {
        ReleaseCom(device);
        ReleaseCom(queue);
        return true;
    }

    std::lock_guard<std::mutex> lock(s_renderMutex);
    s_rendering = true;

    if (InitImGuiLocked(swapChain, device, queue, width, height, bufferCount, format))
    {
        const UINT frameIndex = CurrentBackBufferIndex(swapChain);
        FrameContext& frame = s_frames[frameIndex];
        WaitForFenceValueLocked(frame.fenceValue);

        HRESULT result = frame.allocator->Reset();
        if (SUCCEEDED(result))
        {
            result = s_commandList->Reset(frame.allocator, nullptr);
        }
        if (FAILED(result))
        {
            LYMALINK_LOG("[Direct3D12OverlayLayer][RenderOverlay] command reset failed hr=" + std::to_string(result));
        }
        else
        {
            D3D12_RESOURCE_BARRIER toRenderTarget = TransitionBarrier(frame.renderTarget, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            s_commandList->ResourceBarrier(1, &toRenderTarget);
            s_commandList->OMSetRenderTargets(1, &frame.rtv, FALSE, nullptr);
            s_commandList->SetDescriptorHeaps(1, &s_srvHeap);

            // Pull newest shared-memory state and render overlay widgets
            ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
            ImGui_ImplDX12_NewFrame();
            ImGui::NewFrame();
            const bool claimedNotification = s_overlay.BeginFrame();
            ImTextureID icon = EnsureDirect3D12IconTexture(device, s_commandList, s_overlay.IconPixels(), s_overlay.IconGeneration());
            s_overlay.Draw(width, height, icon);
            if (claimedNotification)
            {
                LYMALINK_LOG("[Direct3D12OverlayLayer][RenderOverlay] claimed notification; renderer=dx12 path=" +
                    std::string(presentPath) + " size=" +
                    std::to_string(width) + "x" + std::to_string(height));
            }
            ImGui::Render();
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), s_commandList);

            D3D12_RESOURCE_BARRIER toPresent = TransitionBarrier(frame.renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            s_commandList->ResourceBarrier(1, &toPresent);
            result = s_commandList->Close();
            if (FAILED(result))
            {
                LYMALINK_LOG("[Direct3D12OverlayLayer][RenderOverlay] command close failed hr=" + std::to_string(result));
            }
            else
            {
                ID3D12CommandList* commandLists[] = {s_commandList};
                queue->ExecuteCommandLists(1, commandLists);
                const UINT64 fenceValue = s_nextFenceValue++;
                if (SUCCEEDED(queue->Signal(s_fence, fenceValue)))
                {
                    frame.fenceValue = fenceValue;
                }
            }
        }
    }

    s_rendering = false;
    ReleaseCom(device);
    ReleaseCom(queue);
    return true;
}

void BeforeResizeBuffers(IDXGISwapChain* swapChain)
{
    std::lock_guard<std::mutex> lock(s_renderMutex);
    if (s_imguiReady && IsActiveSwapChain(swapChain))
    {
        // Resize can change buffer count, so rebuild the DX12 backend on next Present
        LYMALINK_LOG("[Direct3D12OverlayLayer][ResizeBuffers] rebuilding swap-chain resources.");
        ShutdownImGuiLocked();
    }
}

/////////////////////////////////////////////////////////////////////

HRESULT HandleResizeBuffersResult(const char* apiName, HRESULT result)
{
    if (SUCCEEDED(result))
    {
        s_resizeFailureRecoveryFrames.store(0);
        s_loggedResizeFailure.store(false);
        s_loggedResizeRecoveryWait.store(false);
        return result;
    }

    if (!s_loggedResizeFailure.exchange(true))
    {
        LYMALINK_LOG(std::string("[Direct3D12OverlayLayer][") + apiName + "] failed hr=" + std::to_string(result));
    }

    if (result == DXGI_ERROR_INVALID_CALL)
    {
        // Keep the game alive when DXGI rejects a resize during overlay teardown.
        // Overlay stays disabled briefly and reinitializes from the next stable Present.
        s_resizeFailureRecoveryFrames.store(ResizeFailureRecoveryPresentFrames);
        s_loggedResizeRecoveryWait.store(false);
        LYMALINK_LOG(std::string("[Direct3D12OverlayLayer][") + apiName + "] DXGI_ERROR_INVALID_CALL recovered; returning S_OK and reinitializing after pass-through.");
        return S_OK;
    }

    return result;
}

HRESULT WINAPI Hook_CreateSwapChain(IDXGIFactory* factory, IUnknown* device, DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** swapChain)
{
    const HRESULT result = s_realCreateSwapChain ? s_realCreateSwapChain(factory, device, desc, swapChain) : DXGI_ERROR_INVALID_CALL;
    if (SUCCEEDED(result) && swapChain && *swapChain)
    {
        CaptureSwapChainQueue(*swapChain, device, "CreateSwapChain");
        s_loggedUsingFallbackQueue.store(false);
    }
    return result;
}

/////////////////////////////////////////////////////////////////////

HRESULT WINAPI Hook_CreateSwapChainForHwnd(IDXGIFactory2* factory, IUnknown* device, HWND window, const DXGI_SWAP_CHAIN_DESC1* desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreenDesc, IDXGIOutput* restrictToOutput, IDXGISwapChain1** swapChain)
{
    const HRESULT result = s_realCreateSwapChainForHwnd ? s_realCreateSwapChainForHwnd(factory, device, window, desc, fullscreenDesc, restrictToOutput, swapChain) : DXGI_ERROR_INVALID_CALL;
    if (SUCCEEDED(result) && swapChain && *swapChain)
    {
        CaptureSwapChainQueue(*swapChain, device, "CreateSwapChainForHwnd");
        s_loggedUsingFallbackQueue.store(false);
    }
    return result;
}

/////////////////////////////////////////////////////////////////////

HRESULT WINAPI Hook_CreateSwapChainForCoreWindow(IDXGIFactory2* factory, IUnknown* device, IUnknown* window, const DXGI_SWAP_CHAIN_DESC1* desc, IDXGIOutput* restrictToOutput, IDXGISwapChain1** swapChain)
{
    const HRESULT result = s_realCreateSwapChainForCoreWindow ? s_realCreateSwapChainForCoreWindow(factory, device, window, desc, restrictToOutput, swapChain) : DXGI_ERROR_INVALID_CALL;
    if (SUCCEEDED(result) && swapChain && *swapChain)
    {
        CaptureSwapChainQueue(*swapChain, device, "CreateSwapChainForCoreWindow");
        s_loggedUsingFallbackQueue.store(false);
    }
    return result;
}

/////////////////////////////////////////////////////////////////////

HRESULT WINAPI Hook_CreateSwapChainForComposition(IDXGIFactory2* factory, IUnknown* device, const DXGI_SWAP_CHAIN_DESC1* desc, IDXGIOutput* restrictToOutput, IDXGISwapChain1** swapChain)
{
    const HRESULT result = s_realCreateSwapChainForComposition ? s_realCreateSwapChainForComposition(factory, device, desc, restrictToOutput, swapChain) : DXGI_ERROR_INVALID_CALL;
    if (SUCCEEDED(result) && swapChain && *swapChain)
    {
        CaptureSwapChainQueue(*swapChain, device, "CreateSwapChainForComposition");
        s_loggedUsingFallbackQueue.store(false);
    }
    return result;
}

/////////////////////////////////////////////////////////////////////

void WINAPI Hook_ExecuteCommandLists(ID3D12CommandQueue* queue, UINT numCommandLists, ID3D12CommandList* const* commandLists)
{
    // Fallback queue capture for apps that created swap chains before our factory hooks
    AddRefLastDirectQueue(queue);
    if (s_realExecuteCommandLists)
    {
        s_realExecuteCommandLists(queue, numCommandLists, commandLists);
    }
}

/////////////////////////////////////////////////////////////////////

bool CreateHook(void* target, void* detour, void** original, const char* name)
{
    // Create and enable one MinHook detour
    // Follow existing jump stubs first so third-party overlays remain in the call chain instead of being overwritten
    if (!target)
    {
        LYMALINK_LOG(std::string("[Direct3D12OverlayLayer][CreateHook] missing target ") + name);
        return false;
    }

    void* resolvedTarget = ResolveExistingHookTarget(target, name);
    if (!resolvedTarget)
    {
        LYMALINK_LOG(std::string("[Direct3D12OverlayLayer][CreateHook] unresolved target ") + name);
        return false;
    }

    MH_STATUS status = MH_CreateHook(resolvedTarget, detour, original);
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][CreateHook] " + HookStatus(name, status));
        return false;
    }

    if (status == MH_ERROR_ALREADY_CREATED && (!original || !*original))
    {
        LYMALINK_LOG(std::string("[Direct3D12OverlayLayer][CreateHook] already created without original trampoline for ") + name);
        return false;
    }

    status = MH_EnableHook(resolvedTarget);
    if (status != MH_OK && status != MH_ERROR_ENABLED)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][EnableHook] " + HookStatus(name, status));
        return false;
    }
    return true;
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
    constexpr const wchar_t* className = L"LymalinkDx12DummyWindow";
    if (!RegisterDummyWindowClass(instance, className))
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][CreateDummyWindow] RegisterClassExW failed error=" + std::to_string(GetLastError()));
        return nullptr;
    }

    HWND window = CreateWindowExW(
        0,
        className,
        L"Lymalink DX12 Dummy",
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
        LYMALINK_LOG("[Direct3D12OverlayLayer][CreateDummyWindow] CreateWindowExW failed error=" + std::to_string(GetLastError()));
    }
    return window;
}

/////////////////////////////////////////////////////////////////////

bool CreateDummyDeviceAndSwapChain(
    PFN_D3D12CreateDevice createDevice,
    PFN_CreateDXGIFactory2 createFactory,
    HWND window,
    ID3D12Device** device,
    ID3D12CommandQueue** queue,
    IDXGIFactory2** factory,
    IDXGISwapChain** swapChain)
{
    // Need factory functions, hidden window, and output storage
    if (!createDevice || !createFactory || !window || !device || !queue || !factory || !swapChain)
    {
        return false;
    }

    HRESULT result = createDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), reinterpret_cast<void**>(device));
    if (FAILED(result) || !*device)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][CreateDummyDeviceAndSwapChain] D3D12CreateDevice failed hr=" + std::to_string(result));
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    result = (*device)->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(queue));
    if (FAILED(result) || !*queue)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][CreateDummyDeviceAndSwapChain] CreateCommandQueue failed hr=" + std::to_string(result));
        return false;
    }

    result = createFactory(0, __uuidof(IDXGIFactory2), reinterpret_cast<void**>(factory));
    if (FAILED(result) || !*factory)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][CreateDummyDeviceAndSwapChain] CreateDXGIFactory2 failed hr=" + std::to_string(result));
        return false;
    }

    // Minimal windowed parameters are enough for vtable discovery
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = 1;
    desc.Height = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    IDXGISwapChain1* swapChain1 = nullptr;
    result = (*factory)->CreateSwapChainForHwnd(*queue, window, &desc, nullptr, nullptr, &swapChain1);
    if (FAILED(result) || !swapChain1)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][CreateDummyDeviceAndSwapChain] CreateSwapChainForHwnd failed hr=" + std::to_string(result));
        return false;
    }

    result = swapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(swapChain));
    ReleaseCom(swapChain1);
    if (FAILED(result) || !*swapChain)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][CreateDummyDeviceAndSwapChain] QueryInterface(IDXGISwapChain) failed hr=" + std::to_string(result));
        return false;
    }
    return true;
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
    // Do not force D3D12 into non-DX12 processes - Watch briefly for late runtime loads
    HMODULE d3d12 = nullptr;
    HMODULE dxgi = nullptr;
    constexpr int MaxRuntimeWaitMs = 30000;
    constexpr int RuntimePollMs = 100;
    for (int waitedMs = 0; waitedMs <= MaxRuntimeWaitMs && !s_shuttingDown.load(); waitedMs += RuntimePollMs)
    {
        d3d12 = GetModuleHandleW(L"d3d12.dll");
        dxgi = GetModuleHandleW(L"dxgi.dll");
        if (d3d12 && dxgi)
        {
            break;
        }
        Sleep(RuntimePollMs);
    }
    LYMALINK_LOG(std::string("[Direct3D12OverlayLayer][Identity] I am DX12; process uses DX12=") + (d3d12 ? "yes; active" : "no; inactive"));
    if (s_shuttingDown.load())
    {
        return 1;
    }
    if (!d3d12)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][InitThread] d3d12.dll not loaded; layer inactive.");
        return 1;
    }

    if (!dxgi)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][InitThread] dxgi.dll not loaded; layer inactive.");
        return 1;
    }

    if (!WinDxgiOverlayCoordinator::Start())
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][InitThread] DXGI coordinator unavailable.");
        return 1;
    }

    auto createDevice = reinterpret_cast<PFN_D3D12CreateDevice>(GetProcAddress(d3d12, "D3D12CreateDevice"));
    auto createFactory = reinterpret_cast<PFN_CreateDXGIFactory2>(GetProcAddress(dxgi, "CreateDXGIFactory2"));
    if (!createDevice || !createFactory)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][InitThread] required D3D12/DXGI exports missing.");
        return 1;
    }

    // Hidden window backs the temporary dummy swap chain
    HWND window = CreateDummyWindow();
    if (!window)
    {
        return 1;
    }

    // Build dummy D3D12 device/queue/swap chain so vtables can be hooked
    ID3D12Device* device = nullptr;
    ID3D12CommandQueue* queue = nullptr;
    IDXGIFactory2* factory = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    if (!CreateDummyDeviceAndSwapChain(createDevice, createFactory, window, &device, &queue, &factory, &swapChain))
    {
        ReleaseCom(swapChain);
        ReleaseCom(factory);
        ReleaseCom(queue);
        ReleaseCom(device);
        DestroyWindow(window);
        return 1;
    }

    // MinHook must be initialized before creating detours
    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][InitThread] " + HookStatus("MH_Initialize", status));
        ReleaseCom(swapChain);
        ReleaseCom(factory);
        ReleaseCom(queue);
        ReleaseCom(device);
        DestroyWindow(window);
        return 1;
    }
    s_ownsMinHook = status == MH_OK;

    void** swapChainTable = VTable(swapChain);
    if (!swapChainTable)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][InitThread] swap-chain vtable missing.");
        if (s_ownsMinHook)
        {
            MH_Uninitialize();
            s_ownsMinHook = false;
        }
        ReleaseCom(swapChain);
        ReleaseCom(factory);
        ReleaseCom(queue);
        ReleaseCom(device);
        DestroyWindow(window);
        return 1;
    }

    // DXGI Present/Resize hooks are owned by WinDxgiOverlayCoordinator
    // DX12 still installs queue/factory hooks so renderer can submit ImGui command lists
    bool queueCaptureReady = false;

    IDXGIFactory* factory0 = nullptr;
    if (SUCCEEDED(factory->QueryInterface(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&factory0))) && factory0)
    {
        void** factoryTable = VTable(factory0);
        if (factoryTable)
        {
            queueCaptureReady = CreateHook(factoryTable[VTable_CreateSwapChain], reinterpret_cast<void*>(&Hook_CreateSwapChain), reinterpret_cast<void**>(&s_realCreateSwapChain), "IDXGIFactory::CreateSwapChain") || queueCaptureReady;
        }
        ReleaseCom(factory0);
    }

    void** factory2Table = VTable(factory);
    if (factory2Table)
    {
        queueCaptureReady = CreateHook(factory2Table[VTable_CreateSwapChainForHwnd], reinterpret_cast<void*>(&Hook_CreateSwapChainForHwnd), reinterpret_cast<void**>(&s_realCreateSwapChainForHwnd), "IDXGIFactory2::CreateSwapChainForHwnd") || queueCaptureReady;
        queueCaptureReady = CreateHook(factory2Table[VTable_CreateSwapChainForCoreWindow], reinterpret_cast<void*>(&Hook_CreateSwapChainForCoreWindow), reinterpret_cast<void**>(&s_realCreateSwapChainForCoreWindow), "IDXGIFactory2::CreateSwapChainForCoreWindow") || queueCaptureReady;
        queueCaptureReady = CreateHook(factory2Table[VTable_CreateSwapChainForComposition], reinterpret_cast<void*>(&Hook_CreateSwapChainForComposition), reinterpret_cast<void**>(&s_realCreateSwapChainForComposition), "IDXGIFactory2::CreateSwapChainForComposition") || queueCaptureReady;
    }

    void** queueTable = VTable(queue);
    if (queueTable)
    {
        queueCaptureReady = CreateHook(queueTable[VTable_ExecuteCommandLists], reinterpret_cast<void*>(&Hook_ExecuteCommandLists), reinterpret_cast<void**>(&s_realExecuteCommandLists), "ID3D12CommandQueue::ExecuteCommandLists") || queueCaptureReady;
    }

    if (!queueCaptureReady)
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][InitThread] queue capture hooks unavailable; DXGI coordinator remains active.");
        ReleaseCom(swapChain);
        ReleaseCom(factory);
        ReleaseCom(queue);
        ReleaseCom(device);
        DestroyWindow(window);
        return 0;
    }

    // Dummy objects are no longer needed once hooks are installed
    ReleaseCom(swapChain);
    ReleaseCom(factory);
    ReleaseCom(queue);
    ReleaseCom(device);
    DestroyWindow(window);

    // Mark install complete for detach cleanup
    s_hooksReady.store(true);
    LYMALINK_LOG("[Direct3D12OverlayLayer][InitThread] hooks installed.");
    return 0;
}
}

/////////////////////////////////////////////////////////////////////
// Called from WinOverlayEntrypoint.cpp DllMain when this target defines
// LYMALINK_OVERLAY_ATTACH_HOOKS.
/////////////////////////////////////////////////////////////////////

extern "C" __declspec(dllexport) BOOL WINAPI LymalinkDirect3D12RenderSwapChain(IUnknown* swapChainObject, const char* presentPath)
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

extern "C" __declspec(dllexport) BOOL WINAPI LymalinkDirect3D12BeforeResize(IUnknown* swapChainObject)
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

extern "C" __declspec(dllexport) HRESULT WINAPI LymalinkDirect3D12AfterResize(IUnknown* swapChainObject, const char* apiName, HRESULT result, IUnknown* const* presentQueue)
{
    if (s_shuttingDown.load() || !swapChainObject || !apiName)
    {
        return result;
    }

    IDXGISwapChain* swapChain = nullptr;
    if (FAILED(swapChainObject->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&swapChain))) || !swapChain)
    {
        return result;
    }

    if (SUCCEEDED(result) && presentQueue && presentQueue[0] && std::string(apiName) == "ResizeBuffers1")
    {
        CaptureSwapChainQueue(swapChain, presentQueue[0], "ResizeBuffers1");
    }

    const HRESULT adjustedResult = HandleResizeBuffersResult(apiName, result);
    ReleaseCom(swapChain);
    return adjustedResult;
}

/////////////////////////////////////////////////////////////////////

// Export for ABI compatibility; DX12 now uses process-wide MinHook detours and never mutates live swap-chain vtables
extern "C" __declspec(dllexport) BOOL WINAPI LymalinkDirect3D12InstallSwapChainHook(IUnknown* swapChainObject)
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

    ID3D12Device* device = nullptr;
    if (FAILED(swapChain->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void**>(&device))) || !device)
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
    LYMALINK_LOG("[Direct3D12OverlayLayer][Identity] I am DX12; process attach.");
    DisableThreadLibraryCalls(instance);

    HANDLE thread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    if (thread)
    {
        CloseHandle(thread);
    }
    else
    {
        LYMALINK_LOG("[Direct3D12OverlayLayer][Attach] CreateThread failed error=" + std::to_string(GetLastError()));
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
        s_hooksReady.store(false);
        if (s_ownsMinHook)
        {
            MH_Uninitialize();
            s_ownsMinHook = false;
        }
    }
    WinDxgiOverlayCoordinator::Shutdown();
    {
        std::lock_guard<std::mutex> lock(s_renderMutex);
        ShutdownImGuiLocked();
    }
    ReleaseQueueMappings();
    s_overlay.Shutdown();
}
