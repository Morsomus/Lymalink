/////////////////////////////////////////////////////////
// File: WinOverlayEntrypoint.cpp
// Date: 2026-06-28
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Windows overlay DLL entry point and
//              process attach/detach dispatch.
/////////////////////////////////////////////////////////

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>

#include "WinLogger.h"

#include <cstdint>
#include <string>

namespace
{
const char* OverlayArch()
{
#if INTPTR_MAX == INT64_MAX
    return "x64";
#else
    return "x86";
#endif
}
}

/////////////////////////////////////////////////////////////////////

#ifdef LYMALINK_OVERLAY_ATTACH_HOOKS
    extern "C" void LymalinkOverlayOnProcessAttach(HINSTANCE instance);
    extern "C" void LymalinkOverlayOnProcessDetach();
#endif

/////////////////////////////////////////////////////////////////////

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        LYMALINK_LOG(std::string("[OverlayLibrary] Loaded arch=") + OverlayArch());
#ifdef LYMALINK_OVERLAY_ATTACH_HOOKS
        LymalinkOverlayOnProcessAttach(instance);
#else
        (void)instance;
#endif
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        // Prevent deadlock on graphics runtimes while loader-lock cleanup
        if (reserved)
        {
            return TRUE;
        }

#ifdef LYMALINK_OVERLAY_ATTACH_HOOKS
        LymalinkOverlayOnProcessDetach();
#else
        (void)instance;
#endif
        LYMALINK_LOG("[OverlayLibrary] Unloaded");
    }
    else
    {
        (void)instance;
    }

    return TRUE;
}
