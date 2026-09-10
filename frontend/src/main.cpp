/////////////////////////////////////////////////////////
// File: main.cpp
// Date: 2026-05-03
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Lymalink Application entry
/////////////////////////////////////////////////////////

#include "Lymalink.h"
#include "SysTray.h"
#include "Settings.h"
#include "data/DataTransporter.h"
#include "ipc/BackendControl.h"
#if defined(Q_OS_WIN)
    #include "ipc/WinSocketService.h"
    #include <QQuickStyle>
#else
    #include "ipc/DBusService.h"
#endif
#include "tools/Logger.h"
#include "tools/Utils.h"

#include <QApplication>
#include <QIcon>
#include <QLockFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QFontDatabase>
#include <QProcess>

#define ACTIVATION_SERVER_NAME  "org.lymalink.Lymalink"

int main(int argc, char *argv[]) {
    // Enable QML console.log/console.debug output on Fedora
    // QT debug output is disabled by default in Fedora >= 22
    #ifdef QT_DEBUG
        qputenv("QT_LOGGING_RULES", "*.debug=true; qt.*.debug=false");
    #endif

    QCoreApplication::setApplicationName("Lymalink");
    QGuiApplication::setDesktopFileName("lymalink");
#if defined(Q_OS_WIN)
    QQuickStyle::setStyle(QStringLiteral("Basic"));
#endif

    // Single instance guard
    const QString lockPath = QDir::temp().absoluteFilePath("Lymalink.lock");
    QLockFile lockFile(lockPath);
    lockFile.setStaleLockTime(5000); // Stale after 5s
    if (!lockFile.tryLock(100))
    {
        // Another instance is active - request it to activate instead of creating a new process
        // Prevents spawning multiple application processes
        QLocalSocket socket;
        socket.connectToServer(QString::fromLatin1(ACTIVATION_SERVER_NAME)); 
        if (socket.waitForConnected(250))
        {
            socket.write("activate");
            socket.flush();
            socket.waitForBytesWritten(250);
        }
        return 0;
    }

    QApplication app(argc, argv);

    Logger &logger = Logger::Instance();
#if defined(Q_OS_WIN)
    logger.SetLogFile(Logger::DefaultWindowsLogPath(QCoreApplication::applicationName().toLower()));
#else
    logger.SetLogFile(Logger::DefaultLinuxLogPath(QCoreApplication::applicationName().toLower()));
#endif
    logger.Install();

    QFontDatabase::addApplicationFont(":/qt/qml/Lymalink/res/fonts/Inter/Inter-VariableFont_opsz,wght.ttf");
    QFontDatabase::addApplicationFont(":/qt/qml/Lymalink/res/fonts/Inter/Inter-Italic-VariableFont_opsz,wght.ttf");
    QFont defaultFont("Inter");
    defaultFont.setPixelSize(13);
    QGuiApplication::setFont(defaultFont);

    // Set titlebar icon
    app.setWindowIcon(QIcon(":/qt/qml/Lymalink/res/img/BlankBackground_MFC_00002_E.png"));

    Settings* settings = new Settings(&app);
    SysTray* sysTray = new SysTray(&app);
    DataTransporter* dataTransporter = new DataTransporter(settings, &app);
    Lymalink* lymalink = new Lymalink(settings, &app);
    const Error lymalinkInitError = lymalink->Initialize();
    if (lymalinkInitError != Error::NoError)
    {
        qCritical() << "Failed to initialize Lymalink business logic:" << static_cast<int>(lymalinkInitError);
        return -1;
    }
    BackendControl* backendService = nullptr;
    #if defined(Q_OS_WIN)
        backendService = new WinSocketService(&app);
    #else
        backendService = new DBusService(&app);
    #endif

    QQmlApplicationEngine engine;

    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));

    // Set context
    engine.rootContext()->setContextProperty("LYMALINK_APP_VERSION", QStringLiteral(LYMALINK_VERSION));
#if defined(Q_OS_WIN)
    engine.rootContext()->setContextProperty("OS_WIN", true);
#else
    engine.rootContext()->setContextProperty("OS_WIN", false);
