#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantMap>
#include <QVariantList>

namespace Noble {

class StatsService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY summaryChanged)
    Q_PROPERTY(int points READ points NOTIFY summaryChanged)
    Q_PROPERTY(int level READ level NOTIFY summaryChanged)
    Q_PROPERTY(qint64 gameSeconds READ gameSeconds NOTIFY summaryChanged)

public:
    explicit StatsService(QObject *parent = nullptr);

    void init();

    QVariantMap summary() const;
    int points() const { return m_points; }
    int level() const { return m_level; }
    qint64 gameSeconds() const;

    void inc(const QString &key, int by = 1);
    void gameStarted(const QString &instanceId, const QString &name);
    void gameEnded(const QString &instanceId, qint64 seconds, bool crashed);
    void addLauncherMinutes(int minutes);
    void contentInstalled(const QString &kind, int count);
    void noteDay();

signals:
    void summaryChanged();
    void achievementUnlocked(const QVariantMap &achievement);
    void levelUp(int level, const QString &titleRu, const QString &titleEn);

private:
    void load();
    void persist();
    void evaluate();
    void recalc();
    QString dayKey() const;
    QJsonObject counterMap() const;

    QJsonObject m_counters;
    QJsonObject m_perBuild;
    QJsonObject m_achievements;
    QString m_lastDay;
    int m_streak = 0;
    int m_points = 0;
    int m_level = 1;
    bool m_loaded = false;
};

} // namespace Noble
