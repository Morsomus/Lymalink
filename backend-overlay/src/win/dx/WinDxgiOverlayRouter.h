/////////////////////////////////////////////////////////
// File: WinDxgiOverlayRouter.h
// Date: 2026-07-05
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Shared DXGI swap-chain renderer detection
//              and object-hook routing for D3D10/11/12.
/////////////////////////////////////////////////////////

#pragma once

#include <windows.h>
#include <d3d10_1.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi.h>

#include <string>

namespace WinDxgiOverlayRouter
{
// Renderer owner for a DXGI swap chain
enum class Renderer
{
    Unknown,
    Direct3D10,
    Direct3D11,
    Direct3D12
};

/////////////////////////////////////////////////////////////////////

// Release a local COM reference and clear it
template <typename T>
void ReleaseLocal(T*& value)
{
    if (value)
    {
        value->Release();
        value = nullptr;
    }
}

/////////////////////////////////////////////////////////////////////

// Resolve exported callbacks from renderer DLLs - x86 __stdcall exports are decorated as _Name@bytes, while x64 exports are undecorated
inline FARPROC ResolveExport(HMODULE module, const char* exportName, int stdcallBytes)
{
    if (!module || !exportName)
    {
        return nullptr;
    }

    FARPROC proc = GetProcAddress(module, exportName);
    if (proc)
    {
        return proc;
    }

#if !defined(_WIN64)
    const std::string decoratedName = "_" + std::string(exportName) + "@" + std::to_string(stdcallBytes);
    proc = GetProcAddress(module, decoratedName.c_str());
#else
    (void)stdcallBytes;
#endif
    return proc;
}

/////////////////////////////////////////////////////////////////////

// Detect which D3D device owns the swap chain
inline Renderer DetectSwapChainRenderer(IDXGISwapChain* swapChain)
{
    if (!swapChain)
    {
        return Renderer::Unknown;
    }

    ID3D12Device* d3d12Device = nullptr;
    if (SUCCEEDED(swapChain->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void**>(&d3d12Device))) && d3d12Device)
    {
        ReleaseLocal(d3d12Device);
        return Renderer::Direct3D12;
    }

    ID3D10Device* d3d10Device = nullptr;
    if (SUCCEEDED(swapChain->GetDevice(__uuidof(ID3D10Device), reinterpret_cast<void**>(&d3d10Device))) && d3d10Device)
    {
        ReleaseLocal(d3d10Device);
        return Renderer::Direct3D10;
    }

    ID3D11Device* d3d11Device = nullptr;
    if (SUCCEEDED(swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&d3d11Device))) && d3d11Device)
    {
        ReleaseLocal(d3d11Device);
        return Renderer::Direct3D11;
    }

    return Renderer::Unknown;
}

/////////////////////////////////////////////////////////////////////

// Return the overlay DLL that owns hooks for the renderer
inline const wchar_t* ModuleName(Renderer renderer)
{
    switch (renderer)
    {
        case Renderer::Direct3D10:
#if defined(_WIN64)
            return L"lymalink-overlay-dx10-x64.dll";
#else
            return L"lymalink-overlay-dx10-x86.dll";
#endif
        case Renderer::Direct3D11:
#if defined(_WIN64)
            return L"lymalink-overlay-dx11-x64.dll";
#else
            return L"lymalink-overlay-dx11-x86.dll";
#endif
        case Renderer::Direct3D12:
#if defined(_WIN64)
            return L"lymalink-overlay-dx12-x64.dll";
#else
            return L"lymalink-overlay-dx12-x86.dll";
#endif
        default:
            return nullptr;
    }
}

/////////////////////////////////////////////////////////////////////

// Return the exported install function name for the renderer DLL
inline const char* InstallExportName(Renderer renderer)
{
    switch (renderer)
    {
        case Renderer::Direct3D10:
            return "LymalinkDirect3D10InstallSwapChainHook";
        case Renderer::Direct3D11:
            return "LymalinkDirect3D11InstallSwapChainHook";
        case Renderer::Direct3D12:
            return "LymalinkDirect3D12InstallSwapChainHook";
        default:
            return nullptr;
    }
}

/////////////////////////////////////////////////////////////////////

