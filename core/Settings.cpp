#include "Settings.h"

#include "Logger.h"
#include "Paths.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>

#include <algorithm>

namespace Noble {

static QJsonObject makeDefaultAppearance()
{
    return QJsonObject{{QStringLiteral("activeBackgroundId"), QJsonValue::Null},
                       {QStringLiteral("blur"), 0},
                       {QStringLiteral("darken"), 35},
                       {QStringLiteral("saturation"), 100}};
}

Settings::Settings(QObject *parent)
    : QObject(parent), m_store(QDir(Paths::dataDir()).filePath(QStringLiteral("settings.json")), defaults())
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(350);
    connect(&m_saveTimer, &QTimer::timeout, this, &Settings::saveNow);
}

Settings::~Settings()
{
    saveNow();
}

QJsonObject Settings::defaults()
{
    return QJsonObject{
        {QStringLiteral("javaMode"), QStringLiteral("auto")},
        {QStringLiteral("javaPath"), QString()},
        {QStringLiteral("lang"), QStringLiteral("ru")},
        {QStringLiteral("showConsole"), true},
        {QStringLiteral("animations"), true},
        {QStringLiteral("introEnabled"), true},
        {QStringLiteral("introSound"), false},
        {QStringLiteral("fontMode"), QStringLiteral("minecraft")},
        {QStringLiteral("activeFontId"), QJsonValue::Null},
        {QStringLiteral("friendsSteam"), false},
        {QStringLiteral("nobleBuilds"), true},
        {QStringLiteral("profileIcon"), QJsonValue::Null},
        {QStringLiteral("ram"), 4096},
        {QStringLiteral("instancesDir"), QString()},
        {QStringLiteral("runtimeDir"), QString()},
        {QStringLiteral("elybyRedirectUri"), QString()},
        {QStringLiteral("concurrency"), 8},
        {QStringLiteral("autoUpdate"), true},
        {QStringLiteral("theme"), QStringLiteral("dark")},
        {QStringLiteral("density"), QStringLiteral("comfortable")},
        {QStringLiteral("fontScale"), 1.0},
        {QStringLiteral("accent"), QStringLiteral("#e11d2e")},
        {QStringLiteral("backgrounds"), QJsonArray()},
        {QStringLiteral("customFonts"), QJsonArray()},
        {QStringLiteral("appearanceProfiles"), QJsonObject()},
        {QStringLiteral("optimize"), QJsonObject{{QStringLiteral("highPriorityGame"), false}}},
    };
}

void Settings::load()
{
    m_data = m_store.read();
    normalize();
    m_loaded = true;
    saveNow();
}

void Settings::normalize()
{
    const bool hadIntroEnabled = m_data.contains(QStringLiteral("introEnabled"));
    const bool introShown = m_data.value(QStringLiteral("introShown")).toBool();

    const QJsonObject d = defaults();
    for (auto it = d.constBegin(); it != d.constEnd(); ++it) {
        if (!m_data.contains(it.key()))
            m_data.insert(it.key(), it.value());
    }

    static const QStringList secretKeys = {QStringLiteral("microsoftClientId"), QStringLiteral("elybyClientId"),
                                           QStringLiteral("elybyClientSecret"), QStringLiteral("curseforgeApiKey")};
    bool purge = false;
    for (const auto &key : secretKeys) {
        if (m_data.contains(key)) {
            m_data.remove(key);
            purge = true;
        }
    }
    if (purge)
        Logger::info(QStringLiteral("settings.json: удалены устаревшие ключи из открытого конфига"));

    if (!hadIntroEnabled && introShown)
        m_data.insert(QStringLiteral("introEnabled"), false);

    if (!m_data.value(QStringLiteral("fontMode")).toString().isEmpty()) {
        const QString mode = m_data.value(QStringLiteral("fontMode")).toString();
        if (mode != QLatin1String("minecraft") && mode != QLatin1String("system") && mode != QLatin1String("custom"))
            m_data.insert(QStringLiteral("fontMode"), QStringLiteral("minecraft"));
    }
    if (theme() != QLatin1String("light") && theme() != QLatin1String("dark"))
        m_data.insert(QStringLiteral("theme"), QStringLiteral("dark"));
    if (density() != QLatin1String("compact") && density() != QLatin1String("comfortable"))
        m_data.insert(QStringLiteral("density"), QStringLiteral("comfortable"));
    if (!m_data.value(QStringLiteral("lang")).toString().startsWith(QLatin1Char('r'))
        && !m_data.value(QStringLiteral("lang")).toString().startsWith(QLatin1Char('e')))
        m_data.insert(QStringLiteral("lang"), QStringLiteral("ru"));

    m_data.insert(QStringLiteral("ram"), std::clamp(m_data.value(QStringLiteral("ram")).toInt(4096), 1024, 65536));
    m_data.insert(QStringLiteral("concurrency"), std::clamp(m_data.value(QStringLiteral("concurrency")).toInt(8), 1, 32));
    m_data.insert(QStringLiteral("fontScale"), std::clamp(m_data.value(QStringLiteral("fontScale")).toDouble(1.0), 0.8, 1.4));
    if (!m_data.value(QStringLiteral("accent")).toString().isEmpty()) {
        const QString color = m_data.value(QStringLiteral("accent")).toString();
        if (!QColor(color).isValid())
            m_data.insert(QStringLiteral("accent"), QStringLiteral("#e11d2e"));
    }

    if (instancesDir().isEmpty()) {
        const QString dir = defaultInstancesDir();
        m_data.insert(QStringLiteral("instancesDir"), dir);
    }
    if (runtimeDir().isEmpty()) {
        const QString dir = defaultRuntimeDir();
        m_data.insert(QStringLiteral("runtimeDir"), dir);
    }

    QJsonObject profiles = m_data.value(QStringLiteral("appearanceProfiles")).toObject();
    for (auto it = profiles.constBegin(); it != profiles.constEnd(); ++it) {
        QJsonObject appearance = it.value().toObject();
        const QJsonObject base = makeDefaultAppearance();
        if (!appearance.contains(QStringLiteral("activeBackgroundId")))
            appearance.insert(QStringLiteral("activeBackgroundId"), QJsonValue::Null);
        appearance.insert(QStringLiteral("blur"), std::clamp(appearance.value(QStringLiteral("blur")).toInt(), 0, 100));
        appearance.insert(QStringLiteral("darken"), std::clamp(appearance.value(QStringLiteral("darken")).toInt(35), 0, 100));
        appearance.insert(QStringLiteral("saturation"), std::clamp(appearance.value(QStringLiteral("saturation")).toInt(100), 0, 200));
        Q_UNUSED(base)
        profiles.insert(it.key(), appearance);
    }
    m_data.insert(QStringLiteral("appearanceProfiles"), profiles);
}

