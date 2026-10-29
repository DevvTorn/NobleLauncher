#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QJsonObject>

namespace Noble {

struct ProcessResult {
    bool started = false;
    int exitCode = -1;
    QByteArray stdoutData;
    QByteArray stderrData;
    QString error;
};

ProcessResult runProcess(const QString &program, const QStringList &arguments, int timeoutMs = 30000,
                         const QByteArray &input = {}, const QString &workingDir = QString());

ProcessResult runPowerShell(const QString &script, int timeoutMs = 60000);

/** Запуск PowerShell-скрипта с UAC. False — пользователь отменил запрос прав. */
bool runPowerShellElevated(const QString &scriptPath, int timeoutMs = 180000);

bool openPathInExplorer(const QString &path);
bool openWithDefaultApp(const QString &path);
bool openExternalUrl(const QString &url);

void raiseProcessPriority(qint64 pid);

struct BackgroundProcess {
    qint64 pid = 0;
    QString name;
    QString title;
    int memoryMb = 0;
};

QList<BackgroundProcess> listBackgroundProcesses();
QStringList killBackgroundProcesses(const QList<qint64> &pids);

int currentProcessMemoryMb();
qint64 totalPhysicalMemoryMb();
qint64 availablePhysicalMemoryMb();
QString applicationMemoryInfo();

QString steamInstallPath();
bool steamRunning();
bool steamLoggedIn();

QStringList taskkillTree(qint64 pid, bool force);

} // namespace Noble
