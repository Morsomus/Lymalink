/////////////////////////////////////////////////////////
// File: ISoundService.h
// Date: 2026-05-25
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Abstract interface for
//              playing notification sounds
/////////////////////////////////////////////////////////

#pragma once

#include <string>

class ISoundService
{
public:
    virtual ~ISoundService() = default;
    virtual void SetSoundPath(std::string soundPath) = 0;
    virtual bool PlayNotificationSound() = 0;
};
