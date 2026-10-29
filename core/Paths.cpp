#include "Paths.h"

#include "Json.h"
#include "Logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QStandardPaths>
#include <QSet>

namespace Noble::Paths {

QString appDataDir()
{
    const QString value = qEnvironmentVariable("APPDATA");
    return value.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) : value;
}

QString localAppDataDir()
{
    const QString value = qEnvironmentVariable("LOCALAPPDATA");
    return value.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) : value;
}

QString dataDir()
{
    const QString override = qEnvironmentVariable("NOBLE_DATA_DIR");
    if (!override.isEmpty())
        return QDir(override).absolutePath();
    return QDir(appDataDir()).filePath(QStringLiteral("NobleLauncher"));
}

QString cacheDir()
{
    if (!qEnvironmentVariable("NOBLE_DATA_DIR").isEmpty())
        return QDir(dataDir()).filePath(QStringLiteral("cache"));
    return QDir(localAppDataDir()).filePath(QStringLiteral("NobleLauncher/cache"));
}

QString logsDir() { return QDir(dataDir()).filePath(QStringLiteral("logs")); }
QString exeDir() { return QCoreApplication::applicationDirPath(); }
QString instancesDefaultDir() { return QDir(dataDir()).filePath(QStringLiteral("instances")); }
QString runtimeDefaultDir() { return QDir(dataDir()).filePath(QStringLiteral("runtime")); }
QString skinsDir() { return QDir(dataDir()).filePath(QStringLiteral("skins")); }
QString backgroundsDir() { return QDir(dataDir()).filePath(QStringLiteral("backgrounds")); }
QString backgroundsThumbsDir() { return QDir(backgroundsDir()).filePath(QStringLiteral("thumbs")); }
QString fontsDir() { return QDir(dataDir()).filePath(QStringLiteral("fonts")); }
QString profileIconsDir() { return QDir(dataDir()).filePath(QStringLiteral("profile-icons")); }
QString instanceIconsDir() { return QDir(dataDir()).filePath(QStringLiteral("instance-icons")); }
QString optimizerDir() { return QDir(dataDir()).filePath(QStringLiteral("optimizer")); }
QString runtimeJavaDir() { return QDir(runtimeDefaultDir()).filePath(QStringLiteral("java")); }
QString tempDir() { return QDir(cacheDir()).filePath(QStringLiteral("tmp")); }

QStringList legacyDataDirs()
{
    const QString base = appDataDir();
    return {
        QDir(base).filePath(QStringLiteral("Noble Launcher")),
        QDir(base).filePath(QStringLiteral("noble-launcher")),
        QDir(base).filePath(QStringLiteral("HardLauncher")),
        QDir(base).filePath(QStringLiteral("hardlauncher")),
        QDir(base).filePath(QStringLiteral("REDCRAFT Launcher")),
        QDir(base).filePath(QStringLiteral("redcraft-launcher")),
    };
}

QStringList envSearchPaths()
{
    QStringList paths;
    paths << QDir(exeDir()).filePath(QStringLiteral(".env"));
    paths << QDir(dataDir()).filePath(QStringLiteral(".env"));
    for (const auto &legacy : legacyDataDirs())
        paths << QDir(legacy).filePath(QStringLiteral(".env"));
    paths << QDir(QDir::currentPath()).filePath(QStringLiteral(".env"));
    paths.removeDuplicates();
    return paths;
}

void ensureDirs()
{
    for (const auto &dir : {dataDir(), logsDir(), cacheDir(), tempDir(), skinsDir(), backgroundsDir(),
                            backgroundsThumbsDir(), fontsDir(), profileIconsDir(), instanceIconsDir(),
                            optimizerDir(), instancesDefaultDir(), runtimeDefaultDir()})
        QDir().mkpath(dir);
}

QJsonObject migrationLog()
{
    return readJsonObject(QDir(dataDir()).filePath(QStringLiteral("migration-log.json")));
}

QStringList migrationFiles()
{
    return {QStringLiteral("settings.json"), QStringLiteral("accounts.json"), QStringLiteral("stats.json"),
            QStringLiteral("instances-migration.json"), QStringLiteral("noble-builds-cache.json")};
}

QStringList migrationDirs()
{
    return {QStringLiteral("backgrounds"), QStringLiteral("fonts"), QStringLiteral("profile-icons"),
            QStringLiteral("instance-icons"), QStringLiteral("optimizer"), QStringLiteral("skins")};
}

