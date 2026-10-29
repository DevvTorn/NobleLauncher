#include "ModQueue.h"

#include "InstanceManager.h"
#include "Logger.h"
#include "ModrinthService.h"
#include "Pure.h"

#include <QDateTime>
#include <QMetaObject>
#include <QPointer>
#include <QUuid>
#include <QtConcurrent/QtConcurrentRun>

namespace Noble {

static const int kQueueConcurrency = 3;

ModQueue::ModQueue(QObject *parent)
    : QObject(parent)
{
}

QVariantList ModQueue::items() const
{
    QVariantList list;
    for (const auto &item : m_items)
        list.append(toVariant(item));
    return list;
}

double ModQueue::overallProgress() const
{
    if (m_items.isEmpty())
        return 0;
    double total = 0;
    for (const auto &item : m_items)
        total += item.progress;
    return total / m_items.size();
}

int ModQueue::activeCount() const
{
    int active = 0;
    for (const auto &item : m_items) {
        if (item.status == QLatin1String("queued") || item.status == QLatin1String("resolve")
            || item.status == QLatin1String("download") || item.status == QLatin1String("verify")
            || item.status == QLatin1String("install"))
            ++active;
    }
    return active;
}

QVariantMap ModQueue::toVariant(const QueueItem &item) const
{
    return {{QStringLiteral("id"), item.id},
            {QStringLiteral("title"), item.title},
            {QStringLiteral("instanceId"), item.instanceId},
            {QStringLiteral("instanceName"), item.instanceName},
            {QStringLiteral("kind"), item.kind},
            {QStringLiteral("projectId"), item.projectId},
            {QStringLiteral("versionId"), item.versionId},
            {QStringLiteral("status"), item.status},
            {QStringLiteral("progress"), item.progress},
            {QStringLiteral("message"), item.message},
            {QStringLiteral("receivedBytes"), static_cast<double>(item.receivedBytes)},
            {QStringLiteral("totalBytes"), static_cast<double>(item.totalBytes)},
            {QStringLiteral("speedBps"), static_cast<double>(item.speedBps)},
            {QStringLiteral("etaSeconds"), item.etaSeconds},
            {QStringLiteral("error"), item.error},
            {QStringLiteral("installed"), item.installed},
            {QStringLiteral("skipped"), item.skipped}};
}

QueueItem *ModQueue::find(const QString &id)
{
    for (auto &item : m_items) {
        if (item.id == id)
            return &item;
    }
    return nullptr;
}

void ModQueue::enqueue(const QVariantList &entries)
{
    int added = 0;
    for (const auto &value : entries) {
        const QVariantMap entry = value.toMap();
        const QString instanceId = entry.value(QStringLiteral("instanceId")).toString();
        const QString projectId = entry.value(QStringLiteral("projectId")).toString();
        const QString title = entry.value(QStringLiteral("title")).toString();
        if (instanceId.isEmpty() || projectId.isEmpty() || title.isEmpty())
            continue;
        bool duplicate = false;
        for (const auto &item : m_items) {
            if (item.instanceId == instanceId && item.projectId == projectId
                && (item.status == QLatin1String("queued") || item.status == QLatin1String("resolve")
                    || item.status == QLatin1String("download") || item.status == QLatin1String("verify")
                    || item.status == QLatin1String("install"))) {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
            continue;
        QueueItem item;
        item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        item.title = title;
        item.instanceId = instanceId;
        item.instanceName = entry.value(QStringLiteral("instanceName")).toString();
        item.kind = entry.value(QStringLiteral("kind"), QStringLiteral("mod")).toString();
        item.projectId = projectId;
        item.versionId = entry.value(QStringLiteral("versionId")).toString();
        m_items.append(item);
        m_cancel.insert(item.id, std::make_shared<std::atomic_bool>(false));
        ++added;
    }
    if (added > 0) {
        emit queueChanged();
        emit toastRequested(QStringLiteral("Добавлено в очередь: %1").arg(added), QStringLiteral("info"));
        pump();
    }
}

void ModQueue::cancel(const QString &id)
{
    QueueItem *item = find(id);
    if (!item)
        return;
    if (item->status == QLatin1String("done") || item->status == QLatin1String("skipped")
        || item->status == QLatin1String("error") || item->status == QLatin1String("cancelled"))
        return;
    if (auto flag = m_cancel.value(id))
        flag->store(true);
    item->status = QStringLiteral("cancelled");
    item->message = QStringLiteral("Отменено");
    item->speedBps = 0;
    item->etaSeconds = -1;
    emit queueChanged();
}

void ModQueue::retry(const QString &id)
{
    QueueItem *item = find(id);
    if (!item)
        return;
    if (item->status != QLatin1String("error") && item->status != QLatin1String("cancelled")
        && item->status != QLatin1String("skipped"))
        return;
    item->status = QStringLiteral("queued");
    item->message = QStringLiteral("В очереди (повтор)");
    item->progress = 0;
    item->error.clear();
    item->receivedBytes = 0;
    item->totalBytes = 0;
    item->installed.clear();
    item->skipped.clear();
    m_cancel.insert(id, std::make_shared<std::atomic_bool>(false));
    emit queueChanged();
    pump();
}

void ModQueue::clearFinished()
{
    QVector<QueueItem> next;
    for (const auto &item : m_items) {
        if (item.status == QLatin1String("done") || item.status == QLatin1String("skipped")
            || item.status == QLatin1String("error") || item.status == QLatin1String("cancelled"))
            continue;
        next.append(item);
    }
    m_items = next;
    emit queueChanged();
}

void ModQueue::seedDemoItems()
{
    const auto add = [this](const QString &title, const QString &kind, const QString &status, double progress,
                            const QString &message, qint64 received, qint64 total, qint64 speed, int eta) {
        QueueItem item;
        item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        item.title = title;
        item.instanceId = QStringLiteral("inst-1");
        item.instanceName = QStringLiteral("Noble Optimized");
        item.kind = kind;
        item.projectId = title;
        item.status = status;
        item.progress = progress;
        item.message = message;
        item.receivedBytes = received;
        item.totalBytes = total;
        item.speedBps = speed;
        item.etaSeconds = eta;
        if (status == QLatin1String("done"))
            item.installed = {title};
        m_items.append(item);
    };
    add(QStringLiteral("Sodium"), QStringLiteral("mod"), QStringLiteral("done"), 1.0,
        QStringLiteral("Установлено: Sodium"), 0, 0, 0, -1);
    add(QStringLiteral("Iris Shaders"), QStringLiteral("mod"), QStringLiteral("download"), 0.62,
        QStringLiteral("Скачивание Iris Shaders"), 3800000, 6100000, 2400000, 1);
    add(QStringLiteral("Faithful 32x"), QStringLiteral("resourcepack"), QStringLiteral("verify"), 0.9,
        QStringLiteral("Проверка Faithful 32x…"), 0, 0, 0, -1);
    emit queueChanged();
}

void ModQueue::pump()
{
    while (m_active < kQueueConcurrency) {
        QueueItem *next = nullptr;
        for (auto &item : m_items) {
            if (item.status == QLatin1String("queued")) {
                next = &item;
                break;
            }
        }
        if (!next)
            return;
        next->status = QStringLiteral("resolve");
        next->message = QStringLiteral("Подготовка…");
        const QueueItem job = *next;
        const QString id = job.id;
        const auto flag = m_cancel.value(id);
        ++m_active;
        emit queueChanged();
        QPointer<ModQueue> guard(this);
        QtConcurrent::run([guard, job, flag] {
            if (guard)
                guard->runItem(job, flag);
        });
    }
}

void ModQueue::runItem(const QueueItem &job, const std::shared_ptr<std::atomic_bool> &cancelFlag)
{
    const QString id = job.id;
    const QString instanceId = job.instanceId;
    const QString kind = job.kind;
    const QString projectId = job.projectId;
    const QString versionId = job.versionId;
    const QString title = job.title;
    Logger::info(QStringLiteral("Очередь: старт «%1» (%2)").arg(title, kind));

    QPointer<ModQueue> guard(this);
    auto update = [guard, id](const QString &status, const QString &message, double progress, qint64 received, qint64 total) {
        if (!guard)
            return;
        QMetaObject::invokeMethod(guard, [guard, id, status, message, progress, received, total] {
            QueueItem *item = guard->find(id);
            if (!item)
                return;
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (received > 0 && item->receivedBytes != received) {
                const qint64 lastBytes = guard->m_lastBytes.value(id, item->receivedBytes);
                const qint64 lastTime = guard->m_lastTime.value(id, now - 1000);
                const double dt = std::max<qint64>(1, now - lastTime) / 1000.0;
                const double instant = (received - lastBytes) / dt;
                double speed = guard->m_speed.value(id, 0);
                speed = speed > 0 ? speed * 0.6 + instant * 0.4 : instant;
                guard->m_speed.insert(id, speed);
                guard->m_lastBytes.insert(id, received);
                guard->m_lastTime.insert(id, now);
                item->speedBps = std::max<qint64>(0, static_cast<qint64>(speed));
                item->etaSeconds = speed > 1024 && total > received
                    ? static_cast<int>((total - received) / speed)
                    : -1;
            }
            if (!status.isEmpty())
                item->status = status;
            if (!message.isEmpty())
                item->message = message;
            if (progress >= 0)
                item->progress = std::clamp(progress, 0.0, 1.0);
            item->receivedBytes = std::max<qint64>(item->receivedBytes, received);
            item->totalBytes = std::max<qint64>(item->totalBytes, total);
            emit guard->queueChanged();
        }, Qt::QueuedConnection);
    };

    QString error;
    const ContentInstallResult result = ModrinthService::installContent(
        m_instances, instanceId, kind, projectId, versionId,
        ContentInstallReport{[&](const QString &stage, const QString &message, double progress, qint64 received, qint64 total) {
            update(stage, message, progress, received, total);
        }},
        cancelFlag.get(), &error);

    if (!guard)
        return;
    QMetaObject::invokeMethod(guard, [guard, id, result, error, kind, instanceId, title] {
        QueueItem *item = guard->find(id);
        if (!item)
            return;
        const bool cancelled = guard->m_cancel.value(id) && guard->m_cancel.value(id)->load();
        if (cancelled) {
            item->status = QStringLiteral("cancelled");
            item->message = QStringLiteral("Отменено");
            item->speedBps = 0;
            item->etaSeconds = -1;
        } else if (!result.installed.isEmpty()) {
            item->status = QStringLiteral("done");
            item->message = QStringLiteral("Установлено: %1").arg(result.installed.join(QStringLiteral(", ")));
            item->progress = 1;
            item->installed = result.installed;
            item->skipped = result.skipped;
            item->speedBps = 0;
            item->etaSeconds = -1;
            emit guard->contentInstalled(kind, result.installed.size());
            emit guard->instanceTouched(instanceId);
            emit guard->toastRequested(QStringLiteral("Установлено: %1").arg(result.installed.join(QStringLiteral(", "))),
                                       QStringLiteral("success"));
        } else if (!error.isEmpty()) {
            item->status = QStringLiteral("error");
            item->message = QStringLiteral("Ошибка");
            item->error = error;
            item->speedBps = 0;
            item->etaSeconds = -1;
            emit guard->toastRequested(QStringLiteral("Ошибка установки %1: %2").arg(title, error), QStringLiteral("error"));
        } else {
            item->status = QStringLiteral("skipped");
            item->message = QStringLiteral("Подходящая версия не найдена — пропущено");
            item->progress = 1;
            item->skipped = result.skipped;
            item->speedBps = 0;
            item->etaSeconds = -1;
        }
        guard->m_lastBytes.remove(id);
        guard->m_lastTime.remove(id);
        guard->m_speed.remove(id);
        guard->m_active = std::max(0, guard->m_active - 1);
        emit guard->queueChanged();
        guard->pump();
    }, Qt::QueuedConnection);
}

} // namespace Noble
