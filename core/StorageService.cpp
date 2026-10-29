#include "StorageService.h"

#include "Logger.h"
#include "Settings.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>

#include <algorithm>

namespace Noble {

namespace {

QString currentDir(const QString &kind, Settings *settings)
{
    return kind == QLatin1String("runtime") ? settings->runtimeDir() : settings->instancesDir();
}

QString otherDir(const QString &kind, Settings *settings)
{
    return kind == QLatin1String("runtime") ? settings->instancesDir() : settings->runtimeDir();
}

QString overlapProblem(const QString &kind, const QString &target, Settings *settings)
{
    const QDir targetDir(target);
    const QString normalized = targetDir.absolutePath();
    const QString self = QDir(currentDir(kind, settings)).absolutePath();
    const QString other = QDir(otherDir(kind, settings)).absolutePath();
    if (normalized == other)
        return QStringLiteral("Путь совпадает с другой рабочей папкой лаунчера");
    if (normalized.startsWith(other + QDir::separator()))
        return QStringLiteral("Путь находится внутри другой рабочей папки лаунчера");
    if (normalized == self)
        return QString();
    if (normalized.startsWith(self + QDir::separator()))
        return QStringLiteral("Нельзя выбрать папку внутри текущей папки данных");
    if (self.startsWith(normalized + QDir::separator()))
        return QStringLiteral("Нельзя выбрать папку, содержащую текущую папку данных");
    return QString();
}

void measure(const QString &dir, qint64 &bytes, int &files)
{
    QDirIterator it(dir, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (files > 200000)
            return;
        bytes += it.fileInfo().size();
        ++files;
    }
}

bool writable(const QString &dir)
{
    const QString probe = QDir(dir).filePath(QStringLiteral(".write-test-%1").arg(QDateTime::currentMSecsSinceEpoch()));
    QFile file(probe);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write("ok");
    file.close();
    file.remove();
    return true;
}

QString storageLabel(const QString &kind)
{
    return kind == QLatin1String("runtime") ? QStringLiteral("общих файлов игры") : QStringLiteral("сборок");
}

bool copyTree(const QString &from, const QString &to, const StorageProgress &progress, std::atomic_bool *cancel,
              qint64 &copiedBytes, qint64 totalBytes, QString *error)
{
    QDir().mkpath(to);
    const QFileInfoList entries = QDir(from).entryInfoList(QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
    for (const auto &entry : entries) {
        if (cancel && cancel->load()) {
            if (error)
                *error = QStringLiteral("Операция отменена");
            return false;
        }
        const QString target = QDir(to).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyTree(entry.absoluteFilePath(), target, progress, cancel, copiedBytes, totalBytes, error))
                return false;
        } else {
            QDir().mkpath(QFileInfo(target).absolutePath());
            QFile::remove(target);
            if (!QFile::copy(entry.absoluteFilePath(), target)) {
                if (error)
                    *error = QStringLiteral("Не удалось скопировать %1").arg(entry.absoluteFilePath());
                return false;
            }
            copiedBytes += entry.size();
            if (progress)
                progress(QStringLiteral("Копирование: %1 / %2").arg(copiedBytes).arg(totalBytes),
                         totalBytes > 0 ? static_cast<double>(copiedBytes) / totalBytes : 0.5);
        }
    }
    return true;
}

} // namespace

