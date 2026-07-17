/////////////////////////////////////////////////////////
// File: WinDxgiOverlayCoordinator.h
// Date: 2026-07-17
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Process-wide DXGI Present/Resize hook
//              coordinator for D3D10/11/12 overlay DLLs.
/////////////////////////////////////////////////////////

#pragma once

namespace WinDxgiOverlayCoordinator
{
bool Start();
void Shutdown();
}

