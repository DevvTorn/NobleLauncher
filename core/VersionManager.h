#pragma once

#include "LaunchSpec.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QVector>
#include <atomic>
#include <functional>

namespace Noble {

struct VersionSummary {
    QString id;
    QString type;
    QString releaseTime;
};

struct LoaderVersionSummary {
    QString version;
    QString gameVersion;
    bool stable = true;
};

struct Reporter {
    std::function<void(const QString &stage, const QString &message, double progress)> fn;
    void operator()(const QString &stage, const QString &message, double progress) const
    {
        if (fn)
            fn(stage, message, progress);
    }
    explicit operator bool() const { return static_cast<bool>(fn); }
};

struct LibraryProblem {
    QString name;
    QString path;
    QString expectedSha1;
    QString actualSha1;
    qint64 size = 0;
};

class VersionManager
{
public:
    static QVector<VersionSummary> listVersions(bool force = false);
    static QVector<LoaderVersionSummary> listLoaderVersions(const QString &loader, const QString &mcVersion);
    static QString resolveLoaderVersion(const QString &loader, const QString &mcVersion, const QString &requested);

    static QString installInstanceVersion(const QJsonObject &instance, const QString &runtime,
                                          const QString &javaMode, const QString &javaPath, const Reporter &report,
                                          std::atomic_bool *cancel, QString *error);
    static QString ensureInstanceVersion(const QJsonObject &instance, const QString &runtime, const QString &javaMode,
                                         const QString &javaPath, const Reporter &report, std::atomic_bool *cancel,
                                         QString *error);
    static bool isVersionInstalled(const QString &runtime, const QString &versionId);

    static LaunchSpec buildLaunchSpec(const QJsonObject &instance, const QString &runtime, const QString &javaMode,
                                      const QString &javaPath, const LaunchAccount &account, bool needAuthlib,
                                      const QString &authlibJar, const Reporter &report, QString *error);
    static bool ensureAuthlib(const QString &runtime, const Reporter &report, QString *jarPath, QString *error);

    static QJsonObject loadVersion(const QString &runtime, const QString &versionId);

private:
    static QString installLoader(const QJsonObject &instance, const QString &runtime, const QString &loaderVersion,
                                 const QString &javaMode, const QString &javaPath, const Reporter &report,
                                 std::atomic_bool *cancel, QString *error);
};

} // namespace Noble
