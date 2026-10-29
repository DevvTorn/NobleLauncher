#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QDateTime>
#include <cstdint>

namespace Noble {

struct LevelInfo {
    int level = 1;
    QString titleRu;
    QString titleEn;
    int nextAt = 20;
    int toNext = 20;
};

QString offlineUuid(const QString &name);
int requiredJavaMajor(const QString &mcVersion);
QString runtimeComponent(int major);
QString platformKey();
QString neoforgeGameVersion(const QString &version);
QString loaderVersionId(const QString &loader, const QString &mcVersion, const QString &loaderVersion);
QStringList splitArgs(const QString &raw);
QString slugifyLatin(const QString &name);
int parseJavaMajor(const QString &versionOutput);
QList<int> preferredJavaMajors(const QString &mcVersion, const QString &loader = QString());
struct FilteredArgs { QStringList args; bool removed = false; };
FilteredArgs filterUnsafeMemoryFlag(const QStringList &args, int javaMajor);
FilteredArgs filterIncompatibleJvmFlags(const QStringList &args, int javaMajor, bool is64Bit);
struct HeapPlan {
    int heapMb = 4096;
    int initialMb = 512;
    bool clamped = false;
    QString reason;
};
HeapPlan planHeap(int requestedMb, qint64 totalMb, qint64 availableMb, bool is64Bit);
bool mcVersionMatches(const QStringList &ranges, const QString &version);
int compareVersions(const QString &a, const QString &b);
LevelInfo levelForPoints(int points);

QString sha1File(const QString &path, qint64 *size = nullptr);
QString sha512File(const QString &path, qint64 *size = nullptr);
QString sha256File(const QString &path, qint64 *size = nullptr);
QString sha1Bytes(const QByteArray &data);
QString sha256Bytes(const QByteArray &data);
QString md5Hex(const QByteArray &data);

QString humanBytes(qint64 bytes);
QString humanSpeed(qint64 bps);
QString humanDuration(qint64 seconds);
QString humanTimeShort(qint64 seconds);
QString downloadCount(qint64 count);

QString defaultUserAgent();
QString mavenPathFromName(const QString &name);
QString safeFileName(const QString &name);
QString randomHex(int chars);

QString detectImageExt(const QByteArray &head);

} // namespace Noble
