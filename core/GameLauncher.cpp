#include "GameLauncher.h"

#include "Logger.h"
#include "ProcessUtil.h"

#include <QDateTime>
#include <QProcess>
#include <QSet>
#include <QTimer>

namespace Noble {

static QString redactedArguments(const QStringList &arguments)
{
    static const QSet<QString> secretFlags = {
        QStringLiteral("--accessToken"), QStringLiteral("--clientId"), QStringLiteral("--xuid"),
        QStringLiteral("--authSession"), QStringLiteral("--clientToken")
    };
    QStringList safe = arguments;
    for (int i = 0; i < safe.size(); ++i) {
        if (secretFlags.contains(safe.at(i)) && i + 1 < safe.size()) {
            safe[i + 1] = QStringLiteral("***");
            continue;
        }
        for (const auto &flag : secretFlags) {
            if (safe.at(i).startsWith(flag + QLatin1Char('='))) {
                safe[i] = flag + QStringLiteral("=***");
                break;
            }
        }
    }
    return safe.join(QLatin1Char(' '));
}

GameLauncher::GameLauncher(QObject *parent)
    : QObject(parent)
{
    m_elapsedTimer.setInterval(1000);
    connect(&m_elapsedTimer, &QTimer::timeout, this, &GameLauncher::elapsedChanged);
}

bool GameLauncher::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning && m_phase != QLatin1String("idle");
}

qint64 GameLauncher::pid() const
{
    if (!m_process || m_process->state() == QProcess::NotRunning)
        return 0;
    return m_process->processId();
}

int GameLauncher::elapsedSeconds() const
{
    return m_elapsed.isValid() ? static_cast<int>(m_elapsed.elapsed() / 1000) : 0;
}

void GameLauncher::setPhase(const QString &phase)
{
    if (m_phase == phase)
        return;
    m_phase = phase;
    emit phaseChanged(m_phase);
}

void GameLauncher::setConsoleEnabled(bool enabled)
{
    if (m_consoleEnabled == enabled)
        return;
    m_consoleEnabled = enabled;
    if (!m_process)
        return;
    if (enabled) {
        if (m_process->state() != QProcess::NotRunning) {
            emit consoleLine(QStringLiteral("warn"),
                             QStringLiteral("Консоль подключена заново — вывод уже запущенной игры недоступен, "
                                            "показаны новые сообщения лаунчера"));
        }
    } else if (m_process->state() != QProcess::NotRunning) {
        // Реально освобождаем канал: Qt перестаёт читать stdout/stderr и не буферизует их.
        m_process->closeReadChannel(QProcess::StandardOutput);
        m_process->closeReadChannel(QProcess::StandardError);
        m_stdoutBuffer.clear();
        m_stderrBuffer.clear();
    }
}

void GameLauncher::start(const LaunchSpec &spec, bool highPriority, bool consoleEnabled)
{
    if (m_process && m_process->state() != QProcess::NotRunning)
        return;

    m_finishEmitted = false;
    m_stopRequested = false;
    m_consoleEnabled = consoleEnabled;
    m_instanceId = spec.instanceId;
    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
    m_process = new QProcess(this);
    m_process->setProgram(spec.javaPath);
    m_process->setArguments(spec.arguments);
    m_process->setWorkingDirectory(spec.workingDir);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->setStandardInputFile(QProcess::nullDevice());
    if (!consoleEnabled) {
        m_process->setStandardOutputFile(QProcess::nullDevice());
        m_process->setStandardErrorFile(QProcess::nullDevice());
    } else {
        connect(m_process, &QProcess::readyReadStandardOutput, this, [this] { handleReadyRead(false); });
        connect(m_process, &QProcess::readyReadStandardError, this, [this] { handleReadyRead(true); });
    }

    connect(m_process, &QProcess::started, this, [this, highPriority] {
        setPhase(QStringLiteral("running"));
        m_elapsed.start();
        m_elapsedTimer.start();
        emit pidChanged();
        emit elapsedChanged();
        if (highPriority)
            raiseProcessPriority(pid());
        Logger::info(QStringLiteral("Игра запущена (PID %1)").arg(pid()));
        emit gameStarted(pid(), m_instanceId);
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            emit consoleLine(QStringLiteral("error"), QStringLiteral("Не удалось запустить игру: %1").arg(m_process->errorString()));
            if (!m_finishEmitted) {
                m_finishEmitted = true;
                setPhase(QStringLiteral("idle"));
                emit gameFinished(-1, false, m_instanceId);
            }
        }
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) { handleFinished(code, static_cast<int>(status)); });

    setPhase(QStringLiteral("launching"));
    for (const auto &warning : spec.warnings) {
        Logger::warn(warning);
        emit consoleLine(QStringLiteral("warn"), warning);
    }
    Logger::info(QStringLiteral("Запуск игры: %1 %2").arg(spec.javaPath, redactedArguments(spec.arguments).left(1200)));
    m_process->start();
}

