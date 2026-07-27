/////////////////////////////////////////////////////////
// File: DBusService.h
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares DBusService and exposes lymalinkd's
//              D-Bus interface on the session bus
//
// Bus name: org.lymalink.Daemon
// Object path: /org/lymalink/Daemon
//
// Methods (frontend -> backend):
//   Ping()
//   ReloadTarget(int32)
//   ReloadAllTargets()
//   ReloadConfig()
//   RequestActiveTargets()
//   TestToast()
//   TestSound()
// Signals (backend -> frontend):
//   AchievementUnlocked(int32 appid, string key)
//   GameStateChanged(array<int32> appids, string state)
//   TargetDataChanged(int32 appid)
/////////////////////////////////////////////////////////

#pragma once

#include "Defines.h"
#include "Error.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <vector>

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
    void EmitGameStateChanged(const std::vector<int32_t>& appids, const std::string& state);
    void EmitTargetDataChanged(int32_t appid);

    // Callbacks
    std::function<void()> onRequestActiveTargets;
    std::function<void()> onReloadAllTargets;
    std::function<void()> onReloadConfig;
    std::function<void()> onTestToast;
    std::function<void()> onTestSound;

private:
    std::unique_ptr<sdbus::IConnection> m_connection;
    std::unique_ptr<sdbus::IObject> m_object;

    // Method handlers, registered as D-Bus method implementations
    std::string OnPing();
    void OnReloadTarget(int32_t appid);
    void OnReloadAllTargets();
    void OnReloadConfig();
    void OnRequestActiveTargets();
    void OnTestToast();
    void OnTestSound();
};
