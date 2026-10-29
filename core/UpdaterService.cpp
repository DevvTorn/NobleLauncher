#include "UpdaterService.h"

#include "Http.h"
#include "Logger.h"
#include "ProcessUtil.h"

#include <QJsonObject>
#include <QJsonDocument>
#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>

namespace Noble {

UpdaterService::UpdaterService(QObject *parent)
    : QObject(parent)
{
    m_status = {{QStringLiteral("state"), QStringLiteral("idle")}};
}

void UpdaterService::check(bool manual)
{
    m_status = {{QStringLiteral("state"), QStringLiteral("checking")}, {QStringLiteral("manual"), manual}};
    emit statusChanged();
    QPointer<UpdaterService> guard(this);
    QtConcurrent::run([guard] {
        QVariantMap status;
        const HttpResult manifest = httpGet(QUrl(QStringLiteral("https://noblelauncher.ru/update.json")), {}, 15000);
        QString version;
        QString url;
        if (manifest.ok) {
            const QJsonObject object = manifest.object();
            version = object.value(QStringLiteral("version")).toString();
            url = object.value(QStringLiteral("url")).toString();
        }
        if (!manifest.ok) {
            const HttpResult response = httpGet(
                QUrl(QStringLiteral("https://api.github.com/repos/DevvTorn/NobleLauncher/releases/latest")),
                {{QStringLiteral("Accept"), QStringLiteral("application/vnd.github+json")}}, 20000);
            if (!response.ok) {
                status = {{QStringLiteral("state"), QStringLiteral("error")},
                          {QStringLiteral("message"), QStringLiteral("Нет связи с сервером обновлений")}};
            } else {
                const QString tag = response.object().value(QStringLiteral("tag_name")).toString();
                version = tag.startsWith(QLatin1Char('v')) ? tag.mid(1) : tag;
                url = response.object().value(QStringLiteral("html_url")).toString();
            }
        }
        if (status.isEmpty()) {
            if (version.isEmpty()) {
                status = {{QStringLiteral("state"), QStringLiteral("error")},
                          {QStringLiteral("message"), QStringLiteral("Некорректный ответ сервера обновлений")}};
            } else if (version == QLatin1String("2.9.0")) {
                status = {{QStringLiteral("state"), QStringLiteral("not-available")}, {QStringLiteral("version"), version}};
            } else {
                status = {{QStringLiteral("state"), QStringLiteral("available")},
                          {QStringLiteral("version"), version},
                          {QStringLiteral("url"), url}};
            }
        }
        if (!guard)
            return;
        QMetaObject::invokeMethod(guard, [guard, status] {
            if (!guard)
                return;
            guard->m_status = status;
            emit guard->statusChanged();
        }, Qt::QueuedConnection);
    });
}

} // namespace Noble
