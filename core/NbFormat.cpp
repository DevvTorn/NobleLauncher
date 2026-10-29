#include "NbFormat.h"

#include "Http.h"
#include "InstanceManager.h"
#include "Invoke.h"
#include "Json.h"
#include "Logger.h"
#include "ModsScanner.h"
#include "Paths.h"
#include "Pure.h"
#include "Zip.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

namespace Noble::NbFormat {

static QString folderForKind(const QString &kind)
{
    if (kind == QLatin1String("resourcepack"))
        return QStringLiteral("resourcepacks");
    if (kind == QLatin1String("shaderpack"))
        return QStringLiteral("shaderpacks");
    return QStringLiteral("mods");
}

static bool safeRelative(const QString &name)
{
    if (name.isEmpty() || name.contains(QLatin1String("..")))
        return false;
    return !name.contains(QLatin1Char('/')) && !name.contains(QLatin1Char('\\'));
}

static bool safeArchivePath(const QString &name)
{
    if (name.isEmpty() || name.contains(QStringLiteral("..")) || QDir::isAbsolutePath(name))
        return false;
    return !name.startsWith(QLatin1Char('/')) && !name.startsWith(QLatin1Char(92));
}

QJsonObject manifestFor(const QJsonObject &instance, bool embedFiles)
{
    const QVariantMap scan = ModsScanner::scanInstance(instance);
    QJsonArray content;
    const QJsonArray tracked = instance.value(QStringLiteral("mods")).toArray();
    for (const auto &value : scan.value(QStringLiteral("items")).toList()) {
        const QVariantMap item = value.toMap();
        const QString fileName = item.value(QStringLiteral("fileName")).toString();
        const QString base = fileName.endsWith(QLatin1String(".disabled"), Qt::CaseInsensitive)
            ? fileName.left(fileName.size() - 9) : fileName;
        QString url;
        for (const auto &trackedValue : tracked) {
            const QJsonObject record = trackedValue.toObject();
            QString trackedName = record.value(QStringLiteral("fileName")).toString();
            if (trackedName.endsWith(QLatin1String(".disabled"), Qt::CaseInsensitive))
                trackedName.chop(9);
            if (trackedName == base) {
                url = record.value(QStringLiteral("url")).toString();
                break;
            }
        }
        content.append(QJsonObject{{QStringLiteral("kind"), QJsonValue::fromVariant(item.value(QStringLiteral("kind")))},
                                   {QStringLiteral("fileName"), fileName},
                                   {QStringLiteral("name"), QJsonValue::fromVariant(item.value(QStringLiteral("name")))},
                                   {QStringLiteral("version"), QJsonValue::fromVariant(item.value(QStringLiteral("version")))},
                                   {QStringLiteral("author"), QJsonValue::fromVariant(item.value(QStringLiteral("author")))},
                                   {QStringLiteral("enabled"), QJsonValue::fromVariant(item.value(QStringLiteral("enabled")))},
                                   {QStringLiteral("url"), url}});
    }

    QJsonObject settings{
        {QStringLiteral("ram"), instance.value(QStringLiteral("ram")).toInt(4096)},
        {QStringLiteral("jvmArgs"), instance.value(QStringLiteral("jvmArgs")).toString()},
        {QStringLiteral("gameArgs"), instance.value(QStringLiteral("gameArgs")).toString()},
        {QStringLiteral("javaPath"), instance.value(QStringLiteral("javaPath")).toString()},
    };
    QJsonObject meta{
        {QStringLiteral("name"), instance.value(QStringLiteral("name")).toString()},
        {QStringLiteral("version"), instance.value(QStringLiteral("version")).toString()},
        {QStringLiteral("loader"), instance.value(QStringLiteral("loader")).toString()},
        {QStringLiteral("loaderVersion"), instance.value(QStringLiteral("loaderVersion")).toString()},
    };
    QJsonObject manifest{
        {QStringLiteral("format"), QStringLiteral("noble-nb")},
        {QStringLiteral("formatVersion"), kFormatVersion},
        {QStringLiteral("createdBy"), QStringLiteral("Noble Launcher 2.9.0")},
        {QStringLiteral("createdAt"), static_cast<double>(QDateTime::currentMSecsSinceEpoch())},
        {QStringLiteral("embedFiles"), embedFiles},
        {QStringLiteral("instance"), meta},
        {QStringLiteral("settings"), settings},
        {QStringLiteral("content"), content},
        {QStringLiteral("icon"), instance.value(QStringLiteral("icon")).toString()},
        {QStringLiteral("banner"), instance.value(QStringLiteral("banner")).toString()},
    };
    return manifest;
}

bool exportArchive(const QJsonObject &instance, const QString &targetPath, bool embedFiles, const Reporter &report,
                   QString *error)
{
    if (instance.isEmpty()) {
        if (error)
            *error = QStringLiteral("Сборка не найдена");
        return false;
    }
    if (report)
        report(QStringLiteral("export"), QStringLiteral("Подготовка .Nb…"), 0.1);
    QJsonObject manifest = manifestFor(instance, embedFiles);
    QVector<QPair<QString, QByteArray>> rootFiles;
    rootFiles.append({QStringLiteral("noble.json"), QJsonDocument(manifest).toJson(QJsonDocument::Indented)});

    const QString iconName = instance.value(QStringLiteral("icon")).toString();
    QByteArray iconData;
    QString iconEntry;
    if (!iconName.isEmpty()) {
        QFile icon(QDir(Paths::instanceIconsDir()).filePath(iconName));
        if (icon.open(QIODevice::ReadOnly)) {
            iconData = icon.readAll();
            iconEntry = iconName;
            rootFiles.append({iconEntry, iconData});
        }
    }

    if (report)
        report(QStringLiteral("export"), QStringLiteral("Упаковка сборки…"), 0.35);
    QString zipError;
    bool ok = false;
    if (embedFiles) {
        ok = zipWriteFilesAndDirectory(targetPath, rootFiles, instance.value(QStringLiteral("gameDir")).toString(),
                                       {QStringLiteral("logs"), QStringLiteral("crash-reports"),
                                        QStringLiteral(".cache"), QStringLiteral("instance.json")},
                                       &zipError);
    } else {
        ok = zipWriteFiles(targetPath, rootFiles, &zipError);
    }
    if (!ok) {
        if (error)
            *error = zipError;
        return false;
    }
    if (report)
        report(QStringLiteral("export"), QStringLiteral("Сборка сохранена в .Nb"), 1);
    Logger::info(QStringLiteral("Сборка экспортирована в .Nb: %1%2").arg(targetPath, embedFiles ? QStringLiteral(" (с файлами)") : QStringLiteral(" (со ссылками)")));
    return true;
}

QJsonObject importArchive(InstanceManager *instances, const QString &archivePath, const Reporter &report,
                          std::atomic_bool *cancel, QString *error, QStringList *warnings)
{
    auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return QJsonObject();
    };
    if (cancel && cancel->load())
        return fail(QStringLiteral("Операция отменена"));
    if (report)
        report(QStringLiteral("import"), QStringLiteral("Чтение .Nb…"), 0.03);

