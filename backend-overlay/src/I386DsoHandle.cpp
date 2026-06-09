/////////////////////////////////////////////////////////
// File: I386DsoHandle.cpp
// Date: 2026-06-08
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Provides __dso_handle for i386 Flatpak SDK
//              builds where crtbeginS.o is unavailable.
/////////////////////////////////////////////////////////

extern "C"
{
    void* __dso_handle __attribute__((visibility("hidden"))) = &__dso_handle;
}
