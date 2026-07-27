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
//   StartManualAchievementDataScan(int32)
//   CancelManualAchievementDataScan(int32)
//   TestToast()
//   TestSound()
// Signals (backend -> frontend):
//   AchievementUnlocked(int32 appid, string key)
//   GameStateChanged(array<int32> appids, string state)
//   TargetDataChanged(int32 appid)
//   ManualAchievementDataScanFinished(int32 appid, bool found, string reason)
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
    void EmitManualAchievementDataScanFinished(int32_t appid, bool found, const std::string& reason);

    // Callbacks
    std::function<void()> onRequestActiveTargets;
    std::function<void()> onReloadAllTargets;
    std::function<void()> onReloadConfig;
    std::function<void(int32_t)> onStartManualAchievementDataScan;
    std::function<void(int32_t)> onCancelManualAchievementDataScan;
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
    void OnStartManualAchievementDataScan(int32_t appid);
    void OnCancelManualAchievementDataScan(int32_t appid);
    void OnRequestActiveTargets();
    void OnTestToast();
    void OnTestSound();
};
