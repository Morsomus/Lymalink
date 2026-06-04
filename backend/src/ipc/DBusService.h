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
/////////////////////////////////////////////////////////

#pragma once

#include "Defines.h"
#include "Error.h"

#include <cstdint>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <systemd/sd-bus.h>
#include <thread>
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

    // Callbacks
    std::function<void()> onRequestActiveTargets;
    std::function<void()> onReloadAllTargets;
    std::function<void()> onReloadConfig;
    std::function<void()> onTestToast;
    std::function<void()> onTestSound;

private:
    sd_bus* m_bus;
    sd_bus_slot* m_objectSlot;
    std::thread m_eventThread;
    std::atomic<bool> m_running;
    std::recursive_mutex m_busMutex;

    void EventLoop();

    // Method handlers, registered as D-Bus method implementations
    std::string OnPing();
    void OnReloadTarget(int32_t appid);
    void OnReloadAllTargets();
    void OnReloadConfig();
    void OnRequestActiveTargets();
    void OnTestToast();
    void OnTestSound();

    static const sd_bus_vtable m_vtable[];

    static int HandlePing(sd_bus_message* message, void* userdata, sd_bus_error* error);
    static int HandleReloadTarget(sd_bus_message* message, void* userdata, sd_bus_error* error);
    static int HandleReloadAllTargets(sd_bus_message* message, void* userdata, sd_bus_error* error);
    static int HandleReloadConfig(sd_bus_message* message, void* userdata, sd_bus_error* error);
    static int HandleRequestActiveTargets(sd_bus_message* message, void* userdata, sd_bus_error* error);
    static int HandleTestToast(sd_bus_message* message, void* userdata, sd_bus_error* error);
    static int HandleTestSound(sd_bus_message* message, void* userdata, sd_bus_error* error);
};
