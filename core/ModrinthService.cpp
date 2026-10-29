#include "ModrinthService.h"

#include "Http.h"
#include "InstanceManager.h"
#include "Invoke.h"
#include "Json.h"
#include "Logger.h"
#include "Paths.h"
#include "Pure.h"
#include "Zip.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMutex>
#include <QSet>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

namespace Noble {

static const QString kApi = QStringLiteral("https://api.modrinth.com/v2");

namespace {

QString loaderTag(const QString &loader)
{
    if (loader == QLatin1String("Fabric"))
        return QStringLiteral("fabric");
    if (loader == QLatin1String("Forge"))
        return QStringLiteral("forge");
    if (loader == QLatin1String("NeoForge"))
        return QStringLiteral("neoforge");
    if (loader == QLatin1String("Quilt"))
        return QStringLiteral("quilt");
    return QString();
}

QString projectTypeTag(const QString &type)
{
    if (type == QLatin1String("shader"))
        return QStringLiteral("shader");
    return type;
}

QJsonObject getJson(const QString &url, int timeout = 30000)
{
    const HttpResult response = httpGet(QUrl(url), {}, timeout);
    if (!response.ok) {
        Logger::warn(QStringLiteral("Modrinth: %1 — %2").arg(url, response.error));
        return {};
    }
    return response.object();
}

QJsonArray getJsonArray(const QString &url, int timeout = 30000)
{
    const HttpResult response = httpGet(QUrl(url), {}, timeout);
    if (!response.ok) {
        Logger::warn(QStringLiteral("Modrinth: %1 — %2").arg(url, response.error));
        return {};
    }
    return response.json().array();
}

QJsonObject versionToFile(const QJsonObject &version)
{
    QJsonArray files = version.value(QStringLiteral("files")).toArray();
    QJsonObject primary;
    for (const auto &value : files) {
        const QJsonObject file = value.toObject();
        if (file.value(QStringLiteral("primary")).toBool(false)) {
            primary = file;
            break;
        }
    }
    if (primary.isEmpty() && !files.isEmpty())
        primary = files.first().toObject();
    QJsonArray dependencies;
    for (const auto &value : version.value(QStringLiteral("dependencies")).toArray()) {
        const QJsonObject dep = value.toObject();
        dependencies.append(QJsonObject{{QStringLiteral("projectId"), dep.value(QStringLiteral("project_id")).toString()},
                                        {QStringLiteral("versionId"), dep.value(QStringLiteral("version_id")).toString()},
                                        {QStringLiteral("type"), dep.value(QStringLiteral("dependency_type")).toString(QStringLiteral("required"))}});
    }
    return QJsonObject{{QStringLiteral("id"), version.value(QStringLiteral("id")).toString()},
                       {QStringLiteral("name"), version.value(QStringLiteral("name")).toString()},
                       {QStringLiteral("fileName"), primary.value(QStringLiteral("filename")).toString()},
                       {QStringLiteral("downloadUrl"), primary.value(QStringLiteral("url")).toString()},
                       {QStringLiteral("hashes"), primary.value(QStringLiteral("hashes")).toObject()},
                       {QStringLiteral("size"), primary.value(QStringLiteral("size")).toInteger()},
                       {QStringLiteral("version"), version.value(QStringLiteral("version_number")).toString()},
                       {QStringLiteral("gameVersions"), version.value(QStringLiteral("game_versions")).toArray()},
                       {QStringLiteral("loaders"), version.value(QStringLiteral("loaders")).toArray()},
                       {QStringLiteral("dependencies"), dependencies},
                       {QStringLiteral("projectId"), version.value(QStringLiteral("project_id")).toString()}};
}

QString versionsUrl(const QString &projectId, const QString &gameVersion, const QString &loader, const QString &kind)
{
    QUrlQuery query;
    if (!gameVersion.isEmpty())
        query.addQueryItem(QStringLiteral("game_versions"), QString(QJsonDocument(QJsonArray{gameVersion}).toJson(QJsonDocument::Compact)));
    const QString tag = loaderTag(loader);
    if (!tag.isEmpty())
        query.addQueryItem(QStringLiteral("loaders"), QString(QJsonDocument(QJsonArray{tag}).toJson(QJsonDocument::Compact)));
    QString url = QStringLiteral("%1/project/%2/version").arg(kApi, QString::fromUtf8(QUrl::toPercentEncoding(projectId)));
    if (!query.isEmpty())
        url += QLatin1Char('?') + query.toString(QUrl::FullyEncoded);
    return url;
}

QJsonObject fetchVersion(const QString &project, const QString &versionId, const QString &gameVersion, const QString &loader,
                         const QString &kind)
{
    if (!versionId.isEmpty()) {
        const QJsonObject version = getJson(QStringLiteral("%1/version/%2").arg(kApi, QString::fromUtf8(QUrl::toPercentEncoding(versionId))));
        if (!version.isEmpty())
            return version;
    }
    const QJsonArray list = getJsonArray(versionsUrl(project, gameVersion, loader, kind));
    if (!list.isEmpty())
        return list.first().toObject();
    return {};
}

QString resolveVersionId(const QString &projectId, const QString &candidate, const QString &gameVersion, const QString &loader,
                         const QString &kind)
{
    if (candidate.isEmpty())
        return QString();
    const QString effectiveLoader = kind == QLatin1String("mod") ? loader : QString();
    const QJsonArray versions = getJsonArray(versionsUrl(projectId, gameVersion, effectiveLoader, kind));
    for (const auto &value : versions) {
        const QJsonObject version = value.toObject();
        if (version.value(QStringLiteral("id")).toString() == candidate)
            return candidate;
        if (version.value(QStringLiteral("version_number")).toString() == candidate)
            return version.value(QStringLiteral("id")).toString();
    }
    return candidate;
}

bool addRecord(InstanceManager *instances, const QString &instanceId, const QJsonObject &record)
{
    if (!instances)
        return false;
    return invokeBlocking(instances, [instances, instanceId, record] {
        instances->addMod(instanceId, record.toVariantMap());
        return true;
    });
}

} // namespace

QString ModrinthService::folderFor(const QString &kind)
{
    if (kind == QLatin1String("resourcepack"))
        return QStringLiteral("resourcepacks");
    if (kind == QLatin1String("shaderpack"))
        return QStringLiteral("shaderpacks");
    return QStringLiteral("mods");
}

QVariantList ModrinthService::search(const QString &query, const QString &projectType, const QString &gameVersion,
                                     const QString &loader, int offset, int limit)
{
    QJsonArray facets;
    facets.append(QJsonArray{QStringLiteral("project_type:%1").arg(projectTypeTag(projectType))});
    if (!gameVersion.isEmpty())
        facets.append(QJsonArray{QStringLiteral("versions:%1").arg(gameVersion)});
    if (!loader.isEmpty() && projectType == QLatin1String("mod")) {
        const QString tag = loaderTag(loader);
        if (!tag.isEmpty())
            facets.append(QJsonArray{QStringLiteral("categories:%1").arg(tag)});
    }
    QUrlQuery params;
    params.addQueryItem(QStringLiteral("query"), query);
    params.addQueryItem(QStringLiteral("limit"), QString::number(std::clamp(limit, 1, 50)));
    params.addQueryItem(QStringLiteral("offset"), QString::number(std::max(0, offset)));
    params.addQueryItem(QStringLiteral("index"), query.isEmpty() ? QStringLiteral("downloads") : QStringLiteral("relevance"));
    params.addQueryItem(QStringLiteral("facets"), QString::fromUtf8(QJsonDocument(facets).toJson(QJsonDocument::Compact)));
    const HttpResult response = httpGet(QUrl(QStringLiteral("%1/search?%2").arg(kApi, params.toString(QUrl::FullyEncoded))), {}, 30000);
    if (!response.ok) {
        Logger::warn(QStringLiteral("Modrinth search: %1").arg(response.error));
        return {};
    }
    QVariantList results;
    for (const auto &value : response.object().value(QStringLiteral("hits")).toArray()) {
        const QJsonObject hit = value.toObject();
        results.append(QVariantMap{{QStringLiteral("id"), hit.value(QStringLiteral("project_id")).toString()},
                                   {QStringLiteral("slug"), hit.value(QStringLiteral("slug")).toString()},
                                   {QStringLiteral("title"), hit.value(QStringLiteral("title")).toString()},
                                   {QStringLiteral("description"), hit.value(QStringLiteral("description")).toString()},
                                   {QStringLiteral("author"), hit.value(QStringLiteral("author")).toString()},
                                   {QStringLiteral("downloads"), static_cast<double>(hit.value(QStringLiteral("downloads")).toDouble())},
                                   {QStringLiteral("iconUrl"), hit.value(QStringLiteral("icon_url")).toString()},
                                   {QStringLiteral("categories"), hit.value(QStringLiteral("categories")).toArray().toVariantList()},
                                   {QStringLiteral("versions"), hit.value(QStringLiteral("versions")).toArray().toVariantList()},
                                   {QStringLiteral("source"), QStringLiteral("modrinth")},
                                   {QStringLiteral("projectType"), hit.value(QStringLiteral("project_type")).toString(projectType)},
                                   {QStringLiteral("updatedAt"), hit.value(QStringLiteral("date_modified")).toString()}});
    }
    return results;
}

QVariantList ModrinthService::versionsFor(const QString &projectId, const QString &gameVersion, const QString &loader,
                                         const QString &projectType)
{
    QVariantList list;
    const QString effectiveLoader = projectType == QLatin1String("mod") ? loader : QString();
    const QJsonArray versions = getJsonArray(versionsUrl(projectId, gameVersion, effectiveLoader, projectType));
    for (const auto &value : versions)
        list.append(versionToFile(value.toObject()).toVariantMap());
    return list;
}

ContentInstallResult ModrinthService::installContent(InstanceManager *instances, const QString &instanceId,
                                                     const QString &kind, const QString &projectId,
                                                     const QString &versionId, const ContentInstallReport &report,
                                                     std::atomic_bool *cancel, QString *error)
{
    ContentInstallResult result;
    const QJsonObject instance = invokeBlocking(instances, [instances, instanceId] { return instances->get(instanceId); });
    if (instance.isEmpty()) {
        if (error)
            *error = QStringLiteral("Сборка не найдена");
        return result;
    }
    const QString gameDir = instance.value(QStringLiteral("gameDir")).toString();
    const QString gameVersion = instance.value(QStringLiteral("version")).toString();
    const QString loader = instance.value(QStringLiteral("loader")).toString();
    const QString folder = folderFor(kind);
    QDir().mkpath(QDir(gameDir).filePath(folder));

    QSet<QString> visited;
    std::function<void(const QString &, const QString &, int)> installRecursive;
    installRecursive = [&](const QString &project, const QString &version, int depth) {
        if (depth > 6 || (cancel && cancel->load()))
            return;
        const QString key = project + QStringLiteral("@") + (version.isEmpty() ? QStringLiteral("latest") : version);
        if (visited.contains(key))
            return;
        visited.insert(key);
        if (report.fn)
            report.fn(QStringLiteral("resolve"),
                      QStringLiteral("Подбор версии (%1)…").arg(depth > 0 ? QStringLiteral("зависимость") : QStringLiteral("проект")),
                      0.02, 0, 0);
        const QString effectiveLoader = kind == QLatin1String("mod") ? loader : QString();
        QJsonObject versionJson = fetchVersion(project, version, gameVersion, effectiveLoader, kind);
        if (versionJson.isEmpty()) {
            result.skipped << project;
            if (report.fn)
                report.fn(QStringLiteral("resolve"), QStringLiteral("Подходящая версия не найдена — пропущено"), 1, 0, 0);
            return;
        }
        const QJsonObject file = versionToFile(versionJson);
        const QString downloadUrl = file.value(QStringLiteral("downloadUrl")).toString();
        const QString fileName = file.value(QStringLiteral("fileName")).toString();
        if (downloadUrl.isEmpty() || fileName.isEmpty()) {
            result.skipped << project;
            return;
        }
        const QJsonObject projectInfo = getJson(QStringLiteral("%1/project/%2").arg(kApi, QString::fromUtf8(QUrl::toPercentEncoding(project))), 20000);
        const QString title = projectInfo.value(QStringLiteral("title")).toString(file.value(QStringLiteral("name")).toString(fileName));
        const QString target = QDir(QDir(gameDir).filePath(folder)).filePath(fileName);
        const QJsonObject hashes = file.value(QStringLiteral("hashes")).toObject();
        QString downloadError;
        const bool downloaded = downloadFile(QUrl(downloadUrl), target, hashes.value(QStringLiteral("sha1")).toString(),
                                             hashes.value(QStringLiteral("sha512")).toString(), {}, cancel,
                                             [&](qint64 received, qint64 total) {
                                                 if (report.fn) {
                                                     const double ratio = total > 0 ? static_cast<double>(received) / total : 0.4;
                                                     report.fn(QStringLiteral("download"), QStringLiteral("Скачивание %1").arg(title),
                                                               0.05 + 0.8 * ratio, received, total);
                                                 }
                                             }, &downloadError);
        if (!downloaded) {
            if (downloadError.contains(QLatin1String("отменена"))) {
                if (error)
                    *error = downloadError;
                return;
            }
            result.skipped << title;
            if (report.fn)
                report.fn(QStringLiteral("download"), QStringLiteral("Не удалось скачать %1: %2").arg(title, downloadError), 0.2, 0, 0);
            return;
        }
        if (report.fn)
            report.fn(QStringLiteral("verify"), QStringLiteral("Проверка %1…").arg(title), 0.9, 0, 0);
        if (kind == QLatin1String("mod")) {
            addRecord(instances, instanceId,
                      QJsonObject{{QStringLiteral("id"), file.value(QStringLiteral("id")).toString()},
                                  {QStringLiteral("name"), title},
                                  {QStringLiteral("fileName"), fileName},
                                  {QStringLiteral("source"), QStringLiteral("modrinth")},
                                  {QStringLiteral("projectId"), projectInfo.value(QStringLiteral("slug")).toString(project)},
                                  {QStringLiteral("versionId"), file.value(QStringLiteral("id")).toString()},
                                  {QStringLiteral("version"), file.value(QStringLiteral("version")).toString()},
                                  {QStringLiteral("url"), downloadUrl}});
        }
        result.installed << title;
        if (report.fn)
            report.fn(QStringLiteral("install"), QStringLiteral("Установлено: %1").arg(title), 0.95, 0, 0);
        for (const auto &value : file.value(QStringLiteral("dependencies")).toArray()) {
            const QJsonObject dep = value.toObject();
            if (dep.value(QStringLiteral("type")).toString() != QLatin1String("required"))
                continue;
            const QString depProject = dep.value(QStringLiteral("projectId")).toString();
            if (!depProject.isEmpty())
                installRecursive(depProject, dep.value(QStringLiteral("versionId")).toString(), depth + 1);
        }
    };

    installRecursive(projectId, versionId, 0);
    if (result.installed.isEmpty() && result.skipped.isEmpty() && error && error->isEmpty())
        *error = QStringLiteral("Не удалось установить контент");
    return result;
}

QJsonObject ModrinthService::parseDirectUrl(const QString &raw, QString *error)
{
    const QString text = raw.trimmed();
    const QUrl url(text);
    if (!url.isValid() || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))) {
        if (error)
            *error = QStringLiteral("Ссылка должна начинаться с http:// или https://");
        return {};
    }
    const QString host = url.host().toLower();
    const QStringList segments = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (host.endsWith(QLatin1String("curseforge.com"))) {
        if (error)
            *error = QStringLiteral("CurseForge больше не поддерживается — используйте Modrinth или прямую ссылку на файл");
        return {};
    }
    if (host == QLatin1String("modrinth.com") && segments.size() >= 2) {
        const QString type = segments[0];
        if (type == QLatin1String("mod") || type == QLatin1String("modpack") || type == QLatin1String("resourcepack")
            || type == QLatin1String("shader") || type == QLatin1String("shaders")) {
            const QString versionId = segments.size() >= 4 && segments[2] == QLatin1String("version") ? segments[3] : QString();
            return {{QStringLiteral("kind"), QStringLiteral("modrinth")},
                    {QStringLiteral("projectType"), type == QLatin1String("shaders") ? QStringLiteral("shader") : type},
                    {QStringLiteral("slug"), segments[1]},
                    {QStringLiteral("fileId"), versionId},
                    {QStringLiteral("url"), text}};
        }
    }
    if (url.path().endsWith(QLatin1String(".mrpack"), Qt::CaseInsensitive)) {
        return {{QStringLiteral("kind"), QStringLiteral("file")}, {QStringLiteral("projectType"), QStringLiteral("modpack")}, {QStringLiteral("url"), text}};
    }
    if (url.path().endsWith(QLatin1String(".jar"), Qt::CaseInsensitive)) {
        return {{QStringLiteral("kind"), QStringLiteral("file")}, {QStringLiteral("projectType"), QStringLiteral("mod")}, {QStringLiteral("url"), text}};
    }
    return {{QStringLiteral("kind"), QStringLiteral("file")}, {QStringLiteral("url"), text}, {QStringLiteral("name"), segments.isEmpty() ? QString() : segments.last()}};
}

