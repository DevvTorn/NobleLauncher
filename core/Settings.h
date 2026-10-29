#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QVariantList>
#include <QTimer>

#include "Json.h"

namespace Noble {

class Settings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString javaMode READ javaMode WRITE setJavaMode NOTIFY changed)
    Q_PROPERTY(QString javaPath READ javaPath WRITE setJavaPath NOTIFY changed)
    Q_PROPERTY(QString lang READ lang WRITE setLang NOTIFY changed)
    Q_PROPERTY(bool showConsole READ showConsole WRITE setShowConsole NOTIFY changed)
    Q_PROPERTY(bool animations READ animations WRITE setAnimations NOTIFY changed)
    Q_PROPERTY(bool introEnabled READ introEnabled WRITE setIntroEnabled NOTIFY changed)
    Q_PROPERTY(bool introSound READ introSound WRITE setIntroSound NOTIFY changed)
    Q_PROPERTY(QString fontMode READ fontMode WRITE setFontMode NOTIFY changed)
    Q_PROPERTY(QString activeFontId READ activeFontId WRITE setActiveFontId NOTIFY changed)
    Q_PROPERTY(bool friendsSteam READ friendsSteam WRITE setFriendsSteam NOTIFY changed)
    Q_PROPERTY(bool nobleBuilds READ nobleBuilds WRITE setNobleBuilds NOTIFY changed)
    Q_PROPERTY(QString profileIcon READ profileIcon WRITE setProfileIcon NOTIFY changed)
    Q_PROPERTY(int ram READ ram WRITE setRam NOTIFY changed)
    Q_PROPERTY(int concurrency READ concurrency WRITE setConcurrency NOTIFY changed)
    Q_PROPERTY(bool autoUpdate READ autoUpdate WRITE setAutoUpdate NOTIFY changed)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY changed)
    Q_PROPERTY(QString density READ density WRITE setDensity NOTIFY changed)
    Q_PROPERTY(double fontScale READ fontScale WRITE setFontScale NOTIFY changed)
    Q_PROPERTY(QString accent READ accent WRITE setAccent NOTIFY changed)
    Q_PROPERTY(QString instancesDir READ instancesDir NOTIFY changed)
    Q_PROPERTY(QString runtimeDir READ runtimeDir NOTIFY changed)
    Q_PROPERTY(QString elybyRedirectUri READ elybyRedirectUri WRITE setElybyRedirectUri NOTIFY changed)
    Q_PROPERTY(bool optimizeHighPriority READ optimizeHighPriority WRITE setOptimizeHighPriority NOTIFY changed)
    Q_PROPERTY(QString activeBackgroundId READ activeBackgroundId WRITE setActiveBackgroundId NOTIFY changed)
    Q_PROPERTY(int backgroundBlur READ backgroundBlur WRITE setBackgroundBlur NOTIFY changed)
    Q_PROPERTY(int backgroundDarken READ backgroundDarken WRITE setBackgroundDarken NOTIFY changed)
    Q_PROPERTY(int backgroundSaturation READ backgroundSaturation WRITE setBackgroundSaturation NOTIFY changed)
    Q_PROPERTY(QString profileKey READ profileKey NOTIFY changed)

