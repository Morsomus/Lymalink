/////////////////////////////////////////////////////////
// File: SysTray.cpp
// Date: 2026-05-15
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares SysTray
/////////////////////////////////////////////////////////

#include "SysTray.h"

#include <QApplication>
#include <QMenu>
#include <QAction>

/////////////////////////////////////////////////////////////////////

SysTray::SysTray(QObject *parent) : QObject(parent)
{
    Init();
}

SysTray::~SysTray()
{
    // Destructor
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool SysTray::IsAvailable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

/////////////////////////////////////////////////////////////////////

void SysTray::ShowToastNotification(const QString &title, const QString &message)
{
    // QSystemTrayIcon::Information
    // QSystemTrayIcon::Warning
    // QSystemTrayIcon::Critical
    // QSystemTrayIcon::NoIcon

    // Display toast
    m_trayIcon.showMessage(title, message, QSystemTrayIcon::NoIcon, 4000);
}

/////////////////////////////////////////////////////////////////////

void SysTray::SetTrayIconVisibility(bool state)
{
    if (state && !IsAvailable())
    {
        return;
    }

    if (state)
    {
        m_trayIcon.show();
    }
    else
    {
        m_trayIcon.hide();
    }
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void SysTray::Init()
{
    // Set static application icon shown by desktop tray
    m_trayIcon.setIcon(QIcon(":/qt/qml/Lymalink/res/img/BlankBackground_MFC_00002_E.png"));

    // Context menu is kept alive for the tray icon lifetime
    auto *menu = new QMenu();

    // Add required tray actions
    QAction *openAction = menu->addAction("Open");
    QAction *quitAction = menu->addAction("Quit");

    // Open action restores app window and hides tray icon
    connect(openAction, &QAction::triggered, this, [this](){
        m_trayIcon.hide();
        emit signalOpenWindow();
    });

    // Quit action exits full application from tray menu
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    // Single click opens app window from tray
    connect(&m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason){
        if (reason == QSystemTrayIcon::Trigger)
        {
            emit signalOpenWindow();
        }
    });

    m_trayIcon.setContextMenu(menu);
}
