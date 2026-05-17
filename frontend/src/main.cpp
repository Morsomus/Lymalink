/////////////////////////////////////////////////////////
// File: main.cpp
// Date: 2026-05-03
// Author: Morsomus
// Copyright: see /LICENSE
// Description: Lymalink Application entry
/////////////////////////////////////////////////////////

#include "SysTray.h"
#include "Settings.h"

#include <QApplication>
#include <QIcon>
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

    QFontDatabase::addApplicationFont(":/qt/qml/Lymalink/res/fonts/Inter/Inter-VariableFont_opsz,wght.ttf");
    QFontDatabase::addApplicationFont(":/qt/qml/Lymalink/res/fonts/Inter/Inter-Italic-VariableFont_opsz,wght.ttf");
    QFont defaultFont("Inter");
    defaultFont.setPixelSize(13);
    QGuiApplication::setFont(defaultFont);

    // Set titlebar icon
    app.setWindowIcon(QIcon(":/qt/qml/Lymalink/res/img/BlankBackground_MFC_00002_E.png"));

    Settings* settings = new Settings;
    SysTray* sysTray = new SysTray;

    QQmlApplicationEngine engine;

    // Set context
    engine.rootContext()->setContextProperty("LYMALINK_APP_VERSION", QStringLiteral(LYMALINK_VERSION));
    engine.rootContext()->setContextProperty("LICENSE_APP_VERSION", QStringLiteral(LICENSE_VERSION));
    engine.rootContext()->setContextProperty("ctxSettings", settings);
    engine.rootContext()->setContextProperty("ctxSysTray", sysTray);

    // Register
    qmlRegisterSingletonType(QUrl("qrc:/qt/qml/Lymalink/Themes.qml"), "app.themes", 1, 0, "Themes");
    qmlRegisterUncreatableType<Settings>("app.settings", 1, 0, "Settings", "Constants only");

    // Load QML from bundled module
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() {
            qCritical() << "Object creation failed!";
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection
    );

    // Load main QML component
    engine.loadFromModule("Lymalink", "Main");

    settings->TrackWindowSizeSetting(&engine);

    return app.exec();
}
