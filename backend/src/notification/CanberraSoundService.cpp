
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

namespace fs = std::filesystem;

/////////////////////////////////////////////////////////////////////

CanberraSoundService::CanberraSoundService()
{
    m_context = nullptr;
    m_soundPath = "";
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
    if (ca_context_create(&m_context) != CA_SUCCESS)
    {
        Logger::Log("[CanberraSoundService][Init] Failed to create context.");
        return Error::UnknownError;
    }

    ca_context_change_props(m_context,
        CA_PROP_APPLICATION_NAME, "Lymalink",
        nullptr
    );

    m_soundPath = std::move(defaultSoundPath);
    Logger::Log("[CanberraSoundService][Init] Ready.");
    return Error::NoError;
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

bool CanberraSoundService::PlayNotificationSound()
{
    if (!m_context)
    {
        return false;
    }

    std::string soundPath;
    {
        std::lock_guard<std::mutex> lock(m_soundPathMutex);
        soundPath = m_soundPath;
    }

    if (soundPath.empty() || !fs::exists(soundPath))
    {
        Logger::Log("[CanberraSoundService][PlayNotificationSound] Sound file not found: " + soundPath);
        return false;
    }

    const int result = ca_context_play(m_context, 0,
        CA_PROP_MEDIA_FILENAME, soundPath.c_str(),
        CA_PROP_MEDIA_ROLE, "notification",
        nullptr
    );

    if (result != CA_SUCCESS)
    {
        Logger::Log("[CanberraSoundService][PlayNotificationSound] Playback failed: " + std::string(ca_strerror(result)));
        return false;
    }

    Logger::Log("[CanberraSoundService][PlayNotificationSound] Playing: " + soundPath);
    return true;
}
