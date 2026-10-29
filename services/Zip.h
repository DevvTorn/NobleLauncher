#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Noble {

struct ZipEntry {
    QString name;
    bool directory = false;
    qint64 size = 0;
};

bool zipListEntries(const QString &archivePath, QVector<ZipEntry> &entries, QString *error = nullptr);
bool zipContains(const QString &archivePath, const QString &entryName);
bool zipReadEntry(const QString &archivePath, const QString &entryName, QByteArray &data, QString *error = nullptr);
bool zipReadEntryFromData(const QByteArray &archive, const QString &entryName, QByteArray &data, QString *error = nullptr);
bool zipExtractAll(const QString &archivePath, const QString &targetDir, const QStringList &onlyPrefixes = {}, QString *error = nullptr);
bool zipExtractEntryToFile(const QString &archivePath, const QString &entryName, const QString &targetPath, QString *error = nullptr);

bool zipWriteFiles(const QString &targetPath, const QVector<QPair<QString, QByteArray>> &files, QString *error = nullptr);
bool zipAddDirectory(const QString &targetPath, const QString &sourceDir, const QStringList &skipNames = {}, QString *error = nullptr);
bool zipWriteFilesAndDirectory(const QString &targetPath, const QVector<QPair<QString, QByteArray>> &files,
                               const QString &sourceDir, const QStringList &skipNames = {}, QString *error = nullptr);

} // namespace Noble
