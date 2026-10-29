#include "InstanceManager.h"

#include "Json.h"
#include "Logger.h"
#include "Paths.h"
#include "Pure.h"
#include "Settings.h"
#include "Zip.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUrl>
#include <QUuid>

#include <algorithm>

namespace Noble {

InstanceManager::InstanceManager(QObject *parent)
    : QObject(parent)
{
}

void InstanceManager::init(Settings *settings)
{
    m_settings = settings;
    reload();
    migrateLatinDirs();
    reload();
    if (!m_instances.isEmpty() && m_selectedId.isEmpty())
        setSelectedInstanceId(m_instances.first().value(QStringLiteral("id")).toString());
}

QString InstanceManager::root() const
{
    const QString dir = m_settings ? m_settings->instancesDir() : Paths::instancesDefaultDir();
    QDir().mkpath(dir);
    return dir;
}

QString InstanceManager::runtimeRoot() const
{
    const QString dir = m_settings ? m_settings->runtimeDir() : Paths::runtimeDefaultDir();
    QDir().mkpath(dir);
    return dir;
}

void InstanceManager::reload()
{
    m_root = root();
    m_runtime = runtimeRoot();
    const QString previous = m_selectedId;
    m_instances.clear();
    const QFileInfoList dirs = QDir(m_root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const auto &info : dirs) {
        QJsonObject instance = readMeta(info.absoluteFilePath());
        if (!instance.isEmpty())
            m_instances.append(instance);
    }
    std::sort(m_instances.begin(), m_instances.end(), [](const QJsonObject &a, const QJsonObject &b) {
        const qint64 la = static_cast<qint64>(a.value(QStringLiteral("lastLaunched")).toDouble());
        const qint64 lb = static_cast<qint64>(b.value(QStringLiteral("lastLaunched")).toDouble());
        const qint64 ca = static_cast<qint64>(a.value(QStringLiteral("created")).toDouble());
        const qint64 cb = static_cast<qint64>(b.value(QStringLiteral("created")).toDouble());
        return (lb > 0 ? lb : cb) > (la > 0 ? la : ca);
    });
    if (m_selectedId.isEmpty() && !m_instances.isEmpty())
        m_selectedId = m_instances.first().value(QStringLiteral("id")).toString();
    if (!previous.isEmpty() && previous != m_selectedId)
        m_selectedId = previous;
    emit instancesChanged();
    emit selectionChanged();
}

QVariantList InstanceManager::instances() const
{
    QVariantList list;
    for (const auto &instance : m_instances) {
        QVariantMap map = instance.toVariantMap();
        map.insert(QStringLiteral("iconSource"), instance.value(QStringLiteral("icon")).toString().isEmpty()
                                                       ? QString()
                                                       : QUrl::fromLocalFile(QDir(Paths::instanceIconsDir())
                                                                                 .filePath(instance.value(QStringLiteral("icon")).toString()))
                                                             .toString());
        list.append(map);
    }
    return list;
}

QVariantMap InstanceManager::selectedInstance() const
{
    const QJsonObject instance = get(m_selectedId);
    return instance.toVariantMap();
}

void InstanceManager::setSelectedInstanceId(const QString &id)
{
    if (m_selectedId == id)
        return;
    if (!id.isEmpty() && get(id).isEmpty())
        return;
    m_selectedId = id;
    emit selectionChanged();
}

QJsonObject InstanceManager::get(const QString &id) const
{
    for (const auto &instance : m_instances) {
        if (instance.value(QStringLiteral("id")).toString() == id)
            return instance;
    }
    return {};
}

QString InstanceManager::modsDir(const QJsonObject &instance) const
{
    return QDir(instance.value(QStringLiteral("gameDir")).toString()).filePath(QStringLiteral("mods"));
}

QJsonObject InstanceManager::readMeta(const QString &dir) const
{
    const QString file = QDir(dir).filePath(QStringLiteral("instance.json"));
    QJsonObject obj = readJsonObject(file);
    if (obj.isEmpty())
        return {};
    obj.insert(QStringLiteral("gameDir"), dir);
    if (!obj.value(QStringLiteral("mods")).isArray())
        obj.insert(QStringLiteral("mods"), QJsonArray());
    if (!obj.contains(QStringLiteral("loaderVersion")))
        obj.insert(QStringLiteral("loaderVersion"), QString());
    if (!obj.contains(QStringLiteral("jvmArgs")))
        obj.insert(QStringLiteral("jvmArgs"), QString());
    if (!obj.contains(QStringLiteral("gameArgs")))
        obj.insert(QStringLiteral("gameArgs"), QString());
    if (!obj.contains(QStringLiteral("javaPath")))
        obj.insert(QStringLiteral("javaPath"), QString());
    if (!obj.contains(QStringLiteral("ram")))
        obj.insert(QStringLiteral("ram"), 4096);
    if (!obj.contains(QStringLiteral("installedVersionId")))
        obj.insert(QStringLiteral("installedVersionId"), QJsonValue::Null);
    if (!obj.contains(QStringLiteral("icon")) || obj.value(QStringLiteral("icon")).isNull())
        obj.insert(QStringLiteral("icon"), QJsonValue::Null);
    return obj;
}

void InstanceManager::writeMeta(QJsonObject &instance) const
{
    const QString dir = instance.value(QStringLiteral("gameDir")).toString();
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);
    QDir().mkpath(QDir(dir).filePath(QStringLiteral("mods")));
    writeJsonObject(QDir(dir).filePath(QStringLiteral("instance.json")), instance);
}

