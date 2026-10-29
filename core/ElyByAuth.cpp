#include "ElyByAuth.h"

#include "Http.h"
#include "Logger.h"
#include "Pure.h"

#include <QDateTime>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace Noble::ElyBy {

static const QString kAuthServer = QStringLiteral("https://authserver.ely.by/auth");
static const QString kSkinSystem = QStringLiteral("https://skinsystem.ely.by");
static const QString kOAuthAuthorize = QStringLiteral("https://account.ely.by/oauth2/v1/auth");
static const QString kOAuthToken = QStringLiteral("https://account.ely.by/api/oauth2/v1/token");
static const QString kOAuthInfo = QStringLiteral("https://account.ely.by/api/account/v1/info");
static const QString kOAuthScope = QStringLiteral("account_info minecraft_server_session offline_access");

QString defaultRedirectUri()
{
    return QStringLiteral("http://localhost/redcraft/callback");
}

QString resolveRedirectUri(const QString &overrideUri)
{
    const QString value = overrideUri.trimmed();
    return value.isEmpty() ? defaultRedirectUri() : value;
}

static QString normalizeUuid(const QString &uuid)
{
    QString result = uuid;
    result.remove(QLatin1Char('-'));
    return result.toLower();
}

static QString skinUrlFor(const QString &name)
{
    return QStringLiteral("%1/skins/%2.png").arg(kSkinSystem, QString::fromUtf8(QUrl::toPercentEncoding(name)));
}

static QString describeYggdrasilError(const HttpResult &result, const QString &action)
{
    if (result.status == 401 || result.status == 403)
        return QStringLiteral("Ely.by: неверный логин или пароль (%1)").arg(action);
    if (result.status == 429)
        return QStringLiteral("Ely.by: слишком много попыток входа — попробуйте позже");
    const QJsonObject obj = result.object();
    const QString message = obj.value(QStringLiteral("errorMessage")).toString(obj.value(QStringLiteral("error")).toString());
    if (!message.isEmpty())
        return QStringLiteral("Ely.by: %1").arg(message);
    return QStringLiteral("Ely.by: %1 — %2").arg(action, result.error);
}

QString authorizeUrl(const QString &clientId, const QString &redirectUri)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), clientId);
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("redirect_uri"), redirectUri);
    query.addQueryItem(QStringLiteral("scope"), kOAuthScope);
    QUrl url(kOAuthAuthorize);
    url.setQuery(query);
    return url.toString();
}

ElyAuthResult loginWithPassword(const QString &username, const QString &password)
{
    ElyAuthResult result;
    const QString login = username.trimmed();
    if (login.isEmpty() || password.isEmpty()) {
        result.error = QStringLiteral("Укажите логин (или e-mail) и пароль Ely.by");
        return result;
    }
    const QString clientToken = randomHex(32);
    QJsonObject body{{QStringLiteral("username"), login},
                     {QStringLiteral("password"), password},
                     {QStringLiteral("clientToken"), clientToken},
                     {QStringLiteral("requestUser"), true}};
    const HttpResult response = httpPostJson(QUrl(kAuthServer + QStringLiteral("/authenticate")), body);
    if (!response.ok) {
        result.error = describeYggdrasilError(response, QStringLiteral("вход"));
        return result;
    }
    const QJsonObject json = response.object();
    const QJsonObject profile = json.value(QStringLiteral("selectedProfile")).toObject();
    if (profile.isEmpty()) {
        result.error = QStringLiteral("Ely.by: у аккаунта нет игрового профиля (зайдите на ely.by и выберите ник)");
        return result;
    }
    result.accessToken = json.value(QStringLiteral("accessToken")).toString();
    result.clientToken = json.value(QStringLiteral("clientToken")).toString();
    if (result.clientToken.isEmpty())
        result.clientToken = clientToken;
    result.profile.uuid = normalizeUuid(profile.value(QStringLiteral("id")).toString());
    result.profile.name = profile.value(QStringLiteral("name")).toString();
    result.profile.skinUrl = skinUrlFor(result.profile.name);
    result.ok = !result.accessToken.isEmpty() && !result.profile.name.isEmpty();
    if (!result.ok)
        result.error = QStringLiteral("Ely.by: сервер вернул неполный профиль");
    else
        Logger::info(QStringLiteral("Ely.by: вход по паролю выполнен"));
    return result;
}

