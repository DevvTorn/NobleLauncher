#include "SkinService.h"

#include "Logger.h"
#include "Paths.h"
#include "Pure.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace Noble {

static const QString kSkinSystem = QStringLiteral("https://skinsystem.ely.by");

SkinService::SkinService(QObject *parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this))
{
    useDefault();
}

QString SkinService::cacheKey(const QString &uuid) const
{
    if (!uuid.isEmpty())
        return uuid;
    return sha1Bytes(m_name.toUtf8());
}

void SkinService::useDefault()
{
    const bool sourceChanged = m_skinSource != QStringLiteral("qrc:/assets/steve.png");
    m_skinSource = QStringLiteral("qrc:/assets/steve.png");
    m_avatarSource = m_skinSource;
    m_skinName = QStringLiteral("Steve");
    m_cachedPath.clear();
    if (sourceChanged || !m_loading)
        emit skinChanged();
}

void SkinService::clearAccount()
{
    ++m_generation;
    if (m_reply)
        m_reply->abort();
    m_uuid.clear();
    m_name.clear();
    m_url.clear();
    m_error.clear();
    m_force = false;
    setLoading(false);
    useDefault();
}

void SkinService::setAccount(const QString &uuid, const QString &name, const QString &skinUrl)
{
    const QString cleanUuid = QString(uuid).remove(QLatin1Char('-')).toLower();
    const bool sameAccount = cleanUuid == m_uuid && name == m_name;
    m_uuid = cleanUuid;
    m_name = name;
    m_url = skinUrl;
    m_error.clear();
    if (m_name.isEmpty()) {
        ++m_generation;
        if (m_reply)
            m_reply->abort();
        useDefault();
        setLoading(false);
        return;
    }
    loadCached(cacheKey(m_uuid));
    if (sameAccount && !m_cachedPath.isEmpty() && !m_force) {
        setLoading(false);
        return;
    }
    resolveTexture(m_name, m_uuid, m_force);
}

void SkinService::loadCached(const QString &uuid)
{
    if (uuid.isEmpty())
        return;
    const QString path = QDir(Paths::skinsDir()).filePath(uuid + QStringLiteral(".png"));
    if (!QFileInfo::exists(path))
        return;
    m_cachedPath = path;
    prepareTextures(path, uuid);
}

void SkinService::refresh()
{
    if (m_name.isEmpty())
        return;
    m_force = true;
    resolveTexture(m_name, m_uuid, true);
}

void SkinService::resolveTexture(const QString &name, const QString &uuid, bool force)
{
    const int generation = ++m_generation;
    if (m_reply)
        m_reply->abort();
    setLoading(true);

    QUrl url(QStringLiteral("%1/textures/%2").arg(kSkinSystem, QString::fromUtf8(QUrl::toPercentEncoding(name))));
    if (force) {
        QUrlQuery query(url);
        query.addQueryItem(QStringLiteral("t"), QString::number(QDateTime::currentMSecsSinceEpoch()));
        url.setQuery(query);
    }
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, defaultUserAgent());
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferNetwork);

    QNetworkReply *reply = m_network->get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, name, uuid, generation] {
        reply->deleteLater();
        if (generation != m_generation)
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            Logger::warn(QStringLiteral("Скин Ely.by для %1: %2 (HTTP %3)")
                             .arg(name, reply->errorString())
                             .arg(status));
            fallbackDirect(name, uuid);
            return;
        }
        if (status == 204 || status == 404) {
            Logger::info(QStringLiteral("Скин Ely.by для %1 не найден — используется стандартный").arg(name));
            finishWithFallback(QStringLiteral("У этого аккаунта нет скина Ely.by — показан стандартный"));
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonObject skin = root.value(QStringLiteral("SKIN")).toObject();
        const QString textureUrl = skin.value(QStringLiteral("url")).toString();
        if (textureUrl.isEmpty()) {
            Logger::info(QStringLiteral("Скин Ely.by для %1 не задан — используется стандартный").arg(name));
            finishWithFallback(QStringLiteral("У этого аккаунта нет скина Ely.by — показан стандартный"));
            return;
        }
        m_force = false;
        follow(QUrl(textureUrl), uuid, 4);
    });
}

static QUrl upgradedUrl(const QUrl &source)
{
    QUrl url = source;
    if (url.scheme() == QLatin1String("http"))
        url.setScheme(QStringLiteral("https"));
    return url;
}

