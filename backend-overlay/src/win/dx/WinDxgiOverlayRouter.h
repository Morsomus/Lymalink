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
    auto installHook = reinterpret_cast<PFN_InstallSwapChainHook>(GetProcAddress(module, exportName));
    return installHook && installHook(swapChain) == TRUE;
}

}