ElyAuthResult refreshYggdrasil(const QString &accessToken, const QString &clientToken)
{
    ElyAuthResult result;
    QJsonObject body{{QStringLiteral("accessToken"), accessToken},
                     {QStringLiteral("clientToken"), clientToken},
                     {QStringLiteral("requestUser"), false}};
    const HttpResult response = httpPostJson(QUrl(kAuthServer + QStringLiteral("/refresh")), body);
    if (!response.ok) {
        result.error = describeYggdrasilError(response, QStringLiteral("обновление сессии"));
        return result;
    }
    const QJsonObject json = response.object();
    const QJsonObject profile = json.value(QStringLiteral("selectedProfile")).toObject();
    if (profile.isEmpty()) {
        result.error = QStringLiteral("Ely.by: не удалось обновить сессию — войдите заново");
        return result;
    }
    result.accessToken = json.value(QStringLiteral("accessToken")).toString();
    result.clientToken = json.value(QStringLiteral("clientToken")).toString(clientToken);
    result.profile.uuid = normalizeUuid(profile.value(QStringLiteral("id")).toString());
    result.profile.name = profile.value(QStringLiteral("name")).toString();
    result.profile.skinUrl = skinUrlFor(result.profile.name);
    result.ok = !result.accessToken.isEmpty();
    if (!result.ok)
        result.error = QStringLiteral("Ely.by: сервер не вернул access-токен");
    return result;
}

bool validateYggdrasil(const QString &accessToken, const QString &clientToken, bool *networkError)
{
    if (networkError)
        *networkError = false;
    if (accessToken.isEmpty())
        return false;
    QJsonObject body{{QStringLiteral("accessToken"), accessToken}, {QStringLiteral("clientToken"), clientToken}};
    const HttpResult response = httpPostJson(QUrl(kAuthServer + QStringLiteral("/validate")), body);
    if (response.status == 204)
        return true;
    if (response.status == 403)
        return false;
    if (networkError)
        *networkError = response.status == 0 || response.error.startsWith(QLatin1String("Нет связи"));
    return true;
}

static QJsonObject decodeJwtPayload(const QString &token)
{
    const QStringList parts = token.split(QLatin1Char('.'));
    if (parts.size() < 2)
        return {};
    QByteArray data = parts[1].toUtf8();
    data.replace('-', '+');
    data.replace('_', '/');
    while (data.size() % 4)
        data.append('=');
    return QJsonDocument::fromJson(QByteArray::fromBase64(data)).object();
}

static bool isMinecraftUuid(const QString &uuid)
{
    static const QRegularExpression re(QStringLiteral("^[0-9a-f]{32}$"));
    return re.match(uuid).hasMatch();
}