void Settings::saveNow()
{
    if (m_saveTimer.isActive())
        m_saveTimer.stop();
    if (!m_loaded)
        return;
    m_store.replace(m_data);
}

void Settings::scheduleSave()
{
    if (!m_saveTimer.isActive())
        m_saveTimer.start();
}

void Settings::setValue(const QString &key, const QJsonValue &value, bool immediate)
{
    if (m_data.value(key) == value)
        return;
    m_data.insert(key, value);
    if (immediate)
        saveNow();
    else
        scheduleSave();
    emit changed();
}

void Settings::setJavaMode(const QString &value)
{
    if (value != QLatin1String("auto") && value != QLatin1String("manual"))
        return;
    setValue(QStringLiteral("javaMode"), value, true);
}
void Settings::setJavaPath(const QString &value) { setValue(QStringLiteral("javaPath"), value.left(1024)); }
void Settings::setLang(const QString &value)
{
    if (value != QLatin1String("ru") && value != QLatin1String("en"))
        return;
    setValue(QStringLiteral("lang"), value, true);
}
void Settings::setShowConsole(bool value) { setValue(QStringLiteral("showConsole"), value, true); }
void Settings::setAnimations(bool value) { setValue(QStringLiteral("animations"), value, true); }
void Settings::setIntroEnabled(bool value) { setValue(QStringLiteral("introEnabled"), value, true); }
void Settings::setIntroSound(bool value) { setValue(QStringLiteral("introSound"), value, true); }
void Settings::setFontMode(const QString &value)
{
    if (value != QLatin1String("minecraft") && value != QLatin1String("system") && value != QLatin1String("custom"))
        return;
    setValue(QStringLiteral("fontMode"), value, true);
}
void Settings::setActiveFontId(const QString &value)
{
    setValue(QStringLiteral("activeFontId"), value.isEmpty() ? QJsonValue::Null : QJsonValue(value.left(64)), true);
}
void Settings::setFriendsSteam(bool value) { setValue(QStringLiteral("friendsSteam"), value, true); }
void Settings::setNobleBuilds(bool value) { setValue(QStringLiteral("nobleBuilds"), value, true); }
void Settings::setProfileIcon(const QString &value)
{
    if (!value.isEmpty() && !QRegularExpression(QStringLiteral("^icon\\.[a-z0-9]+$"), QRegularExpression::CaseInsensitiveOption).match(value).hasMatch())
        return;
    setValue(QStringLiteral("profileIcon"), value.isEmpty() ? QJsonValue::Null : QJsonValue(value), true);
}
void Settings::setRam(int value) { setValue(QStringLiteral("ram"), std::clamp(value, 1024, 65536)); }
void Settings::setConcurrency(int value) { setValue(QStringLiteral("concurrency"), std::clamp(value, 1, 32), true); }
void Settings::setAutoUpdate(bool value) { setValue(QStringLiteral("autoUpdate"), value, true); }
void Settings::setTheme(const QString &value)
{
    if (value != QLatin1String("dark") && value != QLatin1String("light"))
        return;
    setValue(QStringLiteral("theme"), value, true);
}
void Settings::setDensity(const QString &value)
{
    if (value != QLatin1String("compact") && value != QLatin1String("comfortable"))
        return;
    setValue(QStringLiteral("density"), value, true);
}
void Settings::setFontScale(double value) { setValue(QStringLiteral("fontScale"), std::clamp(value, 0.8, 1.4)); }
void Settings::setAccent(const QString &value)
{
    if (!QColor(value).isValid())
        return;
    setValue(QStringLiteral("accent"), value, true);
}
void Settings::setElybyRedirectUri(const QString &value) { setValue(QStringLiteral("elybyRedirectUri"), value.trimmed().left(256)); }
void Settings::setOptimizeHighPriority(bool value)
{
    QJsonObject optimize = m_data.value(QStringLiteral("optimize")).toObject();
    optimize.insert(QStringLiteral("highPriorityGame"), value);
    setValue(QStringLiteral("optimize"), optimize, true);
}

