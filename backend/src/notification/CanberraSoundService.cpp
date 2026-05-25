/////////////////////////////////////////////////////////
// File: CanberraSoundService.cpp
// Date: 2026-05-25
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements playing notification sounds
//              using libcanberra
/////////////////////////////////////////////////////////

#include "CanberraSoundService.h"
#include "tools/Logger.h"

#include <filesystem>
#include <utility>

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
        Logger::Log("[CanberraSoundService][Init] Failed to create context.");
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
    Logger::Log("[CanberraSoundService][Init] Ready.");
    return err;
}

/////////////////////////////////////////////////////////////////////

void CanberraSoundService::Stop()
{
    if (m_context)
    {
        ca_context_destroy(m_context);
        m_context = nullptr;
    }
}

/////////////////////////////////////////////////////////////////////

void CanberraSoundService::SetSoundPath(std::string soundPath)
{
    std::lock_guard<std::mutex> lock(m_soundPathMutex);
    m_soundPath = std::move(soundPath);
    Logger::Log("[CanberraSoundService][SetSoundPath] Sound path set: " + m_soundPath);
}

/////////////////////////////////////////////////////////////////////

void CanberraSoundService::SetFallbackSoundPath(std::string fallbackSoundPath)
{
    std::lock_guard<std::mutex> lock(m_soundPathMutex);
    m_fallbackSoundPath = std::move(fallbackSoundPath);
    Logger::Log("[CanberraSoundService][SetFallbackSoundPath] Fallback sound path set: " + m_fallbackSoundPath);
}

/////////////////////////////////////////////////////////////////////

bool CanberraSoundService::PlayNotificationSound()
{
    bool soundPlayed = false;

    if (!m_context)
    {
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
        Logger::Log("[CanberraSoundService][PlayNotificationSound] Sound file not found: " + soundPath);

        std::error_code ec;
        if (fallbackSoundPath.empty() || fallbackSoundPath == soundPath || !fs::is_regular_file(fallbackSoundPath, ec))
        {
            return soundPlayed;
        }

        soundPath = fallbackSoundPath;
        Logger::Log("[CanberraSoundService][PlayNotificationSound] Falling back to: " + soundPath);
    }

    // Submit one notification sound playback request
    const int result = ca_context_play(m_context, 0,
        CA_PROP_MEDIA_FILENAME, soundPath.c_str(),
        CA_PROP_MEDIA_ROLE, "notification",
        nullptr
    );

    if (result != CA_SUCCESS)
    {
        Logger::Log("[CanberraSoundService][PlayNotificationSound] Playback failed: " + std::string(ca_strerror(result)));
        return soundPlayed;
    }

    Logger::Log("[CanberraSoundService][PlayNotificationSound] Playing: " + soundPath);
    soundPlayed = true;
    return soundPlayed;
}
