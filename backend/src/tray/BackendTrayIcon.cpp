/////////////////////////////////////////////////////////
// File: BackendTrayIcon.cpp
// Date: 2026-07-29
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Implements daemon-owned system tray icon
/////////////////////////////////////////////////////////

#include "BackendTrayIcon.h"
#include "Defines.h"
#include "tools/Logger.h"

#include <cstdlib>
#include <filesystem>
#include <format>

#if defined(_WIN32)
    #include <QAction>
    #include <QIcon>
    #include <QMenu>
    #include <QProcess>
    #include <QString>
#else
    #include <cerrno>
    #include <cstring>
    #include <gdk-pixbuf/gdk-pixbuf.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

#define START_UI_LABEL      "Start UI"
#define QUIT_BACKEND_LABEL  "Quit Background Service"

#define COMPONENT "BackendTrayIcon"

/////////////////////////////////////////////////////////////////////

BackendTrayIcon::BackendTrayIcon() :
#if defined(_WIN32)
    m_menu(nullptr)
#else
    m_connection(nullptr),
    m_itemObject(nullptr),
    m_menuObject(nullptr),
    m_serviceName(""),
    m_iconPixmap(),
    m_menuRevision(1)
#endif
{
    onQuitBackend = nullptr;
}

BackendTrayIcon::~BackendTrayIcon()
{
    Stop();
}

/////////////////////////////////////////////////////////////////////
////////////////////////////// PUBLIC ///////////////////////////////
/////////////////////////////////////////////////////////////////////

bool BackendTrayIcon::Start(const std::string& iconPath)
{
#if defined(_WIN32)
    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
        LOG_BE(Urgency::Warning, "System tray unavailable.");
        return false;
    }

    m_menu = new QMenu();

    QAction* openAction = m_menu->addAction(START_UI_LABEL);
    QAction* quitAction = m_menu->addAction(QUIT_BACKEND_LABEL);
    QObject::connect(openAction, &QAction::triggered, &m_trayIcon, [this]() { OpenUi(); });
    QObject::connect(quitAction, &QAction::triggered, &m_trayIcon, [this]() { QuitBackend(); });

    m_trayIcon.setIcon(QIcon(QString::fromStdString(iconPath)));
    m_trayIcon.setToolTip(QStringLiteral("Lymalink Background Service"));
    m_trayIcon.setContextMenu(m_menu);
    m_trayIcon.show();
