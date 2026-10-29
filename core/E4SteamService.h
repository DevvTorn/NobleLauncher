#pragma once

#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <functional>
#include <atomic>

namespace Noble {

class InstanceManager;

class E4SteamService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap status READ status NOTIFY statusChanged)

public:
    explicit E4SteamService(QObject *parent = nullptr);

    QVariantMap status() const { return m_status; }
    void refreshStatus();

    static QVariantMap installForInstance(InstanceManager *instances, const QString &instanceId,
                                          const std::function<void(const QString &, const QString &, double)> &report,
                                          std::atomic_bool *cancel, QString *error);

signals:
    void statusChanged();

private:
    QVariantMap m_status;
};

} // namespace Noble
