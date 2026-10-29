#include "AccountManager.h"

#include "Crypto.h"
#include "ElyByAuth.h"
#include "Env.h"
#include "Json.h"
#include "Logger.h"
#include "Paths.h"
#include "Pure.h"
#include "Settings.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QHash>
#include <QHostAddress>
#include <QJsonDocument>
#include <QMetaObject>
#include <QPair>
#include <QPointer>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QtConcurrent/QtConcurrentRun>

namespace Noble {

AccountManager::AccountManager(QObject *parent)
    : QObject(parent)
{
}

void AccountManager::load()
{
    const QString path = QDir(Paths::dataDir()).filePath(QStringLiteral("accounts.json"));
    QJsonObject state = readJsonObject(path);
    m_accounts.clear();
    m_unreadable.clear();
    m_selectedId = state.value(QStringLiteral("selectedId")).toString();
    const QJsonArray array = state.value(QStringLiteral("accounts")).toArray();
    for (const auto &value : array) {
        const QJsonObject obj = value.toObject();
        const QString provider = obj.value(QStringLiteral("provider")).toString();
        if (provider == QLatin1String("microsoft")) {
            Logger::warn(QStringLiteral("Аккаунт %1: вход через Microsoft удалён — аккаунт скрыт")
                             .arg(obj.value(QStringLiteral("name")).toString()));
            continue;
        }
        AccountEntry account;
        account.id = obj.value(QStringLiteral("id")).toString();
        account.provider = provider;
        account.authMode = obj.value(QStringLiteral("authMode")).toString();
        account.name = obj.value(QStringLiteral("name")).toString();
        account.uuid = obj.value(QStringLiteral("uuid")).toString();
        account.skinUrl = obj.value(QStringLiteral("skinUrl")).toString();
        account.expiresAt = static_cast<qint64>(obj.value(QStringLiteral("expiresAt")).toDouble());
        account.saved = obj.value(QStringLiteral("saved")).toBool(true);
        account.addedAt = static_cast<qint64>(obj.value(QStringLiteral("addedAt")).toDouble());
        const QString accessCipher = obj.value(QStringLiteral("accessToken")).toString();
        const QString refreshCipher = obj.value(QStringLiteral("refreshToken")).toString();
        const QString webCipher = obj.value(QStringLiteral("webRefreshToken")).toString();
        const SecretReadResult access = tryDecryptSecret(accessCipher);
        const SecretReadResult refresh = tryDecryptSecret(refreshCipher);
        const SecretReadResult web = tryDecryptSecret(webCipher);
        if (!access.ok || !refresh.ok) {
            Logger::error(QStringLiteral("Не удалось расшифровать токен аккаунта %1 — потребуется повторный вход")
                              .arg(account.name));
            m_unreadable.insert(account.id, {access.ok ? QString() : accessCipher, refresh.ok ? QString() : refreshCipher});
        }
        account.accessToken = account.provider == QLatin1String("offline") ? (access.value.isEmpty() ? QStringLiteral("0") : access.value) : access.value;
        account.refreshToken = refresh.value;
        account.webRefreshToken = web.value;
        if (account.id.isEmpty())
            account.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_accounts.append(account);
    }
    if (!m_selectedId.isEmpty() && !findConst(m_selectedId))
        m_selectedId.clear();
    emit accountsChanged();
    emit selectionChanged();
}

QVariantList AccountManager::accounts() const
{
    QVariantList list;
    for (const auto &account : m_accounts) {
        QVariantMap map = toPublic(account);
        map.insert(QStringLiteral("active"), account.id == m_selectedId);
        list.append(map);
    }
    return list;
}

QVariantMap AccountManager::selectedAccount() const
{
    const AccountEntry *account = findConst(m_selectedId);
    if (!account)
        return {};
    QVariantMap map = toPublic(*account);
    map.insert(QStringLiteral("active"), true);
    return map;
}

QVariantMap AccountManager::toPublic(const AccountEntry &account) const
{
    return {{QStringLiteral("id"), account.id},
            {QStringLiteral("provider"), account.provider},
            {QStringLiteral("authMode"), account.authMode},
            {QStringLiteral("name"), account.name},
            {QStringLiteral("uuid"), account.uuid},
            {QStringLiteral("skinUrl"), account.skinUrl},
            {QStringLiteral("saved"), account.saved},
            {QStringLiteral("addedAt"), account.addedAt},
            {QStringLiteral("expiresAt"), account.expiresAt}};
}

AccountEntry *AccountManager::find(const QString &id) { return const_cast<AccountEntry *>(findConst(id)); }

const AccountEntry *AccountManager::findConst(const QString &id) const
{
    for (const auto &account : m_accounts) {
        if (account.id == id)
            return &account;
    }
    return nullptr;
}

void AccountManager::persist()
{
    QJsonArray array;
    for (const auto &account : m_accounts) {
        QJsonObject obj{{QStringLiteral("id"), account.id},
                        {QStringLiteral("provider"), account.provider},
                        {QStringLiteral("name"), account.name},
                        {QStringLiteral("uuid"), account.uuid},
                        {QStringLiteral("saved"), account.saved},
                        {QStringLiteral("addedAt"), account.addedAt}};
        if (!account.authMode.isEmpty())
            obj.insert(QStringLiteral("authMode"), account.authMode);
        if (!account.skinUrl.isEmpty())
            obj.insert(QStringLiteral("skinUrl"), account.skinUrl);
        if (account.expiresAt > 0)
            obj.insert(QStringLiteral("expiresAt"), static_cast<double>(account.expiresAt));
        const auto unreadable = m_unreadable.value(account.id);
        obj.insert(QStringLiteral("accessToken"),
                   account.accessToken.isEmpty() ? unreadable.first : encryptSecret(account.accessToken));
        obj.insert(QStringLiteral("refreshToken"),
                   account.refreshToken.isEmpty() ? unreadable.second : encryptSecret(account.refreshToken));
        if (!account.webRefreshToken.isEmpty())
            obj.insert(QStringLiteral("webRefreshToken"), encryptSecret(account.webRefreshToken));
        array.append(obj);
    }
    QJsonObject state{{QStringLiteral("accounts"), array}, {QStringLiteral("selectedId"), m_selectedId.isEmpty() ? QJsonValue::Null : QJsonValue(m_selectedId)}};
    const QString path = QDir(Paths::dataDir()).filePath(QStringLiteral("accounts.json"));
    writeJsonObject(path, state);
}

void AccountManager::selectAccount(const QString &id)
{
    if (id.isEmpty()) {
        m_selectedId.clear();
    } else {
        if (!findConst(id))
            return;
        m_selectedId = id;
    }
    persist();
    emit accountsChanged();
    emit selectionChanged();
    if (m_settings)
        m_settings->setProfileKey(m_selectedId.isEmpty() ? QStringLiteral("default") : m_selectedId);
}

void AccountManager::addOffline(const QString &name)
{
    const QString clean = name.trimmed().left(16);
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9_]{3,16}$"));
    if (!re.match(clean).hasMatch()) {
        emit toastRequested(QStringLiteral("Ник должен содержать 3–16 символов: латиница, цифры или подчёркивание"), QStringLiteral("error"));
        return;
    }
    for (auto &account : m_accounts) {
        if (account.provider == QLatin1String("offline") && account.name == clean) {
            m_selectedId = account.id;
            persist();
            emit accountsChanged();
            emit selectionChanged();
            if (m_settings)
                m_settings->setProfileKey(m_selectedId);
            return;
        }
    }
    AccountEntry account;
    account.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    account.provider = QStringLiteral("offline");
    account.name = clean;
    account.uuid = offlineUuid(clean);
    account.accessToken = QStringLiteral("0");
    account.saved = true;
    account.addedAt = QDateTime::currentMSecsSinceEpoch();
    m_accounts.append(account);
    m_selectedId = account.id;
    persist();
    emit accountsChanged();
    emit selectionChanged();
    emit accountAdded(account.id, account.provider);
    if (m_settings)
        m_settings->setProfileKey(m_selectedId);
    Logger::info(QStringLiteral("Добавлен офлайн-аккаунт %1").arg(clean));
}

