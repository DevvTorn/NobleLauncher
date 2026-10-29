#pragma once

#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QStringList>
#include <functional>

namespace Noble {

class Settings;
class InstanceManager;

class OptimizerService
{
public:
    static QVariantMap inspect(Settings *settings, const QVariantList &instances);
    static QVariantList processes();
    static QVariantMap apply(const QStringList &ops, const QList<qint64> &pids, const QVariantList &instances,
                             Settings *settings);
    static QVariantMap rollback(Settings *settings);
    static QString gameExecutable(Settings *settings, const QVariantList &instances);
    static QVariantMap state();
};

} // namespace Noble
