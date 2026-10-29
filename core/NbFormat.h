#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>

namespace Noble {

class InstanceManager;

namespace NbFormat {

constexpr int kFormatVersion = 1;

using Reporter = std::function<void(const QString &stage, const QString &message, double progress)>;

QJsonObject manifestFor(const QJsonObject &instance, bool embedFiles);
bool exportArchive(const QJsonObject &instance, const QString &targetPath, bool embedFiles,
                   const Reporter &report, QString *error);
QJsonObject importArchive(InstanceManager *instances, const QString &archivePath, const Reporter &report,
                          std::atomic_bool *cancel, QString *error, QStringList *warnings);

} // namespace NbFormat

} // namespace Noble
