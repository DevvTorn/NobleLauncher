#include "VersionManager.h"

#include "AuthlibInjector.h"
#include "Http.h"
#include "JavaManager.h"
#include "Json.h"
#include "Logger.h"
#include "Paths.h"
#include "ProcessUtil.h"
#include "Pure.h"
#include "Zip.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonValue>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QUrlQuery>
#include <QXmlStreamReader>

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

namespace Noble {

static const QString kManifestUrl = QStringLiteral("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json");
static const QString kManifestFallback = QStringLiteral("https://launchermeta.mojang.com/mc/game/version_manifest_v2.json");
static const QString kLibraryHost = QStringLiteral("https://libraries.minecraft.net");
static const QString kBmclapi = QStringLiteral("https://bmclapi2.bangbang93.com");
static const QStringList kAssetHosts = {
    QStringLiteral("https://resources.download.minecraft.net"),
    QStringLiteral("https://launcher.mojang.com"),
    QStringLiteral("https://bmclapi2.bangbang93.com/assets"),
};
static const QString kForgePromotions = QStringLiteral("https://files.minecraftforge.net/net/minecraftforge/forge/promotions_slim.json");
static const QString kForgeMetadata = QStringLiteral("https://maven.minecraftforge.net/net/minecraftforge/forge/maven-metadata.xml");
static const QString kNeoForgeMetadata = QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/neoforge/maven-metadata.xml");
static const QString kFabricMeta = QStringLiteral("https://meta.fabricmc.net/v2");
static const QString kQuiltMeta = QStringLiteral("https://meta.quiltmc.org");

namespace {

struct DownloadTask {
    QStringList urls;
    QString target;
    QString sha1;
    qint64 size = 0;
    QString label;
};

struct ManifestCache {
    QJsonObject root;
    qint64 fetchedAt = 0;
    QMutex mutex;
};
ManifestCache &manifestCache()
{
    static ManifestCache cache;
    return cache;
}

bool ensureDir(const QString &dir)
{
    return QDir().mkpath(dir);
}

bool rulesMatch(const QJsonObject &rule)
{
    if (rule.contains(QStringLiteral("os"))) {
        const QJsonObject os = rule.value(QStringLiteral("os")).toObject();
        const QString name = os.value(QStringLiteral("name")).toString();
#if defined(Q_OS_WIN)
        const QString current = QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
        const QString current = QStringLiteral("osx");
#else
        const QString current = QStringLiteral("linux");
#endif
        if (!name.isEmpty() && name != current)
            return false;
        const QString arch = os.value(QStringLiteral("arch")).toString();
        if (!arch.isEmpty()) {
#if defined(Q_PROCESSOR_ARM64)
            const QString currentArch = QStringLiteral("arm64");
#elif defined(Q_PROCESSOR_X86_64) || defined(Q_PROCESSOR_X86)
            const QString currentArch = QStringLiteral("x86_64");
#else
            const QString currentArch = QStringLiteral("x86");
#endif
            if (arch != currentArch && !(arch == QLatin1String("x86") && currentArch == QLatin1String("x86_64")))
                return false;
        }
    }
    if (rule.contains(QStringLiteral("features"))) {
        const QJsonObject features = rule.value(QStringLiteral("features")).toObject();
        for (auto it = features.constBegin(); it != features.constEnd(); ++it) {
            // Лаунчер не включает демо/кастомное разрешение/quick play.
            if (it.value().toBool())
                return false;
        }
    }
    return true;
}

bool rulesAllowed(const QJsonArray &rules)
{
    if (rules.isEmpty())
        return true;
    bool allowed = false;
    for (const auto &value : rules) {
        const QJsonObject rule = value.toObject();
        if (rulesMatch(rule))
            allowed = rule.value(QStringLiteral("action")).toString() != QLatin1String("disallow");
    }
    return allowed;
}

QJsonObject fetchManifest(bool force, QString *error)
{
    auto &cache = manifestCache();
    QMutexLocker lock(&cache.mutex);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!force && !cache.root.isEmpty() && now - cache.fetchedAt < 10 * 60 * 1000)
        return cache.root;
    HttpResult response = httpGet(QUrl(kManifestUrl), {}, 30000);
    if (!response.ok)
        response = httpGet(QUrl(kManifestFallback), {}, 30000);
    if (!response.ok) {
        if (error)
            *error = QStringLiteral("Не удалось загрузить манифест версий Minecraft: %1").arg(response.error);
        if (!cache.root.isEmpty())
            return cache.root;
        return {};
    }
    cache.root = response.object();
    cache.fetchedAt = now;
    return cache.root;
}

QJsonObject manifestEntry(const QString &mcVersion, QString *error)
{
    const QJsonObject manifest = fetchManifest(false, error);
    const QJsonArray versions = manifest.value(QStringLiteral("versions")).toArray();
    for (const auto &value : versions) {
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("id")).toString() == mcVersion)
            return entry;
    }
    if (error)
        *error = QStringLiteral("Версия Minecraft %1 не найдена в манифесте").arg(mcVersion);
    return {};
}

void addLibraryTasks(const QJsonArray &libraries, const QString &runtime, QVector<DownloadTask> &tasks,
                     QVector<QString> &classpath, QVector<DownloadTask> &nativeTasks)
{
    const QString librariesDir = QDir(runtime).filePath(QStringLiteral("libraries"));
    auto addTask = [&](const QJsonObject &artifact, const QString &label, QVector<DownloadTask> &destination,
                       bool addToClasspath) {
        const QString path = artifact.value(QStringLiteral("path")).toString();
        if (path.isEmpty())
            return;
        QString url = artifact.value(QStringLiteral("url")).toString();
        if (url.isEmpty())
            url = kLibraryHost + QLatin1Char('/') + path;
        const QString target = QDir(librariesDir).filePath(path);
        const QString sha1 = artifact.value(QStringLiteral("sha1")).toString();
        const qint64 size = artifact.value(QStringLiteral("size")).toInteger();
        QStringList urls{url};
        const QString canonical = kLibraryHost + QLatin1Char('/') + path;
        if (url != canonical)
            urls << canonical;
        urls << QStringLiteral("https://repo1.maven.org/maven2/") + path;
        urls << kBmclapi + QStringLiteral("/maven/") + path;
        destination.append({urls, target, sha1, size, label});
        if (addToClasspath)
            classpath.append(target);
    };

    for (const auto &value : libraries) {
        const QJsonObject library = value.toObject();
        if (!rulesAllowed(library.value(QStringLiteral("rules")).toArray()))
            continue;
        const QString name = library.value(QStringLiteral("name")).toString();
        QJsonObject artifact = library.value(QStringLiteral("downloads")).toObject().value(QStringLiteral("artifact")).toObject();
        if (artifact.value(QStringLiteral("path")).toString().isEmpty()) {
            const QString path = mavenPathFromName(name);
            if (!path.isEmpty()) {
                artifact.insert(QStringLiteral("path"), path);
                QString base = library.value(QStringLiteral("url")).toString();
                if (base.isEmpty())
                    base = kLibraryHost + QLatin1Char('/');
                if (!base.endsWith(QLatin1Char('/')))
                    base += QLatin1Char('/');
                artifact.insert(QStringLiteral("url"), base + path);
            }
        }
        addTask(artifact, name, tasks, true);

        QJsonObject natives = library.value(QStringLiteral("natives")).toObject();
        const QString classifierRaw = natives.value(QStringLiteral("windows")).toString();
        if (classifierRaw.isEmpty())
            continue;
        QString classifier = classifierRaw;
        classifier.replace(QLatin1String("${arch}"), QStringLiteral("64"));
        QJsonObject classifiers = library.value(QStringLiteral("downloads")).toObject().value(QStringLiteral("classifiers")).toObject();
        QJsonObject nativeArtifact = classifiers.value(classifier).toObject();
        if (nativeArtifact.value(QStringLiteral("path")).toString().isEmpty()) {
            QString mavenName = name;
            const int at = mavenName.indexOf(QLatin1Char('@'));
            if (at >= 0)
                mavenName = mavenName.left(at);
            if (mavenName.count(QLatin1Char(':')) == 2)
                mavenName += QLatin1Char(':') + classifier;
            const QString path = mavenPathFromName(mavenName);
            if (!path.isEmpty()) {
                nativeArtifact.insert(QStringLiteral("path"), path);
                QString base = library.value(QStringLiteral("url")).toString();
                if (base.isEmpty())
                    base = kLibraryHost + QLatin1Char('/');
                if (!base.endsWith(QLatin1Char('/')))
                    base += QLatin1Char('/');
                nativeArtifact.insert(QStringLiteral("url"), base + path);
            }
        }
        addTask(nativeArtifact, name + QStringLiteral(" (natives)"), nativeTasks, false);
    }
}

