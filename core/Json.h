#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QString>
#include <QStringList>

namespace Noble {

class JsonStore
{
public:
    JsonStore(QString filePath, QJsonObject defaults);

    QString filePath() const { return m_filePath; }
    void setFilePath(const QString &path) { m_filePath = path; }

    QJsonObject read();
    QJsonObject write(const QJsonObject &patch);
    QJsonObject replace(const QJsonObject &value);

private:
    QJsonObject persist(const QJsonObject &next);
    QString m_filePath;
    QJsonObject m_defaults;
    QJsonObject m_cache;
    bool m_loaded = false;
};

QJsonObject readJsonObject(const QString &path, bool *ok = nullptr);
bool writeJsonObject(const QString &path, const QJsonObject &obj, QString *error = nullptr);
QJsonDocument readJsonDocument(const QString &path, bool *ok = nullptr);
bool writeJsonDocument(const QString &path, const QJsonDocument &doc, QString *error = nullptr);

QStringList jsonArrayToStringList(const QJsonValue &value);

} // namespace Noble