ElyAuthResult profileFromAccessToken(const QString &accessToken)
{
    ElyAuthResult result;
    const HttpResult info = httpGet(QUrl(kOAuthInfo), {{QStringLiteral("Authorization"), QStringLiteral("Bearer ") + accessToken}});
    if (info.ok) {
        const QJsonObject json = info.object();
        const QString uuid = normalizeUuid(json.value(QStringLiteral("uuid")).toString());
        const QString name = json.value(QStringLiteral("username")).toString();
        if (!name.isEmpty() && isMinecraftUuid(uuid)) {
            result.profile.uuid = uuid;
            result.profile.name = name;
            result.profile.skinUrl = skinUrlFor(name);
            result.ok = true;
            return result;
        }
    } else if (info.status == 401 || info.status == 403) {
        Logger::warn(QStringLiteral("Ely.by: account info отказал — у токена нет scope account_info"));
    }
    const QJsonObject payload = decodeJwtPayload(accessToken);
    const QString name = payload.value(QStringLiteral("username")).toString(
        payload.value(QStringLiteral("nickname")).toString(payload.value(QStringLiteral("preferred_username")).toString()));
    QString uuid = normalizeUuid(payload.value(QStringLiteral("uuid")).toString(payload.value(QStringLiteral("sub")).toString()));
    if (!name.isEmpty() && isMinecraftUuid(uuid)) {
        result.profile.uuid = uuid;
        result.profile.name = name;
        result.profile.skinUrl = skinUrlFor(name);
        result.ok = true;
        return result;
    }
    result.error = QStringLiteral("Ely.by: не удалось получить профиль аккаунта после OAuth-входа. "
                                  "Проверьте scope account_info и minecraft_server_session, затем войдите заново.");
    return result;
}

static ElyAuthResult requestToken(const QByteArray &form)
{
    ElyAuthResult result;
    const HttpResult response = httpPostForm(QUrl(kOAuthToken), form);
    const QJsonObject json = response.object();
    if (!response.ok) {
        const QString details = json.value(QStringLiteral("error_description")).toString(
            json.value(QStringLiteral("error")).toString(response.error));
        result.error = QStringLiteral("Ely.by OAuth: %1").arg(details);
        return result;
    }
    result.accessToken = json.value(QStringLiteral("access_token")).toString();
    if (result.accessToken.isEmpty()) {
        result.error = QStringLiteral("Ely.by OAuth: сервер не вернул access-токен");
        return result;
    }
    result.refreshToken = json.value(QStringLiteral("refresh_token")).toString();
    const qint64 expiresIn = json.value(QStringLiteral("expires_in")).toInteger();
    result.expiresAt = expiresIn > 0 ? QDateTime::currentMSecsSinceEpoch() + expiresIn * 1000 : 0;
    const ElyAuthResult profile = profileFromAccessToken(result.accessToken);
    if (!profile.ok) {
        result.error = profile.error;
        return result;
    }
    result.profile = profile.profile;
    result.ok = true;
    return result;
}

ElyAuthResult exchangeOAuthCode(const QString &clientId, const QString &clientSecret, const QString &redirectUri,
                                const QString &code)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), clientId);
    query.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    query.addQueryItem(QStringLiteral("code"), code);
    query.addQueryItem(QStringLiteral("redirect_uri"), redirectUri);
    if (!clientSecret.isEmpty())
        query.addQueryItem(QStringLiteral("client_secret"), clientSecret);
    ElyAuthResult result = requestToken(query.query(QUrl::FullyEncoded).toUtf8());
    if (result.ok)
        Logger::info(QStringLiteral("Ely.by: OAuth-вход выполнен"));
    return result;
}

ElyAuthResult refreshOAuth(const QString &clientId, const QString &clientSecret, const QString &refreshToken)
{
    if (refreshToken.isEmpty()) {
        ElyAuthResult result;
        result.error = QStringLiteral("Ely.by: нет refresh-токена — войдите заново");
        return result;
    }
    auto build = [&](bool withScope) {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("client_id"), clientId);
        query.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
        query.addQueryItem(QStringLiteral("refresh_token"), refreshToken);
        if (!clientSecret.isEmpty())
            query.addQueryItem(QStringLiteral("client_secret"), clientSecret);
        if (withScope)
            query.addQueryItem(QStringLiteral("scope"), kOAuthScope);
        return query.query(QUrl::FullyEncoded).toUtf8();
    };
    ElyAuthResult result = requestToken(build(true));
    if (!result.ok && result.error.contains(QLatin1String("invalid_scope"), Qt::CaseInsensitive)) {
        Logger::warn(QStringLiteral("Ely.by: обновление без scope (токен выдан с другим набором прав)"));
        result = requestToken(build(false));
    }
    if (result.ok && result.refreshToken.isEmpty())
        result.refreshToken = refreshToken;
    return result;
}

