#include "JavaManager.h"

#include "Http.h"
#include "Json.h"
#include "Logger.h"
#include "Paths.h"
#include "ProcessUtil.h"
#include "Pure.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QQueue>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>
#include <thread>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace Noble {

static QHash<QString, int> g_majorCache;
static QHash<QString, bool> g_bitsCache;
static QVector<JavaInfo> g_detectionCache;
static qint64 g_detectionAt = 0;
static QMutex g_cacheMutex;
static QMutex g_runtimeMutex;
static QString g_runtimeRoot;

static const qint64 kDetectionTtlMs = 10 * 60 * 1000;

void JavaManager::setRuntimeRoot(const QString &path)
{
    QMutexLocker lock(&g_runtimeMutex);
    g_runtimeRoot = path;
}

static QString runtimeRootForSearch()
{
    QMutexLocker lock(&g_runtimeMutex);
    return g_runtimeRoot;
}

void JavaManager::invalidateDetectionCache()
{
    QMutexLocker lock(&g_cacheMutex);
    g_detectionCache.clear();
    g_detectionAt = 0;
}

QString JavaManager::executableFor(const QString &javaHomeOrExe)
{
    if (javaHomeOrExe.isEmpty())
        return QString();
    if (javaHomeOrExe.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive) || javaHomeOrExe.endsWith(QLatin1String("/bin/java"))) {
        return QFileInfo::exists(javaHomeOrExe) ? QDir::toNativeSeparators(javaHomeOrExe) : QString();
    }
    QString exe = QDir(javaHomeOrExe).filePath(QStringLiteral("bin/javaw.exe"));
    if (QFileInfo::exists(exe))
        return QDir::toNativeSeparators(exe);
    exe = QDir(javaHomeOrExe).filePath(QStringLiteral("bin/java.exe"));
    if (QFileInfo::exists(exe))
        return QDir::toNativeSeparators(exe);
    return QString();
}

static QString consoleJavaPath(const QString &exe)
{
    if (exe.endsWith(QLatin1String("javaw.exe"), Qt::CaseInsensitive)) {
        QString java = exe;
        java.replace(java.size() - 9, 9, QStringLiteral("java.exe"));
        if (QFileInfo::exists(java))
            return java;
    }
    return exe;
}

JavaInfo JavaManager::probe(const QString &exe)
{
    JavaInfo info;
    info.path = exe;
    const QFileInfo fileInfo(exe);
    if (!fileInfo.exists() || !fileInfo.isFile() || fileInfo.size() <= 0 || !fileInfo.isReadable())
        return info;
    const QString console = consoleJavaPath(exe);
    ProcessResult result = runProcess(console, {QStringLiteral("-version")}, 8000);
    const QString output = QString::fromLocal8Bit(result.stderrData) + QLatin1Char('\n') + QString::fromLocal8Bit(result.stdoutData);
    if (!result.started || (result.exitCode != 0 && output.trimmed().isEmpty()))
        return info;
    info.major = parseJavaMajor(output);
    info.is64Bit = output.contains(QLatin1String("64-Bit"), Qt::CaseInsensitive)
        || output.contains(QLatin1String("64-bit"), Qt::CaseInsensitive)
        || output.contains(QLatin1String("amd64"), Qt::CaseInsensitive)
        || output.contains(QLatin1String("x86_64"), Qt::CaseInsensitive);
    for (const auto &line : output.split(QLatin1Char('\n'))) {
        if (!line.trimmed().isEmpty()) {
            info.versionLine = line.trimmed();
            break;
        }
    }
    {
        QMutexLocker lock(&g_cacheMutex);
        g_majorCache.insert(exe, info.major);
        g_bitsCache.insert(exe, info.is64Bit);
    }
    return info;
}

int JavaManager::majorVersion(const QString &exe)
{
    {
        QMutexLocker lock(&g_cacheMutex);
        if (g_majorCache.contains(exe))
            return g_majorCache.value(exe);
    }
    return probe(exe).major;
}

static void addRegistryBase(QVector<QString> &paths, const QString &root)
{
    QSettings settings(root, QSettings::NativeFormat);
    for (const auto &value : {QStringLiteral("JavaHome"), QStringLiteral("Path"),
                              QStringLiteral("InstallationPath"), QStringLiteral("Home")}) {
        const QString path = settings.value(value).toString();
        if (!path.isEmpty())
            paths.append(path);
    }
    for (const auto &group : settings.childGroups()) {
        for (const auto &child : {QStringLiteral("JavaHome"), QStringLiteral("Path"), QStringLiteral("InstallationPath"),
                                  QStringLiteral("MSI/Path"), QStringLiteral("hotspot/MSI/Path")}) {
            const QString path = settings.value(group + QLatin1Char('/') + child).toString();
            if (!path.isEmpty())
                paths.append(path);
        }
    }
}

