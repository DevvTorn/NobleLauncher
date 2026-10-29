#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

namespace Noble {

class Settings;

class InstanceManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList instances READ instances NOTIFY instancesChanged)
    Q_PROPERTY(QString selectedInstanceId READ selectedInstanceId WRITE setSelectedInstanceId NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedInstance READ selectedInstance NOTIFY selectionChanged)

public:
    explicit InstanceManager(QObject *parent = nullptr);

    void init(Settings *settings);
    void reload();
    QVariantList instances() const;
    QVariantMap selectedInstance() const;
    QString selectedInstanceId() const { return m_selectedId; }
    void setSelectedInstanceId(const QString &id);

    QJsonObject get(const QString &id) const;
    QString root() const;
    QString runtimeRoot() const;
    QString modsDir(const QJsonObject &instance) const;

    Q_INVOKABLE QVariantMap create(const QVariantMap &data);
    Q_INVOKABLE QVariantMap update(const QString &id, const QVariantMap &patch);
    Q_INVOKABLE void remove(const QString &id, bool deleteFiles);
    Q_INVOKABLE QVariantMap clone(const QString &id, const QString &name = QString());
    Q_INVOKABLE bool exportArchive(const QString &id, const QString &targetPath);
    Q_INVOKABLE QVariantMap importArchive(const QString &zipPath);
    Q_INVOKABLE QString setIcon(const QString &id, const QString &sourcePath);
    Q_INVOKABLE void addMod(const QString &id, const QVariantMap &mod);
    Q_INVOKABLE void removeMod(const QString &id, const QString &modId);
    Q_INVOKABLE void toggleMod(const QString &id, const QString &modId);
    Q_INVOKABLE void replaceMods(const QString &id, const QVariantList &mods);
    Q_INVOKABLE void markLaunched(const QString &id);
    Q_INVOKABLE QVariantMap migrationInfo() const;
    Q_INVOKABLE int rollbackMigration();

    const QVector<QJsonObject> &all() const { return m_instances; }

signals:
    void instancesChanged();
    void selectionChanged();
    void toastRequested(const QString &message, const QString &type);
    void migrationHappened(int count);

private:
    void writeMeta(QJsonObject &instance) const;
    QJsonObject readMeta(const QString &dir) const;
    QString uniqueDir(const QString &root, const QString &base) const;
    void migrateLatinDirs();
    QString migrationLogPath() const;
    QString m_root;
    QString m_runtime;
    QVector<QJsonObject> m_instances;
    QString m_selectedId;
    Settings *m_settings = nullptr;
};

} // namespace Noble