namespace {

struct WebResponse {
    int status = 0;
    QByteArray body;
    QString error;
    QUrl location;
    QUrl finalUrl;
};

struct WebSession {
    QString jwt;
    QHash<QString, QString> cookies;
};

static void collectCookies(QNetworkReply *reply, QHash<QString, QString> &jar)
{
    for (const auto &header : reply->rawHeaderPairs()) {
        if (header.first.compare("Set-Cookie", Qt::CaseInsensitive) != 0)
            continue;
        const QByteArray first = header.second.split(';').first();
        const int equals = first.indexOf('=');
        if (equals <= 0)
            continue;
        const QString name = QString::fromUtf8(first.left(equals)).trimmed();
        const QString value = QString::fromUtf8(first.mid(equals + 1)).trimmed();
        if (!name.isEmpty())
            jar.insert(name, value);
    }
}

static QString cookieHeader(const QHash<QString, QString> &jar)
{
    QStringList pairs;
    for (auto it = jar.constBegin(); it != jar.constEnd(); ++it)
        pairs << it.key() + QLatin1Char('=') + it.value();
    return pairs.join(QStringLiteral("; "));
}

static QUrl secureUrl(const QUrl &source)
{
    QUrl url = source;
    if (url.scheme() == QLatin1String("http"))
        url.setScheme(QStringLiteral("https"));
    return url;
}

static WebResponse webRequest(const QByteArray &verb, const QUrl &start, const QByteArray &body,
                              const QString &contentType, QHash<QString, QString> &cookies,
                              int timeoutMs, bool followRedirects)
{
    WebResponse result;
    QUrl url = secureUrl(start);
    QByteArray method = verb;
    QByteArray payload = body;
    QString type = contentType;
    QNetworkAccessManager network;

    for (int hop = 0; hop < 8; ++hop) {
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, defaultUserAgent());
        request.setRawHeader("Accept", "application/json, text/plain, */*");
        request.setRawHeader("X-Requested-With", "XMLHttpRequest");
        if (url.host() == QLatin1String("ely.by"))
            request.setRawHeader("Referer", "https://ely.by/");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        if (!type.isEmpty())
            request.setHeader(QNetworkRequest::ContentTypeHeader, type);
        if (!cookies.isEmpty())
            request.setRawHeader("Cookie", cookieHeader(cookies).toUtf8());

        QNetworkReply *reply = nullptr;
        if (method == "GET")
            reply = network.get(request);
        else if (method == "POST")
            reply = network.post(request, payload);
        else if (method == "PUT")
            reply = network.put(request, payload);
        else
            reply = network.sendCustomRequest(request, method, payload);

        QEventLoop loop;
        QTimer timer;
        bool timedOut = false;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, reply, [&] {
            timedOut = true;
            reply->abort();
        });
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timer.start(timeoutMs);
        loop.exec();
        timer.stop();

        collectCookies(reply, cookies);
        result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.finalUrl = reply->url();
        result.body = reply->readAll();
        result.location = result.finalUrl.resolved(QUrl::fromEncoded(reply->rawHeader("Location")));
        const QString networkError = reply->error() == QNetworkReply::NoError
            ? QString()
            : reply->errorString();
        reply->deleteLater();

        if (timedOut) {
            result.error = QStringLiteral("таймаут соединения");
            return result;
        }
        if (!networkError.isEmpty() && result.status == 0) {
            result.error = networkError;
            return result;
        }
        const bool redirect = result.status == 301 || result.status == 302 || result.status == 303
            || result.status == 307 || result.status == 308;
        if (followRedirects && redirect && !result.location.isEmpty() && hop < 7) {
            if (result.status == 303 || ((result.status == 301 || result.status == 302) && method == "POST")) {
                method = "GET";
                payload.clear();
                type.clear();
            }
            url = secureUrl(result.location);
            continue;
        }
        if (redirect && !followRedirects)
            return result;
        if (result.status >= 400 || result.status == 0) {
            result.error = result.status > 0 ? QStringLiteral("HTTP %1").arg(result.status)
                                             : QStringLiteral("нет ответа");
        }
        return result;
    }
    result.error = QStringLiteral("слишком много перенаправлений");
    return result;
}