    QString zipError;
    QByteArray manifestData;
    QString manifestEntry;
    QVector<ZipEntry> entries;
    if (!zipListEntries(archivePath, entries, &zipError))
        return fail(QStringLiteral("Не удалось открыть .Nb: %1").arg(zipError));
    for (const auto &entry : entries) {
        if (entry.name.endsWith(QLatin1String("noble.json"))) {
            manifestEntry = entry.name;
            break;
        }
    }
    if (manifestEntry.isEmpty())
        return fail(QStringLiteral("Это не .Nb: отсутствует noble.json"));
    if (!zipReadEntry(archivePath, manifestEntry, manifestData, &zipError))
        return fail(QStringLiteral("Не удалось прочитать манифест: %1").arg(zipError));
    const QJsonObject manifest = QJsonDocument::fromJson(manifestData).object();
    if (manifest.value(QStringLiteral("format")).toString() != QLatin1String("noble-nb"))
        return fail(QStringLiteral("Неизвестный формат манифеста"));
    const int version = manifest.value(QStringLiteral("formatVersion")).toInt();
    if (version <= 0 || version > kFormatVersion)
        return fail(QStringLiteral("Версия формата .Nb (%1) не поддерживается").arg(version));

    const QJsonObject meta = manifest.value(QStringLiteral("instance")).toObject();
    const QString name = meta.value(QStringLiteral("name")).toString().trimmed().left(48);
    if (name.isEmpty())
        return fail(QStringLiteral("В .Nb не указано название сборки"));
    const QJsonObject settings = manifest.value(QStringLiteral("settings")).toObject();
    const QVariantMap createData{
        {QStringLiteral("name"), name},
        {QStringLiteral("version"), meta.value(QStringLiteral("version")).toString()},
        {QStringLiteral("loader"), meta.value(QStringLiteral("loader")).toString(QStringLiteral("Vanilla"))},
        {QStringLiteral("loaderVersion"), meta.value(QStringLiteral("loaderVersion")).toString()},
        {QStringLiteral("ram"), settings.value(QStringLiteral("ram")).toInt(4096)},
        {QStringLiteral("jvmArgs"), settings.value(QStringLiteral("jvmArgs")).toString()},
        {QStringLiteral("gameArgs"), settings.value(QStringLiteral("gameArgs")).toString()},
        {QStringLiteral("javaPath"), settings.value(QStringLiteral("javaPath")).toString()},
    };
    const QVariantMap created = invokeBlocking(instances, [instances, createData] { return instances->create(createData); });
    if (created.isEmpty())
        return fail(QStringLiteral("Не удалось создать сборку"));
    const QString instanceId = created.value(QStringLiteral("id")).toString();
    const QString gameDir = created.value(QStringLiteral("gameDir")).toString();
    QJsonObject instance = invokeBlocking(instances, [instances, instanceId] { return instances->get(instanceId); });