void GameLauncher::handleReadyRead(bool errorChannel)
{
    if (!m_process || !m_consoleEnabled)
        return;
    QByteArray chunk = errorChannel ? m_process->readAllStandardError() : m_process->readAllStandardOutput();
    if (chunk.isEmpty())
        return;
    QByteArray &buffer = errorChannel ? m_stderrBuffer : m_stdoutBuffer;
    buffer += chunk;
    for (;;) {
        const int index = buffer.indexOf('\n');
        if (index < 0)
            break;
        QByteArray line = buffer.left(index);
        buffer.remove(0, index + 1);
        if (line.endsWith('\r'))
            line.chop(1);
        const QString text = QString::fromUtf8(line);
        if (!text.trimmed().isEmpty())
            emit consoleLine(QStringLiteral("game"), text);
    }
}

void GameLauncher::handleFinished(int code, int exitStatus)
{
    if (m_finishEmitted)
        return;
    m_finishEmitted = true;
    Q_UNUSED(exitStatus)
    if (m_process) {
        if (m_consoleEnabled) {
            m_stdoutBuffer += m_process->readAllStandardOutput();
            m_stderrBuffer += m_process->readAllStandardError();
            for (const auto &channel : {m_stdoutBuffer, m_stderrBuffer}) {
                for (const auto &line : channel.split('\n')) {
                    const QString text = QString::fromUtf8(line).trimmed();
                    if (!text.isEmpty())
                        emit consoleLine(QStringLiteral("game"), text);
                }
            }
            m_stdoutBuffer.clear();
            m_stderrBuffer.clear();
        }
    }
    const bool crashed = code != 0 && !m_stopRequested;
    emit consoleLine(code == 0 ? QStringLiteral("success") : QStringLiteral("warn"),
                     QStringLiteral("Игра завершена с кодом %1").arg(code));
    m_elapsedTimer.stop();
    setPhase(QStringLiteral("idle"));
    emit elapsedChanged();
    emit gameFinished(code, crashed, m_instanceId);
    Logger::info(QStringLiteral("Игра завершена с кодом %1").arg(code));
    if (m_process)
        m_process->deleteLater();
    m_process = nullptr;
    emit pidChanged();
}

void GameLauncher::stop()
{
    if (!m_process || m_process->state() == QProcess::NotRunning)
        return;
    if (m_phase == QLatin1String("stopping"))
        return;
    setPhase(QStringLiteral("stopping"));
    m_stopRequested = true;
    const qint64 gamePid = m_process->processId();
    if (gamePid <= 0) {
        m_process->kill();
        return;
    }
    emit consoleLine(QStringLiteral("info"), QStringLiteral("Остановка игры (PID %1)…").arg(gamePid));
#if defined(Q_OS_WIN)
    QProcess::startDetached(QStringLiteral("taskkill.exe"), {QStringLiteral("/PID"), QString::number(gamePid)});
    QTimer::singleShot(8000, this, [this, gamePid] {
        if (m_process && m_process->state() != QProcess::NotRunning) {
            emit consoleLine(QStringLiteral("warn"),
                             QStringLiteral("Игра не завершилась мягко — завершаю принудительно…"));
            QProcess::startDetached(QStringLiteral("taskkill.exe"),
                                    {QStringLiteral("/PID"), QString::number(gamePid), QStringLiteral("/T"),
                                     QStringLiteral("/F")});
        }
    });
#else
    m_process->terminate();
    QTimer::singleShot(8000, this, [this] {
        if (m_process && m_process->state() != QProcess::NotRunning)
            m_process->kill();
    });
#endif
}

} // namespace Noble