static QString stripTags(QString text)
{
    text.remove(QRegularExpression(QStringLiteral("<[^>]+>")));
    return text.trimmed();
}

static QString siteError(const QJsonObject &document)
{
    const QString error = document.value(QStringLiteral("error")).toString();
    if (!error.isEmpty() && !error.contains(QLatin1String("success"), Qt::CaseInsensitive))
        return stripTags(document.value(QStringLiteral("text")).toString(error));
    if (document.contains(QStringLiteral("success")) && !document.value(QStringLiteral("success")).toBool())
        return stripTags(document.value(QStringLiteral("text")).toString(
            document.value(QStringLiteral("message")).toString()));
    return QString();
}

static QString refreshWebJwt(const QString &refreshToken, QString *error)
{
    if (refreshToken.isEmpty())
        return QString();
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("refresh_token"), refreshToken);
    QHash<QString, QString> cookies;
    const WebResponse response = webRequest(
        "POST", QUrl(QStringLiteral("https://account.ely.by/api/authentication/refresh-token")),
        form.query(QUrl::FullyEncoded).toUtf8(), QStringLiteral("application/x-www-form-urlencoded"),
        cookies, 20000, true);
    const QJsonObject document = QJsonDocument::fromJson(response.body).object();
    const QString jwt = document.value(QStringLiteral("access_token")).toString();
    if (response.error.isEmpty() && !jwt.isEmpty()
        && document.value(QStringLiteral("success")).toBool(true))
        return jwt;
    if (error)
        *error = QStringLiteral("Веб-сессия Ely.by истекла — войдите в аккаунт по паролю заново");
    return QString();
}

static bool openWebSession(const QString &jwt, WebSession &session, QString *error)
{
    if (jwt.isEmpty()) {
        if (error)
            *error = QStringLiteral("Ely.by не выдал веб-сессию для смены скина");
        return false;
    }

    QHash<QString, QString> cookies;
    const WebResponse start = webRequest(
        "GET", QUrl(QStringLiteral("https://ely.by/authorization/login")), {}, {},
        cookies, 20000, false);
    const bool redirect = start.status == 301 || start.status == 302 || start.status == 303
        || start.status == 307 || start.status == 308;
    if (!redirect || start.location.host() != QLatin1String("account.ely.by")) {
        if (error)
            *error = QStringLiteral("Ely.by не открыл страницу входа (HTTP %1)").arg(start.status);
        return false;
    }

    QUrl complete(QStringLiteral("https://account.ely.by/api/oauth2/v1/complete"));
    complete.setQuery(start.location.query());
    const WebResponse approved = webRequest(
        "POST", complete, "accept=1", QStringLiteral("application/x-www-form-urlencoded"),
        cookies, 20000, true);
    const QJsonObject approvedJson = QJsonDocument::fromJson(approved.body).object();
    const QString redirectUri = approvedJson.value(QStringLiteral("redirectUri")).toString();
    if (!approved.error.isEmpty() || redirectUri.isEmpty()
        || (approvedJson.contains(QStringLiteral("success"))
            && !approvedJson.value(QStringLiteral("success")).toBool())) {
        if (error) {
            *error = (approved.status == 401 || approved.status == 403)
                ? QStringLiteral("Ely.by не принял веб-сессию — войдите в аккаунт по паролю заново, "
                                 "чтобы менять скин из лаунчера")
                : QStringLiteral("Ely.by не подтвердил вход в веб-сессию (HTTP %1)").arg(approved.status);
        }
        return false;
    }

    const WebResponse finished = webRequest("GET", QUrl(redirectUri), {}, {}, cookies, 20000, true);
    if (!finished.error.isEmpty() || finished.status >= 400) {
        if (error)
            *error = QStringLiteral("Ely.by не завершил вход в веб-сессию (HTTP %1)").arg(finished.status);
        return false;
    }
    session.jwt = jwt;
    session.cookies = cookies;
    return true;
}

