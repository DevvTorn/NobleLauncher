#include "NobleBuildsService.h"

#include "Http.h"
#include "InstanceManager.h"
#include "Invoke.h"
#include "Json.h"
#include "Logger.h"
#include "ModrinthService.h"
#include "NbFormat.h"
#include "Zip.h"
#include "Paths.h"
#include "Pure.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

namespace Noble {

static const QString kNobleManifest = QStringLiteral("https://raw.githubusercontent.com/DevvTorn/NobleLauncher/main/builds/manifest.json");

NobleBuildsService::NobleBuildsService(QObject *parent)
    : QObject(parent)
{
}

QVariantList NobleBuildsService::builds() const
{
    return m_builds.toVariantList();
}

static QJsonObject normalizeBuild(const QJsonObject &raw)
{
    const QString id = raw.value(QStringLiteral("id")).toString().trimmed();
    const QString name = raw.value(QStringLiteral("name")).toString().trimmed();
    if (id.isEmpty() || name.isEmpty())
        return {};
    QString loader = raw.value(QStringLiteral("loader")).toString();
    const QStringList loaders = {QStringLiteral("Vanilla"), QStringLiteral("Forge"), QStringLiteral("Fabric"),
                                 QStringLiteral("Quilt"), QStringLiteral("NeoForge")};
    if (!loaders.contains(loader))
        loader = QStringLiteral("Vanilla");
    QJsonArray mods;
    QJsonArray modLinks;
    for (const auto &mod : raw.value(QStringLiteral("mods")).toArray()) {
        if (mod.isString()) {
            mods.append(mod.toString());
        } else if (mod.isObject()) {
            const QJsonObject entry = mod.toObject();
            const QString name = entry.value(QStringLiteral("name")).toString();
            if (!name.isEmpty())
                mods.append(name);
            if (!entry.value(QStringLiteral("url")).toString().isEmpty())
                modLinks.append(QJsonObject{{QStringLiteral("name"), name},
                                            {QStringLiteral("url"), entry.value(QStringLiteral("url"))}});
        }
    }
    for (const auto &link : raw.value(QStringLiteral("modLinks")).toArray()) {
        const QJsonObject entry = link.toObject();
        if (!entry.value(QStringLiteral("url")).toString().isEmpty())
            modLinks.append(entry);
    }
    return {{QStringLiteral("id"), id},
            {QStringLiteral("name"), name},
            {QStringLiteral("author"), raw.value(QStringLiteral("author")).toString(QStringLiteral("Noble Launcher"))},
            {QStringLiteral("description"), raw.value(QStringLiteral("description")).toString()},
            {QStringLiteral("mcVersion"), raw.value(QStringLiteral("mcVersion")).toString()},
            {QStringLiteral("loader"), loader},
            {QStringLiteral("loaderVersion"), raw.value(QStringLiteral("loaderVersion")).toString()},
            {QStringLiteral("banner"), raw.value(QStringLiteral("banner")).toString()},
            {QStringLiteral("icon"), raw.value(QStringLiteral("icon")).toString()},
            {QStringLiteral("archiveUrl"), raw.value(QStringLiteral("archiveUrl")).toString()},
            {QStringLiteral("mods"), mods},
            {QStringLiteral("modLinks"), modLinks},
            {QStringLiteral("beta"), raw.value(QStringLiteral("beta")).toBool(true)}};
}

void NobleBuildsService::load(bool force)
{
    if (m_loading)
        return;
    if (!force && m_fetchedAt > 0 && QDateTime::currentMSecsSinceEpoch() - m_fetchedAt < 60 * 60 * 1000)
        return;
    m_loading = true;
    emit loadingChanged();
    QPointer<NobleBuildsService> guard(this);
    const QString cacheFile = QDir(Paths::dataDir()).filePath(QStringLiteral("noble-builds-cache.json"));
    QtConcurrent::run([guard, cacheFile] {
        QJsonArray bundledBuilds;
        QFile bundled(QStringLiteral(":/assets/noble-builds-manifest.json"));
        if (bundled.open(QIODevice::ReadOnly)) {
            const QJsonArray raw = QJsonDocument::fromJson(bundled.readAll()).object().value(QStringLiteral("builds")).toArray();
            for (const auto &value : raw) {
                const QJsonObject build = normalizeBuild(value.toObject());
                if (!build.isEmpty())
                    bundledBuilds.append(build);
            }
        }
        QJsonArray builds;
        bool fromCache = false;
        const HttpResult response = httpGet(QUrl(kNobleManifest), {{QStringLiteral("Accept"), QStringLiteral("application/json")}}, 20000);
        if (response.ok) {
            const QJsonArray raw = response.object().value(QStringLiteral("builds")).toArray();
            for (const auto &value : raw) {
                const QJsonObject build = normalizeBuild(value.toObject());
                if (!build.isEmpty())
                    builds.append(build);
            }
            if (builds.size() >= bundledBuilds.size()) {
                writeJsonObject(cacheFile, QJsonObject{{QStringLiteral("at"), static_cast<double>(QDateTime::currentMSecsSinceEpoch())},
                                                       {QStringLiteral("builds"), builds}});
            } else {
                builds = bundledBuilds;
            }
        } else {
            Logger::warn(QStringLiteral("Сборки от Noble: манифест недоступен (%1), использую кэш").arg(response.error));
            const QJsonObject cached = readJsonObject(cacheFile);
            builds = cached.value(QStringLiteral("builds")).toArray();
            if (builds.size() < bundledBuilds.size()) {
                builds = bundledBuilds;
                fromCache = false;
            } else {
                fromCache = true;
            }
        }
        if (builds.isEmpty())
            builds = bundledBuilds;
        if (!guard)
            return;
        QMetaObject::invokeMethod(guard, [guard, builds, fromCache] {
            if (!guard)
                return;
            guard->m_builds = builds;
            guard->m_fromCache = fromCache;
            guard->m_loading = false;
            guard->m_fetchedAt = QDateTime::currentMSecsSinceEpoch();
            emit guard->loadingChanged();
            emit guard->buildsChanged();
        }, Qt::QueuedConnection);
    });
}

QJsonObject NobleBuildsService::install(const QString &buildId, const QJsonArray &builds, InstanceManager *instances,
                                        const std::function<void(const QString &, const QString &, double)> &report,
                                        std::atomic_bool *cancel, QString *error)
{
    QJsonObject build;
    for (const auto &value : builds) {
        const QJsonObject candidate = value.toObject();
        if (candidate.value(QStringLiteral("id")).toString() == buildId) {
            build = candidate;
            break;
        }
    }
    if (build.isEmpty()) {
        if (error)
            *error = QStringLiteral("Сборка не найдена в манифесте");
        return {};
    }
    const QString url = build.value(QStringLiteral("archiveUrl")).toString();
    if (url.isEmpty()) {
        if (error)
            *error = QStringLiteral("Архив этой сборки ещё не опубликован");
        return {};
    }
    const QString fileName = QUrl(url).fileName().isEmpty() ? buildId + QStringLiteral(".zip") : QUrl(url).fileName();
    const QString temp = QDir(Paths::tempDir()).filePath(QStringLiteral("noble-build-%1-%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(fileName));
    QDir().mkpath(Paths::tempDir());
    if (report)
        report(QStringLiteral("modpack"), QStringLiteral("Скачивание «%1»…").arg(build.value(QStringLiteral("name")).toString()), 0.05);
    QString downloadError;
    bool downloaded = false;
    if (url.startsWith(QLatin1String("qrc:/"))) {
        const QString source = QLatin1Char(':') + url.mid(5);
        QDir().mkpath(QFileInfo(temp).absolutePath());
        downloaded = QFile::copy(source, temp);
        if (!downloaded)
            downloadError = QStringLiteral("не найден встроенный архив");
    } else if (url.startsWith(QLatin1String("file:"))) {
        downloaded = QFile::copy(QUrl(url).toLocalFile(), temp);
        if (!downloaded)
            downloadError = QStringLiteral("локальный архив не найден");
    } else {
        downloaded = downloadFile(QUrl(url), temp, {}, {}, {}, cancel, [&](qint64 received, qint64 total) {
            if (total > 0 && report)
                report(QStringLiteral("modpack"),
                       QStringLiteral("«%1»: %2%").arg(build.value(QStringLiteral("name")).toString(),
                                                      QString::number(static_cast<int>(received * 100 / total))),
                       0.05 + 0.6 * (static_cast<double>(received) / total));
        }, &downloadError, 60000);
    }
    if (!downloaded) {
        if (error)
            *error = QStringLiteral("Не удалось скачать сборку: %1").arg(downloadError);
        QFile::remove(temp);
        return {};
    }
    QJsonObject instance;
    QByteArray manifestData;
    QString manifestError;
    if (zipReadEntry(temp, QStringLiteral("noble.json"), manifestData, &manifestError)) {
        instance = NbFormat::importArchive(instances, temp,
                                           [&](const QString &stage, const QString &message, double progress) {
                                               if (report)
                                                   report(stage, message, progress);
                                           },
                                           cancel, error, nullptr);
    } else if (fileName.endsWith(QLatin1String(".mrpack"), Qt::CaseInsensitive)) {
        instance = ModrinthService::installMrpackFile(temp, instances, report, cancel, error);
    } else {
        instance = invokeBlocking(instances, [instances, temp] {
            return QJsonObject::fromVariantMap(instances->importArchive(temp));
        });
    }
    QFile::remove(temp);
    if (instance.isEmpty() && error && error->isEmpty())
        *error = QStringLiteral("Не удалось установить сборку");
    if (!instance.isEmpty())
        Logger::info(QStringLiteral("Сборка от Noble установлена: %1").arg(buildId));
    return instance;
}

} // namespace Noble
