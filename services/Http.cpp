#include "Http.h"

#include "Pure.h"

#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace Noble {

namespace {
QNetworkAccessManager *manager()
{
    thread_local QNetworkAccessManager nam;
    return &nam;
}

QNetworkRequest makeRequest(const QUrl &url, const QHash<QString, QString> &headers)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, defaultUserAgent());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    return request;
}

QString networkError(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError)
        return QString();
    return reply->errorString().isEmpty() ? QStringLiteral("сетевая ошибка") : reply->errorString();
}
} // namespace

HttpResult httpRequest(const QByteArray &verb, const QUrl &url, const QByteArray &body, const QString &contentType,
                       const QHash<QString, QString> &headers, int timeoutMs)
{
    HttpResult result;
    if (!url.isValid()) {
        result.error = QStringLiteral("Некорректная ссылка");
        return result;
    }

    auto allHeaders = headers;
    if (!contentType.isEmpty())
        allHeaders.insert(QStringLiteral("Content-Type"), contentType);
    if (!allHeaders.contains(QStringLiteral("Accept")))
        allHeaders.insert(QStringLiteral("Accept"), QStringLiteral("application/json"));

    QNetworkRequest request = makeRequest(url, allHeaders);
    QNetworkAccessManager *nam = manager();
    QNetworkReply *reply = nullptr;
    if (verb == "GET")
        reply = nam->get(request);
    else if (verb == "POST")
        reply = nam->post(request, body);
    else if (verb == "PUT")
        reply = nam->put(request, body);
    else if (verb == "DELETE")
        reply = nam->deleteResource(request);
    else
        reply = nam->sendCustomRequest(request, verb, body);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.finalUrl = reply->url().toString();
    result.body = reply->readAll();
    const QString error = networkError(reply);
    if (!error.isEmpty()) {
        result.error = timer.isActive() ? error : QStringLiteral("нет ответа за %1 с").arg(timeoutMs / 1000);
        result.ok = false;
        if (result.status >= 400)
            result.error = QStringLiteral("HTTP %1").arg(result.status);
    } else {
        result.ok = result.status >= 200 && result.status < 400;
        if (!result.ok)
            result.error = QStringLiteral("HTTP %1: %2").arg(result.status).arg(QString::fromUtf8(result.body.left(300)));
    }
    reply->deleteLater();
    return result;
}

HttpResult httpGet(const QUrl &url, const QHash<QString, QString> &headers, int timeoutMs)
{
    return httpRequest("GET", url, {}, QString(), headers, timeoutMs);
}

HttpResult httpPostJson(const QUrl &url, const QJsonObject &body, const QHash<QString, QString> &headers, int timeoutMs)
{
    return httpRequest("POST", url, QJsonDocument(body).toJson(QJsonDocument::Compact), QStringLiteral("application/json"), headers, timeoutMs);
}

HttpResult httpPostForm(const QUrl &url, const QByteArray &form, const QHash<QString, QString> &headers, int timeoutMs)
{
    return httpRequest("POST", url, form, QStringLiteral("application/x-www-form-urlencoded"), headers, timeoutMs);
}

HttpResult httpPostRaw(const QUrl &url, const QByteArray &body, const QString &contentType,
                       const QHash<QString, QString> &headers, int timeoutMs)
{
    return httpRequest("POST", url, body, contentType, headers, timeoutMs);
}