static void scanJavaDir(QVector<QString> &paths, const QString &dir)
{
    if (dir.isEmpty())
        return;
    QDir root(dir);
    if (!root.exists())
        return;
    if (QFileInfo::exists(root.filePath(QStringLiteral("bin/javaw.exe")))
        || QFileInfo::exists(root.filePath(QStringLiteral("bin/java.exe")))) {
        paths.append(root.absolutePath());
        return;
    }
    const QFileInfoList subdirs = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for (const auto &sub : subdirs) {
        if (QFileInfo::exists(QDir(sub.absoluteFilePath()).filePath(QStringLiteral("bin/javaw.exe")))
            || QFileInfo::exists(QDir(sub.absoluteFilePath()).filePath(QStringLiteral("bin/java.exe"))))
            paths.append(sub.absoluteFilePath());
    }
}

static void scanJavaTree(QVector<QString> &paths, const QString &root, int maxDepth)
{
    if (root.isEmpty() || maxDepth < 0)
        return;
    QDir dir(root);
    if (!dir.exists())
        return;
    if (QFileInfo::exists(dir.filePath(QStringLiteral("bin/javaw.exe")))
        || QFileInfo::exists(dir.filePath(QStringLiteral("bin/java.exe")))) {
        paths.append(dir.absolutePath());
        return;
    }
    if (maxDepth == 0)
        return;
    for (const auto &entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name))
        scanJavaTree(paths, entry.absoluteFilePath(), maxDepth - 1);
}

static bool isExcludedDirectory(const QString &name)
{
    static const QSet<QString> excluded = {
        QStringLiteral("windows"), QStringLiteral("$recycle.bin"), QStringLiteral("system volume information"),
        QStringLiteral("recovery"), QStringLiteral("perflogs"), QStringLiteral("msocache"),
        QStringLiteral("config.msi"), QStringLiteral("$windows.~bt"), QStringLiteral("$windows.~ws"),
        QStringLiteral("windows.old"), QStringLiteral("programdata"), QStringLiteral("onedrive"),
        QStringLiteral("node_modules"), QStringLiteral(".git"), QStringLiteral(".svn"), QStringLiteral("__pycache__"),
        QStringLiteral(".gradle"), QStringLiteral(".m2"), QStringLiteral(".cache"), QStringLiteral(".cursor"),
        QStringLiteral("temp"), QStringLiteral("tmp"), QStringLiteral("cache"), QStringLiteral("caches"),
        QStringLiteral("packages"), QStringLiteral("windowsapps"), QStringLiteral("driverstore"),
        QStringLiteral("installer"), QStringLiteral("winsxs"), QStringLiteral("servicing"),
        QStringLiteral("softwaredistribution"), QStringLiteral("logs"), QStringLiteral("crash-reports")
    };
    return excluded.contains(name.toLower());
}

static void scanFixedDrives(QVector<QString> &paths)
{
#ifdef Q_OS_WIN
    const DWORD drives = GetLogicalDrives();
    for (int index = 0; index < 26; ++index) {
        if ((drives & (1u << index)) == 0)
            continue;
        QString rootPath = QString(QChar(QLatin1Char('A').unicode() + index)) + QStringLiteral(":/");
        if (GetDriveTypeW(reinterpret_cast<LPCWSTR>(rootPath.utf16())) != DRIVE_FIXED)
            continue;

        QQueue<QPair<QString, int>> queue;
        queue.enqueue({rootPath, 0});
        QElapsedTimer timer;
        timer.start();
        int visited = 0;
        while (!queue.isEmpty() && visited < 5000 && timer.elapsed() < 2500) {
            const auto item = queue.dequeue();
            ++visited;
            QDir dir(item.first);
            if (!dir.exists())
                continue;
            if (QFileInfo::exists(dir.filePath(QStringLiteral("bin/javaw.exe")))
                || QFileInfo::exists(dir.filePath(QStringLiteral("bin/java.exe")))) {
                paths.append(dir.absolutePath());
                continue;
            }
            if (item.second >= 4)
                continue;
            const QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                                                            QDir::Name);
            for (const auto &entry : entries) {
                if (isExcludedDirectory(entry.fileName()))
                    continue;
                queue.enqueue({entry.absoluteFilePath(), item.second + 1});
            }
        }
    }
