#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>
#include <QVector>

#include <functional>

class QTcpServer;
class QTimer;

namespace Noble {

class Settings;

struct AccountEntry {
    QString id;
    QString provider;
    QString authMode;
    QString name;
    QString uuid;
    QString accessToken;
    QString refreshToken;
    QString webRefreshToken;
    QString skinUrl;
    qint64 expiresAt = 0;
    bool saved = true;
    qint64 addedAt = 0;
};

struct LaunchCredentials {
    QString name;
    QString uuid;
    QString accessToken;
    QString userType;
    QString provider;
};

class AccountManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList accounts READ accounts NOTIFY accountsChanged)
    Q_PROPERTY(QString selectedId READ selectedId NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedAccount READ selectedAccount NOTIFY accountsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString oauthStatus READ oauthStatus NOTIFY oauthStatusChanged)

public:
    explicit AccountManager(QObject *parent = nullptr);

    void load();
    QVariantList accounts() const;
    QString selectedId() const { return m_selectedId; }
    QVariantMap selectedAccount() const;
    bool busy() const { return m_busy; }
    QString oauthStatus() const { return m_oauthStatus; }

    AccountEntry *find(const QString &id);
    const AccountEntry *findConst(const QString &id) const;

    Q_INVOKABLE void selectAccount(const QString &id);
    Q_INVOKABLE void addOffline(const QString &name);
    Q_INVOKABLE void addElyByPassword(const QString &username, const QString &password);
    Q_INVOKABLE void addElyByOAuth();
    Q_INVOKABLE void cancelOAuth();
    Q_INVOKABLE void removeAccount(const QString &id);
    Q_INVOKABLE void refreshAccount(const QString &id);

    QJsonObject snapshot(const QString &id) const;

    /** Выполняется в рабочем потоке: обновляет сессию и возвращает данные для запуска. */
    static LaunchCredentials resolveCredentials(const QJsonObject &snapshot, const QString &clientId,
                                                const QString &clientSecret, QString *error,
                                                QJsonObject *updatedSnapshot);
    void applyRefreshedSnapshot(const QString &id, const QJsonObject &snapshot);

    void setSettings(Settings *settings) { m_settings = settings; }

signals:
    void accountsChanged();
    void selectionChanged();
    void busyChanged();
    void oauthStatusChanged();
    void toastRequested(const QString &message, const QString &type);
    void accountAdded(const QString &id, const QString &provider);
    void accountRefreshed(const QString &id);

private:
    void persist();
    void setBusy(bool busy);
    void setOauthStatus(const QString &status);
    void finishOAuth(const QString &code, const QString &error);
    void addElyByResult(bool oauth, const QString &username, const QString &password, const QString &clientId,
                        const QString &clientSecret, const QString &redirectUri, const QString &code);
    QVariantMap toPublic(const AccountEntry &account) const;

    QVector<AccountEntry> m_accounts;
    QHash<QString, QPair<QString, QString>> m_unreadable; // id -> (accessCipher, refreshCipher)
    QString m_selectedId;
    Settings *m_settings = nullptr;
    bool m_busy = false;
    QString m_oauthStatus;

    QTcpServer *m_oauthServer = nullptr;
    QTimer *m_oauthTimer = nullptr;
    QString m_oauthRedirect;
    QString m_oauthClientId;
    QString m_oauthClientSecret;
};

} // namespace Noble
