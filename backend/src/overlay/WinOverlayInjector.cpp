/////////////////////////////////////////////////////////
// File: WinOverlayInjector.cpp
// Date: 2026-07-03
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements Windows overlay injection.
/////////////////////////////////////////////////////////

#include "WinOverlayInjector.h"

#include "Defines.h"
#include "tools/Logger.h"

#include <windows.h>

#include <filesystem>
#include <string>

#define COMPONENT "WinOverlayInjector"

/////////////////////////////////////////////////////////////////////

static std::filesystem::path InstallDirectory()
{
    // Overlay artifacts are packaged beside lymalinkd executable
    wchar_t path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    return length == 0 || length == MAX_PATH ? std::filesystem::path{} : std::filesystem::path(path).parent_path();
}

static bool IsX86Process(HANDLE process)
{
    // IsWow64Process2 identifies target architecture on supported Windows versions
    USHORT processMachine = IMAGE_FILE_MACHINE_UNKNOWN;
    USHORT nativeMachine = IMAGE_FILE_MACHINE_UNKNOWN;
    if (IsWow64Process2(process, &processMachine, &nativeMachine))
    {
        return processMachine == IMAGE_FILE_MACHINE_I386;
    }

    // Fall back to legacy WOW64 detection when newer API is unavailable
    BOOL wow64 = FALSE;
    return IsWow64Process(process, &wow64) && wow64 != FALSE;
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool WinOverlayInjector::InjectOpenGL(uint32_t pid) const
{
    return InjectOverlayLibrary(pid, L"lymalink-overlay-opengl-x86.dll", L"lymalink-overlay-opengl-x64.dll", "OpenGL");
}

/////////////////////////////////////////////////////////////////////

bool WinOverlayInjector::InjectDirect3D9(uint32_t pid) const
{
    return InjectOverlayLibrary(pid, L"lymalink-overlay-dx9-x86.dll", L"lymalink-overlay-dx9-x64.dll", "Direct3D9");
}

/////////////////////////////////////////////////////////////////////

bool WinOverlayInjector::InjectDirect3D10(uint32_t pid) const
{
    return InjectOverlayLibrary(pid, L"lymalink-overlay-dx10-x86.dll", L"lymalink-overlay-dx10-x64.dll", "Direct3D10");
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool WinOverlayInjector::InjectOverlayLibrary(uint32_t pid, const wchar_t* x86Dll, const wchar_t* x64Dll, const char* backendName) const
{
    // Match helper and DLL architecture to target process
    HANDLE target = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!target)
    {
        LOG_BE(Urgency::Warning, "%s overlay injection skipped for pid=%u: process access denied.", backendName, pid);
        return false;
    }
    const bool x86 = IsX86Process(target);
    CloseHandle(target);

    // Both injector helper and overlay DLL must exist for target architecture
    const std::filesystem::path overlayDir = InstallDirectory() / "overlay";
    const std::filesystem::path helper = overlayDir / (x86 ? "lymalink-overlay-injector-x86.exe" : "lymalink-overlay-injector-x64.exe");
    const std::filesystem::path library = overlayDir / (x86 ? x86Dll : x64Dll);
    if (!std::filesystem::is_regular_file(helper) || !std::filesystem::is_regular_file(library))
    {
        LOG_BE(Urgency::Warning, "%s overlay injection skipped for pid=%u: matching artifacts missing.", backendName, pid);
        return false;
    }

    // Helper performs injection and reports result through its exit code
    std::wstring command = L"\"" + helper.wstring() + L"\" --pid " + std::to_wstring(pid) + L" --dll \"" + library.wstring() + L"\"";
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION child{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, overlayDir.c_str(), &startup, &child))
    {
        LOG_BE(Urgency::Warning, "%s overlay injector could not start for pid=%u.", backendName, pid);
        return false;
    }

    // Wait for helper result before releasing process handles
    WaitForSingleObject(child.hProcess, 15000);
    DWORD result = 1;
    GetExitCodeProcess(child.hProcess, &result);
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    if (result != 0)
    {
        LOG_BE(Urgency::Warning, "%s overlay injection failed for pid=%u (code=%lu).", backendName, pid, result);
        return false;
    }

    return true;
}
