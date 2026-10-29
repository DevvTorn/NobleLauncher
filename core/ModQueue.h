#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QHash>
#include <QJsonObject>
#include <memory>
#include <atomic>

namespace Noble {

class InstanceManager;

struct QueueItem {
    QString id;
    QString title;
    QString instanceId;
    QString instanceName;
    QString kind;
    QString projectId;
    QString versionId;
    QString status = QStringLiteral("queued");
    double progress = 0;
    QString message = QStringLiteral("В очереди");
    qint64 receivedBytes = 0;
    qint64 totalBytes = 0;
    qint64 speedBps = 0;
    int etaSeconds = -1;
    QString error;
    QStringList installed;
    QStringList skipped;
};

class ModQueue : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY queueChanged)
    Q_PROPERTY(double overallProgress READ overallProgress NOTIFY queueChanged)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY queueChanged)

public:
    explicit ModQueue(QObject *parent = nullptr);

    void setInstances(InstanceManager *instances) { m_instances = instances; }
    QVariantList items() const;
    double overallProgress() const;
    int activeCount() const;

    void enqueue(const QVariantList &entries);
    Q_INVOKABLE void cancel(const QString &id);
    Q_INVOKABLE void retry(const QString &id);
    Q_INVOKABLE void clearFinished();
    void seedDemoItems();

signals:
    void queueChanged();
    void contentInstalled(const QString &kind, int count);
    void instanceTouched(const QString &instanceId);
    void toastRequested(const QString &message, const QString &type);

private:
    void pump();
    void runItem(const QueueItem &job, const std::shared_ptr<std::atomic_bool> &cancelFlag);
    QueueItem *find(const QString &id);
    QVariantMap toVariant(const QueueItem &item) const;

    QVector<QueueItem> m_items;
    QHash<QString, std::shared_ptr<std::atomic_bool>> m_cancel;
    QHash<QString, qint64> m_lastBytes;
    QHash<QString, qint64> m_lastTime;
    QHash<QString, double> m_speed;
    InstanceManager *m_instances = nullptr;
    int m_active = 0;
};

} // namespace Noble
