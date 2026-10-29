#include "Json.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace Noble {

JsonStore::JsonStore(QString filePath, QJsonObject defaults)
    : m_filePath(std::move(filePath)), m_defaults(std::move(defaults))
{
}

QJsonObject JsonStore::read()
{
    if (m_loaded)
        return m_cache;
    m_loaded = true;
    QFile file(m_filePath);
    QJsonObject parsed;
    if (file.open(QIODevice::ReadOnly)) {
        const auto doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject())
            parsed = doc.object();
    }
    m_cache = m_defaults;
    for (auto it = parsed.constBegin(); it != parsed.constEnd(); ++it)
        m_cache.insert(it.key(), it.value());
    return m_cache;
}

QJsonObject JsonStore::write(const QJsonObject &patch)
{
    QJsonObject next = read();
    for (auto it = patch.constBegin(); it != patch.constEnd(); ++it)
        next.insert(it.key(), it.value());
    return persist(next);
}

QJsonObject JsonStore::replace(const QJsonObject &value)
{
    return persist(value);
}

QJsonObject JsonStore::persist(const QJsonObject &next)
{
    m_cache = next;
    m_loaded = true;
    QString error;
    if (!writeJsonObject(m_filePath, next, &error))
        qWarning("JsonStore: не удалось записать %s: %s", qPrintable(m_filePath), qPrintable(error));
    return next;
}

QJsonObject readJsonObject(const QString &path, bool *ok)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (ok)
            *ok = false;
        return {};
    }
    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (ok)
        *ok = doc.isObject();
    return doc.object();
}

bool writeJsonObject(const QString &path, const QJsonObject &obj, QString *error)
{
    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

QJsonDocument readJsonDocument(const QString &path, bool *ok)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (ok)
            *ok = false;
        return {};
    }
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (ok)
        *ok = parseError.error == QJsonParseError::NoError;
    return doc;
}

bool writeJsonDocument(const QString &path, const QJsonDocument &doc, QString *error)
{
    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

QStringList jsonArrayToStringList(const QJsonValue &value)
{
    QStringList result;
    if (value.isArray()) {
        for (const auto &item : value.toArray()) {
            if (item.isString())
                result << item.toString();
        }
    } else if (value.isString()) {
        result << value.toString();
    }
    return result;
}

} // namespace Noble
