#pragma once

#include <QObject>
#include <QString>

namespace Noble {

class Settings;
class AccountManager;

class ProfileService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString profileIconSource READ profileIconSource NOTIFY profileIconChanged)

public:
    explicit ProfileService(QObject *parent = nullptr);

    void init(Settings *settings, AccountManager *accounts);
    QString profileIconSource() const;
    void setProfileIcon(const QString &sourcePath);
    void uploadSkin(const QString &accountId, const QString &filePath, bool slim);

signals:
    void profileIconChanged();
    void skinUploaded(const QString &message);
    void toastRequested(const QString &message, const QString &type);

private:
    bool copyImage(const QString &source, const QString &prefix, QString *fileName);
    Settings *m_settings = nullptr;
    AccountManager *m_accounts = nullptr;
};

} // namespace Noble
