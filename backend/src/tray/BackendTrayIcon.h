/////////////////////////////////////////////////////////
// File: BackendTrayIcon.h
// Date: 2026-07-29
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Declares daemon-owned system tray icon
/////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
    #include <QSystemTrayIcon>

    class QMenu;
#else
    #include <sdbus-c++/sdbus-c++.h>
#endif

class BackendTrayIcon
{
public:
    BackendTrayIcon();
    ~BackendTrayIcon();

    bool Start(const std::string& iconPath);
    void Stop();

    std::function<void()> onQuitBackend;

private:
#if defined(_WIN32)
    QSystemTrayIcon m_trayIcon;
    QMenu* m_menu;
#else
    using IconPixmap = std::vector<sdbus::Struct<int32_t, int32_t, std::vector<uint8_t>>>;
    using MenuProperties = std::map<std::string, sdbus::Variant>;
    using MenuNode = sdbus::Struct<int32_t, MenuProperties, std::vector<sdbus::Variant>>;

    std::unique_ptr<sdbus::IConnection> m_connection;
    std::unique_ptr<sdbus::IObject> m_itemObject;
    std::unique_ptr<sdbus::IObject> m_menuObject;
    std::string m_serviceName;
    IconPixmap m_iconPixmap;
    uint32_t m_menuRevision;
#endif

    void OpenUi();
    void QuitBackend();

#if !defined(_WIN32)
    bool LoadIconPixmap(const std::string& iconPath);
    bool RegisterStatusNotifierItem();
    BackendTrayIcon::MenuProperties BuildMenuProperties(int32_t id) const;
#endif
};