QVariantMap StorageService::inspect(const QString &kind, const QString &path, Settings *settings)
{
    QVariantMap info;
    const QString input = path.trimmed();
    const QDir target(input);
    info.insert(QStringLiteral("path"), target.absolutePath());
    info.insert(QStringLiteral("exists"), false);
    info.insert(QStringLiteral("writable"), false);
    info.insert(QStringLiteral("freeBytes"), 0.0);
    info.insert(QStringLiteral("totalBytes"), 0.0);
    info.insert(QStringLiteral("sameVolume"), false);
    info.insert(QStringLiteral("currentBytes"), 0.0);
    info.insert(QStringLiteral("currentFiles"), 0);
    if (input.isEmpty() || !QDir::isAbsolutePath(input)) {
        info.insert(QStringLiteral("problem"), QStringLiteral("Укажите полный путь (например D:\\Games\\NobleLauncher)"));
        return info;
    }
    const QString overlap = overlapProblem(kind, target.absolutePath(), settings);
    if (!overlap.isEmpty()) {
        info.insert(QStringLiteral("problem"), overlap);
        return info;
    }
    bool exists = QFileInfo(target.absolutePath()).isDir();
    if (!exists) {
        if (QDir().mkpath(target.absolutePath()))
            exists = true;
        else
            info.insert(QStringLiteral("problem"), QStringLiteral("Не удалось создать папку"));
    }
    info.insert(QStringLiteral("exists"), exists);
    if (exists) {
        info.insert(QStringLiteral("writable"), writable(target.absolutePath()));
        if (!writable(target.absolutePath()))
            info.insert(QStringLiteral("problem"), QStringLiteral("Нет прав на запись в эту папку"));
        const QStorageInfo storage(target.absolutePath());
        if (storage.isValid()) {
            info.insert(QStringLiteral("freeBytes"), static_cast<double>(storage.bytesAvailable()));
            info.insert(QStringLiteral("totalBytes"), static_cast<double>(storage.bytesTotal()));
        }
    }
    qint64 bytes = 0;
    int files = 0;
    const QString source = currentDir(kind, settings);
    if (QFileInfo(source).isDir())
        measure(source, bytes, files);
    info.insert(QStringLiteral("currentBytes"), static_cast<double>(bytes));
    info.insert(QStringLiteral("currentFiles"), files);
    if (exists && QFileInfo(source).isDir()) {
        const QStorageInfo from(source);
        const QStorageInfo to(target.absolutePath());
        const bool same = from.isValid() && to.isValid() && from.rootPath() == to.rootPath();
        info.insert(QStringLiteral("sameVolume"), same);
        const double free = info.value(QStringLiteral("freeBytes")).toDouble();
        if (!same && free > 0 && bytes > free)
            info.insert(QStringLiteral("problem"),
                        QStringLiteral("Недостаточно места: нужно %1 МБ").arg((bytes + 1048575) / 1048576));
    }
    return info;
}

QVariantMap StorageService::move(const QString &kind, const QString &path, const QString &mode, Settings *settings,
                                 const StorageProgress &progress, std::atomic_bool *cancel, QString *error)
{
    QVariantMap result;
    const QString source = currentDir(kind, settings);
    const QString target = QDir(path).absolutePath();
    if (QDir(source).absolutePath() == target) {
        result.insert(QStringLiteral("path"), target);
        result.insert(QStringLiteral("mode"), mode);
        result.insert(QStringLiteral("bytes"), 0.0);
        result.insert(QStringLiteral("files"), 0);
        return result;
    }
    const QVariantMap check = inspect(kind, target, settings);
    if (check.contains(QStringLiteral("problem"))) {
        if (error)
            *error = check.value(QStringLiteral("problem")).toString();
        return {};
    }
    const QFileInfoList existing = QDir(target).entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden);
    if (!existing.isEmpty()) {
        if (error)
            *error = QStringLiteral("Папка не пуста — выберите пустую или создайте новую");
        return {};
    }
    qint64 bytes = check.value(QStringLiteral("currentBytes")).toDouble();
    int files = check.value(QStringLiteral("currentFiles")).toInt();
    if (progress)
        progress(QStringLiteral("Перенос %1: подготовка…").arg(storageLabel(kind)), 0.02);

    const bool sourceExists = QFileInfo(source).isDir();
    if (sourceExists) {
        bool moved = false;
        if (mode == QLatin1String("move"))
            moved = QDir().rename(source, target);
        if (!moved) {
            qint64 copied = 0;
            if (!copyTree(source, target, progress, cancel, copied, bytes, error)) {
                QDir(target).removeRecursively();
                return {};
            }
            if (mode == QLatin1String("move"))
                QDir(source).removeRecursively();
        }
    }
    result.insert(QStringLiteral("path"), target);
    result.insert(QStringLiteral("mode"), mode);
    result.insert(QStringLiteral("bytes"), static_cast<double>(bytes));
    result.insert(QStringLiteral("files"), files);
    if (progress)
        progress(QStringLiteral("Каталог %1 изменён").arg(storageLabel(kind)), 1);
    Logger::info(QStringLiteral("Данные (%1) перенесены: %2 → %3").arg(kind, source, target));
    return result;
}

} // namespace Noble
