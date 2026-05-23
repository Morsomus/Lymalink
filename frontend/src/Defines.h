/////////////////////////////////////////////////////////////////////
// Settings
/////////////////////////////////////////////////////////////////////

// Setting Info
// Linux .config/Lymalink/config.ini
#define ORGANIZATION        "Lymalink"
#define APPLICATION         "config"
// Config Groups
#define GROUP_APPEARANCE    "Appearance"
#define GROUP_INTERFACE     "Interface"
#define GROUP_DISPLAY       "Display"
#define GROUP_STEAM_WEB_API "SteamWebApi"
#define GROUP_DASHBOARD     "Dashboard"

/////////////////////////////////////////////////////////////////////
// Database
/////////////////////////////////////////////////////////////////////

#define DATABASE_CONNECTION_NAME    "lymalink_main"
#define DATABASE_FILE_NAME          "lymalink_database"

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
