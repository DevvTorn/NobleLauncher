#include "BackgroundsService.h"

#include "Logger.h"
#include "Paths.h"
#include "Pure.h"
#include "Settings.h"
#include "ProcessUtil.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonObject>
#include <QUrl>

namespace Noble {

BackgroundsService::BackgroundsService(QObject *parent)
    : QObject(parent)
{
}

void BackgroundsService::init(Settings *settings)
{
    m_settings = settings;
    m_items = settings->backgrounds();
    emit changed();
}

QVariantList BackgroundsService::items() const
{
    QVariantList list;
    for (const auto &value : m_items) {
        const QJsonObject item = value.toObject();
        QVariantMap map = item.toVariantMap();
        map.insert(QStringLiteral("source"), sourceFor(item.value(QStringLiteral("id")).toString()));
        map.insert(QStringLiteral("thumbSource"), thumbFor(item.value(QStringLiteral("id")).toString()));
        list.append(map);
    }
    return list;
}

QString BackgroundsService::sourceFor(const QString &id) const
{
    for (const auto &value : m_items) {
        const QJsonObject item = value.toObject();
        if (item.value(QStringLiteral("id")).toString() == id) {
            const QString file = QDir(Paths::backgroundsDir()).filePath(item.value(QStringLiteral("fileName")).toString());
            return QUrl::fromLocalFile(file).toString();
        }
    }
    return QString();
}

QString BackgroundsService::thumbFor(const QString &id) const
{
    for (const auto &value : m_items) {
        const QJsonObject item = value.toObject();
        if (item.value(QStringLiteral("id")).toString() == id) {
            const QString thumb = item.value(QStringLiteral("thumb")).toString();
            if (!thumb.isEmpty())
                return QUrl::fromLocalFile(QDir(Paths::backgroundsThumbsDir()).filePath(thumb)).toString();
            return QString();
        }
    }
    return QString();
}

void BackgroundsService::importFiles(const QStringList &paths)
{
    if (!m_settings)
        return;
    QDir().mkpath(Paths::backgroundsDir());
    QJsonArray list = m_items;
    QStringList skipped;
    int added = 0;
    int processed = 0;
    for (const auto &path : paths) {
        if (++processed > 20)
            break;
        QFile file(path);
        const QFileInfo info(path);
        static const QStringList allowed = {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("webp")};
        if (!info.isFile() || !allowed.contains(info.suffix().toLower()) || info.size() > 40 * 1024 * 1024) {
            skipped << info.fileName();
            continue;
        }
        if (!file.open(QIODevice::ReadOnly)) {
            skipped << info.fileName();
            continue;
        }
        const QByteArray content = file.readAll();
        const QString ext = detectImageExt(content.left(16));
        if (ext.isEmpty()) {
            skipped << info.fileName();
            continue;
        }
        const QString id = sha1Bytes(content).left(16);
        bool exists = false;
        for (const auto &value : list) {
            if (value.toObject().value(QStringLiteral("id")).toString() == id) {
                exists = true;
                break;
            }
        }
        if (exists)
            continue;
        const QString fileName = id + ext;
        QFile out(QDir(Paths::backgroundsDir()).filePath(fileName));
        if (!out.open(QIODevice::WriteOnly)) {
            skipped << info.fileName();
            continue;
        }
        out.write(content);
        out.close();

        QImage image;
        image.loadFromData(content);
        if (!image.isNull()) {
            const QImage thumb = image.scaledToWidth(std::min(480, image.width()), Qt::SmoothTransformation);
            QDir().mkpath(Paths::backgroundsThumbsDir());
            thumb.save(QDir(Paths::backgroundsThumbsDir()).filePath(id + QStringLiteral(".jpg")), "JPG", 82);
        }
        list.append(QJsonObject{{QStringLiteral("id"), id},
                                {QStringLiteral("name"), info.completeBaseName().left(60)},
                                {QStringLiteral("fileName"), fileName},
                                {QStringLiteral("thumb"), id + QStringLiteral(".jpg")},
                                {QStringLiteral("size"), static_cast<double>(content.size())},
                                {QStringLiteral("addedAt"), static_cast<double>(QDateTime::currentMSecsSinceEpoch())}});
        ++added;
        Logger::info(QStringLiteral("Фон добавлен: %1").arg(fileName));
    }
    m_items = list;
    m_settings->setBackgrounds(m_items);
    if (added > 0)
        emit toastRequested(QStringLiteral("Фон добавлен"), QStringLiteral("success"));
    if (!skipped.isEmpty())
        emit toastRequested(QStringLiteral("Пропущены файлы: %1").arg(skipped.join(QStringLiteral(", "))), QStringLiteral("warn"));
    emit changed();
}

void BackgroundsService::remove(const QString &id)
{
    if (!m_settings)
        return;
    QJsonArray list;
    for (const auto &value : m_items) {
        const QJsonObject item = value.toObject();
        if (item.value(QStringLiteral("id")).toString() == id) {
            QFile::remove(QDir(Paths::backgroundsDir()).filePath(item.value(QStringLiteral("fileName")).toString()));
            const QString thumb = item.value(QStringLiteral("thumb")).toString();
            if (!thumb.isEmpty())
                QFile::remove(QDir(Paths::backgroundsThumbsDir()).filePath(thumb));
            continue;
        }
        list.append(item);
    }
    m_items = list;
    m_settings->setBackgrounds(m_items);
    m_settings->clearBackgroundFromProfiles(id);
    emit changed();
    emit toastRequested(QStringLiteral("Фон удалён"), QStringLiteral("info"));
}

void BackgroundsService::saveThumb(const QString &id, const QImage &image)
{
    if (image.isNull())
        return;
    QDir().mkpath(Paths::backgroundsThumbsDir());
    const QString file = QDir(Paths::backgroundsThumbsDir()).filePath(id + QStringLiteral(".jpg"));
    image.save(file, "JPG", 82);
    QJsonArray list = m_items;
    for (int i = 0; i < list.size(); ++i) {
        QJsonObject item = list.at(i).toObject();
        if (item.value(QStringLiteral("id")).toString() == id) {
            item.insert(QStringLiteral("thumb"), id + QStringLiteral(".jpg"));
            list.replace(i, item);
            break;
        }
    }
    m_items = list;
    if (m_settings)
        m_settings->setBackgrounds(m_items);
    emit changed();
}

void BackgroundsService::openFolder()
{
    QDir().mkpath(Paths::backgroundsDir());
    openWithDefaultApp(Paths::backgroundsDir());
}

} // namespace Noble