bool downloadFile(const QUrl &url, const QString &target, const QString &sha1, const QString &sha512,
                  const QString &sha256, std::atomic_bool *cancel, const DownloadProgress &progress,
                  QString *error, int stallTimeoutMs)
{
    auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };

    if (cancel && cancel->load())
        return fail(QStringLiteral("Операция отменена"));

    QFileInfo info(target);
    QDir().mkpath(info.absolutePath());
    const QString part = target + QStringLiteral(".part");
    QFile::remove(part);

    QFile file(part);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(QStringLiteral("Не удалось создать файл %1: %2").arg(part, file.errorString()));
    }

    QNetworkRequest request = makeRequest(url, {});
    QNetworkReply *reply = manager()->get(request);

    QCryptographicHash hash1(QCryptographicHash::Sha1);
    QCryptographicHash hash512(QCryptographicHash::Sha512);
    QCryptographicHash hash256(QCryptographicHash::Sha256);
    const bool needSha1 = !sha1.isEmpty();
    const bool needSha512 = !sha512.isEmpty();
    const bool needSha256 = !sha256.isEmpty();

    qint64 received = 0;
    qint64 total = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
    bool cancelled = false;
    bool stalled = false;
    QString writeError;

    QEventLoop loop;
    QTimer stallTimer;
    stallTimer.setSingleShot(true);
    QObject::connect(&stallTimer, &QTimer::timeout, reply, [&] {
        stalled = true;
        reply->abort();
    });

    QObject::connect(reply, &QNetworkReply::readyRead, reply, [&] {
        const QByteArray chunk = reply->readAll();
        if (chunk.isEmpty())
            return;
        if (file.write(chunk) != chunk.size()) {
            writeError = file.errorString();
            reply->abort();
            return;
        }
        received += chunk.size();
        if (needSha1)
            hash1.addData(chunk);
        if (needSha512)
            hash512.addData(chunk);
        if (needSha256)
            hash256.addData(chunk);
        if (progress)
            progress(received, total);
        if (cancel && cancel->load()) {
            cancelled = true;
            reply->abort();
            return;
        }
        stallTimer.start(stallTimeoutMs);
    });
    QObject::connect(reply, &QNetworkReply::downloadProgress, reply, [&](qint64 got, qint64 totalBytes) {
        if (total == 0)
            total = totalBytes;
        if (totalBytes > 0)
            total = totalBytes;
        if (progress && got > received)
            progress(got, total);
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    stallTimer.start(stallTimeoutMs);
    loop.exec();
    stallTimer.stop();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString netError = networkError(reply);
    reply->deleteLater();
    file.close();

    auto cleanup = [&] {
        file.close();
        QFile::remove(part);
    };

    if (cancelled || (cancel && cancel->load())) {
        cleanup();
        return fail(QStringLiteral("Операция отменена"));
    }
    if (!writeError.isEmpty()) {
        cleanup();
        return fail(QStringLiteral("Ошибка записи %1: %2").arg(target, writeError));
    }
    if (stalled) {
        cleanup();
        return fail(QStringLiteral("Превышено время ожидания данных: %1").arg(url.toString()));
    }
    if (!netError.isEmpty() || status >= 400) {
        cleanup();
        return fail(QStringLiteral("Не удалось скачать %1: %2").arg(url.toString(), netError.isEmpty() ? QStringLiteral("HTTP %1").arg(status) : netError));
    }

    if (needSha1 && QString::fromLatin1(hash1.result().toHex()) != sha1.toLower()) {
        cleanup();
        return fail(QStringLiteral("Контрольная сумма SHA-1 не совпала для %1").arg(url.toString()));
    }
    if (needSha512 && QString::fromLatin1(hash512.result().toHex()) != sha512.toLower()) {
        cleanup();
        return fail(QStringLiteral("Контрольная сумма SHA-512 не совпала для %1").arg(url.toString()));
    }
    if (needSha256 && QString::fromLatin1(hash256.result().toHex()) != sha256.toLower()) {
        cleanup();
        return fail(QStringLiteral("Контрольная сумма SHA-256 не совпала для %1").arg(url.toString()));
    }

    QFile::remove(target);
    if (!QFile::rename(part, target)) {
        cleanup();
        return fail(QStringLiteral("Не удалось переименовать %1").arg(part));
    }
    return true;
}

bool fileMatchesHash(const QString &path, const QString &sha1)
{
    if (sha1.isEmpty())
        return QFileInfo::exists(path);
    qint64 size = 0;
    const QString actual = Noble::sha1File(path, &size);
    return size > 0 && actual.compare(sha1, Qt::CaseInsensitive) == 0;
}

bool fileMatchesSize(const QString &path, qint64 size)
{
    QFileInfo info(path);
    return info.exists() && info.size() == size;
}

} // namespace Noble