void AccountManager::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void AccountManager::setOauthStatus(const QString &status)
{
    if (m_oauthStatus == status)
        return;
    m_oauthStatus = status;
    emit oauthStatusChanged();
}

void AccountManager::addElyByPassword(const QString &username, const QString &password)
{
    if (m_busy)
        return;
    if (username.trimmed().isEmpty() || password.isEmpty()) {
        emit toastRequested(QStringLiteral("Укажите логин и пароль Ely.by"), QStringLiteral("error"));
        return;
    }
    setBusy(true);
    QPointer<AccountManager> guard(this);
    QtConcurrent::run([guard, username, password] {
        ElyAuthResult result = ElyBy::loginWithPassword(username, password);
        QString webRefresh;
        if (result.ok) {
            QString jwt;
            QString webError;
            if (ElyBy::webLogin(username, password, &jwt, &webRefresh, &webError))
                Logger::info(QStringLiteral("Ely.by: веб-сессия для смены скина получена"));
            else
                Logger::warn(QStringLiteral("Ely.by: веб-сессия недоступна — %1").arg(webError));
        }
        if (!guard)
            return;
        QMetaObject::invokeMethod(guard, [guard, result, webRefresh] {
            if (!guard)
                return;
            guard->setBusy(false);
            if (!result.ok) {
                emit guard->toastRequested(result.error, QStringLiteral("error"));
                return;
            }
            AccountEntry *existing = nullptr;
            for (auto &account : guard->m_accounts) {
                if (account.provider == QLatin1String("elyby") && account.uuid == result.profile.uuid) {
                    existing = &account;
                    break;
                }
            }
            AccountEntry account;
            account.id = existing ? existing->id : QUuid::createUuid().toString(QUuid::WithoutBraces);
            account.provider = QStringLiteral("elyby");
            account.authMode = QStringLiteral("password");
            account.name = result.profile.name;
            account.uuid = result.profile.uuid;
            account.accessToken = result.accessToken;
            account.refreshToken = result.clientToken;
            account.webRefreshToken = webRefresh.isEmpty() && existing ? existing->webRefreshToken : webRefresh;
            account.skinUrl = result.profile.skinUrl;
            account.saved = true;
            account.addedAt = existing ? existing->addedAt : QDateTime::currentMSecsSinceEpoch();
            if (existing)
                *existing = account;
            else
                guard->m_accounts.append(account);
            guard->m_selectedId = account.id;
            guard->persist();
            emit guard->accountsChanged();
            emit guard->selectionChanged();
            emit guard->accountAdded(account.id, account.provider);
            emit guard->toastRequested(QStringLiteral("Вход выполнен: %1").arg(account.name), QStringLiteral("success"));
            if (guard->m_settings)
                guard->m_settings->setProfileKey(guard->m_selectedId);
            Logger::info(QStringLiteral("Ely.by: аккаунт %1 сохранён").arg(account.name));
        }, Qt::QueuedConnection);
    });
}

