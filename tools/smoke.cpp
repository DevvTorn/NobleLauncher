#include "InstanceManager.h"
#include "JavaManager.h"
#include "LaunchSpec.h"
#include "Paths.h"
#include "Pure.h"
#include "VersionManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QTextStream>

#include <atomic>
#include <cstdio>

using namespace Noble;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);
    const QString version = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("1.21.1");
    const QString loader = argc > 2 ? QString::fromLocal8Bit(argv[2]) : QStringLiteral("Vanilla");
    const QString loaderVersion = argc > 3 ? QString::fromLocal8Bit(argv[3]) : QString();

    const QString runtime = QDir(QDir(Paths::dataDir()).absoluteFilePath(QStringLiteral("runtime"))).absolutePath();
    QDir().mkpath(runtime);
    const QString gameDir = QDir(QDir(Paths::dataDir()).absoluteFilePath(QStringLiteral("instances/smoke"))).absolutePath();
    QDir().mkpath(gameDir);

    QJsonObject instance{
        {QStringLiteral("id"), QStringLiteral("smoke")},
        {QStringLiteral("name"), QStringLiteral("Smoke Test")},
        {QStringLiteral("version"), version},
        {QStringLiteral("loader"), loader},
        {QStringLiteral("loaderVersion"), loaderVersion},
        {QStringLiteral("ram"), 2048},
        {QStringLiteral("jvmArgs"), QString()},
        {QStringLiteral("gameArgs"), QString()},
        {QStringLiteral("javaPath"), QString()},
        {QStringLiteral("gameDir"), gameDir},
        {QStringLiteral("installedVersionId"), QJsonValue::Null},
    };

    std::atomic_bool cancelled{false};
    const auto report = [&out](const QString &stage, const QString &message, double progress) {
        out << stage << " " << static_cast<int>(progress * 100) << "%: " << message << "\n";
        out.flush();
    };

    QString error;
    out << "Installing Minecraft " << version << " into " << runtime << "\n";
    const QString installed = VersionManager::installInstanceVersion(instance, runtime, QStringLiteral("auto"), QString(),
                                                                     Reporter{report}, &cancelled, &error);
    if (installed.isEmpty()) {
        err << "INSTALL FAILED: " << error << "\n";
        return 2;
    }
    instance.insert(QStringLiteral("installedVersionId"), installed);
    out << "Installed version id: " << installed << "\n";

    LaunchAccount account{QStringLiteral("Smoke"), QStringLiteral("a01e3843e5213998958af459800e4d11"),
                          QStringLiteral("0"), QStringLiteral("legacy"), QStringLiteral("offline")};
    const LaunchSpec spec = VersionManager::buildLaunchSpec(instance, runtime, QStringLiteral("auto"), QString(), account,
                                                            false, QString(), Reporter{report}, &error);
    if (spec.javaPath.isEmpty()) {
        err << "LAUNCH SPEC FAILED: " << error << "\n";
        return 3;
    }
    out << "Java: " << spec.javaPath << " (" << spec.javaMajor << ")\n";
    const QJsonObject resolved = VersionManager::loadVersion(runtime, installed);
    const QString expectedMainClass = resolved.value(QStringLiteral("mainClass")).toString();
    out << "Args: " << spec.arguments.size() << "\n";
    int mainClassIndex = -1;
    for (int i = 0; i < spec.arguments.size(); ++i) {
        if (spec.arguments.at(i) == expectedMainClass) {
            mainClassIndex = i;
            break;
        }
    }
    if (mainClassIndex < 0) {
        err << "MAIN CLASS NOT FOUND\n";
        return 4;
    }
    out << "MainClass: " << spec.arguments.at(mainClassIndex) << " (expected " << expectedMainClass << ")\n";
    bool hasClientJar = false;
    bool hasLibraries = false;
    for (int i = 0; i < mainClassIndex; ++i) {
        if (spec.arguments.at(i).contains(QLatin1String("versions/1.21.1/1.21.1.jar")))
            hasClientJar = true;
        if (spec.arguments.at(i).contains(QLatin1String("libraries")))
            hasLibraries = true;
    }
    out << "Classpath client jar: " << (hasClientJar ? "yes" : "no") << ", libraries: " << (hasLibraries ? "yes" : "no") << "\n";
    {
        const int gameStart = mainClassIndex + 1;
        QStringList gameArgs = spec.arguments.mid(gameStart);
        if (!gameArgs.contains(QStringLiteral("--username")) || !gameArgs.contains(QStringLiteral("Smoke"))) {
            err << "AUTH ARGS MISSING: " << gameArgs.join(' ') << "\n";
            return 5;
        }
    }
    out << "WorkingDir: " << spec.workingDir << "\n";

    if (app.arguments().contains(QStringLiteral("--launch"))) {
        QString consoleJava = spec.javaPath;
        if (consoleJava.endsWith(QLatin1String("javaw.exe"), Qt::CaseInsensitive))
            consoleJava.replace(consoleJava.size() - 9, 9, QStringLiteral("java.exe"));
        QProcess process;
        process.setProgram(consoleJava);
        process.setArguments(spec.arguments);
        process.setWorkingDirectory(spec.workingDir);
        process.setProcessChannelMode(QProcess::SeparateChannels);
        process.start();
        if (!process.waitForStarted(15000)) {
            err << "GAME FAILED TO START: " << process.errorString() << "\n";
            return 6;
        }
        out << "Game process started, pid=" << process.processId() << "\n";
        if (process.waitForFinished(25000)) {
            const QString output = QString::fromLocal8Bit(process.readAllStandardOutput())
                                   + QString::fromLocal8Bit(process.readAllStandardError());
            err << "GAME EXITED EARLY code=" << process.exitCode() << "\n" << output.right(4000) << "\n";
            return 7;
        }
        out << "GAME RUNNING AFTER 25s — OK\n";
        QStringList killArgs{QStringLiteral("/PID"), QString::number(process.processId()), QStringLiteral("/T"), QStringLiteral("/F")};
        QProcess::execute(QStringLiteral("taskkill.exe"), killArgs);
        process.waitForFinished(5000);
    }
    out << "SMOKE OK\n";
    return 0;
}
