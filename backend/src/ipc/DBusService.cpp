/////////////////////////////////////////////////////////
// File: DBusService.cpp
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements DBusService class for 
//              communication between backend and frontend 
/////////////////////////////////////////////////////////

#include "DBusService.h"
#include "../tools/Logger.h"

#include <sstream>

/////////////////////////////////////////////////////////////////////

DBusService::DBusService() :
    m_connection(nullptr),
    m_object(nullptr)
{
    m_connection = nullptr;
    m_object = nullptr;
    onRequestActiveTargets = nullptr;
    onReloadAllTargets = nullptr;
    onReloadConfig = nullptr;
    onTestToast = nullptr;
}

DBusService::~DBusService()
{
    if (m_connection)
    {
        Stop();
    }
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

Error DBusService::Init()
{
    Error err = Error::NoError;

    try
    {
        // ServiceName wrapper required
        m_connection = sdbus::createSessionBusConnection(sdbus::ServiceName{DBUS_BUS_NAME});

        // ObjectPath wrapper required
        m_object = sdbus::createObject(*m_connection, sdbus::ObjectPath{DBUS_OBJECT_PATH});

        // Register frontend-callable methods and backend-emitted signals
        m_object->addVTable(
            sdbus::registerMethod("Ping")
                .withOutputParamNames("result")
                .implementedAs([this]() -> std::string
                {
                    return OnPing();
                }),

            sdbus::registerMethod("ReloadTarget")
                .withInputParamNames("targetId")
                .implementedAs([this](int32_t targetId)
                {
                    OnReloadTarget(targetId);
                }),

            sdbus::registerMethod("ReloadAllTargets")
                .implementedAs([this]()
                {
                    OnReloadAllTargets();
                }),

            sdbus::registerMethod("ReloadConfig")
                .implementedAs([this]()
                {
                    OnReloadConfig();
                }),

            sdbus::registerMethod("RequestActiveTargets")
                .implementedAs([this]()
                {
                    OnRequestActiveTargets();
                }),

            sdbus::registerMethod("TestToast")
                .implementedAs([this]()
                {
                    OnTestToast();
                }),

            sdbus::registerSignal("AchievementUnlocked")
                .withParameters<int32_t, std::string>("targetId", "key"),

            sdbus::registerSignal("GameStateChanged")
                .withParameters<std::vector<int32_t>, std::string>("targetIds", "state")

        ).forInterface(DBUS_INTERFACE);

        // Non-blocking event loop, runs in internal thread
        m_connection->enterEventLoopAsync();

        Logger::Log("[DBusService][Init] Registered on session bus as: " + std::string(DBUS_BUS_NAME));
    }
    catch (const sdbus::Error& e)
    {
        Logger::Log("[DBusService][Init] Init failed: " + std::string(e.what()));
        err = Error::UnknownError;
        return err;
    }

    return err;
}

/////////////////////////////////////////////////////////////////////

void DBusService::Stop()
{
    // Ignore duplicate stops when service was never started
    if (!m_connection)
    {
        return;
    }

    // Destroy object before leaving bus event loop
    m_object.reset();
    m_connection->leaveEventLoop();
    m_connection.reset();

    Logger::Log("[DBusService][Stop] Stopped.");
}

/////////////////////////////////////////////////////////////////////

void DBusService::EmitAchievementUnlocked(int32_t targetId, const std::string& key)
{
    // Ignore signal requests before DBus object registration
    if (!m_object)
    {
        return;
    }

    try
    {
        m_object->emitSignal(sdbus::SignalName{"AchievementUnlocked"})
            .onInterface(DBUS_INTERFACE)
            .withArguments(targetId, key);

        Logger::Log("[DBusService][EmitAchievementUnlocked] AchievementUnlocked emitted: targetId=" + std::to_string(targetId) + " key=" + key);
    }
    catch (const sdbus::Error& e)
    {
        Logger::Log("[DBusService][EmitAchievementUnlocked] EmitAchievementUnlocked failed: " + std::string(e.what()));
    }
}

/////////////////////////////////////////////////////////////////////

void DBusService::EmitGameStateChanged(int32_t targetId, const std::string& state)
{
    // Reuse vector signal variant for single target events
    EmitGameStateChanged(std::vector<int32_t>{targetId}, state);
}

/////////////////////////////////////////////////////////////////////

void DBusService::EmitGameStateChanged(const std::vector<int32_t>& targetIds, const std::string& state)
{
    // Ignore signal requests before DBus object registration
    if (!m_object)
    {
        return;
    }

    try
    {
        // Notify frontend about active/inactive target state changes
        m_object->emitSignal(sdbus::SignalName{"GameStateChanged"})
            .onInterface(DBUS_INTERFACE)
            .withArguments(targetIds, state);

        // Format target IDs for readable logs
        std::ostringstream ids;
        for (size_t i = 0; i < targetIds.size(); ++i)
        {
            if (i > 0)
            {
                ids << ",";
            }
            ids << targetIds[i];
        }

        Logger::Log("[DBusService][EmitGameStateChanged] GameStateChanged emitted: targetIds=[" + ids.str() + "] state=" + state);
    }
    catch (const sdbus::Error& e)
    {
        Logger::Log("[DBusService][EmitGameStateChanged] EmitGameStateChanged failed: " + std::string(e.what()));
    }
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

std::string DBusService::OnPing()
{
    std::string result = "pong";

    // Return health-check response to DBus caller
    return result;
}

/////////////////////////////////////////////////////////////////////

void DBusService::OnReloadTarget(int32_t targetId)
{
    // Log target-specific reload request for future handler implementation
    Logger::Log("[DBusService][OnReloadTarget] ReloadTarget received: targetId=" + std::to_string(targetId));
    // TODO: trigger immediate reload for this targetId
}

/////////////////////////////////////////////////////////////////////

void DBusService::OnReloadAllTargets()
{
    Logger::Log("[DBusService][OnReloadAllTargets] ReloadAllTargets received.");

    // Forward reload request to daemon if callback is connected
    if (onReloadAllTargets)
    {
        onReloadAllTargets();
    }
}

/////////////////////////////////////////////////////////////////////

void DBusService::OnReloadConfig()
{
    Logger::Log("[DBusService][OnReloadConfig] ReloadConfig received.");

    // Forward config reload request to daemon if callback is connected
    if (onReloadConfig)
    {
        onReloadConfig();
    }
}

/////////////////////////////////////////////////////////////////////

void DBusService::OnRequestActiveTargets()
{
    Logger::Log("[DBusService][OnRequestActiveTargets] RequestActiveTargets received.");

    // Ask daemon to publish currently active targets
    if (onRequestActiveTargets)
    {
        onRequestActiveTargets();
    }
}

/////////////////////////////////////////////////////////////////////

void DBusService::OnTestToast()
{
    Logger::Log("[DBusService][OnTestToast] TestToast received.");

    // Forward notification test request to daemon if callback is connected
    if (onTestToast)
    {
        onTestToast();
    }
}
