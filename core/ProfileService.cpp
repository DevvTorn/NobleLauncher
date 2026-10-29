#include "ProfileService.h"

#include "AccountManager.h"
#include "ElyByAuth.h"
#include "Logger.h"
#include "Paths.h"
#include "Pure.h"
#include "Settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPointer>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

namespace Noble {

ProfileService::ProfileService(QObject *parent)
    : QObject(parent)
{
}

void ProfileService::init(Settings *settings, AccountManager *accounts)
{
    m_settings = settings;
    m_accounts = accounts;
    emit profileIconChanged();
}

QString ProfileService::profileIconSource() const
{
    if (!m_settings)
        return QString();
    const QString icon = m_settings->profileIcon();
    if (icon.isEmpty())
        return QString();
    const QString path = QDir(Paths::profileIconsDir()).filePath(icon);
    if (!QFileInfo::exists(path))
        return QString();
    return QUrl::fromLocalFile(path).toString();
}

bool ProfileService::copyImage(const QString &source, const QString &prefix, QString *fileName)
{
    QFile file(source);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray content = file.readAll();
    if (content.isEmpty() || content.size() > 4 * 1024 * 1024) {
        emit toastRequested(QStringLiteral("Иконка пуста или больше 4 МБ"), QStringLiteral("error"));
        return false;
    }
    const QString ext = detectImageExt(content.left(12));
    if (ext.isEmpty()) {
        emit toastRequested(QStringLiteral("Поддерживаются PNG, JPG и WEBP"), QStringLiteral("error"));
        return false;
    }
    QDir().mkpath(Paths::profileIconsDir());
    const QString name = prefix + ext;
    QFile out(QDir(Paths::profileIconsDir()).filePath(name));
    if (!out.open(QIODevice::WriteOnly))
        return false;
    out.write(content);
    out.close();
    *fileName = name;
    return true;
}

void ProfileService::setProfileIcon(const QString &sourcePath)
{
    if (!m_settings)
        return;
    const QString current = m_settings->profileIcon();
    if (!current.isEmpty())
        QFile::remove(QDir(Paths::profileIconsDir()).filePath(current));
    if (sourcePath.isEmpty()) {
        m_settings->setProfileIcon(QString());
        emit profileIconChanged();
        return;
    }
    QString fileName;
    if (!copyImage(sourcePath, QStringLiteral("icon"), &fileName))
        return;
    m_settings->setProfileIcon(fileName);
    Logger::info(QStringLiteral("Иконка профиля обновлена"));
    emit profileIconChanged();
}

void ProfileService::uploadSkin(const QString &accountId, const QString &filePath, bool slim)
{
    if (!m_accounts)
        return;
    const AccountEntry *account = m_accounts->findConst(accountId);
    if (!account || account->provider != QLatin1String("elyby")) {
        emit toastRequested(QStringLiteral("Смена скина доступна только для аккаунта Ely.by"), QStringLiteral("error"));
        return;
    }
    const QString webRefreshToken = account->webRefreshToken;
    const QString accessToken = account->accessToken;
    const QString refreshToken = account->refreshToken;
    QPointer<ProfileService> guard(this);
    QtConcurrent::run([guard, webRefreshToken, accessToken, refreshToken, filePath, slim] {
        QString error;
        const bool ok = ElyBy::uploadSkinWeb(webRefreshToken, accessToken, refreshToken, filePath, slim, &error);
        if (!guard)
            return;
        QMetaObject::invokeMethod(guard, [guard, ok, error] {
            if (!guard)
                return;
            if (ok) {
                emit guard->skinUploaded(QStringLiteral("Скин обновлён — зайдите в игру, чтобы увидеть его"));
                emit guard->toastRequested(QStringLiteral("Скин обновлён"), QStringLiteral("success"));
            } else {
                emit guard->toastRequested(error, QStringLiteral("error"));
            }
        }, Qt::QueuedConnection);
    });
}

} // namespace Noble