bool downloadBatch(QVector<DownloadTask> tasks, int concurrency, const Reporter &report, const QString &stage,
                   const QString &title, double from, double to, std::atomic_bool *cancel, QString *error)
{
    QVector<DownloadTask> pending;
    int already = 0;
    for (const auto &task : tasks) {
        bool ok = false;
        if (!task.sha1.isEmpty())
            ok = fileMatchesHash(task.target, task.sha1);
        else if (task.size > 0)
            ok = fileMatchesSize(task.target, task.size);
        else
            ok = QFileInfo::exists(task.target);
        if (ok)
            ++already;
        else
            pending.append(task);
    }
    const int total = static_cast<int>(tasks.size());
    if (report)
        report(stage, QStringLiteral("%1: %2/%3").arg(title).arg(already).arg(total),
               from + (to - from) * (total > 0 ? static_cast<double>(already) / total : 1.0));
    if (pending.isEmpty())
        return true;

    std::atomic_int index{0};
    std::atomic_int done{already};
    std::atomic_int failed{0};
    QMutex failuresMutex;
    QStringList failures;
    const int workers = std::max(1, std::min(concurrency, static_cast<int>(pending.size())));
    std::vector<std::thread> threads;
    for (int w = 0; w < workers; ++w) {
        threads.emplace_back([&] {
            for (;;) {
                const int i = index.fetch_add(1);
                if (i >= pending.size() || (cancel && cancel->load()))
                    return;
                const DownloadTask task = pending.at(i);
                QString lastError;
                bool success = false;
                for (int attempt = 0; attempt < 4 && !success; ++attempt) {
                    const QString url = task.urls.value(std::min(attempt, static_cast<int>(task.urls.size()) - 1));
                    success = downloadFile(QUrl(url), task.target, task.sha1, {}, {}, cancel, {}, &lastError);
                    if (!success && cancel && cancel->load())
                        return;
                }
                if (!success) {
                    failed.fetch_add(1);
                    QMutexLocker lock(&failuresMutex);
                    failures << QStringLiteral("%1: %2").arg(task.label, lastError);
                }
                const int d = done.fetch_add(1) + 1;
                if (report && (d % 25 == 0 || d == total)) {
                    report(stage, QStringLiteral("%1: %2/%3").arg(title).arg(d).arg(total),
                           from + (to - from) * (total > 0 ? static_cast<double>(d) / total : 1.0));
                }
            }
        });
    }
    for (auto &thread : threads)
        thread.join();

    if (cancel && cancel->load()) {
        if (error)
            *error = QStringLiteral("Операция отменена");
        return false;
    }
    if (failed.load() > 0) {
        if (report)
            report(stage, QStringLiteral("%1: ошибок — %2").arg(title).arg(failed.load()), to);
        if (error) {
            QStringList preview = failures.mid(0, 4);
            *error = QStringLiteral("Не удалось скачать %1 из %2 файлов (%3). Примеры: %4")
                         .arg(failed.load())
                         .arg(total)
                         .arg(title, preview.join(QStringLiteral("; ")));
        }
        return false;
    }
    return true;
}

QJsonObject versionJsonFromDisk(const QString &runtime, const QString &id)
{
    const QString path = QDir(runtime).filePath(QStringLiteral("versions/%1/%1.json").arg(id));
    return readJsonObject(path);
}

QJsonObject resolveVersionRecursive(const QString &runtime, const QString &id, QSet<QString> &seen, QString *error)
{
    if (seen.contains(id)) {
        if (error)
            *error = QStringLiteral("Циклическое наследование версии %1").arg(id);
        return {};
    }
    seen.insert(id);
    QJsonObject child = versionJsonFromDisk(runtime, id);
    if (child.isEmpty()) {
        if (error)
            *error = QStringLiteral("Не найден JSON версии %1 в общем каталоге игры — установите версию заново").arg(id);
        return {};
    }
    const QString parentId = child.value(QStringLiteral("inheritsFrom")).toString();
    if (parentId.isEmpty())
        return child;
    QJsonObject parent = resolveVersionRecursive(runtime, parentId, seen, error);
    if (parent.isEmpty())
        return {};
    QJsonObject merged = parent;
    static const QStringList overrideKeys = {
        QStringLiteral("mainClass"), QStringLiteral("minecraftArguments"), QStringLiteral("assetIndex"),
        QStringLiteral("assets"), QStringLiteral("downloads"), QStringLiteral("javaVersion"), QStringLiteral("logging"),
        QStringLiteral("type"), QStringLiteral("releaseTime"), QStringLiteral("time"), QStringLiteral("jar"),
        QStringLiteral("minimumLauncherVersion"),
    };
    for (const auto &key : overrideKeys) {
        if (child.contains(key))
            merged.insert(key, child.value(key));
    }
    merged.insert(QStringLiteral("id"), child.value(QStringLiteral("id")).toString(id));
    QJsonArray libraries = parent.value(QStringLiteral("libraries")).toArray();
    for (const auto &lib : child.value(QStringLiteral("libraries")).toArray())
        libraries.append(lib);
    merged.insert(QStringLiteral("libraries"), libraries);
    const QJsonObject parentArgs = parent.value(QStringLiteral("arguments")).toObject();
    const QJsonObject childArgs = child.value(QStringLiteral("arguments")).toObject();
    QJsonObject arguments;
    for (const QString &key : {QStringLiteral("game"), QStringLiteral("jvm")}) {
        QJsonArray array = parentArgs.value(key).toArray();
        for (const auto &item : childArgs.value(key).toArray())
            array.append(item);
        if (!array.isEmpty())
            arguments.insert(key, array);
    }
    if (!arguments.isEmpty())
        merged.insert(QStringLiteral("arguments"), arguments);
    merged.insert(QStringLiteral("inheritsFrom"), parentId);
    return merged;
}