    const QString prefix = manifestEntry.left(manifestEntry.size() - QStringLiteral("noble.json").size());
    const bool embedded = manifest.value(QStringLiteral("embedFiles")).toBool(false);
    QString iconEntry;
    const QString manifestIcon = manifest.value(QStringLiteral("icon")).toString();
    // Значок хранится рядом с манифестом независимо от режима экспорта.
    for (const auto &entry : entries) {
        if (entry.directory)
            continue;
        const QString relative = prefix.isEmpty() ? entry.name : entry.name.mid(prefix.size());
        if (relative.contains(QLatin1Char('/')))
            continue;
        if (!relative.startsWith(QLatin1String("icon.")) && relative != manifestIcon)
            continue;
        QByteArray iconData;
        if (!zipReadEntry(archivePath, entry.name, iconData))
            continue;
        iconEntry = relative;
        const QString temp = QDir(Paths::tempDir()).filePath(QStringLiteral("nb-icon-%1-%2")
                                                                 .arg(QDateTime::currentMSecsSinceEpoch())
                                                                 .arg(relative));
        QDir().mkpath(Paths::tempDir());
        QFile iconFile(temp);
        if (iconFile.open(QIODevice::WriteOnly)) {
            iconFile.write(iconData);
            iconFile.close();
            invokeBlocking(instances, [instances, instanceId, temp] {
                instances->setIcon(instanceId, temp);
                return true;
            });
            QFile::remove(temp);
        }
        break;
    }
    if (report)
        report(QStringLiteral("import"), QStringLiteral("Распаковка файлов…"), 0.12);
    if (embedded) {
        for (const auto &entry : entries) {
            if (cancel && cancel->load())
                return fail(QStringLiteral("Операция отменена"));
            if (entry.directory)
                continue;
            if (!prefix.isEmpty() && !entry.name.startsWith(prefix))
                continue;
            const QString relative = prefix.isEmpty() ? entry.name : entry.name.mid(prefix.size());
            if (relative == QLatin1String("noble.json"))
                continue;
            if (relative == iconEntry)
                continue;
            QByteArray content;
            if (!zipReadEntry(archivePath, entry.name, content))
                continue;
            if (!safeArchivePath(relative)) {
                if (warnings)
                    *warnings << relative;
                continue;
            }
            const QString target = QDir(gameDir).filePath(relative);
            QDir().mkpath(QFileInfo(target).absolutePath());
            QFile file(target);
            if (!file.open(QIODevice::WriteOnly))
                return fail(QStringLiteral("Не удалось записать %1").arg(relative));
            file.write(content);
            file.close();
        }
    }

