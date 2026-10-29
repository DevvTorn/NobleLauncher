#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <atomic>
#include <functional>

namespace Noble {

struct HttpResult {
    bool ok = false;
    int status = 0;
    QByteArray body;
    QString error;
    QString finalUrl;

    QJsonDocument json() const { return QJsonDocument::fromJson(body); }
    QJsonObject object() const { return json().object(); }
};

using DownloadProgress = std::function<void(qint64 received, qint64 total)>;

HttpResult httpGet(const QUrl &url, const QHash<QString, QString> &headers = {}, int timeoutMs = 30000);
HttpResult httpPostJson(const QUrl &url, const QJsonObject &body, const QHash<QString, QString> &headers = {}, int timeoutMs = 30000);
HttpResult httpPostForm(const QUrl &url, const QByteArray &form, const QHash<QString, QString> &headers = {}, int timeoutMs = 30000);
HttpResult httpPostRaw(const QUrl &url, const QByteArray &body, const QString &contentType, const QHash<QString, QString> &headers = {}, int timeoutMs = 30000);
HttpResult httpRequest(const QByteArray &verb, const QUrl &url, const QByteArray &body, const QString &contentType,
                       const QHash<QString, QString> &headers = {}, int timeoutMs = 30000);

bool downloadFile(const QUrl &url, const QString &target, const QString &sha1 = QString(), const QString &sha512 = QString(),
                  const QString &sha256 = QString(), std::atomic_bool *cancel = nullptr,
                  const DownloadProgress &progress = {}, QString *error = nullptr, int stallTimeoutMs = 45000);

bool fileMatchesHash(const QString &path, const QString &sha1);
bool fileMatchesSize(const QString &path, qint64 size);

} // namespace Noble
