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
    m_trayIconActive = false;

    // System tray icon
    m_trayIcon.setIcon(QIcon(":/qt/qml/Lymalink/res/img/BlankBackground_MFC_00002_E.png"));

    // System tray menu
    auto *menu = new QMenu();

    // Menu options
    QAction *openAction = menu->addAction("Open");
    QAction *quitAction = menu->addAction("Quit");

    // Options functionality
    connect(openAction, &QAction::triggered, this, [this](){
        m_trayIcon.hide();
        emit signalOpenWindow();
    });
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    connect(&m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason){
        if (reason == QSystemTrayIcon::Trigger)
        {
            emit signalOpenWindow();
        }
    });

    m_trayIcon.setContextMenu(menu);
}