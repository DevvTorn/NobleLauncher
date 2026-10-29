#include "NewsService.h"

#include "Http.h"
#include "Logger.h"
#include "Pure.h"

#include <QDateTime>
#include <QJsonObject>
#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>

namespace Noble {

NewsService::NewsService(QObject *parent)
    : QObject(parent)
{
}

QVariantList NewsService::items() const
{
    return m_items.toVariantList();
}

void NewsService::load(bool force)
{
    if (m_loading)
        return;
    if (!force && m_fetchedAt > 0 && QDateTime::currentMSecsSinceEpoch() - m_fetchedAt < 15 * 60 * 1000)
        return;
    m_loading = true;
    emit loadingChanged();
    QPointer<NewsService> guard(this);
    QtConcurrent::run([guard] {
        const HttpResult response = httpGet(QUrl(QStringLiteral("https://launchercontent.mojang.com/v2/news.json")), {}, 20000);
        QJsonArray items;
        if (response.ok) {
            const QJsonArray entries = response.object().value(QStringLiteral("entries")).toArray();
            for (const auto &value : entries) {
                const QJsonObject entry = value.toObject();
                QJsonObject item{{QStringLiteral("title"), entry.value(QStringLiteral("title")).toString()},
                                 {QStringLiteral("category"), entry.value(QStringLiteral("category")).toString()},
                                 {QStringLiteral("tag"), entry.value(QStringLiteral("tag")).toString()},
                                 {QStringLiteral("date"), entry.value(QStringLiteral("date")).toString()},
                                 {QStringLiteral("text"), entry.value(QStringLiteral("text")).toString()},
                                 {QStringLiteral("url"), entry.value(QStringLiteral("readMoreLink")).toString()}};
                const QString image = entry.value(QStringLiteral("newsPageImage")).toObject().value(QStringLiteral("url")).toString();
                if (!image.isEmpty())
                    item.insert(QStringLiteral("imageUrl"), QStringLiteral("https://launchercontent.mojang.com") + image);
                items.append(item);
            }
        } else {
            Logger::warn(QStringLiteral("Не удалось загрузить новости: %1").arg(response.error));
        }
        if (!guard)
            return;
        QMetaObject::invokeMethod(guard, [guard, items, ok = response.ok] {
            if (!guard)
                return;
            if (ok) {
                guard->m_items = items;
                guard->m_fetchedAt = QDateTime::currentMSecsSinceEpoch();
            }
            guard->m_loading = false;
            emit guard->loadingChanged();
            emit guard->itemsChanged();
        }, Qt::QueuedConnection);
    });
}

} // namespace Noble