#endif
}

QVector<JavaInfo> JavaManager::detectAll(const QString &extraPath)
{
    if (extraPath.isEmpty()) {
        QMutexLocker lock(&g_cacheMutex);
        if (!g_detectionCache.isEmpty() && g_detectionAt > 0
            && QDateTime::currentMSecsSinceEpoch() - g_detectionAt < kDetectionTtlMs)
            return g_detectionCache;
    }

    QVector<QString> candidates;
    const QString javaHome = qEnvironmentVariable("JAVA_HOME");
    if (!javaHome.isEmpty())
        candidates.append(javaHome);
    for (const char *variable : {"JDK_HOME", "JRE_HOME"}) {
        const QString value = qEnvironmentVariable(variable);
        if (!value.isEmpty())
            candidates.append(value);
    }
    if (!extraPath.isEmpty())
        candidates.append(extraPath);

#ifdef Q_OS_WIN
    const QStringList registryRoots = {
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\JavaSoft\\Java Runtime Environment"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\JavaSoft\\JDK"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\JavaSoft\\Java Development Kit"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\JavaSoft\\Java Runtime Environment"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\JavaSoft\\JDK"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\JavaSoft\\Java Development Kit"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Eclipse Adoptium\\JDK"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Eclipse Foundation\\JDK"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\JDK"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Azul Systems\\Zulu"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Amazon Corretto"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\BellSoft\\LibericaJDK"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\GraalVM"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\SapMachine"),
        QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Eclipse Adoptium\\JDK"),
        QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\JDK"),
        QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Azul Systems\\Zulu"),
        QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Amazon Corretto"),
    };
    for (const auto &root : registryRoots)
        addRegistryBase(candidates, root);

    const QStringList programFiles = {
        qEnvironmentVariable("ProgramFiles"), qEnvironmentVariable("ProgramFiles(x86)"),
    };
    const QStringList vendors = {
        QStringLiteral("Java"), QStringLiteral("Eclipse Adoptium"), QStringLiteral("Microsoft"),
        QStringLiteral("Zulu"), QStringLiteral("BellSoft"), QStringLiteral("Amazon Corretto"),
        QStringLiteral("AdoptOpenJDK"), QStringLiteral("Liberica"), QStringLiteral("Semeru"),
        QStringLiteral("IBM"), QStringLiteral("SapMachine"), QStringLiteral("GraalVM"),
        QStringLiteral("JetBrains"), QStringLiteral("Android")
    };
    for (const auto &base : programFiles) {
        if (base.isEmpty())
            continue;
        for (const auto &vendor : vendors)
            scanJavaTree(candidates, QDir(base).filePath(vendor), 4);
    }

    const QString programData = QDir::fromNativeSeparators(qEnvironmentVariable("ProgramData"));
    for (const auto &vendor : {QStringLiteral("Oracle/Java"), QStringLiteral("Eclipse Adoptium"),
                               QStringLiteral("Microsoft"), QStringLiteral("Zulu"), QStringLiteral("BellSoft"),
                               QStringLiteral("Amazon Corretto"), QStringLiteral("chocolatey/lib")})
        scanJavaTree(candidates, QDir(programData).filePath(vendor), 5);

    const QString localAppData = Paths::localAppDataDir();
    for (const auto &vendor : {QStringLiteral("Programs/Eclipse Adoptium"), QStringLiteral("Programs/Microsoft"),
                               QStringLiteral("Programs/Zulu"), QStringLiteral("Programs/BellSoft"),
                               QStringLiteral("Programs/Amazon Corretto"), QStringLiteral("Programs/GraalVM"),
                               QStringLiteral("JetBrains/Toolbox/apps"), QStringLiteral("Android/Android Studio")})
        scanJavaTree(candidates, QDir(localAppData).filePath(vendor), 6);
