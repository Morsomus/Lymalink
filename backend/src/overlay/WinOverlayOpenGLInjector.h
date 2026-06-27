/////////////////////////////////////////////////////////
// File: WinOverlayOpenGLInjector.h
// Date: 2026-06-21
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Windows OpenGL overlay injection API.
/////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

class WinOverlayOpenGLInjector
{
public:
    // Launches architecture-matched helper to inject OpenGL overlay DLL into target process
    bool InjectOpenGL(uint32_t pid) const;
};