QJsonObject ModrinthService::installFromUrl(InstanceManager *instances, const QString &instanceId, const QString &url,
                                            const ContentInstallReport &report, std::atomic_bool *cancel, QString *error,
                                            QString *message)
{
    QString parseError;
    const QJsonObject info = parseDirectUrl(url, &parseError);
    if (info.isEmpty()) {
        if (error)
            *error = parseError;
        return {};
    }
    if (cancel && cancel->load()) {
        if (error)
            *error = QStringLiteral("Операция отменена");
        return {};
    }
    if (info.value(QStringLiteral("kind")).toString() == QLatin1String("modrinth")) {
        const QString slug = info.value(QStringLiteral("slug")).toString();
        const QJsonObject project = getJson(QStringLiteral("%1/project/%2").arg(kApi, QString::fromUtf8(QUrl::toPercentEncoding(slug))));
        if (project.isEmpty()) {
            if (error)
                *error = QStringLiteral("Проект не найден на Modrinth");
            return {};
        }
        const QString type = project.value(QStringLiteral("project_type")).toString(info.value(QStringLiteral("projectType")).toString());
        const QString projectId = project.value(QStringLiteral("id")).toString();
        QString versionId = info.value(QStringLiteral("fileId")).toString();
        const QJsonObject instance = invokeBlocking(instances, [instances, instanceId] { return instances->get(instanceId); });
        const QString gameVersion = instance.value(QStringLiteral("version")).toString();
        const QString loader = instance.value(QStringLiteral("loader")).toString();
        if (!versionId.isEmpty())
            versionId = resolveVersionId(projectId, versionId, gameVersion, loader, type);
        if (type == QLatin1String("modpack")) {
            const QJsonArray versions = getJsonArray(versionsUrl(projectId, {}, {}, type));
            QJsonObject file;
            for (const auto &value : versions) {
                const QJsonObject candidate = value.toObject();
                if (versionId.isEmpty() || candidate.value(QStringLiteral("id")).toString() == versionId
                    || candidate.value(QStringLiteral("version_number")).toString() == versionId) {
                    file = versionToFile(candidate);
                    break;
                }
            }
            if (file.isEmpty() && !versions.isEmpty())
                file = versionToFile(versions.first().toObject());
            if (file.isEmpty()) {
                if (error)
                    *error = QStringLiteral("Не найдена версия модпака");
                return {};
            }
            const QString temp = QDir(Paths::tempDir()).filePath(QStringLiteral("noble-pack-%1.mrpack").arg(QDateTime::currentMSecsSinceEpoch()));
            QDir().mkpath(Paths::tempDir());
            QString downloadError;
            if (!downloadFile(QUrl(file.value(QStringLiteral("downloadUrl")).toString()), temp, {}, {}, {}, cancel, {}, &downloadError)) {
                if (error)
                    *error = downloadError;
                return {};
            }
            QJsonObject installed = installMrpackFile(temp, instances, [&](const QString &stage, const QString &msg, double progress) {
                if (report.fn)
                    report.fn(stage, msg, progress, 0, 0);
            }, cancel, error);
            QFile::remove(temp);
            if (message)
                *message = QStringLiteral("Модпак установлен");
            return installed;
        }
        if (instanceId.isEmpty()) {
            if (error)
                *error = QStringLiteral("Сначала выберите сборку для установки");
            return {};
        }
        const QString kind = type == QLatin1String("resourcepack") ? QStringLiteral("resourcepack")
            : (type == QLatin1String("shader") ? QStringLiteral("shaderpack") : QStringLiteral("mod"));
        const ContentInstallResult result = installContent(instances, instanceId, kind, projectId, versionId, report, cancel, error);
        if (message)
            *message = result.installed.isEmpty() ? QStringLiteral("Не удалось подобрать совместимую версию")
                                                  : QStringLiteral("Установлено: %1").arg(result.installed.join(QStringLiteral(", ")));
        return {};
    }

    const QString fileName = QUrl(url).fileName().isEmpty() ? QStringLiteral("file-%1.jar").arg(QDateTime::currentMSecsSinceEpoch())
                                                           : QUrl(url).fileName();
    const QString temp = QDir(Paths::tempDir()).filePath(QStringLiteral("noble-direct-%1-%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(fileName));
    QDir().mkpath(Paths::tempDir());
    if (report.fn)
        report.fn(QStringLiteral("download"), QStringLiteral("Скачивание %1…").arg(fileName), 0.05, 0, 0);
    QString downloadError;
    if (!downloadFile(QUrl(url), temp, {}, {}, {}, cancel, [&](qint64 received, qint64 total) {
            if (report.fn && total > 0)
                report.fn(QStringLiteral("download"), QStringLiteral("%1: %2%").arg(fileName).arg(static_cast<int>(received * 100 / total)),
                          0.05 + 0.7 * static_cast<double>(received) / total, received, total);
        }, &downloadError)) {
        if (error)
            *error = downloadError;
        QFile::remove(temp);
        return {};
    }
    if (fileName.endsWith(QLatin1String(".mrpack"), Qt::CaseInsensitive)) {
        QJsonObject installed = installMrpackFile(temp, instances, [&](const QString &stage, const QString &msg, double progress) {
            if (report.fn)
                report.fn(stage, msg, progress, 0, 0);
        }, cancel, error);
        QFile::remove(temp);
        if (message)
            *message = QStringLiteral("Модпак установлен");
        return installed;
    }
    if (!fileName.endsWith(QLatin1String(".jar"), Qt::CaseInsensitive)) {
        QFile::remove(temp);
        if (error)
            *error = QStringLiteral("Поддерживаются прямые ссылки на .jar и .mrpack (модпаки Modrinth)");
        return {};
    }
    if (instanceId.isEmpty()) {
        QFile::remove(temp);
        if (error)
            *error = QStringLiteral("Сначала выберите сборку для установки файла");
        return {};
    }
    const QJsonObject instance = invokeBlocking(instances, [instances, instanceId] { return instances->get(instanceId); });
    if (instance.isEmpty()) {
        QFile::remove(temp);
        if (error)
            *error = QStringLiteral("Сборка не найдена");
        return {};
    }
    const QString target = QDir(QDir(instance.value(QStringLiteral("gameDir")).toString()).filePath(QStringLiteral("mods"))).filePath(fileName);
    QDir().mkpath(QFileInfo(target).absolutePath());
    QFile::remove(target);
    if (!QFile::copy(temp, target)) {
        QFile::remove(temp);
        if (error)
            *error = QStringLiteral("Не удалось скопировать файл в mods");
        return {};
    }
    QFile::remove(temp);
    addRecord(instances, instanceId,
              QJsonObject{{QStringLiteral("id"), fileName},
                          {QStringLiteral("name"), fileName.chopped(4)},
                          {QStringLiteral("fileName"), fileName},
                          {QStringLiteral("source"), QStringLiteral("manual")},
                          {QStringLiteral("url"), url}});
    if (message)
        *message = QStringLiteral("Файл %1 установлен").arg(fileName);
    return {};
}

QJsonObject ModrinthService::installMrpackFile(const QString &archivePath, InstanceManager *instances,
                                               const std::function<void(const QString &, const QString &, double)> &report,
                                               std::atomic_bool *cancel, QString *error)
{
    QByteArray indexData;
    QString zipError;
    if (!zipReadEntry(archivePath, QStringLiteral("modrinth.index.json"), indexData, &zipError)) {
        if (error)
            *error = QStringLiteral("Это не .mrpack: отсутствует modrinth.index.json");
        return {};
    }
    const QJsonObject index = QJsonDocument::fromJson(indexData).object();
    const QJsonObject dependencies = index.value(QStringLiteral("dependencies")).toObject();
    const QString mcVersion = dependencies.value(QStringLiteral("minecraft")).toString();
    QString loader = QStringLiteral("Vanilla");
    QString loaderVersion;
    if (dependencies.contains(QStringLiteral("forge"))) {
        loader = QStringLiteral("Forge");
        loaderVersion = dependencies.value(QStringLiteral("forge")).toString();
    } else if (dependencies.contains(QStringLiteral("neoforge"))) {
        loader = QStringLiteral("NeoForge");
        loaderVersion = dependencies.value(QStringLiteral("neoforge")).toString();
    } else if (dependencies.contains(QStringLiteral("fabric-loader"))) {
        loader = QStringLiteral("Fabric");
        loaderVersion = dependencies.value(QStringLiteral("fabric-loader")).toString();
    } else if (dependencies.contains(QStringLiteral("quilt-loader"))) {
        loader = QStringLiteral("Quilt");
        loaderVersion = dependencies.value(QStringLiteral("quilt-loader")).toString();
    }
    if (mcVersion.isEmpty()) {
        if (error)
            *error = QStringLiteral("В модпаке не указана версия Minecraft");
        return {};
    }
    if (report)
        report(QStringLiteral("modpack"), QStringLiteral("Создание сборки…"), 0.02);
    QVariantMap data{{QStringLiteral("name"), index.value(QStringLiteral("name")).toString(QStringLiteral("Modpack")).left(48)},
                     {QStringLiteral("version"), mcVersion},
                     {QStringLiteral("loader"), loader},
                     {QStringLiteral("loaderVersion"), loaderVersion}};
    const QVariantMap created = invokeBlocking(instances, [instances, data] { return instances->create(data); });
    if (created.isEmpty()) {
        if (error)
            *error = QStringLiteral("Не удалось создать сборку для модпака");
        return {};
    }
    const QString instanceId = created.value(QStringLiteral("id")).toString();
    const QString gameDir = created.value(QStringLiteral("gameDir")).toString();

    QJsonArray files = index.value(QStringLiteral("files")).toArray();
    QVector<QJsonObject> downloads;
    for (const auto &value : files) {
        const QJsonObject file = value.toObject();
        if (file.value(QStringLiteral("env")).toObject().value(QStringLiteral("client")).toString() == QLatin1String("unsupported"))
            continue;
        downloads.append(file);
    }
    std::atomic_int done{0};
    std::atomic_int failed{0};
    std::atomic_int nextIndex{0};
    QMutex reportMutex;
    const int workers = std::min(6, std::max(1, static_cast<int>(downloads.size())));
    std::vector<std::thread> threads;
    for (int w = 0; w < workers; ++w) {
        threads.emplace_back([&] {
            for (;;) {
                const int current = nextIndex.fetch_add(1);
                if (current >= downloads.size() || (cancel && cancel->load()))
                    return;
                const QJsonObject file = downloads.at(current);
                const QString path = file.value(QStringLiteral("path")).toString();
                if (path.isEmpty() || path.contains(QLatin1String("..")))
                    continue;
                const QString target = QDir(gameDir).filePath(path);
                const QJsonObject hashes = file.value(QStringLiteral("hashes")).toObject();
                QString url;
                const QJsonArray urls = file.value(QStringLiteral("downloads")).toArray();
                if (!urls.isEmpty())
                    url = urls.first().toString();
                if (url.isEmpty())
                    continue;
                QString downloadError;
                if (!downloadFile(QUrl(url), target, hashes.value(QStringLiteral("sha1")).toString(),
                                  hashes.value(QStringLiteral("sha512")).toString(), {}, cancel, {}, &downloadError)) {
                    failed.fetch_add(1);
                    continue;
                }
                const int currentDone = done.fetch_add(1) + 1;
                if (report) {
                    QMutexLocker lock(&reportMutex);
                    report(QStringLiteral("modpack"),
                           QStringLiteral("Файлы модпака: %1/%2").arg(currentDone).arg(downloads.size()),
                           0.05 + 0.65 * static_cast<double>(currentDone) / std::max(1, static_cast<int>(downloads.size())));
                }
            }
        });
    }
    for (auto &thread : threads)
        thread.join();
    if (cancel && cancel->load()) {
        if (error)
            *error = QStringLiteral("Операция отменена");
        return {};
    }
    if (failed.load() > 0) {
        Logger::warn(QStringLiteral("Модпак: не скачано файлов — %1").arg(failed.load()));
    }

    QVector<ZipEntry> entries;
    zipListEntries(archivePath, entries);
    for (const auto &entry : entries) {
        if (entry.directory)
            continue;
        QString relative;
        if (entry.name.startsWith(QLatin1String("overrides/")))
            relative = entry.name.mid(10);
        else if (entry.name.startsWith(QLatin1String("client-overrides/")))
            relative = entry.name.mid(17);
        else
            continue;
        if (relative.contains(QLatin1String("..")))
            continue;
        QByteArray content;
        if (!zipReadEntry(archivePath, entry.name, content))
            continue;
        const QString target = QDir(gameDir).filePath(relative);
        QDir().mkpath(QFileInfo(target).absolutePath());
        QFile out(target);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(content);
            out.close();
        }
    }
    if (report)
        report(QStringLiteral("modpack"), QStringLiteral("Модпак установлен"), 1);

    for (const auto &file : downloads) {
        const QString path = file.value(QStringLiteral("path")).toString();
        if (!path.startsWith(QLatin1String("mods/")))
            continue;
        const QString fileName = QFileInfo(path).fileName();
        const QJsonObject hashes = file.value(QStringLiteral("hashes")).toObject();
        QJsonArray urls = file.value(QStringLiteral("downloads")).toArray();
        addRecord(instances, instanceId,
                  QJsonObject{{QStringLiteral("id"), hashes.value(QStringLiteral("sha1")).toString(fileName)},
                              {QStringLiteral("name"), fileName.chopped(4)},
                              {QStringLiteral("fileName"), fileName},
                              {QStringLiteral("source"), QStringLiteral("modrinth")},
                              {QStringLiteral("url"), urls.isEmpty() ? QString() : urls.first().toString()}});
    }
    Logger::info(QStringLiteral("mrpack установлен: %1").arg(index.value(QStringLiteral("name")).toString()));
    return invokeBlocking(instances, [instances, instanceId] { return instances->get(instanceId); });
}

} // namespace Noble