QJsonObject migrateLegacyData()
{
    ensureDirs();
    const QString target = dataDir();
    if (QFileInfo::exists(QDir(target).filePath(QStringLiteral("migration-log.json"))))
        return migrationLog();

    QString sourceDir;
    for (const auto &candidate : legacyDataDirs()) {
        if (!QFileInfo(candidate).isDir())
            continue;
        bool hasMarkers = false;
        for (const auto &marker : {QStringLiteral("settings.json"), QStringLiteral("accounts.json"),
                                   QStringLiteral("instances"), QStringLiteral("runtime")}) {
            if (QFileInfo::exists(QDir(candidate).filePath(marker))) {
                hasMarkers = true;
                break;
            }
        }
        if (hasMarkers) {
            sourceDir = candidate;
            break;
        }
    }
    if (sourceDir.isEmpty())
        return {};

    QJsonArray copied;
    QStringList skipped;
    int count = 0;
    for (const auto &name : migrationFiles()) {
        const QString from = QDir(sourceDir).filePath(name);
        const QString to = QDir(target).filePath(name);
        if (!QFileInfo::exists(from) || QFileInfo::exists(to))
            continue;
        if (QFile::copy(from, to)) {
            copied.append(QJsonObject{{QStringLiteral("from"), from}, {QStringLiteral("to"), to}});
            ++count;
        } else {
            skipped << name;
        }
    }
    for (const auto &name : migrationDirs()) {
        const QString from = QDir(sourceDir).filePath(name);
        const QString to = QDir(target).filePath(name);
        if (!QFileInfo(from).isDir() || QFileInfo(to).isDir())
            continue;
        QString error;
        if (copyDirRecursive(from, to, {}, &error)) {
            copied.append(QJsonObject{{QStringLiteral("from"), from}, {QStringLiteral("to"), to}});
            ++count;
        } else {
            skipped << name;
        }
    }

    QJsonObject log{{QStringLiteral("at"), QDateTime::currentMSecsSinceEpoch()},
                    {QStringLiteral("source"), sourceDir},
                    {QStringLiteral("entries"), copied},
                    {QStringLiteral("skipped"), QJsonArray::fromStringList(skipped)},
                    {QStringLiteral("count"), count}};
    writeJsonObject(QDir(target).filePath(QStringLiteral("migration-log.json")), log);
    if (count > 0)
        Logger::info(QStringLiteral("Миграция: перенесено данных Electron-версии — %1 (источник не удалён)").arg(count));
    return log;
}

bool rollbackMigrationData(QString *error)
{
    const QJsonObject log = migrationLog();
    const QJsonArray entries = log.value(QStringLiteral("entries")).toArray();
    if (entries.isEmpty()) {
        if (error)
            *error = QStringLiteral("Журнал миграции пуст — откатывать нечего");
        return false;
    }
    int restored = 0;
    for (const auto &value : entries) {
        const QJsonObject entry = value.toObject();
        const QString from = entry.value(QStringLiteral("from")).toString();
        const QString to = entry.value(QStringLiteral("to")).toString();
        if (from.isEmpty() || to.isEmpty() || !QFileInfo::exists(from)) {
            if (error)
                *error = QStringLiteral("Исходные данные не найдены: %1").arg(from);
            continue;
        }
        QFileInfo info(from);
        if (info.isDir()) {
            QDir(to).removeRecursively();
            QString copyError;
            if (copyDirRecursive(from, to, {}, &copyError))
                ++restored;
            else if (error)
                *error = copyError;
        } else {
            QFile::remove(to);
            if (QFile::copy(from, to))
                ++restored;
            else if (error)
                *error = QStringLiteral("Не удалось восстановить %1").arg(to);
        }
    }
    Logger::info(QStringLiteral("Откат миграции: восстановлено записей — %1").arg(restored));
    return restored > 0;
}

void clearCache()
{
    QDir(cacheDir()).removeRecursively();
    QDir().mkpath(cacheDir());
    QDir().mkpath(tempDir());
}

QString uniqueSubdir(const QString &root, const QString &base)
{
    QString candidate = base;
    int index = 2;
    while (QFileInfo::exists(QDir(root).filePath(candidate)))
        candidate = base + QStringLiteral("-") + QString::number(index++);
    return QDir(root).filePath(candidate);
}

bool copyDirRecursive(const QString &from, const QString &to, const QStringList &skipNames, QString *error)
{
    QDir source(from);
    if (!source.exists()) {
        if (error)
            *error = QStringLiteral("Папка не найдена: %1").arg(from);
        return false;
    }
    if (!QDir().mkpath(to)) {
        if (error)
            *error = QStringLiteral("Не удалось создать папку: %1").arg(to);
        return false;
    }
    QDirIterator it(from, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString relative = source.relativeFilePath(path);
        if (skipNames.contains(QFileInfo(path).fileName()))
            continue;
        const QString target = QDir(to).filePath(relative);
        if (it.fileInfo().isDir()) {
            QDir().mkpath(target);
        } else {
            QDir().mkpath(QFileInfo(target).absolutePath());
            QFile::remove(target);
            if (!QFile::copy(path, target)) {
                if (error)
                    *error = QStringLiteral("Не удалось скопировать %1").arg(path);
                return false;
            }
        }
    }
    return true;
}

} // namespace Noble::Paths
