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

// TODO: Move elsewhere
#include <QTimer>
#include <QWindow>

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

    QQmlApplicationEngine engine;

    // Expose build version and License information to QML
    engine.rootContext()->setContextProperty("LYMALINK_APP_VERSION", QStringLiteral(LYMALINK_VERSION));
    engine.rootContext()->setContextProperty("LICENSE_APP_VERSION", QStringLiteral(LICENSE_VERSION));

    // Register singleton
    qmlRegisterSingletonType(QUrl("qrc:/qt/qml/Lymalink/Themes.qml"), "app.themes", 1, 0, "Themes");

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

    // TODO: Move elsewhere
    // Window size tracking
    const QList<QObject*> rootObjects = engine.rootObjects();
    if (!rootObjects.isEmpty()) {
        QWindow *window = qobject_cast<QWindow *>(rootObjects.first());

        if (window != nullptr) {
            QTimer *resizeTimer = new QTimer(window);
            resizeTimer->setSingleShot(true);
            resizeTimer->setInterval(2000);

            QObject::connect(window, &QWindow::widthChanged, resizeTimer, qOverload<>(&QTimer::start));
            QObject::connect(window, &QWindow::heightChanged, resizeTimer, qOverload<>(&QTimer::start));
            QObject::connect(resizeTimer, &QTimer::timeout, window, [window]() {
                qDebug() << "Size:" << window->width() << window->height();
            });
        }
    }

    return app.exec();
}
