#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>

namespace Noble {

namespace Paths {

QString dataDir();
QString cacheDir();
QString logsDir();
QString exeDir();
QString appDataDir();
QString localAppDataDir();
QString instancesDefaultDir();
QString runtimeDefaultDir();
QString skinsDir();
QString backgroundsDir();
QString backgroundsThumbsDir();
QString fontsDir();
QString profileIconsDir();
QString instanceIconsDir();
QString optimizerDir();
QString runtimeJavaDir();
QString tempDir();

QStringList legacyDataDirs();
QStringList envSearchPaths();

void ensureDirs();

/** Копирует конфиги/ассеты старой Electron-версии в новый каталог. Исходники не удаляются. */
QJsonObject migrateLegacyData();
QJsonObject migrationLog();
/** Возвращает True, если удалось восстановить старые копии поверх текущих. */
bool rollbackMigrationData(QString *error = nullptr);

void clearCache();

QString uniqueSubdir(const QString &root, const QString &base);
bool copyDirRecursive(const QString &from, const QString &to, const QStringList &skipNames = {}, QString *error = nullptr);

} // namespace Paths

} // namespace Noble