static int skinIdFrom(const QJsonObject &document)
{
    auto valueToId = [](const QJsonValue &value) {
        if (value.isDouble())
            return static_cast<int>(value.toDouble());
        if (value.isString()) {
            bool ok = false;
            const int id = value.toString().toInt(&ok);
            return ok ? id : 0;
        }
        return 0;
    };
    const QJsonObject skin = document.value(QStringLiteral("skin")).toObject();
    const QJsonObject extra = document.value(QStringLiteral("extra")).toObject();
    const QJsonObject extraSkin = extra.value(QStringLiteral("skin")).toObject();
    for (const int id : {valueToId(skin.value(QStringLiteral("id"))),
                          valueToId(document.value(QStringLiteral("id"))),
                          valueToId(extraSkin.value(QStringLiteral("id"))),
                          valueToId(extra.value(QStringLiteral("id")))}) {
        if (id > 0)
            return id;
    }
    const QString url = document.value(QStringLiteral("url")).toString();
    static const QRegularExpression re(QStringLiteral("/skins/s([1-9][0-9]*)"));
    const auto match = re.match(url);
    return match.hasMatch() ? match.captured(1).toInt() : 0;
}

} // namespace

bool webLogin(const QString &login, const QString &password, QString *jwt, QString *refreshToken, QString *error)
{
    if (jwt)
        jwt->clear();
    if (refreshToken)
        refreshToken->clear();
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("login"), login.trimmed());
    form.addQueryItem(QStringLiteral("password"), password);
    form.addQueryItem(QStringLiteral("rememberMe"), QStringLiteral("1"));
    QHash<QString, QString> cookies;
    const WebResponse response = webRequest(
        "POST", QUrl(QStringLiteral("https://account.ely.by/api/authentication/login")),
        form.query(QUrl::FullyEncoded).toUtf8(), QStringLiteral("application/x-www-form-urlencoded"),
        cookies, 20000, true);
    const QJsonObject document = QJsonDocument::fromJson(response.body).object();
    const QString access = document.value(QStringLiteral("access_token")).toString();
    if (!response.error.isEmpty() || access.isEmpty()) {
        const QJsonObject errors = document.value(QStringLiteral("errors")).toObject();
        QString details;
        for (auto it = errors.constBegin(); it != errors.constEnd(); ++it) {
            if (!details.isEmpty())
                details += QStringLiteral(", ");
            details += it.value().toString();
        }
        if (errors.contains(QStringLiteral("totp")))
            details = QStringLiteral("аккаунт защищён 2FA — смена скина доступна на ely.by");
        if (details.isEmpty())
            details = response.error.isEmpty() ? QStringLiteral("нет access_token") : response.error;
        if (error)
            *error = QStringLiteral("Веб-сессия Ely.by: %1").arg(details);
        return false;
    }
    if (jwt)
        *jwt = access;
    if (refreshToken)
        *refreshToken = document.value(QStringLiteral("refresh_token")).toString();
    return true;
}

