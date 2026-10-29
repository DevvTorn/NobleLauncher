#include "AuthlibInjector.h"

#include "Http.h"
#include "Json.h"
#include "Logger.h"
#include "Paths.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QUrl>

namespace Noble::AuthlibInjector {

const char *kServerUrl = "https://authserver.ely.by/api/authlib-injector";

QString ensure(const QString &runtime, const std::function<void(const QString &, const QString &, double)> &report,
               QString *error)
{
    const QString jar = QDir(runtime).filePath(QStringLiteral("authlib-injector.jar"));
    const QString marker = QDir(runtime).filePath(QStringLiteral("authlib-injector.json"));
    const QStringList sources = {
        QStringLiteral("https://authlib-injector.yushi.moe/artifact/latest.json"),
        QStringLiteral("https://bmclapi2.bangbang93.com/mirrors/authlib-injector/artifact/latest.json"),
    };
    QJsonObject info;
    QStringList failures;
    for (const auto &source : sources) {
        const HttpResult response = httpGet(QUrl(source), {}, 15000);
        if (response.ok && response.object().contains(QStringLiteral("download_url"))) {
            info = response.object();
            break;
        }
        failures << QStringLiteral("%1: %2").arg(QUrl(source).host(), response.error);
    }

    if (info.isEmpty()) {
        if (QFileInfo::exists(jar)) {
            if (report)
                report(QStringLiteral("authlib"), QStringLiteral("authlib-injector: используется ранее скачанный файл"), 1);
            return jar;
        }
        if (error)
            *error = QStringLiteral("Не удалось получить данные authlib-injector: %1").arg(failures.join(QStringLiteral("; ")));
        return QString();
    }

    const QString version = info.value(QStringLiteral("version")).toString();
    const QString downloadUrl = info.value(QStringLiteral("download_url")).toString();
    const QString sha256 = info.value(QStringLiteral("checksums")).toObject().value(QStringLiteral("sha256")).toString();
    const QJsonObject installed = readJsonObject(marker);
    const bool upToDate = QFileInfo::exists(jar) && QFileInfo(jar).size() > 0
        && installed.value(QStringLiteral("version")).toString() == version
        && (sha256.isEmpty() || installed.value(QStringLiteral("sha256")).toString() == sha256);
    if (upToDate) {
        if (report)
            report(QStringLiteral("authlib"), QStringLiteral("authlib-injector %1 уже установлен").arg(version), 1);
        return jar;
    }

    if (report)
        report(QStringLiteral("authlib"), QStringLiteral("Скачивание authlib-injector %1…").arg(version), 0.2);
    QStringList urls{downloadUrl};
    if (downloadUrl.contains(QLatin1String("authlib-injector.yushi.moe")))
        urls << QString(downloadUrl).replace(QLatin1String("https://authlib-injector.yushi.moe"),
                                             QLatin1String("https://bmclapi2.bangbang93.com/mirrors/authlib-injector"));
    QString lastError;
    for (const auto &url : urls) {
        if (downloadFile(QUrl(url), jar, {}, {}, sha256, nullptr, [&](qint64 received, qint64 total) {
                if (total > 0 && report)
                    report(QStringLiteral("authlib"),
                           QStringLiteral("authlib-injector: %1%").arg(static_cast<int>(received * 100 / total)),
                           0.2 + 0.7 * (static_cast<double>(received) / total));
            }, &lastError))
            break;
    }
    if (!QFileInfo::exists(jar) || QFileInfo(jar).size() == 0) {
        if (error)
            *error = QStringLiteral("Не удалось скачать authlib-injector: %1").arg(lastError);
        return QString();
    }
    writeJsonObject(marker,
                    QJsonObject{{QStringLiteral("version"), version},
                                {QStringLiteral("sha256"), sha256},
                                {QStringLiteral("updatedAt"), static_cast<double>(QDateTime::currentMSecsSinceEpoch())}});
    Logger::info(QStringLiteral("authlib-injector %1 установлен").arg(version));
    if (report)
        report(QStringLiteral("authlib"), QStringLiteral("authlib-injector %1 готов").arg(version), 1);
    return jar;
}

} // namespace Noble::AuthlibInjector