void SkinService::follow(const QUrl &source, const QString &uuid, int redirectsLeft)
{
    const int generation = m_generation;
    QUrl url = upgradedUrl(source);
    if (!url.isValid() || url.host().isEmpty()) {
        fallbackDirect(m_name, uuid);
        return;
    }
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, defaultUserAgent());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferNetwork);
    QNetworkReply *reply = m_network->get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, uuid, redirectsLeft, generation] {
        reply->deleteLater();
        if (generation != m_generation)
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            const QByteArray location = reply->rawHeader("Location");
            if (redirectsLeft > 0 && !location.isEmpty()) {
                const QUrl next = reply->url().resolved(QUrl(QString::fromUtf8(location)));
                follow(next, uuid, redirectsLeft - 1);
                return;
            }
        }
        if (reply->error() != QNetworkReply::NoError || status >= 400 || status == 0) {
            const QString reason = reply->error() != QNetworkReply::NoError
                ? reply->errorString()
                : QStringLiteral("HTTP %1").arg(status);
            Logger::warn(QStringLiteral("Скин Ely.by: загрузка текстуры не удалась (%1)").arg(reason));
            if (status == 404 || status == 403) {
                finishWithFallback(QStringLiteral("Ely.by не отдал текстуру скина — показан стандартный"));
            } else {
                finishWithFallback(QStringLiteral("Не удалось загрузить скин Ely.by — показан сохранённый"));
            }
            return;
        }
        const QByteArray data = reply->readAll();
        if (!storeTexture(data, uuid)) {
            finishWithFallback(QStringLiteral("Ely.by вернул файл, который не является скином"));
            return;
        }
        const QString path = QDir(Paths::skinsDir()).filePath(cacheKey(uuid) + QStringLiteral(".png"));
        m_cachedPath = path;
        m_force = false;
        prepareTextures(path, uuid);
        setLoading(false);
        Logger::info(QStringLiteral("Скин Ely.by загружен для %1").arg(m_name));
    });
}

void SkinService::fallbackDirect(const QString &name, const QString &uuid)
{
    if (name.isEmpty()) {
        finishWithFallback(QStringLiteral("Скин Ely.by недоступен — показан стандартный"));
        return;
    }
    QUrl url(QStringLiteral("%1/skins/%2.png").arg(kSkinSystem, QString::fromUtf8(QUrl::toPercentEncoding(name))));
    follow(url, uuid, 4);
}

bool SkinService::storeTexture(const QByteArray &data, const QString &uuid)
{
    QImage image;
    if (!image.loadFromData(data) || image.width() < 64 || image.height() < 32) {
        Logger::warn(QStringLiteral("Скин Ely.by: сервер вернул не PNG-текстуру"));
        return false;
    }
    QDir().mkpath(Paths::skinsDir());
    const QString path = QDir(Paths::skinsDir()).filePath(cacheKey(uuid) + QStringLiteral(".png"));
    const QString temp = path + QStringLiteral(".tmp");
    QFile file(temp);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(data);
    file.close();
    QFile::remove(path);
    return QFile::rename(temp, path);
}

void SkinService::prepareTextures(const QString &pngPath, const QString &uuid)
{
    QImage skin(pngPath);
    if (skin.isNull()) {
        useDefault();
        return;
    }
    const QString key = cacheKey(uuid);
    const QString stamp = QString::number(QFileInfo(pngPath).lastModified().toMSecsSinceEpoch());
    QDir().mkpath(Paths::skinsDir());
    const QString skinCopy = QDir(Paths::skinsDir()).filePath(QStringLiteral("%1-%2.png").arg(key, stamp));
    const QString avatar = QDir(Paths::skinsDir()).filePath(QStringLiteral("%1-%2-head.png").arg(key, stamp));
    if (!QFileInfo::exists(skinCopy))
        QFile::copy(pngPath, skinCopy);
    if (skin.width() >= 16 && skin.height() >= 16) {
        const QImage head = skin.copy(8, 8, 8, 8).scaled(64, 64, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        head.save(avatar, "PNG");
    }
    const QString oldSkin = m_skinSource;
    const QString oldAvatar = m_avatarSource;
    m_skinSource = QUrl::fromLocalFile(skinCopy).toString();
    m_avatarSource = QFileInfo::exists(avatar) ? QUrl::fromLocalFile(avatar).toString() : m_skinSource;
    m_skinName = m_name.isEmpty() ? QStringLiteral("Steve") : m_name;
    cleanupOld(key, skinCopy, avatar);
    if (oldSkin != m_skinSource || oldAvatar != m_avatarSource)
        emit skinChanged();
}

void SkinService::cleanupOld(const QString &uuid, const QString &keepSkin, const QString &keepAvatar)
{
    QDir dir(Paths::skinsDir());
    const QStringList files = dir.entryList({uuid + QStringLiteral("-*.png")}, QDir::Files);
    for (const auto &name : files) {
        const QString path = dir.filePath(name);
        if (path != keepSkin && path != keepAvatar)
            QFile::remove(path);
    }
}

void SkinService::finishWithFallback(const QString &message)
{
    if (m_cachedPath.isEmpty())
        useDefault();
    m_error = message;
    setLoading(false, message);
}

void SkinService::setLoading(bool loading, const QString &error)
{
    const bool changed = m_loading != loading || m_error != error;
    m_loading = loading;
    m_error = error;
    if (changed)
        emit loadingChanged();
}

} // namespace Noble
