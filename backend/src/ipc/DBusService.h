/////////////////////////////////////////////////////////
// File: DBusService.h
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares DBusService and exposes lymalinkd's
//              D-Bus interface on the session bus.
//
// Bus name: org.lymalink.Daemon
// Object path: /org/lymalink/Daemon
//
// Methods (frontend -> backend):
//   Ping()
//   ReloadTarget(int32)
//   ReloadAllTargets()
// Signals (backend -> frontend):
//   AchievementUnlocked(int32 appid, string key)
//   GameStateChanged(int32 appid, string state)
/////////////////////////////////////////////////////////

#pragma once

#include "Defines.h"
#include "Error.h"

#include <sdbus-c++/sdbus-c++.h>

class DBusService
{
public:
    DBusService();
    ~DBusService();

    Error Init();
    void  Stop();

    // Signals, called by backend internals to notify frontend
    void EmitAchievementUnlocked(int32_t appid, const std::string& key);
    void EmitGameStateChanged(int32_t appid, const std::string& state);

private:
    std::unique_ptr<sdbus::IConnection> m_connection;
    std::unique_ptr<sdbus::IObject> m_object;

    // Method handlers, registered as D-Bus method implementations
    std::string OnPing();
    void OnReloadTarget(int32_t appid);
    void OnReloadAllTargets();
};
