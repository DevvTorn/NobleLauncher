#pragma once

#include <QObject>
#include <QVariantMap>

namespace Noble {

class UpdaterService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap status READ status NOTIFY statusChanged)

public:
    explicit UpdaterService(QObject *parent = nullptr);

    QVariantMap status() const { return m_status; }
    void check(bool manual = false);

signals:
    void statusChanged();

private:
    QVariantMap m_status;
};

} // namespace Noble
