/////////////////////////////////////////////////////////
// File: DlsymResolver.h
// Date: 2026-06-12
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Resolves the real libc dlsym from injected
//              overlay code without recursing through hooks.
/////////////////////////////////////////////////////////

#pragma once

#include <dlfcn.h>

namespace LymalinkOverlay
{

using DlsymFn = void* (*)(void*, const char*);

inline DlsymFn ResolveRealDlsym()
{
    static DlsymFn realDlsym = []() -> DlsymFn
    {
        constexpr const char* versions[] =
        {
            "GLIBC_2.34",
            "GLIBC_2.2.5",
            "GLIBC_2.1",
            "GLIBC_2.0"
        };

        for (const char* version : versions)
        {
            void* symbol = dlvsym(RTLD_NEXT, "dlsym", version);
            if (symbol)
            {
                return reinterpret_cast<DlsymFn>(symbol);
            }
        }

        return nullptr;
    }();

    return realDlsym;
}

} // namespace LymalinkOverlay