void AccountManager::addElyByOAuth()
{
    if (m_busy)
        return;
    Env &env = Env::instance();
    if (env.elybyClientId().isEmpty() || env.elybyClientSecret().isEmpty()) {
        emit toastRequested(QStringLiteral("Не заданы ключи Ely.by (ELYBY_CLIENT_ID / ELYBY_CLIENT_SECRET) — "
                                           "вход по паролю работает и без них"),
                            QStringLiteral("error"));
        return;
    }
    const QString redirect = ElyBy::resolveRedirectUri(
        m_settings && !m_settings->elybyRedirectUri().isEmpty() ? m_settings->elybyRedirectUri() : env.elybyRedirectUri());
    const QUrl redirectUrl(redirect);
    if (redirectUrl.scheme() != QLatin1String("http") || !redirectUrl.isValid()) {
        emit toastRequested(QStringLiteral("Redirect URI должен быть http-адресом (например http://localhost/redcraft/callback)"),
                            QStringLiteral("error"));
        return;
    }
    const QString host = redirectUrl.host();
    if (host != QLatin1String("localhost") && host != QLatin1String("127.0.0.1")) {
        emit toastRequested(QStringLiteral("Локальный сервер входа может принимать только localhost/127.0.0.1"), QStringLiteral("error"));
        return;
    }
    int port = redirectUrl.port(80);
    if (m_oauthServer) {
        m_oauthServer->deleteLater();
        m_oauthServer = nullptr;
    }
    m_oauthServer = new QTcpServer(this);
    if (!m_oauthServer->listen(QHostAddress::Any, static_cast<quint16>(port))) {
        delete m_oauthServer;
        m_oauthServer = nullptr;
        emit toastRequested(QStringLiteral("Не удалось занять порт %1 для входа Ely.by. Укажите в .env ELYBY_REDIRECT_URI "
                                           "с другим портом (например http://127.0.0.1:28123/callback) и добавьте его в приложение Ely.by.")
                                .arg(port),
                            QStringLiteral("error"));
        return;
    }
    m_oauthRedirect = redirect;
    m_oauthClientId = env.elybyClientId();
    m_oauthClientSecret = env.elybyClientSecret();
    setBusy(true);
    setOauthStatus(QStringLiteral("Ожидание входа в браузере…"));
    connect(m_oauthServer, &QTcpServer::newConnection, this, [this] {
        while (m_oauthServer && m_oauthServer->hasPendingConnections()) {
            QTcpSocket *socket = m_oauthServer->nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                const QByteArray request = socket->readAll();
                const QList<QByteArray> lines = request.split('\n');
                if (lines.isEmpty())
                    return;
                const QList<QByteArray> parts = lines.first().trimmed().split(' ');
                if (parts.size() < 2)
                    return;
                const QUrl url = QUrl::fromEncoded(parts[1]);
                const QUrlQuery query(url);
                const QString code = query.queryItemValue(QStringLiteral("code"));
                const QString error = query.queryItemValue(QStringLiteral("error"));
                const QString description = query.queryItemValue(QStringLiteral("error_description"));
                const QByteArray responseBody = QStringLiteral(
                    "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>Noble Launcher</title></head>"
                    "<body style=\"font-family:Segoe UI,sans-serif;background:#0b0b0d;color:#f2f2f5;display:flex;"
                    "align-items:center;justify-content:center;height:100vh;margin:0\"><div style=\"text-align:center\">"
                    "<div style=\"font-size:40px;color:#e11d2e\">N</div><h2>%1</h2><p style=\"color:#a2a2ad\">"
                    "Можно закрыть эту вкладку и вернуться в лаунчер.</p></div></body></html>")
                                                   .arg(code.isEmpty() ? QStringLiteral("Вход не завершён")
                                                                       : QStringLiteral("Вход выполнен"))
                                                   .toUtf8();
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\nContent-Length: "
                              + QByteArray::number(responseBody.size()) + "\r\n\r\n" + responseBody);
                socket->disconnectFromHost();
                if (!code.isEmpty()) {
                    finishOAuth(code, QString());
                } else if (!error.isEmpty()) {
                    finishOAuth(QString(), description.isEmpty() ? error : description);
                }
            });
        }
    });
    if (m_oauthTimer) {
        m_oauthTimer->stop();
        m_oauthTimer->deleteLater();
    }
    m_oauthTimer = new QTimer(this);
    m_oauthTimer->setSingleShot(true);
    connect(m_oauthTimer, &QTimer::timeout, this, [this] {
        if (m_oauthServer)
            finishOAuth(QString(), QStringLiteral("Истекло время ожидания авторизации Ely.by (5 минут)"));
    });
    m_oauthTimer->start(5 * 60 * 1000);
    QDesktopServices::openUrl(QUrl(ElyBy::authorizeUrl(m_oauthClientId, m_oauthRedirect)));
}