QString expandArgs(QString value, const QHash<QString, QString> &placeholders)
{
    for (auto it = placeholders.constBegin(); it != placeholders.constEnd(); ++it)
        value.replace(QStringLiteral("${%1}").arg(it.key()), it.value());
    return value;
}

QStringList argumentList(const QJsonArray &array, const QHash<QString, QString> &placeholders)
{
    QStringList result;
    for (const auto &value : array) {
        if (value.isString()) {
            result << expandArgs(value.toString(), placeholders);
        } else if (value.isObject()) {
            const QJsonObject object = value.toObject();
            if (!rulesAllowed(object.value(QStringLiteral("rules")).toArray()))
                continue;
            const QJsonValue item = object.value(QStringLiteral("value"));
            if (item.isString()) {
                result << expandArgs(item.toString(), placeholders);
            } else if (item.isArray()) {
                for (const auto &entry : item.toArray()) {
                    if (entry.isString())
                        result << expandArgs(entry.toString(), placeholders);
                }
            }
        }
    }
    return result;
}

QVector<QString> parseMavenVersions(const QByteArray &xml)
{
    QVector<QString> versions;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QLatin1String("version"))
            versions.append(reader.readElementText());
    }
    return versions;
}

QString licenseSafe(const QString &text)
{
    return QString(text).replace(QLatin1Char('\n'), QLatin1Char(' '));
}

} // namespace

QVector<VersionSummary> VersionManager::listVersions(bool force)
{
    QString error;
    const QJsonObject manifest = fetchManifest(force, &error);
    QVector<VersionSummary> list;
    const QJsonArray versions = manifest.value(QStringLiteral("versions")).toArray();
    list.reserve(versions.size());
    for (const auto &value : versions) {
        const QJsonObject entry = value.toObject();
        list.append({entry.value(QStringLiteral("id")).toString(),
                     entry.value(QStringLiteral("type")).toString(QStringLiteral("release")),
                     entry.value(QStringLiteral("releaseTime")).toString()});
    }
    return list;
}

QVector<LoaderVersionSummary> VersionManager::listLoaderVersions(const QString &loader, const QString &mcVersion)
{
    QVector<LoaderVersionSummary> result;
    if (loader == QLatin1String("Vanilla"))
        return {{mcVersion, mcVersion, true}};

    if (loader == QLatin1String("Fabric")) {
        const HttpResult response = httpGet(QUrl(QStringLiteral("%1/versions/loader/%2").arg(kFabricMeta, mcVersion)), {}, 20000);
        if (!response.ok) {
            Logger::warn(QStringLiteral("Fabric: не удалось получить список версий: %1").arg(response.error));
            return result;
        }
        for (const auto &value : response.json().array()) {
            const QJsonObject entry = value.toObject();
            const QString version = entry.value(QStringLiteral("loader")).toObject().value(QStringLiteral("version")).toString();
            if (!version.isEmpty())
                result.append({version, mcVersion, entry.value(QStringLiteral("loader")).toObject().value(QStringLiteral("stable")).toBool(true)});
        }
        std::stable_sort(result.begin(), result.end(), [](const LoaderVersionSummary &a, const LoaderVersionSummary &b) {
            return a.stable && !b.stable;
        });
        return result;
    }

    if (loader == QLatin1String("Quilt")) {
        const HttpResult response = httpGet(QUrl(QStringLiteral("%1/v3/versions/loader/%2").arg(kQuiltMeta, mcVersion)), {}, 20000);
        if (response.ok) {
            static const QRegularExpression prerelease(QStringLiteral("-(beta|pre|rc|alpha|snapshot)"), QRegularExpression::CaseInsensitiveOption);
            for (const auto &value : response.json().array()) {
                const QJsonObject loaderObj = value.toObject().value(QStringLiteral("loader")).toObject();
                const QString version = loaderObj.value(QStringLiteral("version")).toString();
                if (!version.isEmpty())
                    result.append({version, mcVersion, !prerelease.match(version).hasMatch()});
            }
            if (!result.isEmpty()) {
                std::stable_sort(result.begin(), result.end(), [](const LoaderVersionSummary &a, const LoaderVersionSummary &b) {
                    return a.stable && !b.stable;
                });
                return result;
            }
        }
        return result;
    }

    if (loader == QLatin1String("Forge")) {
        const HttpResult promotions = httpGet(QUrl(kForgePromotions), {}, 20000);
        QString recommended = promotions.object().value(QStringLiteral("promos")).toObject().value(mcVersion + QStringLiteral("-recommended")).toString();
        QString latest = promotions.object().value(QStringLiteral("promos")).toObject().value(mcVersion + QStringLiteral("-latest")).toString();
        const HttpResult metadata = httpGet(QUrl(kForgeMetadata), {}, 30000);
        if (metadata.ok) {
            const auto all = parseMavenVersions(metadata.body);
            for (const auto &full : all) {
                const QString prefix = mcVersion + QLatin1Char('-');
                if (!full.startsWith(prefix))
                    continue;
                const QString version = full.mid(prefix.size());
                result.append({version, mcVersion, version == recommended || version == latest});
            }
            std::reverse(result.begin(), result.end());
        } else if (!recommended.isEmpty()) {
            result.append({recommended, mcVersion, true});
            if (!latest.isEmpty() && latest != recommended)
                result.append({latest, mcVersion, true});
        }
        return result;
    }

    if (loader == QLatin1String("NeoForge")) {
        const HttpResult metadata = httpGet(QUrl(kNeoForgeMetadata), {}, 30000);
        if (metadata.ok) {
            const auto all = parseMavenVersions(metadata.body);
            for (const auto &version : all) {
                if (neoforgeGameVersion(version) == mcVersion)
                    result.append({version, mcVersion, true});
            }
            std::reverse(result.begin(), result.end());
        }
        return result;
    }
    return result;
}

QString VersionManager::resolveLoaderVersion(const QString &loader, const QString &mcVersion, const QString &requested)
{
    if (loader == QLatin1String("Vanilla"))
        return mcVersion;
    const QString explicitVersion = requested.trimmed();
    if (!explicitVersion.isEmpty())
        return explicitVersion;
    const auto list = listLoaderVersions(loader, mcVersion);
    for (const auto &entry : list) {
        if (entry.stable && !entry.version.isEmpty())
            return entry.version;
    }
    if (!list.isEmpty())
        return list.first().version;
    return QString();
}

