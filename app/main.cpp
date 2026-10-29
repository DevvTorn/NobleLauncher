#include "AppController.h"

#include "core/Logger.h"
#include "core/Paths.h"
#include "core/SkinMesh.h"
#include "ProcessUtil.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QMutex>
#include <QMutexLocker>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

static void installQtMessageHandler()
{
    static QFile *logFile = nullptr;
    static QMutex mutex;
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &, const QString &message) {
        if (type != QtWarningMsg && type != QtCriticalMsg && type != QtFatalMsg)
            return;
        QMutexLocker lock(&mutex);
        if (!logFile) {
            logFile = new QFile(QDir(Noble::Paths::logsDir()).filePath(QStringLiteral("qt.log")));
            logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
        }
        if (!logFile->isOpen())
            return;
        const QString line = QStringLiteral("[%1] [%2] %3\n")
                                 .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                      type == QtWarningMsg ? QStringLiteral("WARN") : QStringLiteral("ERROR"), message);
        logFile->write(line.toUtf8());
        logFile->flush();
    });
}

int main(int argc, char *argv[])
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("Noble Launcher"));
    QGuiApplication::setApplicationName(QStringLiteral("Noble Launcher"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Noble Launcher"));
    QGuiApplication::setApplicationVersion(QStringLiteral("2.9.0"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/logo.png")));

    QString screenshotDir;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if ((args.at(i) == QLatin1String("--screenshot") || args.at(i) == QLatin1String("--screenshots"))
            && i + 1 < args.size()) {
            screenshotDir = args.at(i + 1);
        } else if (args.at(i).startsWith(QLatin1String("--screenshot="))) {
            screenshotDir = args.at(i).section(QLatin1Char('='), 1);
        }
    }
    const bool screenshotMode = !screenshotDir.isEmpty();
    const bool allowMultiple = args.contains(QStringLiteral("--allow-multi"));

    QLocalServer *localServer = nullptr;
    if (!screenshotMode && !allowMultiple) {
        QLocalSocket probe;
        probe.connectToServer(QStringLiteral("NobleLauncher-2.9.0"));
        if (probe.waitForConnected(250))
            return 0;
        QLocalServer::removeServer(QStringLiteral("NobleLauncher-2.9.0"));
        localServer = new QLocalServer(&app);
        localServer->listen(QStringLiteral("NobleLauncher-2.9.0"));
    }

    QQuickStyle::setStyle(QStringLiteral("Basic"));
    qmlRegisterType<Noble::SkinMesh>("Noble", 1, 0, "SkinMesh");

    Noble::Paths::ensureDirs();

    Noble::AppController controller;
    controller.init();
    installQtMessageHandler();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("App"), &controller);
    const QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(url);
    if (engine.rootObjects().isEmpty())
        return 1;

    QQuickWindow *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (window) {
        window->setTitle(QStringLiteral("Noble Launcher 2.9.0"));
        window->setIcon(QIcon(QStringLiteral(":/assets/logo.png")));
        controller.setMainWindow(window);
        window->show();
        if (localServer) {
            QObject::connect(localServer, &QLocalServer::newConnection, window, [localServer, window] {
                while (localServer->hasPendingConnections()) {
                    QLocalSocket *socket = localServer->nextPendingConnection();
                    socket->deleteLater();
                }
                window->showNormal();
                window->raise();
                window->requestActivate();
            });
        }
        if (screenshotMode) {
            window->resize(1264, 761);
            QTimer::singleShot(900, &controller, [&controller, screenshotDir] {
                controller.startScreenshotMode(screenshotDir);
            });
        }
    }

    QObject::connect(&app, &QGuiApplication::aboutToQuit, [&controller] {
        if (controller.settings())
            controller.settings()->saveNow();
        Noble::Logger::shutdown();
    });

    Noble::Logger::info(QStringLiteral("Память при старте: %1").arg(Noble::applicationMemoryInfo()));
    return app.exec();
}