QString InstanceManager::uniqueDir(const QString &rootDir, const QString &base) const
{
    QString candidate = base;
    int index = 2;
    while (QFileInfo::exists(QDir(rootDir).filePath(candidate)))
        candidate = base + QStringLiteral("-") + QString::number(index++);
    return QDir(rootDir).filePath(candidate);
}

QVariantMap InstanceManager::create(const QVariantMap &data)
{
    const QString name = data.value(QStringLiteral("name")).toString().trimmed().left(48);
    if (name.isEmpty()) {
        emit toastRequested(QStringLiteral("Введите название сборки"), QStringLiteral("error"));
        return {};
    }
    const QString mc = data.value(QStringLiteral("version")).toString();
    if (mc.isEmpty()) {
        emit toastRequested(QStringLiteral("Выберите версию Minecraft"), QStringLiteral("error"));
        return {};
    }
    const QString loader = data.value(QStringLiteral("loader"), QStringLiteral("Vanilla")).toString();
    const QString dir = uniqueDir(m_root, slugifyLatin(name));
    QJsonObject instance{
        {QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {QStringLiteral("name"), name},
        {QStringLiteral("version"), mc},
        {QStringLiteral("loader"), loader},
        {QStringLiteral("loaderVersion"), data.value(QStringLiteral("loaderVersion")).toString().left(64)},
        {QStringLiteral("mods"), QJsonArray()},
        {QStringLiteral("ram"), std::clamp(data.value(QStringLiteral("ram"), m_settings ? m_settings->ram() : 4096).toInt(), 1024, 65536)},
        {QStringLiteral("jvmArgs"), data.value(QStringLiteral("jvmArgs")).toString().left(2000)},
        {QStringLiteral("gameArgs"), data.value(QStringLiteral("gameArgs")).toString().left(2000)},
        {QStringLiteral("javaPath"), data.value(QStringLiteral("javaPath")).toString().left(1024)},
        {QStringLiteral("gameDir"), dir},
        {QStringLiteral("created"), static_cast<double>(QDateTime::currentMSecsSinceEpoch())},
        {QStringLiteral("lastLaunched"), QJsonValue::Null},
        {QStringLiteral("installedVersionId"), QJsonValue::Null},
        {QStringLiteral("icon"), QJsonValue::Null},
    };
    writeMeta(instance);
    Logger::info(QStringLiteral("Сборка создана: %1 (%2)").arg(name, dir));
    reload();
    setSelectedInstanceId(instance.value(QStringLiteral("id")).toString());
    return instance.toVariantMap();
}

QVariantMap InstanceManager::update(const QString &id, const QVariantMap &patch)
{
    QJsonObject current = get(id);
    if (current.isEmpty())
        return {};
    QJsonObject next = current;
    if (patch.contains(QStringLiteral("name"))) {
        const QString name = patch.value(QStringLiteral("name")).toString().trimmed().left(48);
        if (!name.isEmpty())
            next.insert(QStringLiteral("name"), name);
    }
    if (patch.contains(QStringLiteral("version")) && !patch.value(QStringLiteral("version")).toString().isEmpty())
        next.insert(QStringLiteral("version"), patch.value(QStringLiteral("version")).toString());
    if (patch.contains(QStringLiteral("loader")) && !patch.value(QStringLiteral("loader")).toString().isEmpty())
        next.insert(QStringLiteral("loader"), patch.value(QStringLiteral("loader")).toString());
    if (patch.contains(QStringLiteral("loaderVersion")))
        next.insert(QStringLiteral("loaderVersion"), patch.value(QStringLiteral("loaderVersion")).toString());
    if (patch.contains(QStringLiteral("ram")))
        next.insert(QStringLiteral("ram"), std::clamp(patch.value(QStringLiteral("ram")).toInt(), 1024, 65536));
    if (patch.contains(QStringLiteral("jvmArgs")))
        next.insert(QStringLiteral("jvmArgs"), patch.value(QStringLiteral("jvmArgs")).toString().left(2000));
    if (patch.contains(QStringLiteral("gameArgs")))
        next.insert(QStringLiteral("gameArgs"), patch.value(QStringLiteral("gameArgs")).toString().left(2000));
    if (patch.contains(QStringLiteral("javaPath")))
        next.insert(QStringLiteral("javaPath"), patch.value(QStringLiteral("javaPath")).toString().left(1024));
    if (patch.contains(QStringLiteral("installedVersionId")))
        next.insert(QStringLiteral("installedVersionId"), QJsonValue::fromVariant(patch.value(QStringLiteral("installedVersionId"))));
    if (patch.contains(QStringLiteral("lastLaunched")))
        next.insert(QStringLiteral("lastLaunched"), QJsonValue::fromVariant(patch.value(QStringLiteral("lastLaunched"))));

    if (next.value(QStringLiteral("version")).toString() != current.value(QStringLiteral("version")).toString()
        || next.value(QStringLiteral("loader")).toString() != current.value(QStringLiteral("loader")).toString())
        next.insert(QStringLiteral("installedVersionId"), QJsonValue::Null);

    writeMeta(next);
    reload();
    const QJsonObject fresh = get(id);
    return fresh.toVariantMap();
}

void InstanceManager::remove(const QString &id, bool deleteFiles)
{
    const QJsonObject instance = get(id);
    if (instance.isEmpty())
        return;
    const QString dir = instance.value(QStringLiteral("gameDir")).toString();
    if (deleteFiles) {
        const QString normalizedRoot = QDir(m_root).absolutePath();
        const QString normalizedDir = QDir(dir).absolutePath();
        if (!normalizedDir.startsWith(normalizedRoot + QDir::separator())) {
            emit toastRequested(QStringLiteral("Отказано: папка вне каталога сборок"), QStringLiteral("error"));
            return;
        }
        QDir(dir).removeRecursively();
        Logger::info(QStringLiteral("Сборка удалена: %1").arg(instance.value(QStringLiteral("name")).toString()));
    } else {
        QFile::remove(QDir(dir).filePath(QStringLiteral("instance.json")));
    }
    if (m_selectedId == id)
        m_selectedId.clear();
    reload();
}

QVariantMap InstanceManager::clone(const QString &id, const QString &requestedName)
{
    const QJsonObject source = get(id);
    if (source.isEmpty()) {
        emit toastRequested(QStringLiteral("Сборка не найдена"), QStringLiteral("error"));
        return {};
    }
    const QString name = (requestedName.isEmpty() ? source.value(QStringLiteral("name")).toString() + QStringLiteral(" (копия)")
                                                  : requestedName)
                             .trimmed()
                             .left(48);
    const QString dir = uniqueDir(m_root, slugifyLatin(name));
    QString error;
    if (!Paths::copyDirRecursive(source.value(QStringLiteral("gameDir")).toString(), dir,
                                 {QStringLiteral("instance.json"), QStringLiteral("logs"), QStringLiteral("crash-reports")},
                                 &error)) {
        emit toastRequested(QStringLiteral("Клонирование не удалось: %1").arg(error), QStringLiteral("error"));
        return {};
    }
    QJsonObject copy = source;
    copy.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    copy.insert(QStringLiteral("name"), name);
    copy.insert(QStringLiteral("gameDir"), dir);
    copy.insert(QStringLiteral("created"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    copy.insert(QStringLiteral("lastLaunched"), QJsonValue::Null);
    writeMeta(copy);
    reload();
    return copy.toVariantMap();
}

bool InstanceManager::exportArchive(const QString &id, const QString &targetPath)
{
    const QJsonObject instance = get(id);
    if (instance.isEmpty())
        return false;
    QString error;
    const bool ok = zipAddDirectory(targetPath, instance.value(QStringLiteral("gameDir")).toString(),
                                    {QStringLiteral("logs"), QStringLiteral("crash-reports"), QStringLiteral(".cache")},
                                    &error);
    if (!ok)
        emit toastRequested(QStringLiteral("Экспорт не удался: %1").arg(error), QStringLiteral("error"));
    else
        Logger::info(QStringLiteral("Сборка экспортирована: %1").arg(instance.value(QStringLiteral("name")).toString()));
    return ok;
}

QVariantMap InstanceManager::importArchive(const QString &zipPath)
{
    QVector<ZipEntry> entries;
    QString error;
    if (!zipListEntries(zipPath, entries, &error)) {
        emit toastRequested(QStringLiteral("Импорт не удался: %1").arg(error), QStringLiteral("error"));
        return {};
    }
    QString metaName;
    for (const auto &entry : entries) {
        if (entry.name.endsWith(QLatin1String("instance.json"))) {
            metaName = entry.name;
            break;
        }
    }
    if (metaName.isEmpty()) {
        emit toastRequested(QStringLiteral("В архиве нет instance.json — это не сборка Noble Launcher"), QStringLiteral("error"));
        return {};
    }
    QByteArray metaData;
    if (!zipReadEntry(zipPath, metaName, metaData, &error)) {
        emit toastRequested(QStringLiteral("Не удалось прочитать instance.json"), QStringLiteral("error"));
        return {};
    }
    QJsonObject meta = QJsonDocument::fromJson(metaData).object();
    if (meta.isEmpty()) {
        emit toastRequested(QStringLiteral("Повреждён instance.json"), QStringLiteral("error"));
        return {};
    }
    const QString prefix = metaName.left(metaName.size() - QStringLiteral("instance.json").size());
    const QString name = meta.value(QStringLiteral("name")).toString(QStringLiteral("imported"));
    const QString dir = uniqueDir(m_root, slugifyLatin(name));

    for (const auto &entry : entries) {
        if (entry.directory)
            continue;
        if (!prefix.isEmpty() && !entry.name.startsWith(prefix))
            continue;
        const QString relative = prefix.isEmpty() ? entry.name : entry.name.mid(prefix.size());
        if (relative.isEmpty() || relative.contains(QLatin1String("..")))
            continue;
        const QString target = QDir(dir).filePath(relative);
        QDir().mkpath(QFileInfo(target).absolutePath());
        QByteArray data;
        if (!zipReadEntry(zipPath, entry.name, data))
            continue;
        QFile out(target);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(data);
            out.close();
        }
    }

    meta.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    meta.insert(QStringLiteral("gameDir"), dir);
    if (!meta.value(QStringLiteral("mods")).isArray())
        meta.insert(QStringLiteral("mods"), QJsonArray());
    meta.insert(QStringLiteral("created"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    meta.insert(QStringLiteral("lastLaunched"), QJsonValue::Null);
    writeMeta(meta);
    reload();
    Logger::info(QStringLiteral("Сборка импортирована: %1").arg(name));
    return meta.toVariantMap();
}

QString InstanceManager::setIcon(const QString &id, const QString &sourcePath)
{
    QJsonObject instance = get(id);
    if (instance.isEmpty())
        return QString();
    const QString iconsDir = Paths::instanceIconsDir();
    QDir().mkpath(iconsDir);
    const QString current = instance.value(QStringLiteral("icon")).toString();
    if (!current.isEmpty())
        QFile::remove(QDir(iconsDir).filePath(current));
    if (sourcePath.isEmpty()) {
        instance.insert(QStringLiteral("icon"), QJsonValue::Null);
        writeMeta(instance);
        reload();
        return QString();
    }
    QFile file(sourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit toastRequested(QStringLiteral("Не удалось прочитать файл иконки"), QStringLiteral("error"));
        return QString();
    }
    const QByteArray content = file.readAll();
    if (content.isEmpty() || content.size() > 4 * 1024 * 1024) {
        emit toastRequested(QStringLiteral("Иконка пуста или больше 4 МБ"), QStringLiteral("error"));
        return QString();
    }
    const QString ext = Noble::detectImageExt(content.left(12));
    if (ext.isEmpty()) {
        emit toastRequested(QStringLiteral("Поддерживаются PNG, JPG и WEBP"), QStringLiteral("error"));
        return QString();
    }
    const QString fileName = instance.value(QStringLiteral("id")).toString() + ext;
    QFile out(QDir(iconsDir).filePath(fileName));
    if (!out.open(QIODevice::WriteOnly)) {
        emit toastRequested(QStringLiteral("Не удалось сохранить иконку"), QStringLiteral("error"));
        return QString();
    }
    out.write(content);
    out.close();
    instance.insert(QStringLiteral("icon"), fileName);
    writeMeta(instance);
    reload();
    return QUrl::fromLocalFile(QDir(iconsDir).filePath(fileName)).toString();
}

void InstanceManager::addMod(const QString &id, const QVariantMap &mod)
{
    QJsonObject instance = get(id);
    if (instance.isEmpty())
        return;
    QJsonArray mods = instance.value(QStringLiteral("mods")).toArray();
    QJsonObject record{
        {QStringLiteral("id"), mod.value(QStringLiteral("id")).toString()},
        {QStringLiteral("name"), mod.value(QStringLiteral("name")).toString()},
        {QStringLiteral("fileName"), mod.value(QStringLiteral("fileName")).toString()},
        {QStringLiteral("source"), mod.value(QStringLiteral("source"), QStringLiteral("modrinth")).toString()},
        {QStringLiteral("projectId"), mod.value(QStringLiteral("projectId")).toString()},
        {QStringLiteral("versionId"), mod.value(QStringLiteral("versionId")).toString()},
        {QStringLiteral("version"), mod.value(QStringLiteral("version")).toString()},
        {QStringLiteral("url"), mod.value(QStringLiteral("url")).toString()},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("installedAt"), static_cast<double>(QDateTime::currentMSecsSinceEpoch())},
    };
    bool replaced = false;
    for (int i = 0; i < mods.size(); ++i) {
        if (mods.at(i).toObject().value(QStringLiteral("fileName")).toString()
            == record.value(QStringLiteral("fileName")).toString()) {
            mods.replace(i, record);
            replaced = true;
            break;
        }
    }
    if (!replaced)
        mods.append(record);
    instance.insert(QStringLiteral("mods"), mods);
    writeMeta(instance);
    reload();
}

void InstanceManager::removeMod(const QString &id, const QString &modId)
{
    QJsonObject instance = get(id);
    if (instance.isEmpty())
        return;
    QJsonArray mods = instance.value(QStringLiteral("mods")).toArray();
    QJsonArray next;
    for (const auto &value : mods) {
        const QJsonObject mod = value.toObject();
        if (mod.value(QStringLiteral("id")).toString() == modId) {
            QFile::remove(QDir(modsDir(instance)).filePath(mod.value(QStringLiteral("fileName")).toString()));
            continue;
        }
        next.append(mod);
    }
    instance.insert(QStringLiteral("mods"), next);
    writeMeta(instance);
    reload();
}

void InstanceManager::toggleMod(const QString &id, const QString &modId)
{
    QJsonObject instance = get(id);
    if (instance.isEmpty())
        return;
    QJsonArray mods = instance.value(QStringLiteral("mods")).toArray();
    const QString dir = modsDir(instance);
    for (int i = 0; i < mods.size(); ++i) {
        QJsonObject mod = mods.at(i).toObject();
        if (mod.value(QStringLiteral("id")).toString() != modId)
            continue;
        QString fileName = mod.value(QStringLiteral("fileName")).toString();
        const bool enabled = mod.value(QStringLiteral("enabled")).toBool(true);
        QString nextName;
        if (enabled) {
            nextName = fileName.endsWith(QLatin1String(".disabled")) ? fileName : fileName + QStringLiteral(".disabled");
        } else {
            nextName = fileName.endsWith(QLatin1String(".disabled")) ? fileName.left(fileName.size() - 9) : fileName;
        }
        if (QFile::exists(QDir(dir).filePath(fileName)))
            QFile::rename(QDir(dir).filePath(fileName), QDir(dir).filePath(nextName));
        mod.insert(QStringLiteral("fileName"), nextName);
        mod.insert(QStringLiteral("enabled"), !enabled);
        mods.replace(i, mod);
        break;
    }
    instance.insert(QStringLiteral("mods"), mods);
    writeMeta(instance);
    reload();
}

void InstanceManager::replaceMods(const QString &id, const QVariantList &mods)
{
    QJsonObject instance = get(id);
    if (instance.isEmpty())
        return;
    instance.insert(QStringLiteral("mods"), QJsonArray::fromVariantList(mods));
    writeMeta(instance);
    reload();
}

void InstanceManager::markLaunched(const QString &id)
{
    QJsonObject instance = get(id);
    if (instance.isEmpty())
        return;
    instance.insert(QStringLiteral("lastLaunched"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    writeMeta(instance);
    reload();
}

QString InstanceManager::migrationLogPath() const
{
    return QDir(Paths::dataDir()).filePath(QStringLiteral("instances-migration.json"));
}

QVariantMap InstanceManager::migrationInfo() const
{
    const QJsonObject log = readJsonObject(migrationLogPath());
    return log.toVariantMap();
}

void InstanceManager::migrateLatinDirs()
{
    const QFileInfoList entries = QDir(m_root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    QJsonArray moves;
    for (const auto &entry : entries) {
        static const QRegularExpression latin(QStringLiteral("^[a-z0-9._-]+$"), QRegularExpression::CaseInsensitiveOption);
        if (latin.match(entry.fileName()).hasMatch())
            continue;
        QJsonObject instance = readMeta(entry.absoluteFilePath());
        if (instance.isEmpty())
            continue;
        const QString target = uniqueDir(m_root, slugifyLatin(instance.value(QStringLiteral("name")).toString(QStringLiteral("instance"))));
        if (QDir(target).absolutePath() == entry.absoluteFilePath())
            continue;
        if (!QDir().rename(entry.absoluteFilePath(), target))
            continue;
        instance.insert(QStringLiteral("gameDir"), target);
        writeMeta(instance);
        moves.append(QJsonObject{{QStringLiteral("id"), instance.value(QStringLiteral("id"))},
                                 {QStringLiteral("name"), instance.value(QStringLiteral("name"))},
                                 {QStringLiteral("from"), entry.absoluteFilePath()},
                                 {QStringLiteral("to"), target}});
        Logger::info(QStringLiteral("Папка сборки переименована: %1 → %2").arg(entry.fileName(), QFileInfo(target).fileName()));
    }
    if (!moves.isEmpty()) {
        writeJsonObject(migrationLogPath(),
                        QJsonObject{{QStringLiteral("at"), static_cast<double>(QDateTime::currentMSecsSinceEpoch())},
                                    {QStringLiteral("moves"), moves}});
        emit migrationHappened(moves.size());
    }
}

int InstanceManager::rollbackMigration()
{
    const QJsonObject log = readJsonObject(migrationLogPath());
    const QJsonArray moves = log.value(QStringLiteral("moves")).toArray();
    int reverted = 0;
    for (auto it = moves.constEnd(); it != moves.constBegin();) {
        --it;
        const QJsonObject move = it->toObject();
        const QString from = move.value(QStringLiteral("from")).toString();
        const QString to = move.value(QStringLiteral("to")).toString();
        if (from.isEmpty() || to.isEmpty() || QFileInfo::exists(from) || !QFileInfo(to).isDir())
            continue;
        QJsonObject instance = readMeta(to);
        if (instance.isEmpty())
            continue;
        if (QDir().rename(to, from)) {
            instance.insert(QStringLiteral("gameDir"), from);
            writeMeta(instance);
            ++reverted;
        }
    }
    QFile::remove(migrationLogPath());
    if (reverted > 0)
        reload();
    return reverted;
}

} // namespace Noble
