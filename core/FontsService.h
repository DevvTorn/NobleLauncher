#pragma once

#include <QObject>
#include <QJsonArray>
#include <QVariantList>
#include <QStringList>
#include <QHash>

namespace Noble {

class Settings;

class FontsService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString family READ family NOTIFY familyChanged)
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)

public:
    explicit FontsService(QObject *parent = nullptr);

    void init(Settings *settings);
    void apply();
    QString family() const { return m_family; }
    QVariantList items() const;
    Q_INVOKABLE void importFiles(const QStringList &paths);
    Q_INVOKABLE void remove(const QString &id);
    Q_INVOKABLE void openFolder();

signals:
    void changed();
    void familyChanged();
    void toastRequested(const QString &message, const QString &type);

private:
    QString familyForId(const QString &id);
    QString loadFontFile(const QString &path);

    Settings *m_settings = nullptr;
    QJsonArray m_items;
    QString m_family;
    QHash<QString, QString> m_loadedFamilies;
    QString m_pixelFamily;
};

} // namespace Noble
