#include "StatsService.h"

#include "AchievementCatalog.h"
#include "Json.h"
#include "Logger.h"
#include "Paths.h"
#include "Pure.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QSet>

#include <algorithm>

namespace Noble {

StatsService::StatsService(QObject *parent)
    : QObject(parent)
{
}

QString StatsService::dayKey() const
{
    return QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
}

static QStringList allCounterKeys()
{
    QSet<QString> keys;
    for (const auto &def : achievements())
        keys.insert(def.stat);
    const QStringList extra = {
        QStringLiteral("launchesToday"), QStringLiteral("bestBuildSeconds"), QStringLiteral("buildsWithHour"),
        QStringLiteral("consecutiveDays"), QStringLiteral("crashesSeen"), QStringLiteral("launcherMinutes"),
    };
    for (const auto &key : extra)
        keys.insert(key);
    return keys.values();
}

void StatsService::load()
{
    if (m_loaded)
        return;
    m_loaded = true;
    const QString path = QDir(Paths::dataDir()).filePath(QStringLiteral("stats.json"));
    const QJsonObject stored = readJsonObject(path);
    m_counters = stored.value(QStringLiteral("counters")).toObject();
    for (const auto &key : allCounterKeys()) {
        if (!m_counters.contains(key))
            m_counters.insert(key, 0);
    }
    m_perBuild = stored.value(QStringLiteral("perBuild")).toObject();
    m_achievements = stored.value(QStringLiteral("achievements")).toObject();
    m_lastDay = stored.value(QStringLiteral("lastDay")).toString();
    m_streak = stored.value(QStringLiteral("streak")).toInt();
    recalc();
}

void StatsService::init()
{
    load();
    noteDay();
    evaluate();
    persist();
}

void StatsService::persist()
{
    const QString path = QDir(Paths::dataDir()).filePath(QStringLiteral("stats.json"));
    writeJsonObject(path,
                    QJsonObject{{QStringLiteral("counters"), m_counters},
                                {QStringLiteral("perBuild"), m_perBuild},
                                {QStringLiteral("achievements"), m_achievements},
                                {QStringLiteral("lastDay"), m_lastDay.isEmpty() ? QJsonValue::Null : QJsonValue(m_lastDay)},
                                {QStringLiteral("streak"), m_streak}});
}

void StatsService::recalc()
{
    const int previousLevel = m_level;
    int points = 0;
    for (const auto &def : achievements()) {
        if (m_achievements.value(def.id).toDouble() > 0)
            points += def.points;
    }
    m_points = points;
    const LevelInfo info = levelForPoints(points);
    m_level = info.level;
    if (previousLevel != m_level && m_loaded && m_level > previousLevel)
        emit levelUp(m_level, info.titleRu, info.titleEn);
    emit summaryChanged();
}

void StatsService::evaluate()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool changed = false;
    for (const auto &def : achievements()) {
        if (m_achievements.value(def.id).toDouble() > 0)
            continue;
        if (static_cast<qint64>(m_counters.value(def.stat).toDouble()) < def.threshold)
            continue;
        m_achievements.insert(def.id, static_cast<double>(now));
        changed = true;
        const QVariantMap unlocked{{QStringLiteral("id"), def.id},
                                   {QStringLiteral("titleRu"), def.titleRu},
                                   {QStringLiteral("titleEn"), def.titleEn},
                                   {QStringLiteral("descRu"), def.descRu},
                                   {QStringLiteral("descEn"), def.descEn},
                                   {QStringLiteral("points"), def.points},
                                   {QStringLiteral("unlockedAt"), now}};
        Logger::info(QStringLiteral("Достижение получено: «%1» (+%2)").arg(def.titleRu).arg(def.points));
        emit achievementUnlocked(unlocked);
    }
    if (changed) {
        persist();
        recalc();
    }
}

void StatsService::noteDay()
{
    load();
    const QString today = dayKey();
    bool changed = false;
    if (m_lastDay != today) {
        const QString yesterday = QDate::currentDate().addDays(-1).toString(QStringLiteral("yyyy-MM-dd"));
        m_streak = m_lastDay == yesterday ? m_streak + 1 : 1;
        m_lastDay = today;
        const int current = qMax(m_counters.value(QStringLiteral("consecutiveDays")).toInt(), m_streak);
        m_counters.insert(QStringLiteral("consecutiveDays"), current);
        m_counters.insert(QStringLiteral("launchesToday"), 0);
        changed = true;
    }
    const QDate now = QDate::currentDate();
    const int month = now.month();
    if (month == 12 || month == 1 || month == 2) {
        if (m_counters.value(QStringLiteral("seasonWinter")).toInt() == 0) {
            m_counters.insert(QStringLiteral("seasonWinter"), 1);
            changed = true;
        }
    }
    if (month == 10) {
        if (m_counters.value(QStringLiteral("seasonHalloween")).toInt() == 0) {
            m_counters.insert(QStringLiteral("seasonHalloween"), 1);
            changed = true;
        }
    }
    if (month == 1 && now.day() == 1) {
        if (m_counters.value(QStringLiteral("seasonNewYear")).toInt() == 0) {
            m_counters.insert(QStringLiteral("seasonNewYear"), 1);
            changed = true;
        }
    }
    if (changed) {
        persist();
        recalc();
        evaluate();
    }
}

void StatsService::inc(const QString &key, int by)
{
    load();
    const int value = m_counters.value(key).toInt() + by;
    m_counters.insert(key, value);
    persist();
    recalc();
    evaluate();
}

