/////////////////////////////////////////////////////////
// File: DBusService.cpp
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements DBusService class for 
//              communication between backend and frontend 
/////////////////////////////////////////////////////////

#include "DBusService.h"
#include "Defines.h"
#include "../tools/Logger.h"

#include <sstream>

#define COMPONENT "DBusService"

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
    onStartManualAchievementDataScan = nullptr;
    onCancelManualAchievementDataScan = nullptr;
    onTestToast = nullptr;
    onTestSound = nullptr;
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

            sdbus::registerMethod("StartManualAchievementDataScan")
                .withInputParamNames("targetId")
                .implementedAs([this](int32_t targetId)
                {
                    OnStartManualAchievementDataScan(targetId);
                }),

            sdbus::registerMethod("CancelManualAchievementDataScan")
                .withInputParamNames("targetId")
                .implementedAs([this](int32_t targetId)
                {
                    OnCancelManualAchievementDataScan(targetId);
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

            sdbus::registerMethod("TestSound")
                .implementedAs([this]()
                {
                    OnTestSound();
                }),

            sdbus::registerSignal("AchievementUnlocked")
                .withParameters<int32_t, std::string>("targetId", "key"),

            sdbus::registerSignal("GameStateChanged")
                .withParameters<std::vector<int32_t>, std::string>("targetIds", "state"),

            sdbus::registerSignal("TargetDataChanged")
                .withParameters<int32_t>("targetId"),

            sdbus::registerSignal("ManualAchievementDataScanFinished")
                .withParameters<int32_t, bool, std::string>("targetId", "found", "reason")

        ).forInterface(DBUS_INTERFACE);

        // Non-blocking event loop, runs in internal thread
        m_connection->enterEventLoopAsync();

        LOG_BE(Urgency::Debug, "Registered on session bus as: %s", DBUS_BUS_NAME);
    }
    catch (const sdbus::Error& e)
    {
        LOG_BE(Urgency::Critical, "Init failed: %s", e.what());
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

    LOG_BE(Urgency::Debug, "Stopped.");
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

        LOG_BE(Urgency::Debug, "AchievementUnlocked emitted: targetId=%d key=%s", targetId, key.c_str());
    }
    catch (const sdbus::Error& e)
    {
        LOG_BE(Urgency::Critical, "EmitGameStateChanged failed: %s", e.what());
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

        LOG_BE(Urgency::Debug, "GameStateChanged emitted: targetIds=[%s] state=%s", ids.str().c_str(), state.c_str());
    }
    catch (const sdbus::Error& e)
    {
        LOG_BE(Urgency::Critical, "EmitGameStateChanged failed: %s", e.what());
    }
}

/////////////////////////////////////////////////////////////////////

void DBusService::EmitTargetDataChanged(int32_t targetId)
{
    if (!m_object)
    {
        return;
    }

    try
    {
        m_object->emitSignal(sdbus::SignalName{"TargetDataChanged"})
            .onInterface(DBUS_INTERFACE)
            .withArguments(targetId);

        LOG_BE(Urgency::Debug, "TargetDataChanged emitted: targetId=%d", targetId);
    }
    catch (const sdbus::Error& e)
    {
        LOG_BE(Urgency::Critical, "EmitTargetDataChanged failed: %s", e.what());
    }
}

/////////////////////////////////////////////////////////////////////

void DBusService::EmitManualAchievementDataScanFinished(int32_t targetId, bool found, const std::string& reason)
{
    // Ignore signal requests before DBus object registration
    if (!m_object)
    {
        return;
    }

    try
    {
        m_object->emitSignal(sdbus::SignalName{"ManualAchievementDataScanFinished"})
            .onInterface(DBUS_INTERFACE)
            .withArguments(targetId, found, reason);

        LOG_BE(Urgency::Debug, "ManualAchievementDataScanFinished emitted: targetId=%d found=%d reason=%s", targetId, found, reason.c_str());
    }
    catch (const sdbus::Error& e)
    {
        LOG_BE(Urgency::Critical, "EmitManualAchievementDataScanFinished failed: %s", e.what());
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
    LOG_BE(Urgency::Debug, "ReloadTarget received: targetId=%d", targetId);
    // TODO: trigger immediate reload for this targetId
}

/////////////////////////////////////////////////////////////////////

void DBusService::OnReloadAllTargets()
{
    LOG_BE(Urgency::Debug, "ReloadAllTargets received.");

    // Forward reload request to daemon if callback is connected
    if (onReloadAllTargets)
    {
        onReloadAllTargets();
    }
}

/////////////////////////////////////////////////////////////////////

void DBusService::OnReloadConfig()
{
    LOG_BE(Urgency::Debug, "ReloadConfig received.");

    // Forward config reload request to daemon if callback is connected
    if (onReloadConfig)
    {
        onReloadConfig();
    }
}

/////////////////////////////////////////////////////////////////////

void DBusService::OnStartManualAchievementDataScan(int32_t targetId)
{
    LOG_BE(Urgency::Debug, "StartManualAchievementDataScan received: targetId=%d", targetId);

    // Forward target-specific scan request to daemon if callback is connected
    if (onStartManualAchievementDataScan)
    {
        onStartManualAchievementDataScan(targetId);
    }
}

/////////////////////////////////////////////////////////////////////

void DBusService::OnCancelManualAchievementDataScan(int32_t targetId)
{
    LOG_BE(Urgency::Debug, "CancelManualAchievementDataScan received: targetId=%d", targetId);

    // Forward target-specific cancel request to daemon if callback is connected
    if (onCancelManualAchievementDataScan)
    {
        onCancelManualAchievementDataScan(targetId);
    }
}

/////////////////////////////////////////////////////////////////////

void DBusService::OnRequestActiveTargets()
{
    LOG_BE(Urgency::Debug, "RequestActiveTargets received.");

    // Ask daemon to publish currently active targets
    if (onRequestActiveTargets)
    {
        onRequestActiveTargets();
    }
}

/////////////////////////////////////////////////////////////////////

void DBusService::OnTestToast()
{
    LOG_BE(Urgency::Info, "TestToast received.");

    // Forward notification test request to daemon if callback is connected
    if (onTestToast)
    {
        onTestToast();
    }
}

/////////////////////////////////////////////////////////////////////

void DBusService::OnTestSound()
{
    LOG_BE(Urgency::Info, "TestSound received.");

    // Forward sound-only test request to daemon if callback is connected
    if (onTestSound)
    {
        onTestSound();
    }
}
