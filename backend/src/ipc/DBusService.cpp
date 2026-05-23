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

/////////////////////////////////////////////////////////////////////

DBusService::DBusService()
{
    // Constructor
}

DBusService::~DBusService()
{
    Stop();
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

Error DBusService::Init()
{
    try
    {
        // ServiceName wrapper required
        m_connection = sdbus::createSessionBusConnection(sdbus::ServiceName{DBUS_BUS_NAME});

        // ObjectPath wrapper required
        m_object = sdbus::createObject(*m_connection, sdbus::ObjectPath{DBUS_OBJECT_PATH});

        // method/signal registration via addVTable on the object
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

            sdbus::registerSignal("AchievementUnlocked")
                .withParameters<int32_t, std::string>("targetId", "key"),

            sdbus::registerSignal("GameStateChanged")
                .withParameters<int32_t, std::string>("targetId", "state")

        ).forInterface(DBUS_INTERFACE);

        // Non-blocking event loop, runs in internal thread
        m_connection->enterEventLoopAsync();

        Logger::Log("[DBusService] Registered on session bus as: " + std::string(DBUS_BUS_NAME));
    }
    catch (const sdbus::Error& e)
    {
        Logger::Log("[DBusService] Init failed: " + std::string(e.what()));
        return Error::UnknownError;
    }

    return Error::NoError;
}

/////////////////////////////////////////////////////////////////////

void DBusService::Stop()
{
    if (!m_connection) return;

    m_object.reset();
    m_connection->leaveEventLoop();
    m_connection.reset();

    Logger::Log("[DBusService] Stopped.");
}

/////////////////////////////////////////////////////////////////////

void DBusService::EmitAchievementUnlocked(int32_t targetId, const std::string& key)
{
    if (!m_object) return;
    try
    {
        m_object->emitSignal(sdbus::SignalName{"AchievementUnlocked"})
            .onInterface(DBUS_INTERFACE)
            .withArguments(targetId, key);

        Logger::Log("[DBusService] AchievementUnlocked emitted: targetId=" + std::to_string(targetId) + " key=" + key);
    }
    catch (const sdbus::Error& e)
    {
        Logger::Log("[DBusService] EmitAchievementUnlocked failed: " + std::string(e.what()));
    }
}

/////////////////////////////////////////////////////////////////////

void DBusService::EmitGameStateChanged(int32_t targetId, const std::string& state)
{
    if (!m_object) return;
    try
    {
        m_object->emitSignal(sdbus::SignalName{"GameStateChanged"})
            .onInterface(DBUS_INTERFACE)
            .withArguments(targetId, state);

        Logger::Log("[DBusService] GameStateChanged emitted: targetId=" + std::to_string(targetId) + " state=" + state);
    }
    catch (const sdbus::Error& e)
    {
        Logger::Log("[DBusService] EmitGameStateChanged failed: " + std::string(e.what()));
    }
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

std::string DBusService::OnPing()
{
    return "pong";
}

/////////////////////////////////////////////////////////////////////

void DBusService::OnReloadTarget(int32_t targetId)
{
    Logger::Log("[DBusService] ReloadTarget received: targetId=" + std::to_string(targetId));
    // TODO: trigger immediate reload for this targetId
}

/////////////////////////////////////////////////////////////////////

void DBusService::OnReloadAllTargets()
{
    Logger::Log("[DBusService] ReloadAllTargets received.");
    // TODO: Inform trigger full DB reload
}
