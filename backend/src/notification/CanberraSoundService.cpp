/////////////////////////////////////////////////////////
// File: CanberraSoundService.cpp
// Date: 2026-05-25
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements playing notification sounds
//              using libcanberra
/////////////////////////////////////////////////////////

#include "CanberraSoundService.h"
#include "Defines.h"
#include "tools/Logger.h"

#include <filesystem>
#include <utility>

#define COMPONENT "CanberraSoundService"

namespace fs = std::filesystem;

/////////////////////////////////////////////////////////////////////

CanberraSoundService::CanberraSoundService() :
    m_soundPathMutex()
{
    m_context = nullptr;
    m_soundPath = "";
    m_fallbackSoundPath = "";
}

CanberraSoundService::~CanberraSoundService()
{
    Stop();
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

Error CanberraSoundService::Init(std::string defaultSoundPath)
{
    Error err = Error::NoError;

    // Create libcanberra context before configuring playback properties
    if (ca_context_create(&m_context) != CA_SUCCESS)
    {
        LOG_BE(Urgency::Critical, "Failed to create libcanberra context.");
        err = Error::UnknownError;
        return err;
    }

    // Set application name shown by audio backends
    ca_context_change_props(m_context,
        CA_PROP_APPLICATION_NAME, "Lymalink",
        nullptr
    );

    // Store default notification sound path from daemon configuration
    m_soundPath = std::move(defaultSoundPath);
    m_fallbackSoundPath = m_soundPath;
    
    LOG_BE(Urgency::Debug, "Ready.");
    return err;
}

/////////////////////////////////////////////////////////////////////

void CanberraSoundService::Stop()
{
    if (!m_context)
    {
        return;
    }

    ca_context_destroy(m_context);
    m_context = nullptr;
    
    LOG_BE(Urgency::Debug, "Stopped and context destroyed.");
}

/////////////////////////////////////////////////////////////////////

void CanberraSoundService::SetSoundPath(std::string soundPath)
{
    std::lock_guard<std::mutex> lock(m_soundPathMutex);
    m_soundPath = std::move(soundPath);
    LOG_BE(Urgency::Debug, "Sound path set: %s", m_soundPath.c_str());
}

/////////////////////////////////////////////////////////////////////

void CanberraSoundService::SetFallbackSoundPath(std::string fallbackSoundPath)
{
    std::lock_guard<std::mutex> lock(m_soundPathMutex);
    m_fallbackSoundPath = std::move(fallbackSoundPath);
    LOG_BE(Urgency::Debug, "Fallback sound path set: %s", m_fallbackSoundPath.c_str());
}

/////////////////////////////////////////////////////////////////////

bool CanberraSoundService::PlayNotificationSound()
{
    bool soundPlayed = false;

    if (!m_context)
    {
        LOG_BE(Urgency::Warning, "Cannot play sound: service not initialized.");
        return soundPlayed;
    }

    std::string soundPath;
    std::string fallbackSoundPath;
    {
        std::lock_guard<std::mutex> lock(m_soundPathMutex);
        soundPath = m_soundPath;
        fallbackSoundPath = m_fallbackSoundPath;
    }

    if (soundPath.empty() || !fs::exists(soundPath))
    {
        LOG_BE(Urgency::Warning, "Sound file not found: %s", soundPath.c_str());

        std::error_code ec;
        if (fallbackSoundPath.empty() || fallbackSoundPath == soundPath || !fs::is_regular_file(fallbackSoundPath, ec))
        {
            LOG_BE(Urgency::Critical, "Fallback sound file is also invalid or missing. Aborting playback.");
            return soundPlayed;
        }

        soundPath = fallbackSoundPath;
        LOG_BE(Urgency::Info, "Falling back to: %s", soundPath.c_str());
    }

    // Submit one notification sound playback request
    const int result = ca_context_play(m_context, 0,
        CA_PROP_MEDIA_FILENAME, soundPath.c_str(),
        CA_PROP_MEDIA_ROLE, "notification",
        nullptr
    );

    if (result != CA_SUCCESS)
    {
        LOG_BE(Urgency::Critical, "Playback failed: %s", ca_strerror(result));
        return soundPlayed;
    }

    LOG_BE(Urgency::Debug, "Playing: %s", soundPath.c_str());
    soundPlayed = true;
    return soundPlayed;
}