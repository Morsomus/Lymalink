/////////////////////////////////////////////////////////
// File: SystemdNotify.cpp
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements SystemdNotify class for sd_notify
/////////////////////////////////////////////////////////

#include "SystemdNotify.h"
#include "Defines.h"
#include "../tools/Logger.h"

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define COMPONENT "SystemdNotify"

/////////////////////////////////////////////////////////////////////

SystemdNotify::SystemdNotify()
{
    m_available = false;
    m_socket_path = "";

    // Detect systemd notify socket once at startup
    const char* sock = std::getenv("NOTIFY_SOCKET");
    if (!sock || sock[0] == '\0')
    {
        LOG_BE(Urgency::Info, "NOTIFY_SOCKET not set - running outside systemd or Type=simple");
        return;
    }

    m_socket_path = sock;
    m_available = true;
    LOG_BE(Urgency::Debug, "NOTIFY_SOCKET=%s", m_socket_path.c_str());
}

SystemdNotify::~SystemdNotify()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void SystemdNotify::NotifyReady()
{
    // Tell systemd daemon initialization completed
    Notify("READY=1");
    LOG_BE(Urgency::Debug, "READY=1 sent");
}

/////////////////////////////////////////////////////////////////////

void SystemdNotify::NotifyStatus(const std::string& msg)
{
    // Publish status text visible in systemctl status output
    Notify("STATUS=" + msg);
    LOG_BE(Urgency::Debug, "Status updated: %s", msg.c_str());
}

/////////////////////////////////////////////////////////////////////

void SystemdNotify::NotifyStopping()
{
    // Tell systemd shutdown sequence has started
    Notify("STOPPING=1");
    LOG_BE(Urgency::Debug, "STOPPING=1 sent");
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void SystemdNotify::Notify(const std::string& payload)
{
    // No-op when daemon is not launched by systemd with Type=notify
    if (!m_available)
    {
        return;
    }
        
    // Open datagram socket for one sd_notify payload
    int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
    {
        LOG_BE(Urgency::Critical, "socket() failed: %s", strerror(errno));
        return;
    }

    // Support both abstract sockets (@...) and path sockets
    const std::string& path = m_socket_path;
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path[0] == '@')
    {
        // Abstract socket - replace '@' with null byte
        addr.sun_path[0] = '\0';
        path.copy(addr.sun_path + 1, sizeof(addr.sun_path) - 2, 1);
    }
    else
    {
        path.copy(addr.sun_path, sizeof(addr.sun_path) - 1);
    }

    // Send raw sd_notify payload then close socket immediately
    socklen_t addr_len = offsetof(struct sockaddr_un, sun_path) + (path[0] == '@' ? path.size() : path.size() + 1);
    ssize_t sent = sendto(fd, payload.c_str(), payload.size(), MSG_NOSIGNAL, reinterpret_cast<struct sockaddr*>(&addr), addr_len);
    if (sent < 0)
    {
        LOG_BE(Urgency::Critical, "sendto() failed for payload '%s': %s", payload.c_str(), strerror(errno));
    }

    close(fd);
}