void AccountManager::cancelOAuth()
{
    if (m_oauthServer)
        finishOAuth(QString(), QStringLiteral("Вход отменён"));
}

void AccountManager::finishOAuth(const QString &code, const QString &error)
{
    if (!m_oauthServer)
        return;
    m_oauthServer->close();
    m_oauthServer->deleteLater();
    m_oauthServer = nullptr;
    if (m_oauthTimer) {
        m_oauthTimer->stop();
        m_oauthTimer->deleteLater();
        m_oauthTimer = nullptr;
    }
    const QString clientId = m_oauthClientId;
    const QString clientSecret = m_oauthClientSecret;
    const QString redirect = m_oauthRedirect;
    if (!error.isEmpty()) {
        setBusy(false);
        setOauthStatus(QString());
        emit toastRequested(QStringLiteral("Ely.by: %1").arg(error), QStringLiteral("error"));
        return;
    }
    setOauthStatus(QStringLiteral("Получение токена…"));
    QPointer<AccountManager> guard(this);
    QtConcurrent::run([guard, clientId, clientSecret, redirect, code] {
        ElyAuthResult result = ElyBy::exchangeOAuthCode(clientId, clientSecret, redirect, code);
        if (!guard)
            return;
        QMetaObject::invokeMethod(guard, [guard, result] {
            if (!guard)
                return;
            guard->setBusy(false);
            guard->setOauthStatus(QString());
            if (!result.ok) {
                emit guard->toastRequested(result.error, QStringLiteral("error"));
                return;
            }
            AccountEntry *existing = nullptr;
            for (auto &account : guard->m_accounts) {
                if (account.provider == QLatin1String("elyby") && account.uuid == result.profile.uuid) {
                    existing = &account;
                    break;
                }
            }
            AccountEntry account;
            account.id = existing ? existing->id : QUuid::createUuid().toString(QUuid::WithoutBraces);
            account.provider = QStringLiteral("elyby");
            account.authMode = QStringLiteral("oauth");
            account.name = result.profile.name;
            account.uuid = result.profile.uuid;
            account.accessToken = result.accessToken;
            account.refreshToken = result.refreshToken;
            account.webRefreshToken = existing ? existing->webRefreshToken : QString();
            account.skinUrl = result.profile.skinUrl;
            account.expiresAt = result.expiresAt;
            account.saved = true;
            account.addedAt = existing ? existing->addedAt : QDateTime::currentMSecsSinceEpoch();
            if (existing)
                *existing = account;
            else
                guard->m_accounts.append(account);
            guard->m_selectedId = account.id;
            guard->persist();
            emit guard->accountsChanged();
            emit guard->selectionChanged();
            emit guard->accountAdded(account.id, account.provider);
            emit guard->toastRequested(QStringLiteral("Вход выполнен: %1").arg(account.name), QStringLiteral("success"));
            if (guard->m_settings)
                guard->m_settings->setProfileKey(guard->m_selectedId);
            Logger::info(QStringLiteral("Ely.by: OAuth-аккаунт %1 сохранён").arg(account.name));
        }, Qt::QueuedConnection);
    });
}

