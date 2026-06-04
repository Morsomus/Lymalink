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

#include <cerrno>
#include <sstream>
#include <cstring>
#include <poll.h>

#define COMPONENT "DBusService"


/**
 * @brief D-Bus method and signal dispatch table.
 *        Maps remote procedure calls to static handler methods.
 *        SD_BUS_VTABLE_UNPRIVILEGED allows any session user to invoke these methods.
 */
const sd_bus_vtable DBusService::m_vtable[] =
{
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Ping", "", "s", DBusService::HandlePing, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("ReloadTarget", "i", "", DBusService::HandleReloadTarget, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("ReloadAllTargets", "", "", DBusService::HandleReloadAllTargets, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("ReloadConfig", "", "", DBusService::HandleReloadConfig, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("RequestActiveTargets", "", "", DBusService::HandleRequestActiveTargets, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("TestToast", "", "", DBusService::HandleTestToast, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("TestSound", "", "", DBusService::HandleTestSound, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("AchievementUnlocked", "is", 0),
    SD_BUS_SIGNAL("GameStateChanged", "ais", 0),
    SD_BUS_VTABLE_END
};

/////////////////////////////////////////////////////////////////////

DBusService::DBusService() :
    m_bus(nullptr),
    m_objectSlot(nullptr),
    m_running(false)
{
    onRequestActiveTargets = nullptr;
    onReloadAllTargets = nullptr;
    onReloadConfig = nullptr;
    onTestToast = nullptr;
    onTestSound = nullptr;
}

