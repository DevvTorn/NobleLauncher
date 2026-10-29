#include "Zip.h"

#include "miniz.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include <memory>

namespace Noble {

namespace {

struct ReaderCloser {
    mz_zip_archive *archive;
    ~ReaderCloser()
    {
        if (archive)
            mz_zip_reader_end(archive);
    }
};

struct WriterCloser {
    mz_zip_archive *archive;
    ~WriterCloser()
    {
        if (archive)
            mz_zip_writer_end(archive);
    }
};

bool initReader(mz_zip_archive &archive, const QString &path, QString *error)
{
    memset(&archive, 0, sizeof(archive));
    const QByteArray file = QFile::encodeName(path);
    if (!mz_zip_reader_init_file(&archive, file.constData(), 0)) {
        if (error)
            *error = QStringLiteral("Не удалось открыть архив: %1").arg(path);
        return false;
    }
    return true;
}

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}
} // namespace

bool zipListEntries(const QString &archivePath, QVector<ZipEntry> &entries, QString *error)
{
    mz_zip_archive archive;
    if (!initReader(archive, archivePath, error))
        return false;
    ReaderCloser closer{&archive};
    entries.clear();
    const mz_uint count = mz_zip_reader_get_num_files(&archive);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&archive, i, &stat))
            continue;
        ZipEntry entry;
        entry.name = QString::fromUtf8(stat.m_filename);
        entry.directory = mz_zip_reader_is_file_a_directory(&archive, i) != MZ_FALSE;
        entry.size = static_cast<qint64>(stat.m_uncomp_size);
        entries.push_back(entry);
    }
    return true;
}

bool zipContains(const QString &archivePath, const QString &entryName)
{
    mz_zip_archive archive;
    if (!initReader(archive, archivePath, nullptr))
        return false;
    ReaderCloser closer{&archive};
    return mz_zip_reader_locate_file(&archive, entryName.toUtf8().constData(), nullptr, 0) >= 0;
}

bool zipReadEntryFromData(const QByteArray &archiveData, const QString &entryName, QByteArray &data, QString *error)
{
    mz_zip_archive archive;
    memset(&archive, 0, sizeof(archive));
    if (!mz_zip_reader_init_mem(&archive, archiveData.constData(), static_cast<size_t>(archiveData.size()), 0)) {
        setError(error, QStringLiteral("Повреждён ZIP-архив"));
        return false;
    }
    ReaderCloser closer{&archive};
    const int index = mz_zip_reader_locate_file(&archive, entryName.toUtf8().constData(), nullptr, 0);
    if (index < 0) {
        setError(error, QStringLiteral("В архиве нет файла %1").arg(entryName));
        return false;
    }
    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&archive, index, &stat)) {
        setError(error, QStringLiteral("Не удалось прочитать файл %1").arg(entryName));
        return false;
    }
    data.resize(static_cast<int>(stat.m_uncomp_size));
    if (stat.m_uncomp_size > 0
        && !mz_zip_reader_extract_to_mem(&archive, index, data.data(), stat.m_uncomp_size, 0)) {
        setError(error, QStringLiteral("Не удалось распаковать %1").arg(entryName));
        return false;
    }
    return true;
}

bool zipReadEntry(const QString &archivePath, const QString &entryName, QByteArray &data, QString *error)
{
    mz_zip_archive archive;
    if (!initReader(archive, archivePath, error))
        return false;
    ReaderCloser closer{&archive};
    const int index = mz_zip_reader_locate_file(&archive, entryName.toUtf8().constData(), nullptr, 0);
    if (index < 0) {
        setError(error, QStringLiteral("В архиве нет файла %1").arg(entryName));
        return false;
    }
    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&archive, index, &stat)) {
        setError(error, QStringLiteral("Не удалось прочитать файл %1").arg(entryName));
        return false;
    }
    data.resize(static_cast<int>(stat.m_uncomp_size));
    if (stat.m_uncomp_size > 0
        && !mz_zip_reader_extract_to_mem(&archive, index, data.data(), stat.m_uncomp_size, 0)) {
        setError(error, QStringLiteral("Не удалось распаковать %1").arg(entryName));
        return false;
    }
    return true;
}

bool zipExtractEntryToFile(const QString &archivePath, const QString &entryName, const QString &targetPath, QString *error)
{
    QByteArray data;
    if (!zipReadEntry(archivePath, entryName, data, error))
        return false;
    QDir().mkpath(QFileInfo(targetPath).absolutePath());
    QFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("Не удалось записать %1").arg(targetPath));
        return false;
    }
    file.write(data);
    return true;
}

bool zipExtractAll(const QString &archivePath, const QString &targetDir, const QStringList &onlyPrefixes, QString *error)
{
    mz_zip_archive archive;
    if (!initReader(archive, archivePath, error))
        return false;
    ReaderCloser closer{&archive};
    const mz_uint count = mz_zip_reader_get_num_files(&archive);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&archive, i, &stat))
            continue;
        if (mz_zip_reader_is_file_a_directory(&archive, i))
            continue;
        QString name = QString::fromUtf8(stat.m_filename);
        if (name.contains(QLatin1String("..")))
            continue;
        if (!onlyPrefixes.isEmpty()) {
            bool matches = false;
            for (const auto &prefix : onlyPrefixes) {
                if (name.startsWith(prefix)) {
                    matches = true;
                    break;
                }
            }
            if (!matches)
                continue;
        }
        const QString target = QDir(targetDir).filePath(name);
        QDir().mkpath(QFileInfo(target).absolutePath());
        if (!mz_zip_reader_extract_to_file(&archive, i, QFile::encodeName(target).constData(), 0)) {
            setError(error, QStringLiteral("Не удалось распаковать %1").arg(name));
            return false;
        }
    }
    return true;
}