#else
    if (!LoadIconPixmap(iconPath))
    {
        LOG_BE(Urgency::Warning, "Tray icon load failed: %s", iconPath.c_str());
    }

    try
    {
        m_serviceName = std::format("org.freedesktop.StatusNotifierItem-{}-1", getpid());
        m_connection = sdbus::createSessionBusConnection(sdbus::ServiceName{m_serviceName});
        m_itemObject = sdbus::createObject(*m_connection, sdbus::ObjectPath{"/StatusNotifierItem"});
        m_menuObject = sdbus::createObject(*m_connection, sdbus::ObjectPath{"/org/lymalink/Daemon/TrayMenu"});

        // Export StatusNotifierItem properties expected by tray hosts
        m_itemObject->addVTable(
            sdbus::registerProperty("Category").withGetter([]() { return std::string("SystemServices"); }),
            sdbus::registerProperty("Id").withGetter([]() { return std::string("lymalinkd"); }),
            sdbus::registerProperty("Title").withGetter([]() { return std::string("Lymalink Background Service"); }),
            sdbus::registerProperty("Status").withGetter([]() { return std::string("Active"); }),
            sdbus::registerProperty("WindowId").withGetter([]() { return uint32_t{0}; }),
            sdbus::registerProperty("IconName").withGetter([]() { return std::string(""); }),
            sdbus::registerProperty("IconPixmap").withGetter([this]() { return m_iconPixmap; }),
            sdbus::registerProperty("OverlayIconName").withGetter([]() { return std::string(""); }),
            sdbus::registerProperty("OverlayIconPixmap").withGetter([]() { return IconPixmap{}; }),
            sdbus::registerProperty("AttentionIconName").withGetter([]() { return std::string(""); }),
            sdbus::registerProperty("AttentionIconPixmap").withGetter([]() { return IconPixmap{}; }),
            sdbus::registerProperty("AttentionMovieName").withGetter([]() { return std::string(""); }),
            sdbus::registerProperty("ToolTip").withGetter([]()
            {
                using ToolTip = sdbus::Struct<std::string, IconPixmap, std::string, std::string>;
                return ToolTip{"", IconPixmap{}, "Lymalink Background Service", "lymalinkd is running"};
            }),
            sdbus::registerProperty("ItemIsMenu").withGetter([]() { return false; }),
            sdbus::registerProperty("Menu").withGetter([]() { return sdbus::ObjectPath{"/org/lymalink/Daemon/TrayMenu"}; }),
            sdbus::registerMethod("ContextMenu").withInputParamNames("x", "y").implementedAs([](int32_t, int32_t) {}),
            sdbus::registerMethod("Activate").withInputParamNames("x", "y").implementedAs([this](int32_t, int32_t) { OpenUi(); }),
            sdbus::registerMethod("SecondaryActivate").withInputParamNames("x", "y").implementedAs([](int32_t, int32_t) {}),
            sdbus::registerMethod("Scroll").withInputParamNames("delta", "orientation").implementedAs([](int32_t, std::string) {})
        ).forInterface("org.kde.StatusNotifierItem");

        // Export dbusmenu model used by StatusNotifier hosts for context menus
        m_menuObject->addVTable(
            sdbus::registerProperty("Version").withGetter([]() { return uint32_t{3}; }),
            sdbus::registerProperty("TextDirection").withGetter([]() { return std::string("ltr"); }),
            sdbus::registerProperty("Status").withGetter([]() { return std::string("normal"); }),
            sdbus::registerProperty("IconThemePath").withGetter([]() { return std::vector<std::string>{}; }),
            sdbus::registerMethod("GetLayout")
                .withInputParamNames("parentId", "recursionDepth", "propertyNames")
                .withOutputParamNames("revision", "layout")
                .implementedAs([this](int32_t, int32_t, std::vector<std::string>)
                {
                    std::vector<sdbus::Variant> children;
                    // Menu IDs are consumed by Event() when tray host reports a click
                    children.emplace_back(MenuNode{1, BuildMenuProperties(1), {}});
                    children.emplace_back(MenuNode{2, BuildMenuProperties(2), {}});

                    return std::make_tuple(m_menuRevision, MenuNode{0, BuildMenuProperties(0), std::move(children)});
                }),
            sdbus::registerMethod("Event")
                .withInputParamNames("id", "eventId", "data", "timestamp")
                .implementedAs([this](int32_t id, const std::string& eventId, sdbus::Variant, uint32_t)
                {
                    if (eventId != "clicked")
                    {
                        return;
                    }
                    if (id == 1)
                    {
                        OpenUi();
                    }
                    else if (id == 2)
                    {
                        QuitBackend();
                    }
                }),
            sdbus::registerMethod("AboutToShow")
                .withInputParamNames("id")
                .withOutputParamNames("needUpdate")
                .implementedAs([](int32_t) { return false; }),
            sdbus::registerMethod("GetGroupProperties")
                .withInputParamNames("ids", "propertyNames")
                .withOutputParamNames("properties")
                .implementedAs([this](std::vector<int32_t> ids, std::vector<std::string>)
                {
                    std::vector<sdbus::Struct<int32_t, MenuProperties>> properties;
                    for (int32_t id : ids)
                    {
                        properties.emplace_back(id, BuildMenuProperties(id));
                    }
                    return properties;
                }),
            sdbus::registerMethod("GetProperty")
                .withInputParamNames("id", "name")
                .withOutputParamNames("value")
                .implementedAs([this](int32_t id, const std::string& name)
                {
                    const MenuProperties properties = BuildMenuProperties(id);
                    const auto it = properties.find(name);
                    return it != properties.end() ? it->second : sdbus::Variant(std::string(""));
                }),
            sdbus::registerMethod("AboutToShowGroup")
                .withInputParamNames("ids")
                .withOutputParamNames("updatesNeeded", "idErrors")
                .implementedAs([](std::vector<int32_t>)
                {
                    return std::make_tuple(std::vector<int32_t>{}, std::vector<int32_t>{});
                }),
            sdbus::registerSignal("LayoutUpdated").withParameters<uint32_t, int32_t>("revision", "parent")
        ).forInterface("com.canonical.dbusmenu");

        m_connection->enterEventLoopAsync();

        if (!RegisterStatusNotifierItem())
        {
            // Some Linux desktops do not provide a StatusNotifier watcher - daemon must still run
            LOG_BE(Urgency::Warning, "StatusNotifier watcher unavailable.");
            return false;
        }
    }
    catch (const sdbus::Error& e)
    {
        LOG_BE(Urgency::Warning, "Start failed: %s", e.what());
        Stop();
        return false;
    }
#endif

    LOG_BE(Urgency::Debug, "Started.");
    return true;
}

/////////////////////////////////////////////////////////////////////

void BackendTrayIcon::Stop()
{
#if defined(_WIN32)
    m_trayIcon.hide();
    delete m_menu;
    m_menu = nullptr;
#else
    m_menuObject.reset();
    m_itemObject.reset();

    if (m_connection)
    {
        m_connection->leaveEventLoop();
        m_connection.reset();
    }
#endif
}

