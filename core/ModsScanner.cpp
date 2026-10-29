#include "ModsScanner.h"

#include "Logger.h"
#include "Paths.h"
#include "Pure.h"
#include "Zip.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>

namespace Noble {

namespace {

struct ZipMeta {
    QString name;
    QString version;
    QString author;
    QString description;
    QString iconPath;
    QStringList mcRange;
    QString loader;
};

QString listOf(const QJsonValue &value)
{
    if (value.isString())
        return value.toString();
    if (value.isArray()) {
        QStringList items;
        for (const auto &entry : value.toArray()) {
            if (entry.isString())
                items << entry.toString();
        }
        return items.join(QStringLiteral(", "));
    }
    return QString();
}

QString readText(const QString &archive, const QString &entry)
{
    QByteArray data;
    if (!zipReadEntry(archive, entry, data))
        return QString();
    return QString::fromUtf8(data);
}

QJsonObject readJson(const QString &archive, const QString &entry)
{
    const QString text = readText(archive, entry);
    if (text.isEmpty())
        return {};
    return QJsonDocument::fromJson(text.toUtf8()).object();
}

ZipMeta parseFabric(const QJsonObject &json)
{
    ZipMeta meta;
    meta.name = json.value(QStringLiteral("name")).toString(json.value(QStringLiteral("id")).toString());
    meta.version = json.value(QStringLiteral("version")).toString();
    meta.author = listOf(json.value(QStringLiteral("authors")));
    const QJsonValue description = json.value(QStringLiteral("description"));
    meta.description = description.isString() ? description.toString()
                                              : description.toObject().value(QStringLiteral("text")).toString();
    const QJsonValue icon = json.value(QStringLiteral("icon"));
    if (icon.isString()) {
        meta.iconPath = icon.toString();
    } else if (icon.isObject()) {
        const QJsonObject iconObject = icon.toObject();
        if (!iconObject.isEmpty())
            meta.iconPath = iconObject.constBegin().value().toString();
    }
    const QJsonValue minecraft = json.value(QStringLiteral("depends")).toObject().value(QStringLiteral("minecraft"));
    if (minecraft.isString())
        meta.mcRange << minecraft.toString();
    else if (minecraft.isArray()) {
        for (const auto &value : minecraft.toArray()) {
            if (value.isString())
                meta.mcRange << value.toString();
        }
    }
    return meta;
}

ZipMeta parseForgeToml(const QString &text)
{
    ZipMeta meta;
    auto pick = [&](const QString &key) {
        const QRegularExpression re(QStringLiteral("^\\s*%1\\s*=\\s*[\"']([^\"']*)[\"']").arg(key),
                                    QRegularExpression::MultilineOption);
        const auto match = re.match(text);
        return match.hasMatch() ? match.captured(1) : QString();
    };
    meta.name = pick(QStringLiteral("displayName"));
    if (meta.name.isEmpty())
        meta.name = pick(QStringLiteral("modId"));
    meta.version = pick(QStringLiteral("version"));
    meta.description = pick(QStringLiteral("description"));
    meta.iconPath = pick(QStringLiteral("logoFile"));
    const QRegularExpression authors(QStringLiteral("authors\\s*=\\s*\\[([^\\]]*)\\]"), QRegularExpression::MultilineOption);
    const auto authorsMatch = authors.match(text);
    if (authorsMatch.hasMatch()) {
        meta.author = authorsMatch.captured(1);
        meta.author.remove(QLatin1Char('"'));
        meta.author.remove(QLatin1Char('\''));
        meta.author = meta.author.trimmed();
    }
    const QRegularExpression dep(QStringLiteral("\\[\\[dependencies[^\\]]*\\]\\][\\s\\S]*?modId\\s*=\\s*[\"']minecraft[\"'][\\s\\S]*?versionRange\\s*=\\s*[\"']([^\"']+)[\"']"));
    const auto depMatch = dep.match(text);
    if (depMatch.hasMatch())
        meta.mcRange << depMatch.captured(1);
    meta.loader = QStringLiteral("forge");
    return meta;
}

ZipMeta parseMcmodInfo(const QJsonDocument &doc)
{
    ZipMeta meta;
    QJsonObject first;
    if (doc.isArray() && !doc.array().isEmpty())
        first = doc.array().first().toObject();
    else if (doc.isObject()) {
        const QJsonArray modList = doc.object().value(QStringLiteral("modList")).toArray();
        first = modList.isEmpty() ? doc.object() : modList.first().toObject();
    }
    if (first.isEmpty())
        return meta;
    meta.name = first.value(QStringLiteral("name")).toString(first.value(QStringLiteral("modid")).toString());
    meta.version = first.value(QStringLiteral("version")).toString();
    meta.author = listOf(first.value(QStringLiteral("authorList"))).isEmpty()
        ? first.value(QStringLiteral("author")).toString()
        : listOf(first.value(QStringLiteral("authorList")));
    meta.description = first.value(QStringLiteral("description")).toString();
    meta.iconPath = first.value(QStringLiteral("logoFile")).toString();
    meta.mcRange << first.value(QStringLiteral("mcversion")).toString();
    meta.loader = QStringLiteral("forge");
    return meta;
}

ZipMeta parseMeta(const QString &archive, const QString &kind)
{
    if (kind == QLatin1String("mod")) {
        QJsonObject fabric = readJson(archive, QStringLiteral("fabric.mod.json"));
        if (!fabric.isEmpty()) {
            ZipMeta meta = parseFabric(fabric);
            meta.loader = QStringLiteral("fabric");
            return meta;
        }
        QJsonObject quilt = readJson(archive, QStringLiteral("quilt.mod.json"));
        if (!quilt.isEmpty()) {
            const QJsonObject quiltLoader = quilt.value(QStringLiteral("quilt_loader")).toObject();
            ZipMeta meta;
            meta.name = quiltLoader.value(QStringLiteral("metadata")).toObject().value(QStringLiteral("name")).toString(
                quiltLoader.value(QStringLiteral("id")).toString());
            meta.version = quiltLoader.value(QStringLiteral("version")).toString();
            meta.description = quiltLoader.value(QStringLiteral("metadata")).toObject().value(QStringLiteral("description")).toString();
            const QJsonValue icon = quiltLoader.value(QStringLiteral("metadata")).toObject().value(QStringLiteral("icon"));
            meta.iconPath = icon.isString() ? icon.toString() : QString();
            meta.author = listOf(quiltLoader.value(QStringLiteral("metadata")).toObject().value(QStringLiteral("contributors")));
            const QJsonObject depends = quiltLoader.value(QStringLiteral("depends")).toObject();
            const QJsonValue minecraft = depends.value(QStringLiteral("minecraft"));
            if (minecraft.isString())
                meta.mcRange << minecraft.toString();
            else if (minecraft.isArray()) {
                for (const auto &value : minecraft.toArray()) {
                    if (value.isString())
                        meta.mcRange << value.toString();
                }
            }
            meta.loader = QStringLiteral("quilt");
            return meta;
        }
        QString toml = readText(archive, QStringLiteral("META-INF/mods.toml"));
        if (toml.isEmpty())
            toml = readText(archive, QStringLiteral("META-INF/neoforge.mods.toml"));
        if (!toml.isEmpty())
            return parseForgeToml(toml);
        const QString mcmod = readText(archive, QStringLiteral("mcmod.info"));
        if (!mcmod.isEmpty())
            return parseMcmodInfo(QJsonDocument::fromJson(mcmod.toUtf8()));
        return {};
    }
    if (kind == QLatin1String("resourcepack")) {
        const QJsonObject pack = readJson(archive, QStringLiteral("pack.mcmeta"));
        if (pack.isEmpty())
            return {};
        const QJsonObject info = pack.value(QStringLiteral("pack")).toObject();
        ZipMeta meta;
        const QJsonValue description = info.value(QStringLiteral("description"));
        meta.description = description.isString() ? description.toString()
                                                  : description.toObject().value(QStringLiteral("text")).toString();
        if (info.contains(QStringLiteral("pack_format")))
            meta.version = QStringLiteral("pack_format %1").arg(info.value(QStringLiteral("pack_format")).toInt());
        return meta;
    }
    const QString iris = readText(archive, QStringLiteral("iris.properties"));
    if (!iris.isEmpty()) {
        const auto match = QRegularExpression(QStringLiteral("name\\s*=\\s*(.+)")).match(iris);
        if (match.hasMatch()) {
            ZipMeta meta;
            meta.name = match.captured(1).trimmed();
            return meta;
        }
    }
    return {};
}

QString warningFor(const ZipMeta &meta, const QJsonObject &instance, const QString &kind)
{
    if (kind != QLatin1String("mod"))
        return QString();
    if (!meta.mcRange.isEmpty() && !mcVersionMatches(meta.mcRange, instance.value(QStringLiteral("version")).toString()))
        return QStringLiteral("Для Minecraft %1, а сборка на %2")
            .arg(meta.mcRange.join(QStringLiteral(" / ")), instance.value(QStringLiteral("version")).toString());
    const QString expected = instance.value(QStringLiteral("loader")).toString().toLower();
    if (!meta.loader.isEmpty() && expected != QLatin1String("vanilla")) {
        const bool ok = meta.loader == expected
            || (meta.loader == QLatin1String("fabric") && (expected == QLatin1String("quilt") || expected == QLatin1String("fabric")))
            || (meta.loader == QLatin1String("quilt") && (expected == QLatin1String("quilt") || expected == QLatin1String("fabric")));
        if (!ok)
            return QStringLiteral("Мод для %1, а сборка на %2").arg(meta.loader, instance.value(QStringLiteral("loader")).toString());
    }
    return QString();
}

QString cacheIcon(const QString &archive, const QString &entryPath)
{
    if (entryPath.isEmpty() || entryPath.contains(QLatin1String("..")))
        return QString();
    QByteArray data;
    if (!zipReadEntry(archive, entryPath, data) || data.isEmpty() || data.size() > 256 * 1024)
        return QString();
    const QString ext = detectImageExt(data.left(12));
    if (ext.isEmpty())
        return QString();
    QDir().mkpath(QDir(Paths::cacheDir()).filePath(QStringLiteral("mod-icons")));
    const QString name = sha1Bytes(data) + ext;
    const QString target = QDir(Paths::cacheDir()).filePath(QStringLiteral("mod-icons/") + name);
    if (!QFileInfo::exists(target)) {
        QFile file(target);
        if (!file.open(QIODevice::WriteOnly))
            return QString();
        file.write(data);
        file.close();
    }
    return QUrl::fromLocalFile(target).toString();
}

} // namespace

QString ModsScanner::kindDir(const QString &kind)
{
    if (kind == QLatin1String("resourcepack"))
        return QStringLiteral("resourcepacks");
    if (kind == QLatin1String("shaderpack"))
        return QStringLiteral("shaderpacks");
    return QStringLiteral("mods");
}

QVariantMap ModsScanner::scanInstance(const QJsonObject &instance)
{
    const QJsonArray trackedMods = instance.value(QStringLiteral("mods")).toArray();
    auto trackedName = [&](const QString &fileName) {
        QString base = fileName;
        if (base.endsWith(QLatin1String(".disabled"), Qt::CaseInsensitive))
            base.chop(9);
        for (const auto &value : trackedMods) {
            QString tracked = value.toObject().value(QStringLiteral("fileName")).toString();
            if (tracked.endsWith(QLatin1String(".disabled"), Qt::CaseInsensitive))
                tracked.chop(9);
            if (tracked == base)
                return true;
        }
        return false;
    };

    QVariantList items;
    QVariantList dirs;
    const QString gameDir = instance.value(QStringLiteral("gameDir")).toString();
    for (const QString &kind : {QStringLiteral("mod"), QStringLiteral("resourcepack"), QStringLiteral("shaderpack")}) {
        const QString dir = QDir(gameDir).filePath(kindDir(kind));
        QDir().mkpath(dir);
        int count = 0;
        const QFileInfoList entries = QDir(dir).entryInfoList(QDir::Files, QDir::Name);
        for (const auto &info : entries) {
            const QString fileName = info.fileName();
            const QString lower = fileName.toLower();
            if (!lower.endsWith(QLatin1String(".jar")) && !lower.endsWith(QLatin1String(".zip"))
                && !lower.endsWith(QLatin1String(".jar.disabled")) && !lower.endsWith(QLatin1String(".zip.disabled")))
                continue;
            const bool enabled = !lower.endsWith(QLatin1String(".disabled"));
            const QString archive = enabled ? info.absoluteFilePath()
                                            : QString(info.absoluteFilePath()).chopped(9);
            ZipMeta meta;
            QString icon;
            if (QFileInfo::exists(archive)) {
                meta = parseMeta(archive, kind);
                icon = cacheIcon(archive, meta.iconPath);
            }
            QString base = fileName;
            if (!enabled)
                base.chop(9);
            base.remove(QRegularExpression(QStringLiteral("\\.(jar|zip)$"), QRegularExpression::CaseInsensitiveOption));
            QVariantMap item;
            item.insert(QStringLiteral("fileName"), fileName);
            item.insert(QStringLiteral("kind"), kind);
            item.insert(QStringLiteral("name"), (meta.name.isEmpty() ? base : meta.name).left(120));
            item.insert(QStringLiteral("version"), meta.version);
            item.insert(QStringLiteral("author"), meta.author);
            item.insert(QStringLiteral("description"), meta.description.left(300));
            item.insert(QStringLiteral("size"), static_cast<double>(info.size()));
            item.insert(QStringLiteral("modifiedAt"), static_cast<double>(info.lastModified().toMSecsSinceEpoch()));
            item.insert(QStringLiteral("enabled"), enabled);
            item.insert(QStringLiteral("source"), trackedName(fileName) ? QStringLiteral("launcher") : QStringLiteral("manual"));
            item.insert(QStringLiteral("iconSource"), icon);
            item.insert(QStringLiteral("warning"), warningFor(meta, instance, kind));
            items.append(item);
            ++count;
        }
        dirs.append(QVariantMap{{QStringLiteral("kind"), kind}, {QStringLiteral("path"), dir}, {QStringLiteral("count"), count}});
    }
    return {{QStringLiteral("instanceId"), instance.value(QStringLiteral("id"))},
            {QStringLiteral("scannedAt"), static_cast<double>(QDateTime::currentMSecsSinceEpoch())},
            {QStringLiteral("items"), items},
            {QStringLiteral("dirs"), dirs}};
}

} // namespace Noble
