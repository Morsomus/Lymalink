/////////////////////////////////////////////////////////
// File: Lymalinkd.h
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declaration of Lymalinkd backend service
/////////////////////////////////////////////////////////

#pragma once

#include "Error.h"
#include "service/SystemdNotify.h"

#include <atomic>
#include <signal.h>
#include <thread>

class Lymalinkd
{
public:
    Lymalinkd();
    ~Lymalinkd();

    Error Main();

private:
    SystemdNotify m_notify;
    std::atomic_bool m_running{true};
    std::thread m_signalThread;

    Error Init();
    void  Shutdown();
    void  Monitor();
    void  SignalThread(sigset_t mask);
};
