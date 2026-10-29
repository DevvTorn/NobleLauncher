#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace Noble {

class Env
{
public:
    static Env &instance();

    void load();
    QString value(const QString &key) const;
    QString trimmed(const QString &key) const;
    bool has(const QString &key) const;
    QStringList missingRequired() const;
    bool anyFileExists() const;
    QStringList searchedFiles() const;
    QString ensureExample();
    QString ensureNextToExecutable();

    QString elybyClientId() const { return trimmed(QStringLiteral("ELYBY_CLIENT_ID")); }
    QString elybyClientSecret() const { return trimmed(QStringLiteral("ELYBY_CLIENT_SECRET")); }
    QString elybyRedirectUri() const { return trimmed(QStringLiteral("ELYBY_REDIRECT_URI")); }

    static const QStringList &requiredKeys();

private:
    QJsonObject m_values;
    QStringList m_files;
    QString m_examplePath;
};

} // namespace Noble