void StatsService::gameStarted(const QString &instanceId, const QString &name)
{
    load();
    noteDay();
    m_counters.insert(QStringLiteral("gameLaunches"), m_counters.value(QStringLiteral("gameLaunches")).toInt() + 1);
    m_counters.insert(QStringLiteral("launchesToday"), m_counters.value(QStringLiteral("launchesToday")).toInt() + 1);
    QJsonObject build = m_perBuild.value(instanceId).toObject();
    build.insert(QStringLiteral("name"), name);
    build.insert(QStringLiteral("seconds"), build.value(QStringLiteral("seconds")).toDouble());
    build.insert(QStringLiteral("launches"), build.value(QStringLiteral("launches")).toInt() + 1);
    m_perBuild.insert(instanceId, build);
    persist();
    recalc();
    evaluate();
}

void StatsService::gameEnded(const QString &instanceId, qint64 seconds, bool crashed)
{
    load();
    if (seconds > 0) {
        m_counters.insert(QStringLiteral("gameSeconds"), m_counters.value(QStringLiteral("gameSeconds")).toDouble() + seconds);
        QJsonObject build = m_perBuild.value(instanceId).toObject();
        if (build.isEmpty()) {
            build.insert(QStringLiteral("name"), instanceId);
            build.insert(QStringLiteral("launches"), 1);
        }
        build.insert(QStringLiteral("seconds"), build.value(QStringLiteral("seconds")).toDouble() + seconds);
        m_perBuild.insert(instanceId, build);
        qint64 best = 0;
        int withHour = 0;
        for (auto it = m_perBuild.constBegin(); it != m_perBuild.constEnd(); ++it) {
            const qint64 value = static_cast<qint64>(it.value().toObject().value(QStringLiteral("seconds")).toDouble());
            best = std::max(best, value);
            if (value >= 3600)
                ++withHour;
        }
        m_counters.insert(QStringLiteral("bestBuildSeconds"), static_cast<double>(best));
        m_counters.insert(QStringLiteral("buildsWithHour"), withHour);
    }
    if (crashed)
        m_counters.insert(QStringLiteral("crashesSeen"), m_counters.value(QStringLiteral("crashesSeen")).toInt() + 1);
    persist();
    recalc();
    evaluate();
}

void StatsService::addLauncherMinutes(int minutes)
{
    if (minutes <= 0)
        return;
    inc(QStringLiteral("launcherMinutes"), minutes);
}

void StatsService::contentInstalled(const QString &kind, int count)
{
    if (count <= 0)
        return;
    load();
    const QString key = kind == QLatin1String("mod") ? QStringLiteral("modsInstalled")
        : kind == QLatin1String("resourcepack") ? QStringLiteral("resourcepacksInstalled")
                                                : QStringLiteral("shaderpacksInstalled");
    m_counters.insert(key, m_counters.value(key).toInt() + count);
    m_counters.insert(QStringLiteral("queueInstalls"), m_counters.value(QStringLiteral("queueInstalls")).toInt() + 1);
    persist();
    recalc();
    evaluate();
}

QVariantMap StatsService::summary() const
{
    StatsService *self = const_cast<StatsService *>(this);
    self->load();
    QVariantList achievementList;
    int unlocked = 0;
    for (const auto &def : achievements()) {
        const double at = m_achievements.value(def.id).toDouble();
        if (at > 0)
            ++unlocked;
        achievementList.append(QVariantMap{{QStringLiteral("id"), def.id},
                                           {QStringLiteral("titleRu"), def.titleRu},
                                           {QStringLiteral("titleEn"), def.titleEn},
                                           {QStringLiteral("descRu"), def.descRu},
                                           {QStringLiteral("descEn"), def.descEn},
                                           {QStringLiteral("points"), def.points},
                                           {QStringLiteral("stat"), def.stat},
                                           {QStringLiteral("threshold"), static_cast<double>(def.threshold)},
                                           {QStringLiteral("unlockedAt"), at > 0 ? QVariant(at) : QVariant()}});
    }
    QVariantList perBuild;
    for (auto it = m_perBuild.constBegin(); it != m_perBuild.constEnd(); ++it) {
        const QJsonObject build = it.value().toObject();
        perBuild.append(QVariantMap{{QStringLiteral("id"), it.key()},
                                    {QStringLiteral("name"), build.value(QStringLiteral("name")).toString()},
                                    {QStringLiteral("seconds"), static_cast<double>(build.value(QStringLiteral("seconds")).toDouble())},
                                    {QStringLiteral("launches"), build.value(QStringLiteral("launches")).toInt()}});
    }
    std::sort(perBuild.begin(), perBuild.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("seconds")).toDouble() > b.toMap().value(QStringLiteral("seconds")).toDouble();
    });
    const LevelInfo level = levelForPoints(m_points);
    return {{QStringLiteral("counters"), m_counters.toVariantMap()},
            {QStringLiteral("perBuild"), perBuild},
            {QStringLiteral("achievements"), achievementList},
            {QStringLiteral("unlockedCount"), unlocked},
            {QStringLiteral("totalAchievements"), achievements().size()},
            {QStringLiteral("points"), m_points},
            {QStringLiteral("level"), level.level},
            {QStringLiteral("levelTitleRu"), level.titleRu},
            {QStringLiteral("levelTitleEn"), level.titleEn},
            {QStringLiteral("nextAt"), level.nextAt},
            {QStringLiteral("toNext"), level.toNext},
            {QStringLiteral("streak"), m_streak}};
}

qint64 StatsService::gameSeconds() const
{
    return static_cast<qint64>(m_counters.value(QStringLiteral("gameSeconds")).toDouble());
}

} // namespace Noble