    const QJsonArray content = manifest.value(QStringLiteral("content")).toArray();
    const int total = static_cast<int>(content.size());
    int done = 0;
    for (const auto &value : content) {
        if (cancel && cancel->load())
            return fail(QStringLiteral("Операция отменена"));
        const QJsonObject item = value.toObject();
        const QString kind = item.value(QStringLiteral("kind")).toString();
        const QString fileName = item.value(QStringLiteral("fileName")).toString();
        const QString url = item.value(QStringLiteral("url")).toString();
        if (!safeRelative(fileName)) {
            if (warnings)
                *warnings << fileName;
            ++done;
            continue;
        }
        const QString folder = folderForKind(kind);
        const QString target = QDir(QDir(gameDir).filePath(folder)).filePath(fileName);
        if (!embedded && !url.isEmpty()) {
            QString downloadError;
            if (!downloadFile(QUrl(url), target, item.value(QStringLiteral("sha1")).toString(), {}, {}, cancel,
                              [&](qint64 received, qint64 totalBytes) {
                                  if (report)
                                      report(QStringLiteral("download"),
                                             QStringLiteral("%1: %2%").arg(item.value(QStringLiteral("name")).toString(fileName))
                                                 .arg(totalBytes > 0 ? static_cast<int>(received * 100 / totalBytes) : 0),
                                             0.15 + 0.75 * (total > 0
                                                                ? (done + (totalBytes > 0 ? static_cast<double>(received) / totalBytes : 0.5)) / total
                                                                : 0.5));
                              }, &downloadError)) {
                if (warnings)
                    *warnings << QStringLiteral("%1: %2").arg(fileName, downloadError);
            }
        } else if (!embedded && url.isEmpty()) {
            if (warnings)
                *warnings << QStringLiteral("%1 (нет ссылки для скачивания)").arg(fileName);
        }
        if (kind == QLatin1String("mod")) {
            invokeBlocking(instances, [instances, instanceId, item, fileName] {
                instances->addMod(instanceId,
                                  QVariantMap{{QStringLiteral("id"), item.value(QStringLiteral("name")).toString(fileName)},
                                              {QStringLiteral("name"), item.value(QStringLiteral("name")).toString(fileName.chopped(4))},
                                              {QStringLiteral("fileName"), fileName},
                                              {QStringLiteral("source"), QStringLiteral("manual")},
                                              {QStringLiteral("version"), item.value(QStringLiteral("version")).toString()},
                                              {QStringLiteral("url"), item.value(QStringLiteral("url")).toString()}});
                return true;
            });
        }
        ++done;
        if (report)
            report(QStringLiteral("import"), QStringLiteral("Файлы: %1/%2").arg(done).arg(total),
                   0.15 + 0.8 * (total > 0 ? static_cast<double>(done) / total : 1));
    }

    instance = invokeBlocking(instances, [instances, instanceId] { return instances->get(instanceId); });
    if (report)
        report(QStringLiteral("import"), QStringLiteral("Сборка импортирована"), 1);
    Logger::info(QStringLiteral("Сборка импортирована из .Nb: %1").arg(name));
    return instance;
}

} // namespace Noble::NbFormat
