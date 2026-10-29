#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>
#include <QVariantMap>
#include <QStringList>
#include <atomic>
#include <functional>

namespace Noble {

class InstanceManager;

struct ContentInstallReport {
    std::function<void(const QString &stage, const QString &message, double progress, qint64 received, qint64 total)> fn;
};

struct ContentInstallResult {
    QStringList installed;
    QStringList skipped;
};

class ModrinthService
{
public:
    static QVariantList search(const QString &query, const QString &projectType, const QString &gameVersion,
                               const QString &loader, int offset, int limit);
    static QVariantList versionsFor(const QString &projectId, const QString &gameVersion, const QString &loader,
                                   const QString &projectType = QStringLiteral("mod"));
    static ContentInstallResult installContent(InstanceManager *instances, const QString &instanceId,
                                               const QString &kind, const QString &projectId,
                                               const QString &versionId, const ContentInstallReport &report,
                                               std::atomic_bool *cancel, QString *error);
    static QJsonObject parseDirectUrl(const QString &url, QString *error);
    static QJsonObject installFromUrl(InstanceManager *instances, const QString &instanceId, const QString &url,
                                      const ContentInstallReport &report, std::atomic_bool *cancel, QString *error,
                                      QString *message);
    static QJsonObject installMrpackFile(const QString &archivePath, InstanceManager *instances,
                                         const std::function<void(const QString &, const QString &, double)> &report,
                                         std::atomic_bool *cancel, QString *error);
    static QString folderFor(const QString &kind);
};

} // namespace Noble
