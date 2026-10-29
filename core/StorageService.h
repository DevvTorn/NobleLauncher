#pragma once

#include <QString>
#include <QVariantMap>
#include <atomic>
#include <functional>

namespace Noble {

class Settings;

using StorageProgress = std::function<void(const QString &message, double progress)>;

class StorageService
{
public:
    static QVariantMap inspect(const QString &kind, const QString &path, Settings *settings);
    static QVariantMap move(const QString &kind, const QString &path, const QString &mode, Settings *settings,
                            const StorageProgress &progress, std::atomic_bool *cancel, QString *error);
};

} // namespace Noble
