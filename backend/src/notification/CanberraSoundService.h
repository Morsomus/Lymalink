/////////////////////////////////////////////////////////
// File: CanberraSoundService.h
// Date: 2026-05-25
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares playing notification sounds
//              using libcanberra
/////////////////////////////////////////////////////////

#pragma once

#include "ISoundService.h"
#include "Error.h"

#include <canberra.h>
#include <mutex>
#include <string>

class CanberraSoundService : public ISoundService
{
public:
    CanberraSoundService();
    ~CanberraSoundService();

    Error Init(std::string defaultSoundPath);
    void Stop();

    void SetSoundPath(std::string soundPath) override;
    bool PlayNotificationSound() override;

private:
    ca_context* m_context;
    std::mutex m_soundPathMutex;
    std::string m_soundPath;
};
