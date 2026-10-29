#pragma once

#include <QJsonObject>
#include <QVariantList>

namespace Noble {

class ModsScanner
{
public:
    static QVariantMap scanInstance(const QJsonObject &instance);
    static QString kindDir(const QString &kind);
};

} // namespace Noble
