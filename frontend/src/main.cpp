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
#include "ipc/DBusService.h"
#include "tools/Logger.h"
#include "tools/Utils.h"

#include <QApplication>
#include <QIcon>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QFontDatabase>

int main(int argc, char *argv[]) {
    // Enable QML console.log/console.debug output on Fedora
    // QT debug output is disabled by default in Fedora >= 22
    #ifdef QT_DEBUG
        qputenv("QT_LOGGING_RULES", "*.debug=true; qt.*.debug=false");
    #endif

    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("Lymalink");

    Logger &logger = Logger::Instance();
    logger.SetLogFile(Logger::DefaultLinuxLogPath(QCoreApplication::applicationName().toLower()));
    logger.Install();

    // Single instance guard
    const QString lockPath = QDir::temp().absoluteFilePath("Lymalink.lock");
    QLockFile lockFile(lockPath);
    lockFile.setStaleLockTime(5000); // Stale after 5s

    if (!lockFile.tryLock(100)) {
        // Another instance is already running
        return 0;
    }

    QFontDatabase::addApplicationFont(":/qt/qml/Lymalink/res/fonts/Inter/Inter-VariableFont_opsz,wght.ttf");
    QFontDatabase::addApplicationFont(":/qt/qml/Lymalink/res/fonts/Inter/Inter-Italic-VariableFont_opsz,wght.ttf");
    QFont defaultFont("Inter");
    defaultFont.setPixelSize(13);
    QGuiApplication::setFont(defaultFont);

    // Set titlebar icon
    app.setWindowIcon(QIcon(":/qt/qml/Lymalink/res/img/BlankBackground_MFC_00002_E.png"));

    Settings* settings = new Settings(&app);
    SysTray* sysTray = new SysTray(&app);
    DBusService* dbusService = new DBusService(&app);
    Lymalink* lymalink = new Lymalink(&app);
    const Error lymalinkInitError = lymalink->Initialize();
    if (lymalinkInitError != Error::NoError)
    {
        qCritical() << "Failed to initialize Lymalink backend:" << static_cast<int>(lymalinkInitError);
        return -1;
    }

    QQmlApplicationEngine engine;

    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));

    // Set context
    engine.rootContext()->setContextProperty("LYMALINK_APP_VERSION", QStringLiteral(LYMALINK_VERSION));
    engine.rootContext()->setContextProperty("LICENSE_MD_TEXT", Utils::ReadTextResource(QStringLiteral(":/qt/qml/Lymalink/res/docs/LICENSE.md")));
    engine.rootContext()->setContextProperty("THIRD_PARTY_LICENSES_LINUX_MD_TEXT", Utils::ReadTextResource(QStringLiteral(":/qt/qml/Lymalink/res/docs/THIRD-PARTY-LICENSES-LINUX.md")));
    engine.rootContext()->setContextProperty("CREDITS_MD_TEXT", Utils::ReadTextResource(QStringLiteral(":/qt/qml/Lymalink/res/docs/CREDITS.md")));
    engine.rootContext()->setContextProperty("USER_GUIDE_0_8_0_BETA_MD_TEXT", Utils::ReadTextResource(QStringLiteral(":/qt/qml/Lymalink/res/docs/help/user-guide-0.8.0-beta.md")));
    engine.rootContext()->setContextProperty("ctxLymalink", lymalink);
    engine.rootContext()->setContextProperty("ctxSettings", settings);
    engine.rootContext()->setContextProperty("ctxSysTray", sysTray);
    engine.rootContext()->setContextProperty("ctxDBusService", dbusService);

    // Register
    qmlRegisterSingletonType(QUrl("qrc:/qt/qml/Lymalink/Themes.qml"), "app.themes", 1, 0, "Themes");
    qmlRegisterUncreatableType<Settings>("app.settings", 1, 0, "Settings", "Constants only");

    QObject::connect(&app, &QCoreApplication::aboutToQuit, dbusService, &DBusService::StopServiceIfNotEnabled);

    // Load QML from bundled module
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app, [](QObject *obj, const QUrl &objUrl) {
            Q_UNUSED(objUrl)
            if (!obj) {
                qCritical() << "Object creation failed!";
                QCoreApplication::exit(-1);
            }
        },
        Qt::QueuedConnection
    );

    // Load main QML component
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Lymalink/Main.qml")));

    settings->TrackWindowSizeSetting(&engine);

    return app.exec();
}