#endif
    engine.rootContext()->setContextProperty("LICENSE_MD_TEXT", Utils::ReadTextResource(QStringLiteral(":/qt/qml/Lymalink/res/docs/LICENSE.md")));
#if defined(Q_OS_WIN)
    engine.rootContext()->setContextProperty("THIRD_PARTY_LICENSES_MD_TEXT", Utils::ReadTextResource(QStringLiteral(":/qt/qml/Lymalink/res/docs/THIRD-PARTY-LICENSES-WIN.md")));
#else
    engine.rootContext()->setContextProperty("THIRD_PARTY_LICENSES_MD_TEXT", Utils::ReadTextResource(QStringLiteral(":/qt/qml/Lymalink/res/docs/THIRD-PARTY-LICENSES-LINUX.md")));
#endif
    engine.rootContext()->setContextProperty("CREDITS_MD_TEXT", Utils::ReadTextResource(QStringLiteral(":/qt/qml/Lymalink/res/docs/CREDITS.md")));
#if defined(Q_OS_WIN)
    engine.rootContext()->setContextProperty("USER_GUIDE_MD_TEXT", Utils::ReadTextResource(QStringLiteral(":/qt/qml/Lymalink/res/docs/help/user-guide-0.9.x-beta-win.md")));
#else
    engine.rootContext()->setContextProperty("USER_GUIDE_MD_TEXT", Utils::ReadTextResource(QStringLiteral(":/qt/qml/Lymalink/res/docs/help/user-guide-0.9.x-beta-linux.md")));
#endif
    engine.rootContext()->setContextProperty("ctxLymalink", lymalink);
    engine.rootContext()->setContextProperty("ctxSettings", settings);
    engine.rootContext()->setContextProperty("ctxSysTray", sysTray);
    engine.rootContext()->setContextProperty("ctxBackendService", backendService);
    engine.rootContext()->setContextProperty("ctxDataTransporter", dataTransporter);

    // Register
    qmlRegisterSingletonType(QUrl("qrc:/qt/qml/Lymalink/Themes.qml"), "app.themes", 1, 0, "Themes");
    qmlRegisterUncreatableType<Settings>("app.settings", 1, 0, "Settings", "Constants only");

    QObject::connect(&app, &QCoreApplication::aboutToQuit, lymalink, &Lymalink::CancelSteamHydration);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, backendService, &BackendControl::StopServiceIfNotEnabled);

    // Handle QML loading failures with a critical exit
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() {
            qCritical() << "Object creation failed!";
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection
    );

    // Load main QML component
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Lymalink/Main.qml")));

    // Local Socket Activation Server
    // Remove previous socket file (if any) to avoid "Server already listening" errors.
    QLocalServer::removeServer(QString::fromLatin1(ACTIVATION_SERVER_NAME));
    QLocalServer activationServer;
    if (activationServer.listen(QString::fromLatin1(ACTIVATION_SERVER_NAME)))
    {
        // Handle new connections from other instances or external launchers
        QObject::connect(&activationServer, &QLocalServer::newConnection, &app, [&]() {
            while (QLocalSocket *socket = activationServer.nextPendingConnection())
            {
                // Drain and close incoming activation sockets
                socket->deleteLater();
            }

            // If QML hasn't loaded yet, ignore activation request
            if (engine.rootObjects().isEmpty())
            {
                return;
            }

            // Bring existing window to front and raise it above others
            QObject *root = engine.rootObjects().first();
            QMetaObject::invokeMethod(root, "restoreFromBackground");
        });
    }
    else
    {
        qWarning() << "Failed to create activation server:" << activationServer.errorString();
    }

    settings->TrackWindowSizeSetting(&engine);

    const int exitCode = app.exec();
    const bool restartRequested = app.property("lymalink.restartRequested").toBool();
    if (restartRequested && exitCode == 0)
    {
        const QString executablePath = QCoreApplication::applicationFilePath();
        const QString workingDirectory = QCoreApplication::applicationDirPath();
        const QStringList arguments = QCoreApplication::arguments().mid(1);

        activationServer.close();
        lockFile.unlock();

        if (!QProcess::startDetached(executablePath, arguments, workingDirectory))
        {
            qCritical() << "Failed to restart Lymalink:" << executablePath;
            return -1;
        }
    }

    return exitCode;
}
