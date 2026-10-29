#pragma once

#include <QString>

namespace Noble {

struct ElyProfile {
    QString uuid;
    QString name;
    QString skinUrl;
};

struct ElyAuthResult {
    bool ok = false;
    QString error;
    QString accessToken;
    QString clientToken;
    QString refreshToken;
    qint64 expiresAt = 0;
    ElyProfile profile;
};

namespace ElyBy {

QString defaultRedirectUri();
QString resolveRedirectUri(const QString &overrideUri);
QString authorizeUrl(const QString &clientId, const QString &redirectUri);
ElyAuthResult loginWithPassword(const QString &username, const QString &password);
ElyAuthResult refreshYggdrasil(const QString &accessToken, const QString &clientToken);
bool validateYggdrasil(const QString &accessToken, const QString &clientToken, bool *networkError = nullptr);
ElyAuthResult exchangeOAuthCode(const QString &clientId, const QString &clientSecret, const QString &redirectUri,
                                const QString &code);
ElyAuthResult refreshOAuth(const QString &clientId, const QString &clientSecret, const QString &refreshToken);
ElyAuthResult profileFromAccessToken(const QString &accessToken);

/** Веб-сессия Ely.by нужна для смены скина: публичного API загрузки у Ely.by нет. */
bool webLogin(const QString &login, const QString &password, QString *jwt, QString *refreshToken, QString *error = nullptr);
bool uploadSkinWeb(const QString &webRefreshToken, const QString &fallbackJwt, const QString &fallbackRefreshToken,
                   const QString &filePath, bool slim, QString *error = nullptr);

} // namespace ElyBy

} // namespace Noble