void AccountManager::removeAccount(const QString &id)
{
    const bool wasSelected = m_selectedId == id;
    for (int i = 0; i < m_accounts.size(); ++i) {
        if (m_accounts.at(i).id == id) {
            m_accounts.removeAt(i);
            break;
        }
    }
    m_unreadable.remove(id);
    if (wasSelected)
        m_selectedId = m_accounts.isEmpty() ? QString() : m_accounts.first().id;
    persist();
    emit accountsChanged();
    emit selectionChanged();
    if (m_settings)
        m_settings->setProfileKey(m_selectedId.isEmpty() ? QStringLiteral("default") : m_selectedId);
}

void AccountManager::refreshAccount(const QString &id)
{
    AccountEntry *account = find(id);
    if (!account)
        return;
    if (account->provider != QLatin1String("elyby")) {
        emit toastRequested(QStringLiteral("Офлайн-аккаунту обновление не требуется"), QStringLiteral("info"));
        return;
    }
    if (m_busy)
        return;
    setBusy(true);
    const QJsonObject snap = snapshot(id);
    const Env &env = Env::instance();
    const QString clientId = env.elybyClientId();
    const QString clientSecret = env.elybyClientSecret();
    QPointer<AccountManager> guard(this);
    QtConcurrent::run([guard, snap, clientId, clientSecret, id] {
        QString error;
        QJsonObject updated;
        const QJsonObject account = snap;
        const QString mode = account.value(QStringLiteral("authMode")).toString();
        ElyAuthResult result;
        if (mode == QLatin1String("oauth")) {
            result = ElyBy::refreshOAuth(clientId, clientSecret, account.value(QStringLiteral("refreshToken")).toString());
        } else {
            result = ElyBy::refreshYggdrasil(account.value(QStringLiteral("accessToken")).toString(),
                                             account.value(QStringLiteral("refreshToken")).toString());
        }
        if (result.ok) {
            updated = account;
            updated.insert(QStringLiteral("accessToken"), result.accessToken);
            updated.insert(QStringLiteral("refreshToken"), result.refreshToken);
            updated.insert(QStringLiteral("expiresAt"), static_cast<double>(result.expiresAt));
            updated.insert(QStringLiteral("name"), result.profile.name);
            updated.insert(QStringLiteral("uuid"), result.profile.uuid);
            updated.insert(QStringLiteral("skinUrl"), result.profile.skinUrl);
        } else {
            error = result.error;
        }
        if (!guard)
            return;
        QMetaObject::invokeMethod(guard, [guard, updated, error, id] {
            if (!guard)
                return;
            guard->setBusy(false);
            if (updated.isEmpty()) {
                emit guard->toastRequested(error, QStringLiteral("error"));
                return;
            }
            guard->applyRefreshedSnapshot(id, updated);
            emit guard->accountsChanged();
            emit guard->accountRefreshed(id);
            emit guard->toastRequested(QStringLiteral("Сессия Ely.by обновлена"), QStringLiteral("success"));
        }, Qt::QueuedConnection);
    });
}

