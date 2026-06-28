/////////////////////////////////////////////////////////
// File: OpenGLInjector.cpp
// Date: 2026-06-27
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Helper executable for Windows OpenGL
//              overlay DLL injection.
/////////////////////////////////////////////////////////

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
enum ExitCode
{
    // Stable exit codes let lymalinkd report which injection stage failed
    Success = 0,
    BadArguments = 2,
    MissingDll = 3,
    OpenProcessFailed = 4,
    AllocationFailed = 5,
    WriteFailed = 6,
    LoaderMissing = 7,
    RemoteThreadFailed = 8,
    RemoteLoadFailed = 9,
    WaitFailed = 10
};

/////////////////////////////////////////////////////////////////////

#ifdef LYMALINK_OVERLAY_DISABLE_LOGGING

#define Log(...) ((void)sizeof(__VA_ARGS__))

#else

std::filesystem::path ResolveLogPath()
{
    char localAppData[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
    {
        return std::filesystem::path(localAppData) / "Lymalink" / "logs" / "lymalink-overlay.log";
    }

    char tempPath[MAX_PATH]{};
    const DWORD tempLength = GetTempPathA(MAX_PATH, tempPath);
    if (tempLength > 0 && tempLength < MAX_PATH)
    {
        return std::filesystem::path(tempPath) / "lymalink-overlay.log";
    }

    return "lymalink-overlay.log";
}

/////////////////////////////////////////////////////////////////////

std::string WideToUtf8(const std::wstring& value)
{
    if (value.empty())
    {
        return {};
    }

    const int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (required <= 0)
    {
        return "<utf8-conversion-failed>";
    }

    std::string result(static_cast<size_t>(required - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), required, nullptr, nullptr);
    return result;
}

/////////////////////////////////////////////////////////////////////

std::string Timestamp()
{
    std::time_t now = std::time(nullptr);
    struct tm localTime{};
    char timestamp[64]{};
    if (localtime_s(&localTime, &now) == 0)
    {
        std::strftime(timestamp, sizeof(timestamp), "%a %b %d %H:%M:%S %Y", &localTime);
    }
    else
    {
        std::snprintf(timestamp, sizeof(timestamp), "unknown-time");
    }
    return timestamp;
}

/////////////////////////////////////////////////////////////////////

void Log(const std::wstring& message)
{
    const std::wstring line = L"[LymalinkOpenGLInjector] " + message + L"\n";
    OutputDebugStringW(line.c_str());
    std::wcerr << line;

    const std::filesystem::path logPath = ResolveLogPath();
    std::error_code ec;
    std::filesystem::create_directories(logPath.parent_path(), ec);

    std::ofstream out(logPath, std::ios::app);
    if (out.is_open())
    {
        out << Timestamp() << " pid=" << GetCurrentProcessId() << " " << WideToUtf8(line);
    }
}

#endif

/////////////////////////////////////////////////////////////////////

bool ParseUint32(const wchar_t* text, DWORD& value)
{
    // PIDs are passed as decimal text by lymalinkd; reject partial or zero parses
    if (!text || *text == L'\0')
    {
        return false;
    }

    wchar_t* end = nullptr;
    const unsigned long parsed = std::wcstoul(text, &end, 10);
    if (!end || *end != L'\0' || parsed == 0 || parsed > 0xFFFFFFFFUL)
    {
        return false;
    }

    value = static_cast<DWORD>(parsed);
    return true;
}

/////////////////////////////////////////////////////////////////////

bool ParseArguments(int argc, wchar_t** argv, DWORD& pid, std::filesystem::path& dllPath)
{
    // Keep the helper intentionally small: one target PID and one architecture-matched DLL path
    for (int i = 1; i < argc; ++i)
    {
        const std::wstring arg = argv[i] ? argv[i] : L"";
        if (arg == L"--pid" && i + 1 < argc)
        {
            if (!ParseUint32(argv[++i], pid))
            {
                return false;
            }
            continue;
        }

        if (arg == L"--dll" && i + 1 < argc)
        {
            dllPath = argv[++i];
            continue;
        }

        return false;
    }

    return pid != 0 && !dllPath.empty();
}
}

/////////////////////////////////////////////////////////////////////