public:
    explicit Settings(QObject *parent = nullptr);
    ~Settings() override;

    void load();
    Q_INVOKABLE void saveNow();

    QString javaMode() const { return m_data.value(QStringLiteral("javaMode")).toString(); }
    QString javaPath() const { return m_data.value(QStringLiteral("javaPath")).toString(); }
    QString lang() const { return m_data.value(QStringLiteral("lang")).toString(); }
    bool showConsole() const { return m_data.value(QStringLiteral("showConsole")).toBool(); }
    bool animations() const { return m_data.value(QStringLiteral("animations")).toBool(); }
    bool introEnabled() const { return m_data.value(QStringLiteral("introEnabled")).toBool(); }
    bool introSound() const { return m_data.value(QStringLiteral("introSound")).toBool(); }
    QString fontMode() const { return m_data.value(QStringLiteral("fontMode")).toString(); }
    QString activeFontId() const { return m_data.value(QStringLiteral("activeFontId")).toString(); }
    bool friendsSteam() const { return m_data.value(QStringLiteral("friendsSteam")).toBool(); }
    bool nobleBuilds() const { return m_data.value(QStringLiteral("nobleBuilds")).toBool(); }
    QString profileIcon() const { return m_data.value(QStringLiteral("profileIcon")).toString(); }
    int ram() const { return m_data.value(QStringLiteral("ram")).toInt(); }
    int concurrency() const { return m_data.value(QStringLiteral("concurrency")).toInt(); }
    bool autoUpdate() const { return m_data.value(QStringLiteral("autoUpdate")).toBool(); }
    QString theme() const { return m_data.value(QStringLiteral("theme")).toString(); }
    QString density() const { return m_data.value(QStringLiteral("density")).toString(); }
    double fontScale() const { return m_data.value(QStringLiteral("fontScale")).toDouble(); }
    QString accent() const { return m_data.value(QStringLiteral("accent")).toString(); }
    QString instancesDir() const { return m_data.value(QStringLiteral("instancesDir")).toString(); }
    QString runtimeDir() const { return m_data.value(QStringLiteral("runtimeDir")).toString(); }
    QString elybyRedirectUri() const { return m_data.value(QStringLiteral("elybyRedirectUri")).toString(); }
    bool optimizeHighPriority() const
    {
        return m_data.value(QStringLiteral("optimize")).toObject().value(QStringLiteral("highPriorityGame")).toBool();
    }
    QString profileKey() const { return m_profileKey; }
    QString activeBackgroundId() const { return appearance().value(QStringLiteral("activeBackgroundId")).toString(); }
    int backgroundBlur() const { return appearance().value(QStringLiteral("blur")).toInt(); }
    int backgroundDarken() const { return appearance().value(QStringLiteral("darken")).toInt(); }
    int backgroundSaturation() const { return appearance().value(QStringLiteral("saturation")).toInt(); }

    void setJavaMode(const QString &value);
    void setJavaPath(const QString &value);
    void setLang(const QString &value);
    void setShowConsole(bool value);
    void setAnimations(bool value);
    void setIntroEnabled(bool value);
    void setIntroSound(bool value);
    void setFontMode(const QString &value);
    void setActiveFontId(const QString &value);
    void setFriendsSteam(bool value);
    void setNobleBuilds(bool value);
    void setProfileIcon(const QString &value);
    void setRam(int value);
    void setConcurrency(int value);
    void setAutoUpdate(bool value);
    void setTheme(const QString &value);
    void setDensity(const QString &value);
    void setFontScale(double value);
    void setAccent(const QString &value);
    void setElybyRedirectUri(const QString &value);
    void setOptimizeHighPriority(bool value);
    void setActiveBackgroundId(const QString &value);
    void setBackgroundBlur(int value);
    void setBackgroundDarken(int value);
    void setBackgroundSaturation(int value);

    void setDirectories(const QString &instancesDir, const QString &runtimeDir);

    void setProfileKey(const QString &key);
    QJsonObject appearance() const;
    QJsonObject appearanceProfiles() const { return m_data.value(QStringLiteral("appearanceProfiles")).toObject(); }
    void setAppearanceProfiles(const QJsonObject &profiles);
    void clearBackgroundFromProfiles(const QString &backgroundId);

    QJsonArray backgrounds() const { return m_data.value(QStringLiteral("backgrounds")).toArray(); }
    void setBackgrounds(const QJsonArray &list);
    QJsonArray customFonts() const { return m_data.value(QStringLiteral("customFonts")).toArray(); }
    void setCustomFonts(const QJsonArray &list);

    Q_INVOKABLE void reset();
    Q_INVOKABLE void reload();
    Q_INVOKABLE void save();
    Q_INVOKABLE void replayIntro();
    Q_INVOKABLE QString defaultInstancesDir() const;
    Q_INVOKABLE QString defaultRuntimeDir() const;

signals:
    void changed();
    void introReplayRequested();

private:
    void setValue(const QString &key, const QJsonValue &value, bool immediate = false);
    void scheduleSave();
    QJsonObject defaults();
    void normalize();

    JsonStore m_store;
    QJsonObject m_data;
    QTimer m_saveTimer;
    QString m_profileKey = QStringLiteral("default");
    bool m_loaded = false;
};

} // namespace Noble