QJsonObject AccountManager::snapshot(const QString &id) const
{
    const AccountEntry *account = findConst(id);
    if (!account)
        return {};
    return {{QStringLiteral("id"), account->id},
            {QStringLiteral("provider"), account->provider},
            {QStringLiteral("authMode"), account->authMode},
            {QStringLiteral("name"), account->name},
            {QStringLiteral("uuid"), account->uuid},
            {QStringLiteral("accessToken"), account->accessToken},
            {QStringLiteral("refreshToken"), account->refreshToken},
            {QStringLiteral("webRefreshToken"), account->webRefreshToken},
            {QStringLiteral("skinUrl"), account->skinUrl},
            {QStringLiteral("expiresAt"), static_cast<double>(account->expiresAt)}};
}

LaunchCredentials AccountManager::resolveCredentials(const QJsonObject &snapshot, const QString &clientId,
                                                     const QString &clientSecret, QString *error,
                                                     QJsonObject *updatedSnapshot)
{
    auto offline = [](const QString &name) {
        LaunchCredentials credentials;
        credentials.name = name.isEmpty() ? QStringLiteral("Player") : name;
        credentials.uuid = offlineUuid(credentials.name).remove(QLatin1Char('-'));
        credentials.accessToken = QStringLiteral("0");
        credentials.userType = QStringLiteral("legacy");
        credentials.provider = QStringLiteral("offline");
        return credentials;
    };

    if (snapshot.isEmpty())
        return offline(QStringLiteral("Player"));

    LaunchCredentials credentials;
    credentials.name = snapshot.value(QStringLiteral("name")).toString();
    credentials.uuid = snapshot.value(QStringLiteral("uuid")).toString().remove(QLatin1Char('-'));
    credentials.accessToken = snapshot.value(QStringLiteral("accessToken")).toString();
    credentials.userType = QStringLiteral("mojang");
    credentials.provider = snapshot.value(QStringLiteral("provider")).toString();

    if (credentials.provider == QLatin1String("offline")) {
        credentials.accessToken = QStringLiteral("0");
        credentials.userType = QStringLiteral("legacy");
        return credentials;
    }

    const QString mode = snapshot.value(QStringLiteral("authMode")).toString();
    const qint64 expiresAt = static_cast<qint64>(snapshot.value(QStringLiteral("expiresAt")).toDouble());
    bool expired = expiresAt > 0 && expiresAt - 60000 < QDateTime::currentMSecsSinceEpoch();
    if (!expired && mode != QLatin1String("oauth")) {
        bool networkError = false;
        const bool valid = ElyBy::validateYggdrasil(credentials.accessToken,
                                                    snapshot.value(QStringLiteral("refreshToken")).toString(), &networkError);
        if (!networkError)
            expired = !valid;
    }
    const QString refreshToken = snapshot.value(QStringLiteral("refreshToken")).toString();
    if (expired) {
        if (refreshToken.isEmpty()) {
            if (error)
                *error = QStringLiteral("Сессия Ely.by истекла — войдите в аккаунт заново");
            return {};
        }
        ElyAuthResult result = mode == QLatin1String("oauth")
            ? ElyBy::refreshOAuth(clientId, clientSecret, refreshToken)
            : ElyBy::refreshYggdrasil(credentials.accessToken, refreshToken);
        if (!result.ok) {
            if (error)
                *error = QStringLiteral("Сессия Ely.by истекла, а обновить её не удалось: %1").arg(result.error);
            return {};
        }
        credentials.name = result.profile.name;
        credentials.uuid = result.profile.uuid.remove(QLatin1Char('-'));
        credentials.accessToken = result.accessToken;
        if (updatedSnapshot) {
            *updatedSnapshot = snapshot;
            updatedSnapshot->insert(QStringLiteral("accessToken"), result.accessToken);
            updatedSnapshot->insert(QStringLiteral("refreshToken"), result.refreshToken);
            updatedSnapshot->insert(QStringLiteral("expiresAt"), static_cast<double>(result.expiresAt));
            updatedSnapshot->insert(QStringLiteral("name"), result.profile.name);
            updatedSnapshot->insert(QStringLiteral("uuid"), result.profile.uuid);
            updatedSnapshot->insert(QStringLiteral("skinUrl"), result.profile.skinUrl);
        }
    }
    if (credentials.accessToken.isEmpty()) {
        if (error)
            *error = QStringLiteral("Нет сохранённого токена для аккаунта %1 — войдите заново").arg(credentials.name);
        return {};
    }
    return credentials;
}

void AccountManager::applyRefreshedSnapshot(const QString &id, const QJsonObject &snapshot)
{
    AccountEntry *account = find(id);
    if (!account)
        return;
    account->accessToken = snapshot.value(QStringLiteral("accessToken")).toString(account->accessToken);
    account->refreshToken = snapshot.value(QStringLiteral("refreshToken")).toString(account->refreshToken);
    account->webRefreshToken = snapshot.value(QStringLiteral("webRefreshToken")).toString(account->webRefreshToken);
    account->expiresAt = static_cast<qint64>(snapshot.value(QStringLiteral("expiresAt")).toDouble());
    account->name = snapshot.value(QStringLiteral("name")).toString(account->name);
    account->uuid = snapshot.value(QStringLiteral("uuid")).toString(account->uuid);
    account->skinUrl = snapshot.value(QStringLiteral("skinUrl")).toString(account->skinUrl);
    persist();
}

} // namespace Noble
