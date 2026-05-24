/////////////////////////////////////////////////////////
// File: SystemdNotify.h
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares SystemdNotify which sends sd_notify messages
//              to systemd via $NOTIFY_SOCKET without libsystemd.
//              All methods are no-ops when not running under systemd.
/////////////////////////////////////////////////////////

#pragma once

#include <string>

class SystemdNotify
{
public:
    SystemdNotify();
    ~SystemdNotify();

    void NotifyReady();                         // Send READY=1, call once Init has completed successfully
    void NotifyStatus(const std::string& msg);  // Send STATUS=<msg>, visible in: systemctl --user status lymalinkd    
    void NotifyStopping();                      // Send STOPPING=1, call before shutdown begins

private:
    bool m_available;
    std::string m_socket_path;

    void Notify(const std::string& payload);    // Sends raw string to $NOTIFY_SOCKET. No-op if socket is not set.
};
