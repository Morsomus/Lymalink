/////////////////////////////////////////////////////////
// File: Defines.h
// Date: 2026-05-15
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Project wide definitions
/////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////
// Settings
/////////////////////////////////////////////////////////////////////

// Setting Info
// Linux .config/Lymalink/config.ini
#define ORGANIZATION        "Lymalink"
#define APPLICATION         "config"
// Config Groups
#define GROUP_APPLICATION   "Application"
#define GROUP_APPEARANCE    "Appearance"
#define GROUP_INTERFACE     "Interface"
#define GROUP_DISPLAY       "Display"
#define GROUP_BACKGROUND_SERVICE "BackgroundService"
#define GROUP_STEAM_WEB_API "SteamWebApi"
#define GROUP_DASHBOARD     "Dashboard"

/////////////////////////////////////////////////////////////////////
// Database
/////////////////////////////////////////////////////////////////////

#define DATABASE_CONNECTION_NAME        "lymalink_main"
#define DATABASE_FILE_NAME              "lymalink_database"
#define DATABASE_TABLE_EMU_GAMES        "steam_emu_games"
#define DATABASE_TABLE_EMU_ACHIEVEMENTS "steam_emu_achievements"
#define DATABASE_TABLE_GAMES            "steam_games"
#define DATABASE_TABLE_ACHIEVEMENTS     "steam_achievements"

/////////////////////////////////////////////////////////////////////
// D-Bus
/////////////////////////////////////////////////////////////////////

#define DBUS_BUS_NAME                   "org.lymalink.Daemon"
#define DBUS_OBJECT_PATH                "/org/lymalink/Daemon"
#define DBUS_INTERFACE                  "org.lymalink.Daemon"
#define DBUS_SYSTEMD_BUS_NAME           "org.freedesktop.systemd1"
#define DBUS_SYSTEMD_OBJECT_PATH        "/org/freedesktop/systemd1"
#define DBUS_SYSTEMD_MANAGER_INTERFACE  "org.freedesktop.systemd1.Manager"
#define DBUS_SYSTEMD_UNIT_INTERFACE     "org.freedesktop.systemd1.Unit"
#define DBUS_PROPERTIES_INTERFACE       "org.freedesktop.DBus.Properties"
#define DBUS_SYSTEMD_UNIT_NAME          "lymalinkd.service"

/////////////////////////////////////////////////////////////////////
// Backend Service
/////////////////////////////////////////////////////////////////////

#define DEFAULT_NOTIFICATION_SOUND      "universfield-new-notification-04-326127.ogg"

/////////////////////////////////////////////////////////////////////
// Assets
/////////////////////////////////////////////////////////////////////

#define LYMALINK_APP_ICON_PATH          "icons/hicolor/256x256/apps/lymalink.png"
#define LYMALINK_TEST_ICON_PATH         "Lymalink/64x64-lymalink-test-icon.png"

/////////////////////////////////////////////////////////////////////
// Logging
/////////////////////////////////////////////////////////////////////

#define LOG_LYMALINK_FRONTEND_MAX_SIZE      5242880
#define LOG_LYMALINK_FRONTEND_MAX_BACKUPS   1

#define LOG_BE(level, fmt, ...) Logger::Instance().Log(level, COMPONENT, __func__, fmt, ##__VA_ARGS__)
#define LOG_LYMALINK_BACKEND_MAX_SIZE       5242880
#define LOG_LYMALINK_BACKEND_MAX_BACKUPS    1
