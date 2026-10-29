#pragma once

#include <QObject>
#include <QString>
#include <QPointer>

class QNetworkAccessManager;
class QNetworkReply;
class QUrl;

namespace Noble {

class SkinService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString skinSource READ skinSource NOTIFY skinChanged)
    Q_PROPERTY(QString avatarSource READ avatarSource NOTIFY skinChanged)
    Q_PROPERTY(QString skinName READ skinName NOTIFY skinChanged)
    Q_PROPERTY(bool skinLoading READ skinLoading NOTIFY loadingChanged)
    Q_PROPERTY(QString skinError READ skinError NOTIFY loadingChanged)

public:
    explicit SkinService(QObject *parent = nullptr);

    QString skinSource() const { return m_skinSource; }
    QString avatarSource() const { return m_avatarSource; }
    QString skinName() const { return m_skinName; }
    bool skinLoading() const { return m_loading; }
    QString skinError() const { return m_error; }

    void setAccount(const QString &uuid, const QString &name, const QString &skinUrl);
    void clearAccount();
    void refresh();

signals:
    void skinChanged();
    void loadingChanged();
    void toastRequested(const QString &message, const QString &type);

private:
    void useDefault();
    void loadCached(const QString &uuid);
    void resolveTexture(const QString &name, const QString &uuid, bool force);
    void follow(const QUrl &url, const QString &uuid, int redirectsLeft);
    void fallbackDirect(const QString &name, const QString &uuid);
    bool storeTexture(const QByteArray &data, const QString &uuid);
    void prepareTextures(const QString &pngPath, const QString &uuid);
    void cleanupOld(const QString &uuid, const QString &keepSkin, const QString &keepAvatar);
    void finishWithFallback(const QString &message);
    void setLoading(bool loading, const QString &error = QString());
    QString cacheKey(const QString &uuid) const;

    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    int m_generation = 0;
    QString m_uuid;
    QString m_name;
    QString m_url;
    QString m_skinSource;
    QString m_avatarSource;
    QString m_skinName;
    QString m_cachedPath;
    bool m_loading = false;
    bool m_force = false;
    QString m_error;
};

} // namespace Noble