void Settings::setDirectories(const QString &instances, const QString &runtime)
{
    m_data.insert(QStringLiteral("instancesDir"), instances);
    m_data.insert(QStringLiteral("runtimeDir"), runtime);
    saveNow();
    emit changed();
}

void Settings::setProfileKey(const QString &key)
{
    const QString next = key.trimmed().isEmpty() ? QStringLiteral("default") : key;
    if (m_profileKey == next)
        return;
    m_profileKey = next;
    emit changed();
}

QJsonObject Settings::appearance() const
{
    const QJsonObject profiles = appearanceProfiles();
    QJsonObject value = profiles.value(m_profileKey).toObject();
    const QJsonObject base = makeDefaultAppearance();
    for (auto it = base.constBegin(); it != base.constEnd(); ++it) {
        if (!value.contains(it.key()))
            value.insert(it.key(), it.value());
    }
    value.insert(QStringLiteral("blur"), std::clamp(value.value(QStringLiteral("blur")).toInt(), 0, 100));
    value.insert(QStringLiteral("darken"), std::clamp(value.value(QStringLiteral("darken")).toInt(35), 0, 100));
    value.insert(QStringLiteral("saturation"), std::clamp(value.value(QStringLiteral("saturation")).toInt(100), 0, 200));
    const QString bg = value.value(QStringLiteral("activeBackgroundId")).toString();
    value.insert(QStringLiteral("activeBackgroundId"), bg.isEmpty() ? QJsonValue::Null : QJsonValue(bg));
    return value;
}

void Settings::setAppearanceProfiles(const QJsonObject &profiles)
{
    setValue(QStringLiteral("appearanceProfiles"), profiles, true);
}

void Settings::clearBackgroundFromProfiles(const QString &backgroundId)
{
    QJsonObject profiles = appearanceProfiles();
    bool changedAny = false;
    for (auto it = profiles.constBegin(); it != profiles.constEnd(); ++it) {
        QJsonObject appearance = it.value().toObject();
        if (appearance.value(QStringLiteral("activeBackgroundId")).toString() == backgroundId) {
            appearance.insert(QStringLiteral("activeBackgroundId"), QJsonValue::Null);
            profiles.insert(it.key(), appearance);
            changedAny = true;
        }
    }
    if (changedAny)
        setAppearanceProfiles(profiles);
}

static void setAppearanceField(Settings *settings, const QString &key, const QJsonValue &value)
{
    QJsonObject profiles = settings->appearanceProfiles();
    QJsonObject current = profiles.value(settings->profileKey()).toObject();
    current.insert(key, value);
    profiles.insert(settings->profileKey(), current);
    settings->setAppearanceProfiles(profiles);
}

void Settings::setActiveBackgroundId(const QString &value) { setAppearanceField(this, QStringLiteral("activeBackgroundId"), value.isEmpty() ? QJsonValue::Null : QJsonValue(value)); }
void Settings::setBackgroundBlur(int value) { setAppearanceField(this, QStringLiteral("blur"), std::clamp(value, 0, 100)); }
void Settings::setBackgroundDarken(int value) { setAppearanceField(this, QStringLiteral("darken"), std::clamp(value, 0, 100)); }
void Settings::setBackgroundSaturation(int value) { setAppearanceField(this, QStringLiteral("saturation"), std::clamp(value, 0, 200)); }

void Settings::setBackgrounds(const QJsonArray &list)
{
    setValue(QStringLiteral("backgrounds"), list, true);
}

void Settings::setCustomFonts(const QJsonArray &list)
{
    setValue(QStringLiteral("customFonts"), list, true);
}

void Settings::reset()
{
    const QJsonArray backgrounds = this->backgrounds();
    QJsonObject fresh = defaults();
    fresh.insert(QStringLiteral("instancesDir"), defaultInstancesDir());
    fresh.insert(QStringLiteral("runtimeDir"), defaultRuntimeDir());
    fresh.insert(QStringLiteral("backgrounds"), backgrounds);
    m_data = fresh;
    saveNow();
    emit changed();
}

void Settings::reload()
{
    m_store = JsonStore(m_store.filePath(), defaults());
    m_data = m_store.read();
    m_loaded = true;
    normalize();
    emit changed();
}

void Settings::save() { saveNow(); }

void Settings::replayIntro() { emit introReplayRequested(); }

QString Settings::defaultInstancesDir() const { return Paths::instancesDefaultDir(); }
QString Settings::defaultRuntimeDir() const { return Paths::runtimeDefaultDir(); }

} // namespace Noble
