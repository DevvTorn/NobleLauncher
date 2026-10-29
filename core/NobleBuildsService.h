#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonArray>
#include <atomic>
#include <functional>

namespace Noble {

class InstanceManager;

struct NobleBuildInstallReport {
    std::function<void(const QString &stage, const QString &message, double progress)> fn;
};

class NobleBuildsService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList builds READ builds NOTIFY buildsChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool fromCache READ fromCache NOTIFY buildsChanged)

public:
    explicit NobleBuildsService(QObject *parent = nullptr);

    QVariantList builds() const;
    bool loading() const { return m_loading; }
    bool fromCache() const { return m_fromCache; }
    void load(bool force = false);

    static QJsonObject install(const QString &buildId, const QJsonArray &builds, InstanceManager *instances,
                               const std::function<void(const QString &, const QString &, double)> &report,
                               std::atomic_bool *cancel, QString *error);

signals:
    void buildsChanged();
    void loadingChanged();

private:
    QJsonArray m_builds;
    bool m_loading = false;
    bool m_fromCache = false;
    qint64 m_fetchedAt = 0;
};

} // namespace Noble