bool uploadSkinWeb(const QString &webRefreshToken, const QString &fallbackJwt, const QString &fallbackRefreshToken,
                   const QString &filePath, bool slim, QString *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Не удалось прочитать файл скина");
        return false;
    }
    const QByteArray data = file.readAll();
    if (data.isEmpty() || data.size() > 196608) {
        if (error)
            *error = QStringLiteral("Ely.by принимает PNG-скин до 192 КБ");
        return false;
    }
    if (!(static_cast<unsigned char>(data[0]) == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G')) {
        if (error)
            *error = QStringLiteral("Скин должен быть PNG-файлом");
        return false;
    }

    QString sessionError;
    QString jwt;
    if (!webRefreshToken.isEmpty())
        jwt = refreshWebJwt(webRefreshToken, &sessionError);
    if (jwt.isEmpty() && fallbackJwt.startsWith(QLatin1String("eyJ")))
        jwt = fallbackJwt;
    if (jwt.isEmpty() && !fallbackRefreshToken.isEmpty())
        jwt = refreshWebJwt(fallbackRefreshToken, &sessionError);
    if (jwt.isEmpty()) {
        if (error) {
            *error = sessionError.isEmpty()
                ? QStringLiteral("Ely.by не выдал веб-сессию. Войдите в аккаунт по паролю заново, "
                                 "чтобы менять скин из лаунчера (или смените скин на ely.by)")
                : sessionError;
        }
        return false;
    }

    WebSession session;
    if (!openWebSession(jwt, session, &sessionError)) {
        if (error)
            *error = sessionError;
        return false;
    }

    const QString boundary = QStringLiteral("----NobleLauncher%1").arg(randomHex(16));
    QByteArray body;
    body += QStringLiteral("--%1\r\nContent-Disposition: form-data; name=\"file\"; filename=\"skin.png\"\r\n"
                           "Content-Type: image/png\r\n\r\n").arg(boundary).toUtf8();
    body += data;
    body += "\r\n";
    body += QStringLiteral("--%1\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\n%2\r\n")
                .arg(boundary, slim ? QStringLiteral("slim") : QStringLiteral("classic"))
                .toUtf8();
    body += QStringLiteral("--%1--\r\n").arg(boundary).toUtf8();

    const WebResponse uploaded = webRequest(
        "POST", QUrl(QStringLiteral("https://ely.by/api/legacy/skins")), body,
        QStringLiteral("multipart/form-data; boundary=%1").arg(boundary),
        session.cookies, 30000, true);
    const QJsonObject uploadedJson = QJsonDocument::fromJson(uploaded.body).object();
    const QString uploadError = siteError(uploadedJson);
    if (!uploaded.error.isEmpty() || !uploadError.isEmpty()) {
        if (error) {
            *error = uploadError.isEmpty()
                ? QStringLiteral("Ely.by отклонил скин: HTTP %1").arg(uploaded.status)
                : QStringLiteral("Ely.by отклонил скин: %1").arg(uploadError);
        }
        return false;
    }
    const int skinId = skinIdFrom(uploadedJson);
    if (skinId <= 0) {
        if (error)
            *error = QStringLiteral("Ely.by принял файл, но не вернул ID скина — попробуйте ещё раз");
        return false;
    }

    QUrlQuery wearForm;
    wearForm.addQueryItem(QStringLiteral("skinId"), QString::number(skinId));
    const WebResponse worn = webRequest(
        "PUT", QUrl(QStringLiteral("https://ely.by/api/legacy/users/skin")),
        wearForm.query(QUrl::FullyEncoded).toUtf8(), QStringLiteral("application/x-www-form-urlencoded"),
        session.cookies, 20000, true);
    const QJsonObject wornJson = QJsonDocument::fromJson(worn.body).object();
    const QString wearError = siteError(wornJson);
    if (!worn.error.isEmpty() || !wearError.isEmpty()) {
        if (error) {
            *error = wearError.isEmpty()
                ? QStringLiteral("Скин загружен, но Ely.by не применил его: HTTP %1").arg(worn.status)
                : QStringLiteral("Скин загружен, но Ely.by не применил его: %1").arg(wearError);
        }
        return false;
    }
    Logger::info(QStringLiteral("Скин Ely.by загружен и применён через веб-сессию"));
    return true;
}

} // namespace Noble::ElyBy
