/////////////////////////////////////////////////////////
// File: WinOverlayInjector.h
// Date: 2026-07-03
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Windows overlay injection API.
/////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

class WinOverlayInjector
{
public:
    // Launches architecture-matched helper to inject OpenGL overlay DLL into target process
    bool InjectOpenGL(uint32_t pid) const;

    // Launches architecture-matched helper to inject Direct3D 9 overlay DLL into target process
    bool InjectDirect3D9(uint32_t pid) const;

private:
    bool InjectOverlayLibrary(uint32_t pid, const wchar_t* x86Dll, const wchar_t* x64Dll, const char* backendName) const;
};
