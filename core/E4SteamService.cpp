#include "E4SteamService.h"

#include "Http.h"
#include "InstanceManager.h"
#include "Invoke.h"
#include "Logger.h"
#include "Paths.h"
#include "ProcessUtil.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

namespace Noble {

static const QString kReleasesApi = QStringLiteral("https://api.github.com/repos/Kamilhik/e4steam/releases?per_page=20");
static const QString kSteamDownload = QStringLiteral("https://store.steampowered.com/about/");

static QVariantMap readSteamStatus()
{
    const QString path = steamInstallPath();
    const bool running = steamRunning();
    const bool loggedIn = steamLoggedIn();
    return {{QStringLiteral("supported"), true},
            {QStringLiteral("installed"), !path.isEmpty() || running},
            {QStringLiteral("running"), running},
            {QStringLiteral("loggedIn"), loggedIn},
            {QStringLiteral("installPath"), path},
            {QStringLiteral("downloadUrl"), kSteamDownload}};
}

E4SteamService::E4SteamService(QObject *parent)
    : QObject(parent)
{
    m_status = readSteamStatus();
}

void E4SteamService::refreshStatus()
{
    QVariantMap next = readSteamStatus();
    if (next == m_status)
        return;
    m_status = next;
    emit statusChanged();
}

struct SteamAsset {
    QString name;
    QString url;
    qint64 size = 0;
};

struct SteamRelease {
    QString tag;
    QString name;
    QVector<SteamAsset> assets;
};

static QVector<SteamRelease> releases()
{
    QVector<SteamRelease> list;
    const HttpResult response = httpGet(QUrl(kReleasesApi), {{QStringLiteral("Accept"), QStringLiteral("application/vnd.github+json")}}, 20000);
    if (!response.ok)
        return list;
    for (const auto &value : response.json().array()) {
        const QJsonObject release = value.toObject();
        SteamRelease item;
        item.tag = release.value(QStringLiteral("tag_name")).toString();
        item.name = release.value(QStringLiteral("name")).toString(item.tag);
        for (const auto &assetValue : release.value(QStringLiteral("assets")).toArray()) {
            const QJsonObject asset = assetValue.toObject();
            item.assets.append({asset.value(QStringLiteral("name")).toString(),
                                asset.value(QStringLiteral("browser_download_url")).toString(),
                                asset.value(QStringLiteral("size")).toInteger()});
        }
        list.append(item);
    }
    return list;
}

static bool pickAsset(const QVector<SteamRelease> &list, const QString &loader, const QString &mcVersion, SteamRelease *releaseOut,
                      SteamAsset *assetOut)
{
    QStringList loaderTags;
    if (loader == QLatin1String("Fabric"))
        loaderTags << QStringLiteral("fabric");
    else if (loader == QLatin1String("Forge"))
        loaderTags << QStringLiteral("forge");
    else if (loader == QLatin1String("NeoForge"))
        loaderTags << QStringLiteral("neoforge") << QStringLiteral("neo-forge");
    else if (loader == QLatin1String("Quilt"))
        loaderTags << QStringLiteral("quilt");
    const QString shortVersion = mcVersion.section(QLatin1Char('.'), 0, 1);
    int bestScore = 0;
    for (const auto &release : list) {
        for (const auto &asset : release.assets) {
            const QString name = asset.name.toLower();
            if (!name.endsWith(QLatin1String(".jar")))
                continue;
            int score = 0;
            for (const auto &tag : loaderTags) {
                if (name.contains(tag)) {
                    score += 2;
                    break;
                }
            }
            if (name.contains(mcVersion))
                score += 4;
            else if (name.contains(shortVersion))
                score += 2;
            if (score > bestScore) {
                bestScore = score;
                *releaseOut = release;
                *assetOut = asset;
            }
        }
    }
    return bestScore > 0;
}

QVariantMap E4SteamService::installForInstance(InstanceManager *instances, const QString &instanceId,
                                               const std::function<void(const QString &, const QString &, double)> &report,
                                               std::atomic_bool *cancel, QString *error)
{
    const QJsonObject instance = invokeBlocking(instances, [instances, instanceId] { return instances->get(instanceId); });
    if (instance.isEmpty()) {
        if (error)
            *error = QStringLiteral("Сборка не найдена");
        return {};
    }
    const QString loader = instance.value(QStringLiteral("loader")).toString();
    if (loader == QLatin1String("Vanilla")) {
        if (error)
            *error = QStringLiteral("e4steam работает только на сборках с загрузчиком (Fabric, Forge, NeoForge или Quilt)");
        return {};
    }
    if (report)
        report(QStringLiteral("friends"), QStringLiteral("Поиск e4steam для %1 %2…").arg(loader, instance.value(QStringLiteral("version")).toString()), 0.1);
    const QVector<SteamRelease> list = releases();
    SteamRelease release;
    SteamAsset asset;
    if (!pickAsset(list, loader, instance.value(QStringLiteral("version")).toString(), &release, &asset)) {
        if (error)
            *error = QStringLiteral("Не найден JAR e4steam для %1 %2").arg(loader, instance.value(QStringLiteral("version")).toString());
        return {};
    }
    const QString modsDir = QDir(instance.value(QStringLiteral("gameDir")).toString()).filePath(QStringLiteral("mods"));
    QDir().mkpath(modsDir);
    const QFileInfoList existing = QDir(modsDir).entryInfoList(QDir::Files);
    for (const auto &file : existing) {
        if (file.fileName().contains(QLatin1String("e4steam"), Qt::CaseInsensitive) && file.fileName() != asset.name) {
            QFile::remove(file.absoluteFilePath());
            Logger::info(QStringLiteral("e4steam: удалён старый файл %1").arg(file.fileName()));
        }
    }
    const QString target = QDir(modsDir).filePath(asset.name);
    if (QFileInfo::exists(target) && (asset.size == 0 || QFileInfo(target).size() == asset.size)) {
        if (report)
            report(QStringLiteral("friends"), QStringLiteral("e4steam %1 уже установлен").arg(release.tag), 1);
    } else {
        QString downloadError;
        if (!downloadFile(QUrl(asset.url), target, {}, {}, {}, cancel, [&](qint64 received, qint64 total) {
                if (total > 0 && report)
                    report(QStringLiteral("friends"), QStringLiteral("e4steam: %1%").arg(static_cast<int>(received * 100 / total)),
                           0.3 + 0.6 * static_cast<double>(received) / total);
            }, &downloadError)) {
            if (error)
                *error = downloadError;
            return {};
        }
    }
    invokeBlocking(instances, [instances, instanceId, release, asset] {
        instances->addMod(instanceId,
                          QVariantMap{{QStringLiteral("id"), QStringLiteral("e4steam-") + release.tag},
                                      {QStringLiteral("name"), QStringLiteral("e4steam %1 (друзья через Steam)").arg(release.tag)},
                                      {QStringLiteral("fileName"), asset.name},
                                      {QStringLiteral("source"), QStringLiteral("manual")},
                                      {QStringLiteral("version"), release.tag},
                                      {QStringLiteral("url"), asset.url}});
        return true;
    });
    Logger::info(QStringLiteral("e4steam %1 установлен").arg(release.tag));
    if (report)
        report(QStringLiteral("friends"), QStringLiteral("e4steam %1 установлен").arg(release.tag), 1);
    return {{QStringLiteral("fileName"), asset.name},
            {QStringLiteral("version"), release.tag},
            {QStringLiteral("assetName"), asset.name}};
}

} // namespace Noble