#endif

    QStringList runtimeRoots;
    const QString configuredRoot = runtimeRootForSearch();
    if (!configuredRoot.isEmpty())
        runtimeRoots << QDir(configuredRoot).filePath(QStringLiteral("java"));
    runtimeRoots << QDir(Paths::runtimeDefaultDir()).filePath(QStringLiteral("java"))
                 << QDir(QDir::fromNativeSeparators(qEnvironmentVariable("APPDATA"))).filePath(QStringLiteral(".minecraft/runtime"));
    for (const auto &legacy : Paths::legacyDataDirs())
        runtimeRoots << QDir(legacy).filePath(QStringLiteral("runtime/java"));
    for (const auto &root : runtimeRoots)
        scanJavaTree(candidates, root, 5);

    const QString userProfile = QDir::fromNativeSeparators(qEnvironmentVariable("USERPROFILE"));
    for (const auto &relative : {QStringLiteral(".jdks"), QStringLiteral(".sdkman/candidates/java"),
                                 QStringLiteral(".gradle/jdks"), QStringLiteral("scoop/apps"),
                                 QStringLiteral(".graalvm"), QStringLiteral("graalvm")})
        scanJavaTree(candidates, QDir(userProfile).filePath(relative), 5);

    const QString whereJava = QStandardPaths::findExecutable(QStringLiteral("javaw.exe"));
    if (!whereJava.isEmpty())
        candidates.append(whereJava);
    const QString whereJava2 = QStandardPaths::findExecutable(QStringLiteral("java"));
    if (!whereJava2.isEmpty())
        candidates.append(whereJava2);

    scanFixedDrives(candidates);

    QVector<JavaInfo> found;
    QSet<QString> seen;
    for (const auto &candidate : candidates) {
        const QString exe = executableFor(candidate);
        if (exe.isEmpty())
            continue;
        const QFileInfo infoFile(exe);
        if (!infoFile.isReadable())
            continue;
        const QString canonical = infoFile.canonicalFilePath();
        if (canonical.isEmpty() || seen.contains(canonical))
            continue;
        seen.insert(canonical);
        const JavaInfo info = probe(exe);
        if (info.major > 0 || !info.versionLine.isEmpty())
            found.append(info);
    }
    std::sort(found.begin(), found.end(), [](const JavaInfo &a, const JavaInfo &b) {
        if (a.major != b.major)
            return a.major > b.major;
        if (a.is64Bit != b.is64Bit)
            return a.is64Bit;
        return a.path < b.path;
    });
    if (extraPath.isEmpty()) {
        QMutexLocker lock(&g_cacheMutex);
        g_detectionCache = found;
        g_detectionAt = QDateTime::currentMSecsSinceEpoch();
    }
    return found;
}

QVariantList JavaManager::detectAllVariant(const QString &extraPath)
{
    QVariantList list;
    for (const auto &info : detectAll(extraPath)) {
        const QString bits = info.is64Bit ? QStringLiteral("64-bit") : QStringLiteral("32-bit");
        list.append(QVariantMap{{QStringLiteral("path"), info.path},
                                {QStringLiteral("versionLine"), info.versionLine},
                                {QStringLiteral("majorVersion"), info.major},
                                {QStringLiteral("is64Bit"), info.is64Bit},
                                {QStringLiteral("label"), QStringLiteral("Java %1 · %2 · %3")
                                                               .arg(info.major > 0 ? QString::number(info.major) : QStringLiteral("?"),
                                                                    bits, info.path)}});
    }
    return list;
}