static bool installVanilla(const QString &mcVersion, const QString &runtime, const Reporter &report,
                           std::atomic_bool *cancel, QString *error)
{
    if (cancel && cancel->load()) {
        if (error)
            *error = QStringLiteral("Операция отменена");
        return false;
    }
    report(QStringLiteral("version"), QStringLiteral("Загрузка манифеста %1…").arg(mcVersion), 0.14);
    QString manifestError;
    const QJsonObject entry = manifestEntry(mcVersion, &manifestError);
    if (entry.isEmpty()) {
        if (error)
            *error = manifestError;
        return false;
    }
    const QString versionJsonUrl = entry.value(QStringLiteral("url")).toString();
    const HttpResult versionResponse = httpGet(QUrl(versionJsonUrl), {}, 30000);
    if (!versionResponse.ok) {
        if (error)
            *error = QStringLiteral("Не удалось скачать JSON версии %1: %2").arg(mcVersion, versionResponse.error);
        return false;
    }
    const QJsonObject versionJson = versionResponse.object();
    const QString versionDir = QDir(runtime).filePath(QStringLiteral("versions/%1").arg(mcVersion));
    ensureDir(versionDir);
    const QString versionFile = QDir(versionDir).filePath(mcVersion + QStringLiteral(".json"));
    writeJsonObject(versionFile, versionJson);

    QVector<DownloadTask> libraryTasks;
    QVector<DownloadTask> nativeTasks;
    QVector<QString> classpath;
    addLibraryTasks(versionJson.value(QStringLiteral("libraries")).toArray(), runtime, libraryTasks, classpath, nativeTasks);

    QVector<DownloadTask> allLibraryTasks = libraryTasks;
    for (const auto &task : nativeTasks)
        allLibraryTasks.append(task);

    report(QStringLiteral("libraries"), QStringLiteral("Библиотеки: 0/%1").arg(allLibraryTasks.size()), 0.20);
    if (!downloadBatch(allLibraryTasks, 6, report, QStringLiteral("libraries"), QStringLiteral("Библиотеки"), 0.20,
                       0.42, cancel, error))
        return false;

    const QJsonObject client = versionJson.value(QStringLiteral("downloads")).toObject().value(QStringLiteral("client")).toObject();
    if (!client.isEmpty()) {
        const QString target = QDir(versionDir).filePath(mcVersion + QStringLiteral(".jar"));
        QStringList urls;
        const QString url = client.value(QStringLiteral("url")).toString();
        if (!url.isEmpty())
            urls << url;
        urls << kBmclapi + QStringLiteral("/version/%1/client").arg(mcVersion);
        DownloadTask jarTask{urls, target, client.value(QStringLiteral("sha1")).toString(),
                             client.value(QStringLiteral("size")).toInteger(), QStringLiteral("client.jar")};
        if (!downloadBatch({jarTask}, 1, report, QStringLiteral("version"), QStringLiteral("Клиентский jar"), 0.42,
                           0.46, cancel, error))
            return false;
    }

    const QJsonObject assetIndex = versionJson.value(QStringLiteral("assetIndex")).toObject();
    if (!assetIndex.isEmpty()) {
        const QString indexId = assetIndex.value(QStringLiteral("id")).toString();
        const QString indexPath = QDir(runtime).filePath(QStringLiteral("assets/indexes/%1.json").arg(indexId));
        const QString indexUrl = assetIndex.value(QStringLiteral("url")).toString();
        if (!QFileInfo::exists(indexPath) || !fileMatchesHash(indexPath, assetIndex.value(QStringLiteral("sha1")).toString())) {
            QString downloadError;
            if (!downloadFile(QUrl(indexUrl), indexPath, assetIndex.value(QStringLiteral("sha1")).toString(), {}, {}, cancel, {}, &downloadError)
                && !(QFileInfo::exists(indexPath) && QFileInfo(indexPath).size() > 0)) {
                if (error)
                    *error = QStringLiteral("Не удалось скачать индекс ассетов %1: %2").arg(indexId, downloadError);
                return false;
            }
        }
        QJsonObject index = readJsonObject(indexPath);
        const QJsonObject objects = index.value(QStringLiteral("objects")).toObject();
        QVector<DownloadTask> assetTasks;
        QSet<QString> seen;
        for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
            const QJsonObject object = it.value().toObject();
            const QString hash = object.value(QStringLiteral("hash")).toString();
            if (hash.size() < 2)
                continue;
            const QString head = hash.left(2);
            const QString target = QDir(runtime).filePath(QStringLiteral("assets/objects/%1/%2").arg(head, hash));
            if (seen.contains(target))
                continue;
            seen.insert(target);
            QStringList urls;
            for (const auto &host : kAssetHosts)
                urls << QStringLiteral("%1/%2/%3").arg(host, head, hash);
            assetTasks.append({urls, target, hash, object.value(QStringLiteral("size")).toInteger(), hash});
        }
        report(QStringLiteral("assets"), QStringLiteral("Ассеты: 0/%1").arg(assetTasks.size()), 0.46);
        if (!downloadBatch(assetTasks, 8, report, QStringLiteral("assets"), QStringLiteral("Ассеты"), 0.46, 0.85, cancel, error))
            return false;
    }

    const QJsonObject logging = versionJson.value(QStringLiteral("logging")).toObject();
    const QJsonObject loggingClient = logging.value(QStringLiteral("client")).toObject();
    const QJsonObject loggingFile = loggingClient.value(QStringLiteral("file")).toObject();
    if (!loggingFile.isEmpty()) {
        const QString id = loggingFile.value(QStringLiteral("id")).toString();
        const QString target = QDir(runtime).filePath(QStringLiteral("assets/log_configs/%1").arg(id));
        if (!QFileInfo::exists(target) || !fileMatchesHash(target, loggingFile.value(QStringLiteral("sha1")).toString())) {
            QString downloadError;
            if (!downloadFile(QUrl(loggingFile.value(QStringLiteral("url")).toString()), target,
                              loggingFile.value(QStringLiteral("sha1")).toString(), {}, {}, cancel, {}, &downloadError, 20000))
                Logger::warn(QStringLiteral("Не удалось скачать конфиг логирования: %1").arg(downloadError));
        }
    }
    report(QStringLiteral("version"), QStringLiteral("Minecraft %1 установлен").arg(mcVersion), 0.86);
    return true;
}