/////////////////////////////////////////////////////////////////////
///////////////////////////// PRIVATE ///////////////////////////////
/////////////////////////////////////////////////////////////////////

void BackendTrayIcon::OpenUi()
{
    // Tray action intentionally launches frontend without checking if it already runs
#if defined(_WIN32)
    const QString localAppData = QString::fromLocal8Bit(std::getenv("LOCALAPPDATA") ? std::getenv("LOCALAPPDATA") : "");
    const QString frontendPath = localAppData + QStringLiteral("\\Programs\\Lymalink\\Lymalink.exe");
    if (!QProcess::startDetached(frontendPath))
    {
        LOG_BE(Urgency::Warning, "Failed to start frontend: %s", frontendPath.toStdString().c_str());
    }
#else
    const pid_t child = fork();
    if (child < 0)
    {
        LOG_BE(Urgency::Warning, "fork failed while starting frontend: %s", strerror(errno));
        return;
    }

    if (child == 0)
    {
        execlp("systemctl", "systemctl", "--user", "start", "lymalink-ui.service", static_cast<char*>(nullptr));
        _exit(127);
    }

    int status = 0;
    if (waitpid(child, &status, 0) < 0)
    {
        LOG_BE(Urgency::Warning, "waitpid failed while starting frontend: %s", strerror(errno));
        return;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        LOG_BE(Urgency::Warning, "systemctl failed while starting frontend service.");
    }
#endif
}

/////////////////////////////////////////////////////////////////////

void BackendTrayIcon::QuitBackend()
{
    // Tray quit action stops only lymalinkd, never frontend
    if (onQuitBackend)
    {
        onQuitBackend();
    }
}

/////////////////////////////////////////////////////////////////////

#if !defined(_WIN32)
bool BackendTrayIcon::LoadIconPixmap(const std::string& iconPath)
{
    GError* error = nullptr;
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file(iconPath.c_str(), &error);
    if (!pixbuf)
    {
        if (error)
        {
            LOG_BE(Urgency::Warning, "gdk_pixbuf_new_from_file failed: %s", error->message);
            g_error_free(error);
        }
        return false;
    }

    const int width = gdk_pixbuf_get_width(pixbuf);
    const int height = gdk_pixbuf_get_height(pixbuf);
    const int channels = gdk_pixbuf_get_n_channels(pixbuf);
    const int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    const bool hasAlpha = gdk_pixbuf_get_has_alpha(pixbuf);
    const guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);

    std::vector<uint8_t> argb;
    argb.reserve(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);

    // StatusNotifier IconPixmap requires packed ARGB bytes
    for (int y = 0; y < height; ++y)
    {
        const guchar* row = pixels + y * rowstride;
        for (int x = 0; x < width; ++x)
        {
            const guchar* pixel = row + x * channels;
            const uint8_t red = pixel[0];
            const uint8_t green = pixel[1];
            const uint8_t blue = pixel[2];
            const uint8_t alpha = hasAlpha ? pixel[3] : 255;

            argb.push_back(alpha);
            argb.push_back(red);
            argb.push_back(green);
            argb.push_back(blue);
        }
    }

    m_iconPixmap = {IconPixmap::value_type{width, height, std::move(argb)}};
    g_object_unref(pixbuf);
    return true;
}

/////////////////////////////////////////////////////////////////////

bool BackendTrayIcon::RegisterStatusNotifierItem()
{
    try
    {
        auto proxy = sdbus::createProxy(
            sdbus::ServiceName{"org.kde.StatusNotifierWatcher"},
            sdbus::ObjectPath{"/StatusNotifierWatcher"});

        proxy->callMethod("RegisterStatusNotifierItem")
            .onInterface("org.kde.StatusNotifierWatcher")
            .withArguments(m_serviceName);

        return true;
    }
    catch (const sdbus::Error& e)
    {
        LOG_BE(Urgency::Warning, "RegisterStatusNotifierItem failed: %s", e.what());
        return false;
    }
}

/////////////////////////////////////////////////////////////////////

BackendTrayIcon::MenuProperties BackendTrayIcon::BuildMenuProperties(int32_t id) const
{
    if (id == 0)
    {
        MenuProperties rootProperties;
        rootProperties.emplace("children-display", sdbus::Variant(std::string("submenu")));
        return rootProperties;
    }

    const char* label = nullptr;
    if (id == 1)
    {
        label = START_UI_LABEL;
    }
    else if (id == 2)
    {
        label = QUIT_BACKEND_LABEL;
    }
    else
    {
        return {};
    }

    MenuProperties properties;
    properties.emplace("label", sdbus::Variant(std::string(label)));
    properties.emplace("enabled", sdbus::Variant(true));
    properties.emplace("visible", sdbus::Variant(true));
    return properties;
}
#endif