bool zipWriteFiles(const QString &targetPath, const QVector<QPair<QString, QByteArray>> &files, QString *error)
{
    QDir().mkpath(QFileInfo(targetPath).absolutePath());
    QFile::remove(targetPath);
    mz_zip_archive archive;
    memset(&archive, 0, sizeof(archive));
    if (!mz_zip_writer_init_file(&archive, QFile::encodeName(targetPath).constData(), 0)) {
        setError(error, QStringLiteral("Не удалось создать архив %1").arg(targetPath));
        return false;
    }
    WriterCloser closer{&archive};
    for (const auto &file : files) {
        const QByteArray name = file.first.toUtf8();
        if (!mz_zip_writer_add_mem(&archive, name.constData(), file.second.constData(), static_cast<size_t>(file.second.size()),
                                   MZ_DEFAULT_COMPRESSION)) {
            setError(error, QStringLiteral("Не удалось добавить %1 в архив").arg(file.first));
            return false;
        }
    }
    if (!mz_zip_writer_finalize_archive(&archive)) {
        setError(error, QStringLiteral("Не удалось завершить архив"));
        return false;
    }
    return true;
}

bool zipAddDirectory(const QString &targetPath, const QString &sourceDir, const QStringList &skipNames, QString *error)
{
    QDir root(sourceDir);
    if (!root.exists()) {
        setError(error, QStringLiteral("Папка не найдена: %1").arg(sourceDir));
        return false;
    }
    QDir().mkpath(QFileInfo(targetPath).absolutePath());
    QFile::remove(targetPath);
    mz_zip_archive archive;
    memset(&archive, 0, sizeof(archive));
    if (!mz_zip_writer_init_file(&archive, QFile::encodeName(targetPath).constData(), 0)) {
        setError(error, QStringLiteral("Не удалось создать архив %1").arg(targetPath));
        return false;
    }
    WriterCloser closer{&archive};

    QDirIterator it(sourceDir, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString relative = root.relativeFilePath(path);
        bool skip = false;
        for (const auto &part : relative.split(QLatin1Char('/'))) {
            if (skipNames.contains(part)) {
                skip = true;
                break;
            }
        }
        if (skip)
            continue;
        const QByteArray name = relative.toUtf8();
        if (!mz_zip_writer_add_file(&archive, name.constData(), QFile::encodeName(path).constData(), nullptr, 0,
                                    MZ_DEFAULT_COMPRESSION)) {
            setError(error, QStringLiteral("Не удалось добавить %1 в архив").arg(relative));
            return false;
        }
    }
    if (!mz_zip_writer_finalize_archive(&archive)) {
        setError(error, QStringLiteral("Не удалось завершить архив"));
        return false;
    }
    return true;
}

bool zipWriteFilesAndDirectory(const QString &targetPath, const QVector<QPair<QString, QByteArray>> &files,
                               const QString &sourceDir, const QStringList &skipNames, QString *error)
{
    QDir root(sourceDir);
    if (!root.exists()) {
        setError(error, QStringLiteral("Папка не найдена: %1").arg(sourceDir));
        return false;
    }
    QDir().mkpath(QFileInfo(targetPath).absolutePath());
    QFile::remove(targetPath);
    mz_zip_archive archive;
    memset(&archive, 0, sizeof(archive));
    if (!mz_zip_writer_init_file(&archive, QFile::encodeName(targetPath).constData(), 0)) {
        setError(error, QStringLiteral("Не удалось создать архив %1").arg(targetPath));
        return false;
    }
    WriterCloser closer{&archive};
    for (const auto &file : files) {
        const QByteArray name = file.first.toUtf8();
        if (!mz_zip_writer_add_mem(&archive, name.constData(), file.second.constData(),
                                   static_cast<size_t>(file.second.size()), MZ_DEFAULT_COMPRESSION)) {
            setError(error, QStringLiteral("Не удалось добавить %1 в архив").arg(file.first));
            return false;
        }
    }
    QDirIterator it(sourceDir, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString relative = root.relativeFilePath(path);
        bool skip = false;
        for (const auto &part : relative.split(QLatin1Char('/'))) {
            if (skipNames.contains(part)) {
                skip = true;
                break;
            }
        }
        if (skip)
            continue;
        const QByteArray name = relative.toUtf8();
        if (!mz_zip_writer_add_file(&archive, name.constData(), QFile::encodeName(path).constData(), nullptr, 0,
                                    MZ_DEFAULT_COMPRESSION)) {
            setError(error, QStringLiteral("Не удалось добавить %1 в архив").arg(relative));
            return false;
        }
    }
    if (!mz_zip_writer_finalize_archive(&archive)) {
        setError(error, QStringLiteral("Не удалось завершить архив"));
        return false;
    }
    return true;
}

} // namespace Noble