static QString installFabricLike(const QString &loader, const QString &mcVersion, const QString &loaderVersion,
                                 const QString &runtime, const Reporter &report, std::atomic_bool *cancel, QString *error)
{
    const QString url = loader == QLatin1String("Fabric")
        ? QStringLiteral("%1/versions/loader/%2/%3/profile/json").arg(kFabricMeta, mcVersion, loaderVersion)
        : QStringLiteral("%1/v3/versions/loader/%2/%3/profile/json").arg(kQuiltMeta, mcVersion, loaderVersion);
    report(QStringLiteral("loader"), QStringLiteral("Установка %1 %2…").arg(loader, loaderVersion), 0.88);
    const HttpResult response = httpGet(QUrl(url), {}, 30000);
    if (!response.ok) {
        if (error)
            *error = QStringLiteral("Не удалось получить профиль %1: %2").arg(loader, response.error);
        return QString();
    }
    const QJsonObject profile = response.object();
    QString id = profile.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
        id = loaderVersionId(loader, mcVersion, loaderVersion);
    const QString dir = QDir(runtime).filePath(QStringLiteral("versions/%1").arg(id));
    ensureDir(dir);
    writeJsonObject(QDir(dir).filePath(id + QStringLiteral(".json")), profile);

    QVector<DownloadTask> tasks;
    QVector<QString> classpath;
    QVector<DownloadTask> natives;
    addLibraryTasks(profile.value(QStringLiteral("libraries")).toArray(), runtime, tasks, classpath, natives);
    for (const auto &task : natives)
        tasks.append(task);
    if (!downloadBatch(tasks, 6, report, QStringLiteral("loader"), QStringLiteral("Библиотеки %1").arg(loader), 0.90,
                       0.98, cancel, error))
        return QString();
    report(QStringLiteral("loader"), QStringLiteral("%1 %2 установлен").arg(loader, loaderVersion), 0.99);
    Logger::info(QStringLiteral("Загрузчик установлен: %1 %2 (%3)").arg(loader, loaderVersion, id));
    return id;
}

static QString installForgeLike(const QString &loader, const QString &mcVersion, const QString &loaderVersion,
                                const QString &runtime, const QString &javaModePath, const Reporter &report,
                                std::atomic_bool *cancel, QString *error)
{
    const QString installerUrl = loader == QLatin1String("Forge")
        ? QStringLiteral("https://maven.minecraftforge.net/net/minecraftforge/forge/%1-%2/forge-%1-%2-installer.jar")
              .arg(mcVersion, loaderVersion)
        : QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/neoforge/%1/neoforge-%1-installer.jar")
              .arg(loaderVersion);
    const QString tempDir = QDir(runtime).filePath(QStringLiteral("temp"));
    ensureDir(tempDir);
    const QString installer = QDir(tempDir).filePath(
        QStringLiteral("%1-installer-%2.jar").arg(loader.toLower(), loaderVersion));
    report(QStringLiteral("loader"), QStringLiteral("Скачивание установщика %1 %2…").arg(loader, loaderVersion), 0.88);
    QString downloadError;
    if (!downloadFile(QUrl(installerUrl), installer, {}, {}, {}, cancel, {}, &downloadError, 60000)) {
        if (error)
            *error = QStringLiteral("Не удалось скачать установщик %1: %2").arg(loader, downloadError);
        return QString();
    }

    const QString profilesFile = QDir(runtime).filePath(QStringLiteral("launcher_profiles.json"));
    if (!QFileInfo::exists(profilesFile)) {
        writeJsonObject(profilesFile,
                        QJsonObject{{QStringLiteral("profiles"), QJsonObject{}},
                                    {QStringLiteral("settings"), QJsonObject{}},
                                    {QStringLiteral("version"), 3}});
    }
    const int required = requiredJavaMajor(mcVersion);
    QString javaExe;
    QString javaError;
    const PickedJava picked = JavaManager::pickFor(mcVersion, javaModePath, loader, &javaError);
    if (!picked.path.isEmpty()) {
        javaExe = picked.path;
    } else {
        javaExe = JavaManager::installRuntime(required, runtime, {}, cancel, &javaError);
    }
    if (javaExe.isEmpty()) {
        if (error)
            *error = QStringLiteral("Для установки %1 не найдена Java: %2").arg(loader, javaError);
        return QString();
    }
    const QString consoleJava = javaExe.endsWith(QLatin1String("javaw.exe"), Qt::CaseInsensitive)
        ? QString(javaExe).replace(javaExe.size() - 9, 9, QStringLiteral("java.exe"))
        : javaExe;
    report(QStringLiteral("loader"), QStringLiteral("Установщик %1 %2 работает…").arg(loader, loaderVersion), 0.90);
    ProcessResult run = runProcess(consoleJava, {QStringLiteral("-jar"), installer, QStringLiteral("--installClient"), runtime},
                                   15 * 60 * 1000, {}, runtime);
    const QString output = licenseSafe(QString::fromLocal8Bit(run.stdoutData) + QStringLiteral(" ")
                                       + QString::fromLocal8Bit(run.stderrData));
    if (run.exitCode != 0)
        Logger::warn(QStringLiteral("Установщик %1 завершился с кодом %2: %3").arg(loader).arg(run.exitCode).arg(output.left(1000)));

    QString expectedId = loaderVersionId(loader, mcVersion, loaderVersion);
    QString jsonPath = QDir(runtime).filePath(QStringLiteral("versions/%1/%1.json").arg(expectedId));
    if (!QFileInfo::exists(jsonPath)) {
        // Иногда установщик создаёт каталог с другим id — ищем свежий профиль.
        const QFileInfoList dirs = QDir(QDir(runtime).filePath(QStringLiteral("versions")))
                                       .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
        for (const auto &dir : dirs) {
            const QString candidate = dir.fileName();
            if (loader == QLatin1String("Forge") && !candidate.contains(QStringLiteral("forge")))
                continue;
            if (loader == QLatin1String("NeoForge") && !candidate.startsWith(QLatin1String("neoforge-")))
                continue;
            if (QFileInfo::exists(QDir(dir.absoluteFilePath()).filePath(candidate + QStringLiteral(".json")))) {
                expectedId = candidate;
                jsonPath = QDir(dir.absoluteFilePath()).filePath(candidate + QStringLiteral(".json"));
                break;
            }
        }
    }
    if (!QFileInfo::exists(jsonPath)) {
        if (error)
            *error = QStringLiteral("Установщик %1 завершился, но профиль версии не найден. Вывод: %2").arg(loader, output.left(500));
        return QString();
    }
    report(QStringLiteral("loader"), QStringLiteral("%1 %2 установлен").arg(loader, loaderVersion), 0.99);
    Logger::info(QStringLiteral("Загрузчик установлен: %1 %2 (%3)").arg(loader, loaderVersion, expectedId));
    return expectedId;
}

QString VersionManager::installLoader(const QJsonObject &instance, const QString &runtime, const QString &loaderVersion,
                                      const QString &javaMode, const QString &javaPath, const Reporter &report,
                                      std::atomic_bool *cancel, QString *error)
{
    const QString loader = instance.value(QStringLiteral("loader")).toString();
    const QString mcVersion = instance.value(QStringLiteral("version")).toString();
    if (loader == QLatin1String("Vanilla") || loader.isEmpty())
        return mcVersion;
    const QString manual = instance.value(QStringLiteral("javaPath")).toString().isEmpty()
        ? (javaMode == QLatin1String("manual") ? javaPath : QString())
        : instance.value(QStringLiteral("javaPath")).toString();
    if (loader == QLatin1String("Fabric") || loader == QLatin1String("Quilt"))
        return installFabricLike(loader, mcVersion, loaderVersion, runtime, report, cancel, error);
    if (loader == QLatin1String("Forge") || loader == QLatin1String("NeoForge"))
        return installForgeLike(loader, mcVersion, loaderVersion, runtime, manual, report, cancel, error);
    if (error)
        *error = QStringLiteral("Неизвестный загрузчик: %1").arg(loader);
    return QString();
}

