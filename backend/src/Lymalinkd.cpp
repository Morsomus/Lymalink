/////////////////////////////////////////////////////////
// File: Lymalinkd.cpp
// Date: 2026-05-22
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implementation of Lymalinkd backend service
/////////////////////////////////////////////////////////

#include "Lymalinkd.h"
#include "tools/Logger.h"

#include <sys/signalfd.h>

/////////////////////////////////////////////////////////////////////

Lymalinkd::Lymalinkd()
{
    // Constructor
}

Lymalinkd::~Lymalinkd()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

Error Lymalinkd::Main()
{
	// Block SIGTERM and SIGINT from normal delivery, signal thread will read them
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    if (sigprocmask(SIG_BLOCK, &mask, nullptr) < 0)
    {
        Logger::Log("[Lymalinkd] sigprocmask failed: " + std::string(strerror(errno)));
        return Error::UnknownError;
    }

    Error err = Init();
    if (err != Error::NoError)
    {
        Logger::Log("[Lymalinkd] Init failed, exiting.");
        return err;
    }

    m_running.store(true);
    m_signalThread = std::thread(&Lymalinkd::SignalThread, this, mask);

    Monitor();

    if (m_signalThread.joinable())
    {
        m_signalThread.join();
    }

    Shutdown();
    return Error::NoError;
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void Lymalinkd::Monitor()
{
    Logger::Log("[Lymalinkd] Entering main loop.");

	uint32_t tick = 0;

    while (m_running.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // 10ms tick - TODO periodic tasks (e.g. /proc poll)
        tick += 1;
        if (tick % 6000 == 0)
        {
            Logger::Log("[Lymalinkd] tick: %d", tick / 6000);
        }
    }
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::SignalThread(sigset_t mask)
{
    int sfd = signalfd(-1, &mask, SFD_CLOEXEC);
    if (sfd < 0)
    {
        Logger::Log("[Lymalinkd] signalfd failed: " + std::string(strerror(errno)));
        m_running.store(false);
        return;
    }

    while (m_running.load())
    {
        struct signalfd_siginfo info{};
        const ssize_t bytes = read(sfd, &info, sizeof(info));

        if (bytes < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            Logger::Log("[Lymalinkd] signalfd read failed: " + std::string(strerror(errno)));
            m_running.store(false);
            break;
        }

        if (bytes != sizeof(info))
        {
            Logger::Log("[Lymalinkd] signalfd read returned incomplete signal info.");
            continue;
        }

        Logger::Log("[Lymalinkd] Signal received: " + std::to_string(info.ssi_signo));
        m_running.store(false);
        break;
    }

    close(sfd);
}

/////////////////////////////////////////////////////////////////////

Error Lymalinkd::Init()
{
	Error err = Error::NoError;

    // TODO: Init DB, watchers
	err = m_dbus.Init();
    if (err != Error::NoError)
    {
        Logger::Log("[Lymalinkd] DBusService init failed.");
        return err;
    }

    // Signal systemd that we are ready (no-op if not under systemd)
    m_notify.NotifyReady();
    m_notify.NotifyStatus("Running");

	Logger::Log("[Lymalinkd] Init complete.");
    return err;
}

/////////////////////////////////////////////////////////////////////

void Lymalinkd::Shutdown()
{
    Logger::Log("[Lymalinkd] Shutdown initiated.");
    m_notify.NotifyStopping();

	m_dbus.Stop();

    // TODO: Stop watchers
    // TODO: Close DB connection

    Logger::Log("[Lymalinkd] Shutdown complete.");
}
