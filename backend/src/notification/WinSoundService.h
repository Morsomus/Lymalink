/////////////////////////////////////////////////////////
// File: WinSoundService.h
// Date: 2026-06-20
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares Windows notification sound adapter
/////////////////////////////////////////////////////////

#pragma once

#include "Error.h"
#include "notification/ISoundService.h"

#include <memory>
#include <mutex>
#include <string>

struct ma_engine;

class WinSoundService : public ISoundService
{
public:
    WinSoundService();
    ~WinSoundService() override;

    Error Init(std::string defaultSoundPath);
    void Stop();
    void SetSoundPath(std::string soundPath) override;
    void SetFallbackSoundPath(std::string fallbackSoundPath);
    bool PlayNotificationSound() override;

private:
    std::mutex m_mutex;
    std::unique_ptr<ma_engine> m_engine;
    std::string m_soundPath;
    std::string m_fallbackSoundPath;
};