QString VersionManager::installInstanceVersion(const QJsonObject &instance, const QString &runtime,
                                               const QString &javaMode, const QString &javaPath, const Reporter &report,
                                               std::atomic_bool *cancel, QString *error)
{
    const QString mcVersion = instance.value(QStringLiteral("version")).toString();
    if (mcVersion.isEmpty()) {
        if (error)
            *error = QStringLiteral("В сборке не указана версия Minecraft");
        return QString();
    }
    report(QStringLiteral("java"), QStringLiteral("Проверка Java для Minecraft %1…").arg(mcVersion), 0.02);
    const int required = requiredJavaMajor(mcVersion);
    QString javaError;
    JavaManager::installRuntime(required, runtime, [&](int done, int total) {
        report(QStringLiteral("java"), QStringLiteral("Загрузка Java %1: %2/%3").arg(required).arg(done).arg(total),
               total > 0 ? 0.03 + 0.09 * (static_cast<double>(done) / total) : 0.05);
    }, cancel, &javaError);
    if (!javaError.isEmpty())
        Logger::warn(QStringLiteral("Java %1 не скачалась (%2) — будет использована системная").arg(required).arg(javaError));

    if (!installVanilla(mcVersion, runtime, report, cancel, error))
        return QString();

    const QString loader = instance.value(QStringLiteral("loader")).toString();
    if (loader.isEmpty() || loader == QLatin1String("Vanilla"))
        return mcVersion;

    QString loaderVersion = instance.value(QStringLiteral("loaderVersion")).toString();
    if (loaderVersion.trimmed().isEmpty()) {
        loaderVersion = resolveLoaderVersion(loader, mcVersion, loaderVersion);
        if (loaderVersion.isEmpty()) {
            if (error)
                *error = QStringLiteral("Не найдено ни одной версии %1 для Minecraft %2").arg(loader, mcVersion);
            return QString();
        }
        Logger::info(QStringLiteral("Загрузчик %1: автоматически выбрана версия %2").arg(loader, loaderVersion));
    }
    report(QStringLiteral("loader"), QStringLiteral("Установка %1 %2…").arg(loader, loaderVersion), 0.88);
    const QString id = installLoader(instance, runtime, loaderVersion, javaMode, javaPath, report, cancel, error);
    if (id.isEmpty() && error && error->isEmpty())
        *error = QStringLiteral("Не удалось установить загрузчик %1").arg(loader);
    return id;
}

QString VersionManager::ensureInstanceVersion(const QJsonObject &instance, const QString &runtime, const QString &javaMode,
                                              const QString &javaPath, const Reporter &report, std::atomic_bool *cancel,
                                              QString *error)
{
    const QString installed = instance.value(QStringLiteral("installedVersionId")).toString();
    if (!installed.isEmpty() && isVersionInstalled(runtime, installed))
        return installed;
    return installInstanceVersion(instance, runtime, javaMode, javaPath, report, cancel, error);
}

bool VersionManager::isVersionInstalled(const QString &runtime, const QString &versionId)
{
    if (versionId.isEmpty())
        return false;
    return QFileInfo::exists(QDir(runtime).filePath(QStringLiteral("versions/%1/%1.json").arg(versionId)));
}

QJsonObject VersionManager::loadVersion(const QString &runtime, const QString &versionId)
{
    QSet<QString> seen;
    QString error;
    const QJsonObject result = resolveVersionRecursive(runtime, versionId, seen, &error);
    if (!error.isEmpty())
        Logger::warn(error);
    return result;
}

bool VersionManager::ensureAuthlib(const QString &runtime, const Reporter &report, QString *jarPath, QString *error)
{
    const QString jar = AuthlibInjector::ensure(runtime, [&](const QString &stage, const QString &message, double progress) {
        report(stage, message, 0.95 + 0.04 * progress);
    }, error);
    if (jarPath)
        *jarPath = jar;
    return !jar.isEmpty();
}