PickedJava JavaManager::pickFor(const QString &mcVersion, const QString &manualPath, const QString &loader, QString *error)
{
    PickedJava picked;
    const QList<int> preferred = preferredJavaMajors(mcVersion, loader);

    if (!manualPath.isEmpty()) {
        const QString exe = executableFor(manualPath);
        if (exe.isEmpty()) {
            if (error)
                *error = QStringLiteral("Java не найдена по пути: %1").arg(manualPath);
            return {};
        }
        const JavaInfo info = probe(exe);
        if (info.versionLine.isEmpty() && info.major <= 0) {
            if (error)
                *error = QStringLiteral("Java по пути %1 не запускается").arg(manualPath);
            return {};
        }
        if (info.major > 0 && !preferred.contains(info.major)) {
            QStringList preferredText;
            for (int value : preferred)
                preferredText << QString::number(value);
            picked.warnings << QStringLiteral("Java %1 не входит в рекомендуемые для Minecraft %2 (%3), но выбрана вручную")
                                   .arg(info.major)
                                   .arg(mcVersion)
                                   .arg(preferredText.join(QLatin1Char('/')));
        }
        if (info.major <= 0)
            picked.warnings << QStringLiteral("Не удалось определить версию Java — используется ручной путь");
        picked.path = exe;
        picked.major = info.major;
        picked.is64Bit = info.is64Bit;
        if (!info.is64Bit)
            picked.warnings << QStringLiteral("Выбрана 32-битная Java — память будет ограничена 1536 МБ");
        return picked;
    }

    const QVector<JavaInfo> all = detectAll();
    QVector<JavaInfo> usable;
    for (const auto &info : all) {
        if (info.major > 0)
            usable.append(info);
    }
    if (usable.size() > 1)
        picked.warnings << QStringLiteral("Найдено Java: %1 — выбрана наиболее подходящая").arg(usable.size());

    auto choose = [&](const JavaInfo &info) {
        picked.path = info.path;
        picked.major = info.major;
        picked.is64Bit = info.is64Bit;
        if (!info.is64Bit)
            picked.warnings << QStringLiteral("Выбрана 32-битная Java — память будет ограничена 1536 МБ");
    };
    for (int wanted : preferred) {
        for (const auto &info : usable) {
            if (info.major == wanted && info.is64Bit) {
                choose(info);
                return picked;
            }
        }
        for (const auto &info : usable) {
            if (info.major == wanted) {
                choose(info);
                return picked;
            }
        }
    }
    const int needed = requiredJavaMajor(mcVersion);
    QVector<JavaInfo> newer;
    for (const auto &info : usable) {
        if (info.major > needed)
            newer.append(info);
    }
    std::sort(newer.begin(), newer.end(), [](const JavaInfo &a, const JavaInfo &b) {
        if (a.major != b.major)
            return a.major < b.major;
        return a.is64Bit && !b.is64Bit;
    });
    if (!newer.isEmpty()) {
        choose(newer.first());
        picked.warnings << QStringLiteral("Java %1 не найдена — используется Java %2").arg(needed).arg(picked.major);
        return picked;
    }

    if (error) {
        QStringList preferredText;
        for (int value : preferred)
            preferredText << QString::number(value);
        const QString loaderText = loader.isEmpty() || loader == QLatin1String("Vanilla")
            ? QString()
            : QStringLiteral(" (%1)").arg(loader);
        *error = QStringLiteral("Не найдена Java %1 для Minecraft %2%3. Установите её в «Настройки → Java» "
                                "(встроенная runtime) или укажите путь к javaw.exe. Подойдёт Adoptium Temurin: "
                                "https://adoptium.net/temurin/releases/?version=%4")
                     .arg(preferredText.join(QStringLiteral(" или ")), mcVersion, loaderText,
                          QString::number(preferred.first()));
    }
    return {};
}

bool JavaManager::validateJvm(const QString &exe, const QStringList &args, QString *error)
{
    const QString console = consoleJavaPath(exe);
    if (!QFileInfo::exists(console)) {
        if (error)
            *error = QStringLiteral("рядом с javaw.exe нет java.exe для проверки аргументов");
        return false;
    }
    QStringList probeArgs = args;
    probeArgs << QStringLiteral("-version");
    const ProcessResult result = runProcess(console, probeArgs, 10000);
    const QString output = (QString::fromLocal8Bit(result.stderrData) + QLatin1Char('\n')
                            + QString::fromLocal8Bit(result.stdoutData)).trimmed();
    if (!result.started) {
        if (error)
            *error = QStringLiteral("Java не запускается: %1").arg(result.error);
        return false;
    }
    const QString lower = output.toLower();
    const QStringList markers = {
        QStringLiteral("could not create the java virtual machine"),
        QStringLiteral("could not reserve enough space"),
        QStringLiteral("invalid maximum heap size"),
        QStringLiteral("unrecognized vm option"),
        QStringLiteral("unrecognized option"),
        QStringLiteral("improperly specified vm option"),
        QStringLiteral("error occurred during initialization"),
        QStringLiteral("error: could not"),
    };
    bool failed = false;
    for (const auto &marker : markers) {
        if (lower.contains(marker)) {
            failed = true;
            break;
        }
    }
    if (!failed && result.exitCode != 0 && output.isEmpty())
        failed = true;
    if (!failed)
        return true;
    QString reason;
    for (const auto &line : output.split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;
        const QString lineLower = trimmed.toLower();
        if (lineLower.contains(QLatin1String("error")) || lineLower.contains(QLatin1String("could not"))
            || lineLower.contains(QLatin1String("unrecognized")) || lineLower.contains(QLatin1String("invalid"))) {
            reason = trimmed;
            break;
        }
    }
    if (reason.isEmpty())
        reason = output.left(240);
    if (error)
        *error = reason;
    return false;
}