// Route a swap chain to the correct renderer DLL and install its object hook
inline bool InstallSwapChainHook(Renderer renderer, IDXGISwapChain* swapChain)
{
    const wchar_t* moduleName = ModuleName(renderer);
    const char* exportName = InstallExportName(renderer);
    if (!moduleName || !exportName || !swapChain)
    {
        return false;
    }

    HMODULE module = GetModuleHandleW(moduleName);
    if (!module)
    {
        return false;
    }

    using PFN_InstallSwapChainHook = BOOL(WINAPI*)(IUnknown*);
    auto installHook = reinterpret_cast<PFN_InstallSwapChainHook>(ResolveExport(module, exportName, 4));
    return installHook && installHook(swapChain) == TRUE;
}

/////////////////////////////////////////////////////////////////////

// Route Present/Present1 to the renderer DLL that owns this swap chain
inline bool RenderSwapChain(Renderer renderer, IDXGISwapChain* swapChain, const char* presentPath)
{
    const wchar_t* moduleName = ModuleName(renderer);
    const char* exportName = nullptr;

    switch (renderer)
    {
        case Renderer::Direct3D10:
            exportName = "LymalinkDirect3D10RenderSwapChain";
            break;
        case Renderer::Direct3D11:
            exportName = "LymalinkDirect3D11RenderSwapChain";
            break;
        case Renderer::Direct3D12:
            exportName = "LymalinkDirect3D12RenderSwapChain";
            break;
        default:
            break;
    }

    if (!moduleName || !exportName || !swapChain || !presentPath)
    {
        return false;
    }

    HMODULE module = GetModuleHandleW(moduleName);
    if (!module)
    {
        return false;
    }

    using PFN_RenderSwapChain = BOOL(WINAPI*)(IUnknown*, const char*);
    auto render = reinterpret_cast<PFN_RenderSwapChain>(ResolveExport(module, exportName, 8));
    return render && render(swapChain, presentPath) == TRUE;
}

/////////////////////////////////////////////////////////////////////

// Notify the renderer DLL before a resize invalidates backbuffer resources
inline bool BeforeResize(Renderer renderer, IDXGISwapChain* swapChain)
{
    const wchar_t* moduleName = ModuleName(renderer);
    const char* exportName = nullptr;

    switch (renderer)
    {
        case Renderer::Direct3D10:
            exportName = "LymalinkDirect3D10BeforeResize";
            break;
        case Renderer::Direct3D11:
            exportName = "LymalinkDirect3D11BeforeResize";
            break;
        case Renderer::Direct3D12:
            exportName = "LymalinkDirect3D12BeforeResize";
            break;
        default:
            break;
    }

    if (!moduleName || !exportName || !swapChain)
    {
        return false;
    }

    HMODULE module = GetModuleHandleW(moduleName);
    if (!module)
    {
        return false;
    }

    using PFN_BeforeResize = BOOL(WINAPI*)(IUnknown*);
    auto beforeResize = reinterpret_cast<PFN_BeforeResize>(ResolveExport(module, exportName, 4));
    return beforeResize && beforeResize(swapChain) == TRUE;
}

/////////////////////////////////////////////////////////////////////

// Notify the renderer DLL after resize returns - DX12 can adjust the HRESULT for recovery
inline HRESULT AfterResize(Renderer renderer, IDXGISwapChain* swapChain, const char* apiName, HRESULT result, IUnknown* const* presentQueue)
{
    const wchar_t* moduleName = ModuleName(renderer);
    const char* exportName = nullptr;

    switch (renderer)
    {
        case Renderer::Direct3D10:
            exportName = "LymalinkDirect3D10AfterResize";
            break;
        case Renderer::Direct3D11:
            exportName = "LymalinkDirect3D11AfterResize";
            break;
        case Renderer::Direct3D12:
            exportName = "LymalinkDirect3D12AfterResize";
            break;
        default:
            break;
    }

    if (!moduleName || !exportName || !swapChain || !apiName)
    {
        return result;
    }

    HMODULE module = GetModuleHandleW(moduleName);
    if (!module)
    {
        return result;
    }

    using PFN_AfterResize = HRESULT(WINAPI*)(IUnknown*, const char*, HRESULT, IUnknown* const*);
    auto afterResize = reinterpret_cast<PFN_AfterResize>(ResolveExport(module, exportName, 16));
    return afterResize ? afterResize(swapChain, apiName, result, presentQueue) : result;
}

}