DBusService::~DBusService()
{
    if (m_bus)
    {
        Stop();
    }
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

Error DBusService::Init()
{
    int rc = sd_bus_open_user(&m_bus);
    if (rc < 0)
    {
        LOG_BE(Urgency::Critical, "Init failed: sd_bus_open_user: %s", strerror(-rc));
        return Error::UnknownError;
    }

    // Register our object path and interface with the D-Bus method/signal dispatch table
    rc = sd_bus_add_object_vtable(m_bus, &m_objectSlot, DBUS_OBJECT_PATH, DBUS_INTERFACE, m_vtable, this);
    if (rc < 0)
    {
        LOG_BE(Urgency::Critical, "Init failed: sd_bus_add_object_vtable: %s", strerror(-rc));
        Stop();
        return Error::UnknownError;
    }

    // Request ownership of the D-Bus bus name
    rc = sd_bus_request_name(m_bus, DBUS_BUS_NAME, 0);
    if (rc < 0)
    {
        LOG_BE(Urgency::Critical, "Init failed: sd_bus_request_name: %s", strerror(-rc));
        Stop();
        return Error::UnknownError;
    }

    m_running.store(true);
    // Spawn background thread to process D-Bus events asynchronously
    m_eventThread = std::thread(&DBusService::EventLoop, this);

    LOG_BE(Urgency::Debug, "Registered on session bus as: %s", DBUS_BUS_NAME);
    return Error::NoError;
}

/////////////////////////////////////////////////////////////////////

void DBusService::Stop()
{
    if (!m_bus)
    {
        return;
    }

    m_running.store(false);
    {
        // Protect bus operations during shutdown
        std::lock_guard<std::recursive_mutex> lock(m_busMutex);
        sd_bus_flush(m_bus);
    }
    if (m_eventThread.joinable())
    {
        m_eventThread.join();
    }

    // Release bus name and free object slot
    sd_bus_release_name(m_bus, DBUS_BUS_NAME);
    m_objectSlot = sd_bus_slot_unref(m_objectSlot);
    m_bus = sd_bus_flush_close_unref(m_bus);    // Close and unreference the bus connection

    LOG_BE(Urgency::Debug, "Stopped.");
}

/////////////////////////////////////////////////////////////////////

void DBusService::EmitAchievementUnlocked(int32_t targetId, const std::string& key)
{
    if (!m_bus)
    {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(m_busMutex);

    // Emit the signal with type signature "is" (int32, string)
    const int rc = sd_bus_emit_signal(m_bus, DBUS_OBJECT_PATH, DBUS_INTERFACE, "AchievementUnlocked", "is", targetId, key.c_str());
    if (rc < 0)
    {
        LOG_BE(Urgency::Critical, "EmitAchievementUnlocked failed: %s", strerror(-rc));
        return;
    }
    sd_bus_flush(m_bus);

    LOG_BE(Urgency::Debug, "AchievementUnlocked emitted: targetId=%d key=%s", targetId, key.c_str());
}

/////////////////////////////////////////////////////////////////////

void DBusService::EmitGameStateChanged(int32_t targetId, const std::string& state)
{
    EmitGameStateChanged(std::vector<int32_t>{targetId}, state);
}

/////////////////////////////////////////////////////////////////////

// Manually constructs the D-Bus message, handles errors, and logs the output
void DBusService::EmitGameStateChanged(const std::vector<int32_t>& targetIds, const std::string& state)
{
    if (!m_bus)
    {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(m_busMutex);
    sd_bus_message* message = nullptr;
    int rc = sd_bus_message_new_signal(m_bus, &message, DBUS_OBJECT_PATH, DBUS_INTERFACE, "GameStateChanged");
    if (rc >= 0)
    {
        rc = sd_bus_message_open_container(message, SD_BUS_TYPE_ARRAY, "i");
    }
    for (size_t i = 0; rc >= 0 && i < targetIds.size(); ++i)
    {
        rc = sd_bus_message_append(message, "i", targetIds[i]);
    }
    if (rc >= 0)
    {
        rc = sd_bus_message_close_container(message);
    }
    if (rc >= 0)
    {
        rc = sd_bus_message_append(message, "s", state.c_str());
    }
    if (rc >= 0)
    {
        rc = sd_bus_send(m_bus, message, nullptr);
    }
    if (rc >= 0)
    {
        rc = sd_bus_flush(m_bus);
    }
    message = sd_bus_message_unref(message);

    if (rc < 0)
    {
        LOG_BE(Urgency::Critical, "EmitGameStateChanged failed: %s", strerror(-rc));
        return;
    }

    // Format target IDs for logging
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

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void DBusService::EventLoop()
{
    while (m_running.load())
    {
        int busFd = -1;
        int busEvents = 0;
        {
            std::lock_guard<std::recursive_mutex> lock(m_busMutex);

            // Process pending D-Bus messages. Returns >0 if processed, 0 if none, <0 on error
            int rc = sd_bus_process(m_bus, nullptr);
            if (rc < 0)
            {
                LOG_BE(Urgency::Critical, "D-Bus event processing failed: %s", strerror(-rc));
                break;
            }
            if (rc > 0)
            {
                continue;   // Messages were processed, check again immediately
            }

            // If no pending messages, prepare to block until activity occurs
            busFd = sd_bus_get_fd(m_bus);
            busEvents = sd_bus_get_events(m_bus);
            if (busFd < 0 || busEvents < 0)
            {
                LOG_BE(Urgency::Critical, "D-Bus poll setup failed: %s", strerror(busFd < 0 ? -busFd : -busEvents));
                break;
            }
        }

        struct pollfd pollFd;
        pollFd.fd = busFd;
        pollFd.events = static_cast<short>(busEvents);
        pollFd.revents = 0;

        // Block until D-Bus activity or timeout (100ms)
        const int rc = poll(&pollFd, 1, 100);
        if (rc < 0)
        {
            if (errno == EINTR)
            {
                continue;   // Interrupted by signal, resume loop
            }
            LOG_BE(Urgency::Critical, "D-Bus poll failed: %s", strerror(errno));
            break;
        }
    }
}

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

/////////////////////////////////////////////////////////////////////

int DBusService::HandlePing(sd_bus_message* message, void* userdata, sd_bus_error*)
{
    auto* service = static_cast<DBusService*>(userdata);
    const std::string result = service->OnPing();
    return sd_bus_reply_method_return(message, "s", result.c_str());
}

/////////////////////////////////////////////////////////////////////

int DBusService::HandleReloadTarget(sd_bus_message* message, void* userdata, sd_bus_error*)
{
    int32_t targetId = 0;
    const int rc = sd_bus_message_read(message, "i", &targetId);
    if (rc < 0)
    {
        return rc;
    }

    static_cast<DBusService*>(userdata)->OnReloadTarget(targetId);
    return sd_bus_reply_method_return(message, "");
}

/////////////////////////////////////////////////////////////////////

int DBusService::HandleReloadAllTargets(sd_bus_message* message, void* userdata, sd_bus_error*)
{
    static_cast<DBusService*>(userdata)->OnReloadAllTargets();
    return sd_bus_reply_method_return(message, "");
}

/////////////////////////////////////////////////////////////////////

int DBusService::HandleReloadConfig(sd_bus_message* message, void* userdata, sd_bus_error*)
{
    static_cast<DBusService*>(userdata)->OnReloadConfig();
    return sd_bus_reply_method_return(message, "");
}

/////////////////////////////////////////////////////////////////////

int DBusService::HandleRequestActiveTargets(sd_bus_message* message, void* userdata, sd_bus_error*)
{
    static_cast<DBusService*>(userdata)->OnRequestActiveTargets();
    return sd_bus_reply_method_return(message, "");
}

/////////////////////////////////////////////////////////////////////

int DBusService::HandleTestToast(sd_bus_message* message, void* userdata, sd_bus_error*)
{
    static_cast<DBusService*>(userdata)->OnTestToast();
    return sd_bus_reply_method_return(message, "");
}

/////////////////////////////////////////////////////////////////////

int DBusService::HandleTestSound(sd_bus_message* message, void* userdata, sd_bus_error*)
{
    static_cast<DBusService*>(userdata)->OnTestSound();
    return sd_bus_reply_method_return(message, "");
}