LaunchSpec VersionManager::buildLaunchSpec(const QJsonObject &instance, const QString &runtime,
                                           const QString &javaMode, const QString &javaPath, const LaunchAccount &account,
                                           bool needAuthlib, const QString &authlibJar, const Reporter &report,
                                           QString *error)
{
    LaunchSpec spec;
    const QString versionId = instance.value(QStringLiteral("installedVersionId")).toString();
    if (versionId.isEmpty() || !isVersionInstalled(runtime, versionId)) {
        if (error)
            *error = QStringLiteral("Версия не установлена — установите её в параметрах сборки");
        return spec;
    }
    const QString mcVersion = instance.value(QStringLiteral("version")).toString();
    const QString loader = instance.value(QStringLiteral("loader")).toString();
    const QJsonObject version = loadVersion(runtime, versionId);
    if (version.isEmpty()) {
        if (error)
            *error = QStringLiteral("Не удалось прочитать профиль версии %1").arg(versionId);
        return spec;
    }

    report(QStringLiteral("launch"), QStringLiteral("Подбор Java…"), 0.9);
    QString pickError;
    const QString manual = instance.value(QStringLiteral("javaPath")).toString().isEmpty()
        ? (javaMode == QLatin1String("manual") ? javaPath : QString())
        : instance.value(QStringLiteral("javaPath")).toString();
    const PickedJava picked = JavaManager::pickFor(mcVersion, manual, loader, &pickError);
    if (picked.path.isEmpty()) {
        if (error)
            *error = pickError;
        return spec;
    }
    for (const auto &warning : picked.warnings)
        Logger::warn(warning);

    QVector<DownloadTask> libraryTasks;
    QVector<DownloadTask> nativeTasks;
    QVector<QString> classpath;
    addLibraryTasks(version.value(QStringLiteral("libraries")).toArray(), runtime, libraryTasks, classpath, nativeTasks);

    QVector<DownloadTask> allTasks = libraryTasks;
    for (const auto &task : nativeTasks)
        allTasks.append(task);

    QVector<LibraryProblem> problems;
    for (const auto &task : allTasks) {
        if (QFileInfo::exists(task.target)) {
            if (!task.sha1.isEmpty() && !fileMatchesHash(task.target, task.sha1)) {
                problems.append({task.label, task.target, task.sha1, sha1File(task.target), QFileInfo(task.target).size()});
            }
        } else {
            problems.append({task.label, task.target, task.sha1, QString(), 0});
        }
    }
    if (!problems.isEmpty()) {
        report(QStringLiteral("libraries"),
               QStringLiteral("Проверка библиотек: %1 файл(ов) повреждены — перекачиваю").arg(problems.size()), 0.9);
        QSet<QString> badTargets;
        for (const auto &problem : problems)
            badTargets.insert(problem.path);
        QVector<DownloadTask> repair;
        for (const auto &task : allTasks) {
            if (badTargets.contains(task.target)) {
                QFile::remove(task.target);
                repair.append(task);
            }
        }
        QString repairError;
        if (!downloadBatch(repair, 4, report, QStringLiteral("libraries"), QStringLiteral("Восстановление библиотек"),
                           0.90, 0.94, nullptr, &repairError)) {
            if (error)
                *error = QStringLiteral("Не удалось восстановить библиотеки: %1").arg(repairError);
            return spec;
        }
    }

    const QString jarId = version.value(QStringLiteral("jar")).toString(instance.value(QStringLiteral("version")).toString());
    const QString clientJar = QDir(runtime).filePath(QStringLiteral("versions/%1/%1.jar").arg(jarId));
    const QJsonObject clientInfo = version.value(QStringLiteral("downloads")).toObject().value(QStringLiteral("client")).toObject();
    const QString clientSha1 = clientInfo.value(QStringLiteral("sha1")).toString();
    if (!QFileInfo::exists(clientJar) || (!clientSha1.isEmpty() && !fileMatchesHash(clientJar, clientSha1))) {
        report(QStringLiteral("launch"), QStringLiteral("Восстановление клиентского jar…"), 0.94);
        QStringList urls;
        if (!clientInfo.value(QStringLiteral("url")).toString().isEmpty())
            urls << clientInfo.value(QStringLiteral("url")).toString();
        urls << kBmclapi + QStringLiteral("/version/%1/client").arg(jarId);
        QString jarError;
        if (!downloadFile(QUrl(urls.first()), clientJar, clientSha1, {}, {}, nullptr, {}, &jarError)) {
            bool repaired = false;
            for (int i = 1; i < urls.size(); ++i) {
                if (downloadFile(QUrl(urls.at(i)), clientJar, clientSha1, {}, {}, nullptr, {}, &jarError)) {
                    repaired = true;
                    break;
                }
            }
            if (!repaired && !QFileInfo::exists(clientJar)) {
                if (error)
                    *error = QStringLiteral("Клиентский jar повреждён: %1").arg(jarError);
                return spec;
            }
        }
    }
    if (QFileInfo::exists(clientJar))
        classpath.append(clientJar);

    const QJsonObject assetIndex = version.value(QStringLiteral("assetIndex")).toObject();
    if (!assetIndex.isEmpty()) {
        const QString indexId = assetIndex.value(QStringLiteral("id")).toString();
        const QString indexPath = QDir(runtime).filePath(QStringLiteral("assets/indexes/%1.json").arg(indexId));
        if (!QFileInfo::exists(indexPath)) {
            QString indexError;
            const QString indexUrl = assetIndex.value(QStringLiteral("url")).toString();
            downloadFile(QUrl(indexUrl), indexPath, assetIndex.value(QStringLiteral("sha1")).toString(), {}, {}, nullptr, {}, &indexError);
        }
        const QJsonObject objects = readJsonObject(indexPath).value(QStringLiteral("objects")).toObject();
        QVector<DownloadTask> missing;
        QSet<QString> seen;
        for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
            const QJsonObject object = it.value().toObject();
            const QString hash = object.value(QStringLiteral("hash")).toString();
            if (hash.size() < 2)
                continue;
            const QString head = hash.left(2);
            const QString target = QDir(runtime).filePath(QStringLiteral("assets/objects/%1/%2").arg(head, hash));
            if (seen.contains(target))
                continue;
            seen.insert(target);
            const qint64 size = object.value(QStringLiteral("size")).toInteger();
            if (QFileInfo::exists(target) && (size <= 0 || QFileInfo(target).size() == size))
                continue;
            QStringList urls;
            for (const auto &host : kAssetHosts)
                urls << QStringLiteral("%1/%2/%3").arg(host, head, hash);
            missing.append({urls, target, hash, size, hash});
        }
        if (!missing.isEmpty()) {
            report(QStringLiteral("assets"), QStringLiteral("Докачка ассетов: %1…").arg(missing.size()), 0.94);
            QString assetError;
            if (!downloadBatch(missing, 8, report, QStringLiteral("assets"), QStringLiteral("Ассеты"), 0.94, 0.95, nullptr, &assetError))
                Logger::warn(QStringLiteral("Не все ассеты скачались: %1").arg(assetError));
        }
    }

    const QString nativesDir = QDir(runtime).filePath(QStringLiteral("natives/%1").arg(versionId));
    QDir(nativesDir).removeRecursively();
    ensureDir(nativesDir);
    for (const auto &task : nativeTasks) {
        if (!QFileInfo::exists(task.target))
            continue;
        QString extractError;
        zipExtractAll(task.target, nativesDir, {}, &extractError);
    }

    const QString cleanUuid = QString(account.uuid).remove(QLatin1Char('-'));
    QHash<QString, QString> placeholders;
    placeholders.insert(QStringLiteral("auth_player_name"), account.name);
    placeholders.insert(QStringLiteral("version_name"), versionId);
    placeholders.insert(QStringLiteral("game_directory"), instance.value(QStringLiteral("gameDir")).toString());
    placeholders.insert(QStringLiteral("assets_root"), QDir(runtime).filePath(QStringLiteral("assets")));
    placeholders.insert(QStringLiteral("game_assets"), QDir(runtime).filePath(QStringLiteral("assets/virtual/legacy")));
    placeholders.insert(QStringLiteral("assets_index_name"), assetIndex.value(QStringLiteral("id")).toString());
    placeholders.insert(QStringLiteral("auth_uuid"), cleanUuid);
    placeholders.insert(QStringLiteral("auth_access_token"), account.accessToken);
    placeholders.insert(QStringLiteral("auth_session"), QStringLiteral("token:%1:%2").arg(account.accessToken, cleanUuid));
    placeholders.insert(QStringLiteral("clientid"), QString());
    placeholders.insert(QStringLiteral("auth_xuid"), QString());
    placeholders.insert(QStringLiteral("user_type"), account.userType);
    placeholders.insert(QStringLiteral("version_type"), version.value(QStringLiteral("type")).toString(QStringLiteral("release")));
    placeholders.insert(QStringLiteral("user_properties"), QStringLiteral("{}"));
    placeholders.insert(QStringLiteral("natives_directory"), nativesDir);
    placeholders.insert(QStringLiteral("launcher_name"), QStringLiteral("Noble Launcher"));
    placeholders.insert(QStringLiteral("launcher_version"), QStringLiteral("2.9.0"));
    placeholders.insert(QStringLiteral("classpath"), classpath.join(QLatin1Char(';')));
    placeholders.insert(QStringLiteral("classpath_separator"), QStringLiteral(";"));
    placeholders.insert(QStringLiteral("library_directory"), QDir(runtime).filePath(QStringLiteral("libraries")));
    placeholders.insert(QStringLiteral("resolution_width"), QStringLiteral("1280"));
    placeholders.insert(QStringLiteral("resolution_height"), QStringLiteral("720"));
    placeholders.insert(QStringLiteral("quickPlayPath"), QString());

    const int requestedRam = instance.value(QStringLiteral("ram")).toInt(4096);
    const HeapPlan heap = planHeap(requestedRam, totalPhysicalMemoryMb(), availablePhysicalMemoryMb(),
                                   picked.is64Bit);
    int heapMb = heap.heapMb;
    int initialMb = heap.initialMb;
    QStringList launchWarnings = picked.warnings;
    if (heap.clamped) {
        launchWarnings << heap.reason;
        Logger::warn(heap.reason);
        report(QStringLiteral("launch"), heap.reason, 0.9);
    }
    if (!picked.is64Bit)
        launchWarnings << QStringLiteral("Java 32-bit: -Xmx ограничен 1536 МБ, возможны проблемы с тяжёлыми модами");

    const QJsonObject arguments = version.value(QStringLiteral("arguments")).toObject();
    const QStringList jsonJvm = argumentList(arguments.value(QStringLiteral("jvm")).toArray(), placeholders);
    const FilteredArgs filtered = filterIncompatibleJvmFlags(
        splitArgs(instance.value(QStringLiteral("jvmArgs")).toString()), picked.major, picked.is64Bit);
    QStringList extraJvm = filtered.args;
    if (filtered.removed) {
        const QString message = QStringLiteral("Убраны несовместимые с Java %1 JVM-флаги")
                                    .arg(picked.major > 0 ? QString::number(picked.major) : QStringLiteral("?"));
        launchWarnings << message;
        Logger::warn(message);
    }

    auto validateHeap = [&](int mb, const QStringList &flags, QString *reason) {
        QStringList probe{QStringLiteral("-Xmx%1M").arg(mb), QStringLiteral("-Xms%1M").arg(std::min(initialMb, mb))};
        probe << flags;
        return JavaManager::validateJvm(picked.path, probe, reason);
    };
    QString jvmError;
    if (!validateHeap(heapMb, extraJvm, &jvmError)) {
        if (!extraJvm.isEmpty()) {
            const QString message = QStringLiteral("Дополнительные JVM-флаги несовместимы с Java %1 — использую безопасный набор")
                                        .arg(picked.major);
            launchWarnings << message;
            Logger::warn(message + QStringLiteral(": ") + jvmError);
            extraJvm.clear();
        }
        if (!validateHeap(heapMb, extraJvm, &jvmError)) {
            const int reduced = std::min(heapMb, 1024);
            if (reduced < heapMb) {
                heapMb = std::max(512, reduced);
                initialMb = std::min(initialMb, heapMb / 2);
                const QString message = QStringLiteral("JVM не смогла выделить %1 МБ — уменьшено до %2 МБ")
                                            .arg(heap.heapMb)
                                            .arg(heapMb);
                launchWarnings << message;
                Logger::warn(message + QStringLiteral(": ") + jvmError);
            }
            if (!validateHeap(heapMb, extraJvm, &jvmError)) {
                heapMb = 512;
                initialMb = 256;
                if (!validateHeap(heapMb, extraJvm, &jvmError)) {
                    if (error) {
                        *error = QStringLiteral("Java %1 (%2) не запускается с подготовленными аргументами: %3. "
                                                "Выберите другую Java в настройках или переустановите её.")
                                     .arg(picked.major > 0 ? QString::number(picked.major) : QStringLiteral("?"),
                                          picked.path, jvmError);
                    }
                    spec.warnings = launchWarnings;
                    return spec;
                }
            }
        }
    }

    QStringList jvmArgs;
    jvmArgs << QStringLiteral("-Xmx%1M").arg(heapMb)
            << QStringLiteral("-Xms%1M").arg(std::min(initialMb, heapMb));
    jvmArgs << QStringLiteral("-Dminecraft.launcher.brand=Noble Launcher")
            << QStringLiteral("-Dminecraft.launcher.version=2.9.0");

    if (jsonJvm.isEmpty()) {
        jvmArgs << QStringLiteral("-Djava.library.path=%1").arg(nativesDir);
        jvmArgs << QStringLiteral("-cp") << classpath.join(QLatin1Char(';'));
    } else {
        jvmArgs << jsonJvm;
        bool hasClasspath = false;
        for (const auto &arg : jvmArgs) {
            if (arg == QLatin1String("-classpath") || arg == QLatin1String("-cp"))
                hasClasspath = true;
        }
        if (!hasClasspath)
            jvmArgs << QStringLiteral("-cp") << classpath.join(QLatin1Char(';'));
    }
    const QJsonObject loggingFile = version.value(QStringLiteral("logging")).toObject().value(QStringLiteral("client")).toObject().value(QStringLiteral("file")).toObject();
    if (!loggingFile.isEmpty()) {
        const QString config = QDir(runtime).filePath(QStringLiteral("assets/log_configs/%1").arg(loggingFile.value(QStringLiteral("id")).toString()));
        if (QFileInfo::exists(config))
            jvmArgs << QStringLiteral("-Dlog4j.configurationFile=%1").arg(config);
    }
    if (needAuthlib && !authlibJar.isEmpty())
        jvmArgs << QStringLiteral("-javaagent:%1=%2").arg(authlibJar, QString::fromLatin1(AuthlibInjector::kServerUrl));
    jvmArgs << extraJvm;

    QStringList gameArgs;
    if (arguments.contains(QStringLiteral("game")) && !arguments.value(QStringLiteral("game")).toArray().isEmpty()) {
        gameArgs = argumentList(arguments.value(QStringLiteral("game")).toArray(), placeholders);
    } else {
        const QString legacy = version.value(QStringLiteral("minecraftArguments")).toString();
        if (!legacy.isEmpty()) {
            for (const auto &arg : splitArgs(legacy))
                gameArgs << expandArgs(arg, placeholders);
        }
    }
    gameArgs << splitArgs(instance.value(QStringLiteral("gameArgs")).toString());

    spec.javaPath = picked.path;
    spec.javaMajor = picked.major;
    spec.java64Bit = picked.is64Bit;
    spec.warnings = launchWarnings;
    spec.arguments = jvmArgs;
    spec.arguments << version.value(QStringLiteral("mainClass")).toString() << gameArgs;
    spec.workingDir = instance.value(QStringLiteral("gameDir")).toString();
    spec.instanceId = instance.value(QStringLiteral("id")).toString();
    spec.versionId = versionId;
    if (spec.arguments.isEmpty() || version.value(QStringLiteral("mainClass")).toString().isEmpty()) {
        if (error)
            *error = QStringLiteral("Повреждён профиль версии: нет mainClass");
        return {};
    }
    return spec;
}

} // namespace Noble
