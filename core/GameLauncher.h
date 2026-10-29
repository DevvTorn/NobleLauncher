#pragma once

#include "LaunchSpec.h"

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QPointer>

class QProcess;

namespace Noble {

class GameLauncher : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString phase READ phase NOTIFY phaseChanged)
    Q_PROPERTY(qint64 pid READ pid NOTIFY pidChanged)
    Q_PROPERTY(int elapsedSeconds READ elapsedSeconds NOTIFY elapsedChanged)

public:
    explicit GameLauncher(QObject *parent = nullptr);

    void start(const LaunchSpec &spec, bool highPriority, bool consoleEnabled);
    void stop();
    void setConsoleEnabled(bool enabled);
    bool isRunning() const;
    QString phase() const { return m_phase; }
    qint64 pid() const;
    int elapsedSeconds() const;

signals:
    void phaseChanged(const QString &phase);
    void pidChanged();
    void elapsedChanged();
    void consoleLine(const QString &type, const QString &text);
    void gameStarted(qint64 pid, const QString &instanceId);
    void gameFinished(int code, bool crashed, const QString &instanceId);

private:
    void setPhase(const QString &phase);
    void handleFinished(int code, int exitStatus);
    void handleReadyRead(bool errorChannel);

    QPointer<QProcess> m_process;
    QString m_phase = QStringLiteral("idle");
    QString m_instanceId;
    QElapsedTimer m_elapsed;
    QTimer m_elapsedTimer;
    bool m_stopRequested = false;
    bool m_consoleEnabled = true;
    bool m_finishEmitted = false;
    QByteArray m_stdoutBuffer;
    QByteArray m_stderrBuffer;
};

} // namespace Noble
