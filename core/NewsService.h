#pragma once

#include <QObject>
#include <QVariantList>
#include <QJsonArray>

namespace Noble {

class NewsService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

public:
    explicit NewsService(QObject *parent = nullptr);

    QVariantList items() const;
    bool loading() const { return m_loading; }
    void load(bool force = false);

signals:
    void itemsChanged();
    void loadingChanged();

private:
    QJsonArray m_items;
    qint64 m_fetchedAt = 0;
    bool m_loading = false;
};

} // namespace Noble