QString JavaManager::installRuntime(int major, const QString &runtimeRoot, const JavaProgress &progress,
                                    std::atomic_bool *cancel, QString *error)
{
    const QString component = runtimeComponent(major);
    const QString destination = QDir(runtimeRoot).filePath(QStringLiteral("java/%1-%2").arg(component).arg(major));
    const QString existing = executableFor(destination);
    if (!existing.isEmpty())
        return existing;

    const QUrl allUrl(QStringLiteral("https://launchermeta.mojang.com/v1/products/java-runtime/2ec0cc96c44e5a76b9c8b7c39df7210883d12871/all.json"));
    const HttpResult allResponse = httpGet(allUrl, {}, 30000);
    if (!allResponse.ok) {
        if (error)
            *error = QStringLiteral("Не удалось получить манифест Java Mojang: %1").arg(allResponse.error);
        return QString();
    }
    const QJsonObject all = allResponse.object();
    const QJsonArray components = all.value(platformKey()).toObject().value(component).toArray();
    if (components.isEmpty()) {
        if (error)
            *error = QStringLiteral("Для платформы %1 нет компонента %2").arg(platformKey(), component);
        return QString();
    }
    const QString manifestUrl = components.first().toObject().value(QStringLiteral("manifest")).toObject().value(QStringLiteral("url")).toString();
    const HttpResult manifestResponse = httpGet(QUrl(manifestUrl), {}, 30000);
    if (!manifestResponse.ok) {
        if (error)
            *error = QStringLiteral("Не удалось получить манифест Java: %1").arg(manifestResponse.error);
        return QString();
    }
    const QJsonObject files = manifestResponse.object().value(QStringLiteral("files")).toObject();
    struct Task { QString relative; QString url; QString sha1; };
    QVector<Task> tasks;
    for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
        const QJsonObject file = it.value().toObject();
        if (file.value(QStringLiteral("type")).toString() != QLatin1String("file"))
            continue;
        const QJsonObject raw = file.value(QStringLiteral("downloads")).toObject().value(QStringLiteral("raw")).toObject();
        const QString url = raw.value(QStringLiteral("url")).toString();
        if (url.isEmpty())
            continue;
        tasks.append({it.key(), url, raw.value(QStringLiteral("sha1")).toString()});
    }
    if (tasks.isEmpty()) {
        if (error)
            *error = QStringLiteral("Манифест Java пуст");
        return QString();
    }
    QDir().mkpath(destination);
    std::atomic_int index{0};
    std::atomic_int done{0};
    std::atomic_int failed{0};
    QStringList failures;
    QMutex failuresMutex;
    const int workers = std::min(6, static_cast<int>(tasks.size()));
    std::vector<std::thread> threads;
    for (int w = 0; w < workers; ++w) {
        threads.emplace_back([&] {
            for (;;) {
                const int i = index.fetch_add(1);
                if (i >= tasks.size() || (cancel && cancel->load()))
                    return;
                const Task &task = tasks.at(i);
                const QString target = QDir(destination).filePath(task.relative);
                QDir().mkpath(QFileInfo(target).absolutePath());
                if (fileMatchesHash(target, task.sha1)) {
                    const int d = done.fetch_add(1) + 1;
                    if (progress)
                        progress(d, tasks.size());
                    continue;
                }
                QString downloadError;
                if (!downloadFile(QUrl(task.url), target, task.sha1, {}, {}, cancel, {}, &downloadError)) {
                    failed.fetch_add(1);
                    QMutexLocker lock(&failuresMutex);
                    failures << task.relative;
                    continue;
                }
                const int d = done.fetch_add(1) + 1;
                if (progress)
                    progress(d, tasks.size());
            }
        });
    }
    for (auto &thread : threads)
        thread.join();
    if (cancel && cancel->load()) {
        if (error)
            *error = QStringLiteral("Загрузка Java отменена");
        return QString();
    }
    if (failed.load() > 0) {
        if (error)
            *error = QStringLiteral("Не удалось скачать %1 файлов Java (например %2)")
                         .arg(failed.load())
                         .arg(failures.value(0));
        return QString();
    }
    const QString exe = executableFor(destination);
    if (exe.isEmpty()) {
        if (error)
            *error = QStringLiteral("Java установлена, но исполняемый файл не найден");
        return QString();
    }
    Logger::info(QStringLiteral("Java %1 установлена: %2").arg(major).arg(exe));
    return exe;
}

} // namespace Noble