int wmain(int argc, wchar_t** argv)
{
    DWORD pid = 0;
    std::filesystem::path dllPath;
    if (!ParseArguments(argc, argv, pid, dllPath))
    {
        Log(L"usage: lymalink-overlay-injector --pid <pid> --dll <path>");
        return ExitCode::BadArguments;
    }

    std::error_code ec;
    dllPath = std::filesystem::absolute(dllPath, ec);
    if (ec || !std::filesystem::is_regular_file(dllPath, ec))
    {
        Log(L"DLL path does not exist: " + dllPath.wstring());
        return ExitCode::MissingDll;
    }

    // Remote LoadLibraryW needs thread creation plus write access for the DLL path buffer
    constexpr DWORD access =
        PROCESS_CREATE_THREAD |
        PROCESS_QUERY_LIMITED_INFORMATION |
        PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE |
        PROCESS_VM_READ;
    HANDLE process = OpenProcess(access, FALSE, pid);
    if (!process)
    {
        Log(L"OpenProcess failed pid=" + std::to_wstring(pid) + L" error=" + std::to_wstring(GetLastError()));
        return ExitCode::OpenProcessFailed;
    }

    const std::wstring dllString = dllPath.wstring();
    const SIZE_T bytes = (dllString.size() + 1) * sizeof(wchar_t);
    // Allocate a UTF-16 DLL path inside the target process for LoadLibraryW
    void* remoteString = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteString)
    {
        Log(L"VirtualAllocEx failed error=" + std::to_wstring(GetLastError()));
        CloseHandle(process);
        return ExitCode::AllocationFailed;
    }

    SIZE_T written = 0;
    // Copy the absolute DLL path into the target process before starting the loader thread
    if (!WriteProcessMemory(process, remoteString, dllString.c_str(), bytes, &written) || written != bytes)
    {
        Log(L"WriteProcessMemory failed error=" + std::to_wstring(GetLastError()));
        VirtualFreeEx(process, remoteString, 0, MEM_RELEASE);
        CloseHandle(process);
        return ExitCode::WriteFailed;
    }

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    // Architecture-matched helper resolves the LoadLibraryW entry used as the remote thread start
    auto* loadLibrary = kernel32 ? reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(kernel32, "LoadLibraryW")) : nullptr;
    if (!loadLibrary)
    {
        Log(L"LoadLibraryW resolver failed error=" + std::to_wstring(GetLastError()));
        VirtualFreeEx(process, remoteString, 0, MEM_RELEASE);
        CloseHandle(process);
        return ExitCode::LoaderMissing;
    }

    // Run LoadLibraryW in the game process; the overlay DLL installs its hooks from DllMain
    HANDLE thread = CreateRemoteThread(process, nullptr, 0, loadLibrary, remoteString, 0, nullptr);
    if (!thread)
    {
        Log(L"CreateRemoteThread failed pid=" + std::to_wstring(pid) + L" error=" + std::to_wstring(GetLastError()));
        VirtualFreeEx(process, remoteString, 0, MEM_RELEASE);
        CloseHandle(process);
        return ExitCode::RemoteThreadFailed;
    }

    // Wait for LoadLibraryW to finish so lymalinkd knows whether injection actually succeeded
    const DWORD wait = WaitForSingleObject(thread, 15000);
    if (wait != WAIT_OBJECT_0)
    {
        Log(L"remote LoadLibraryW wait failed pid=" + std::to_wstring(pid) + L" wait=" + std::to_wstring(wait));
        CloseHandle(thread);
        VirtualFreeEx(process, remoteString, 0, MEM_RELEASE);
        CloseHandle(process);
        return ExitCode::WaitFailed;
    }

    DWORD remoteResult = 0;
    // LoadLibraryW returns the remote HMODULE; zero means the DLL failed to load
    GetExitCodeThread(thread, &remoteResult);
    CloseHandle(thread);
    VirtualFreeEx(process, remoteString, 0, MEM_RELEASE);
    CloseHandle(process);

    if (remoteResult == 0)
    {
        Log(L"remote LoadLibraryW returned null pid=" + std::to_wstring(pid));
        return ExitCode::RemoteLoadFailed;
    }

    Log(L"injected pid=" + std::to_wstring(pid) + L" dll=" + dllPath.wstring());
    return ExitCode::Success;
}
