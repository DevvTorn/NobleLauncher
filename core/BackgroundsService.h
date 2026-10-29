#pragma once

#include <QObject>
#include <QJsonArray>
#include <QVariantList>
#include <QStringList>

namespace Noble {

class Settings;

class BackgroundsService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)

public:
    explicit BackgroundsService(QObject *parent = nullptr);

    void init(Settings *settings);
    QVariantList items() const;
    QString sourceFor(const QString &id) const;
    QString thumbFor(const QString &id) const;

    void importFiles(const QStringList &paths);
    Q_INVOKABLE void remove(const QString &id);
    void saveThumb(const QString &id, const QImage &image);
    Q_INVOKABLE void openFolder();

signals:
    void changed();
    void toastRequested(const QString &message, const QString &type);

private:
    QJsonArray m_items;
    Settings *m_settings = nullptr;
};

} // namespace Noble
