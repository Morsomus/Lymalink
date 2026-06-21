/////////////////////////////////////////////////////////
// File: WinSoundService.cpp
// Date: 2026-06-20
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements Windows notification sound adapter
/////////////////////////////////////////////////////////

#include "WinSoundService.h"
#include "Defines.h"
#include "tools/Logger.h"

#include <filesystem>
#include <utility>

#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>
#undef STB_VORBIS_HEADER_ONLY

#define STB_VORBIS_INCLUDE_STB_VORBIS_H
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#define COMPONENT "WinSoundService"

/////////////////////////////////////////////////////////////////////

WinSoundService::WinSoundService() :
    m_mutex()
{
    m_soundPath = {};
    m_fallbackSoundPath = {};
}

WinSoundService::~WinSoundService()
{
    Stop();
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

Error WinSoundService::Init(std::string defaultSoundPath)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto engine = std::make_unique<ma_engine>();
    const ma_result result = ma_engine_init(nullptr, engine.get());
    if (result != MA_SUCCESS)
    {
        LOG_BE(Urgency::Critical, "Failed to initialize Miniaudio engine: %s", ma_result_description(result));
        return Error::UnknownError;
    }

    m_engine = std::move(engine);
    m_soundPath = std::move(defaultSoundPath);
    m_fallbackSoundPath = m_soundPath;

    LOG_BE(Urgency::Debug, "Ready.");
    return Error::NoError;
}

/////////////////////////////////////////////////////////////////////

void WinSoundService::Stop()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_engine)
    {
        return;
    }

    ma_engine_uninit(m_engine.get());
    m_engine.reset();
    LOG_BE(Urgency::Debug, "Stopped Miniaudio engine.");
}

/////////////////////////////////////////////////////////////////////

void WinSoundService::SetSoundPath(std::string soundPath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_soundPath = std::move(soundPath);
    LOG_BE(Urgency::Debug, "Sound path set: %s", m_soundPath.c_str());
}

/////////////////////////////////////////////////////////////////////

void WinSoundService::SetFallbackSoundPath(std::string fallbackSoundPath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_fallbackSoundPath = std::move(fallbackSoundPath);
    LOG_BE(Urgency::Debug, "Fallback sound path set: %s", m_fallbackSoundPath.c_str());
}

/////////////////////////////////////////////////////////////////////

bool WinSoundService::PlayNotificationSound()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_engine)
    {
        LOG_BE(Urgency::Warning, "Cannot play sound: service not initialized.");
        return false;
    }

    std::string soundPath = m_soundPath;
    if (soundPath.empty() || !std::filesystem::is_regular_file(soundPath))
    {
        LOG_BE(Urgency::Warning, "Sound file not found: %s", soundPath.c_str());

        if (m_fallbackSoundPath.empty() || m_fallbackSoundPath == soundPath || !std::filesystem::is_regular_file(m_fallbackSoundPath))
        {
            LOG_BE(Urgency::Critical, "Fallback sound file is also invalid or missing. Aborting playback.");
            return false;
        }

        soundPath = m_fallbackSoundPath;
        LOG_BE(Urgency::Info, "Falling back to: %s", soundPath.c_str());
    }

    const ma_result result = ma_engine_play_sound(m_engine.get(), soundPath.c_str(), nullptr);
    if (result != MA_SUCCESS)
    {
        LOG_BE(Urgency::Critical, "Playback failed: %s", ma_result_description(result));
        return false;
    }

    LOG_BE(Urgency::Debug, "Playing: %s", soundPath.c_str());
    return true;
}

// Compile stb_vorbis after all C++ headers and code. It exposes short legacy macros, which must not affect the standard library declarations above
#include <stb_vorbis.c>
