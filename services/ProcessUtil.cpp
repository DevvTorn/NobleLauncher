#include "ProcessUtil.h"

#include "Json.h"
#include "Logger.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>

#include <algorithm>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#include <shellapi.h>
#endif

namespace Noble {

ProcessResult runProcess(const QString &program, const QStringList &arguments, int timeoutMs,
                         const QByteArray &input, const QString &workingDir)
{
    ProcessResult result;
    QProcess process;
    if (!workingDir.isEmpty())
        process.setWorkingDirectory(workingDir);
    process.setProgram(program);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    if (!process.waitForStarted(10000)) {
        result.error = process.errorString();
        return result;
    }
    result.started = true;
    if (!input.isEmpty())
        process.write(input);
    process.closeWriteChannel();
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(3000);
        result.error = QStringLiteral("Превышено время ожидания (%1 с)").arg(timeoutMs / 1000);
        result.exitCode = -1;
    } else {
        result.exitCode = process.exitCode();
    }
    result.stdoutData = process.readAllStandardOutput();
    result.stderrData = process.readAllStandardError();
    return result;
}

ProcessResult runPowerShell(const QString &script, int timeoutMs)
{
    return runProcess(QStringLiteral("powershell.exe"),
                      {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"), QStringLiteral("-ExecutionPolicy"),
                       QStringLiteral("Bypass"), QStringLiteral("-Command"), script},
                      timeoutMs);
}

bool runPowerShellElevated(const QString &scriptPath, int timeoutMs)
{
    const QString escaped = QString(scriptPath).replace(QLatin1Char('"'), QStringLiteral("\"\""));
    const QString command = QStringLiteral(
        "$p = Start-Process -FilePath 'powershell.exe' -Verb RunAs -Wait -PassThru "
        "-ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File','\"%1\"'); exit $p.ExitCode")
                                .arg(escaped);
    const ProcessResult result = runProcess(QStringLiteral("powershell.exe"),
                                            {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                                             QStringLiteral("-Command"), command},
                                            timeoutMs);
    return result.started && result.exitCode == 0;
}

bool openPathInExplorer(const QString &path)
{
#ifdef Q_OS_WIN
    const QString native = QDir::toNativeSeparators(path);
    QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,") + native});
    return true;
#else
    QProcess::startDetached(QStringLiteral("xdg-open"), {QFileInfo(path).absolutePath()});
    return true;
#endif
}

bool openWithDefaultApp(const QString &path)
{
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

bool openExternalUrl(const QString &url)
{
    const QUrl parsed(url);
    if (!parsed.isValid() || (parsed.scheme() != QLatin1String("http") && parsed.scheme() != QLatin1String("https")
                              && parsed.scheme() != QLatin1String("steam")))
        return false;
    return QDesktopServices::openUrl(parsed);
}

void raiseProcessPriority(qint64 pid)
{
#ifdef Q_OS_WIN
    HANDLE handle = OpenProcess(PROCESS_SET_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!handle) {
        Logger::warn(QStringLiteral("Не удалось открыть процесс игры для повышения приоритета"));
        return;
    }
    if (SetPriorityClass(handle, HIGH_PRIORITY_CLASS))
        Logger::info(QStringLiteral("Игре назначен высокий приоритет (PID %1)").arg(pid));
    else
        Logger::warn(QStringLiteral("Не удалось поднять приоритет процесса игры (PID %1)").arg(pid));
    CloseHandle(handle);
#else
    Q_UNUSED(pid)
#endif
}

static const QStringList &protectedProcesses()
{
    static const QStringList names = {
        QStringLiteral("system"), QStringLiteral("idle"), QStringLiteral("registry"),
        QStringLiteral("memory compression"), QStringLiteral("secure system"), QStringLiteral("smss"),
        QStringLiteral("csrss"), QStringLiteral("wininit"), QStringLiteral("winlogon"),
        QStringLiteral("services"), QStringLiteral("lsass"), QStringLiteral("dwm"), QStringLiteral("svchost"),
        QStringLiteral("fontdrvhost"), QStringLiteral("sihost"), QStringLiteral("taskhostw"),
        QStringLiteral("runtimebroker"), QStringLiteral("searchhost"), QStringLiteral("startmenuexperiencehost"),
        QStringLiteral("shellexperiencehost"), QStringLiteral("textinputhost"), QStringLiteral("ctfmon"),
        QStringLiteral("spoolsv"), QStringLiteral("audiodg"), QStringLiteral("explorer"), QStringLiteral("wmiprvse"),
        QStringLiteral("securityhealthservice"), QStringLiteral("securityhealthsystray"), QStringLiteral("msmpeng"),
        QStringLiteral("nissrv"), QStringLiteral("searchindexer"), QStringLiteral("searchprotocolhost"),
        QStringLiteral("searchfilterhost"), QStringLiteral("conhost"), QStringLiteral("dllhost"),
        QStringLiteral("backgroundtaskhost"), QStringLiteral("noble launcher"), QStringLiteral("noblelauncher"),
        QStringLiteral("powershell"), QStringLiteral("pwsh"), QStringLiteral("cmd"), QStringLiteral("windows terminal"),
        QStringLiteral("openconsole"),
    };
    return names;
}

QList<BackgroundProcess> listBackgroundProcesses()
{
    QList<BackgroundProcess> result;
#ifdef Q_OS_WIN
    const QString script = QStringLiteral(
        "Get-Process | Where-Object { $_.MainWindowHandle -ne 0 -and $_.WorkingSet64 -ge 104857600 } | "
        "Select-Object Id,ProcessName,MainWindowTitle,WorkingSet64 | ConvertTo-Json -Compress");
    const ProcessResult run = runPowerShell(script, 30000);
    if (!run.started)
        return result;
    const QJsonDocument doc = QJsonDocument::fromJson(run.stdoutData.trimmed());
    QJsonArray array;
    if (doc.isArray())
        array = doc.array();
    else if (doc.isObject())
        array.append(doc.object());
    const qint64 ownPid = QCoreApplication::applicationPid();
    for (const auto &value : array) {
        const QJsonObject item = value.toObject();
        const qint64 pid = static_cast<qint64>(item.value(QStringLiteral("Id")).toDouble());
        if (pid <= 0 || pid == ownPid)
            continue;
        const QString name = item.value(QStringLiteral("ProcessName")).toString();
        if (protectedProcesses().contains(name.toLower()))
            continue;
        BackgroundProcess process;
        process.pid = pid;
        process.name = name;
        process.title = item.value(QStringLiteral("MainWindowTitle")).toString().left(120);
        process.memoryMb = static_cast<int>(item.value(QStringLiteral("WorkingSet64")).toDouble() / 1048576.0);
        result.append(process);
    }
    std::sort(result.begin(), result.end(), [](const BackgroundProcess &a, const BackgroundProcess &b) {
        return a.memoryMb > b.memoryMb;
    });
    if (result.size() > 25)
        result = result.mid(0, 25);
#endif
    return result;
}

QStringList killBackgroundProcesses(const QList<qint64> &pids)
{
    QStringList killed;
    if (pids.isEmpty())
        return killed;
    const auto candidates = listBackgroundProcesses();
    QList<qint64> allowed;
    for (const auto &pid : pids) {
        for (const auto &candidate : candidates) {
            if (candidate.pid == pid) {
                allowed << pid;
                break;
            }
        }
    }
    for (qint64 pid : allowed) {
        runProcess(QStringLiteral("taskkill.exe"), {QStringLiteral("/PID"), QString::number(pid)}, 15000);
    }
    QThread::msleep(600);
    const auto alive = listBackgroundProcesses();
    for (qint64 pid : allowed) {
        bool stillAlive = false;
        for (const auto &process : alive) {
            if (process.pid == pid) {
                stillAlive = true;
                break;
            }
        }
        if (stillAlive)
            runProcess(QStringLiteral("taskkill.exe"),
                       {QStringLiteral("/PID"), QString::number(pid), QStringLiteral("/F")}, 15000);
        for (const auto &candidate : candidates) {
            if (candidate.pid == pid) {
                killed << candidate.name;
                break;
            }
        }
    }
    return killed;
}

int currentProcessMemoryMb()
{
#ifdef Q_OS_WIN
    // Task Manager показывает private working set: общие страницы Qt-библиотек не учитываются.
    HANDLE process = GetCurrentProcess();
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    const SIZE_T pageSize = info.dwPageSize;
    const SIZE_T batch = 4096;
    std::vector<PSAPI_WORKING_SET_EX_INFORMATION> entries(batch);
    SIZE_T privateBytes = 0;
    MEMORY_BASIC_INFORMATION region;
    BYTE *address = nullptr;
    while (VirtualQueryEx(process, address, &region, sizeof(region)) == sizeof(region)) {
        if (region.State == MEM_COMMIT) {
            const SIZE_T pages = (region.RegionSize + pageSize - 1) / pageSize;
            for (SIZE_T start = 0; start < pages; start += batch) {
                const SIZE_T count = std::min(batch, pages - start);
                for (SIZE_T i = 0; i < count; ++i) {
                    entries[i].VirtualAddress = static_cast<PVOID>(
                        static_cast<BYTE *>(region.BaseAddress) + (start + i) * pageSize);
                }
                if (QueryWorkingSetEx(process, entries.data(),
                                      static_cast<DWORD>(count * sizeof(PSAPI_WORKING_SET_EX_INFORMATION)))) {
                    for (SIZE_T i = 0; i < count; ++i) {
                        if (entries[i].VirtualAttributes.Valid && !entries[i].VirtualAttributes.Shared)
                            privateBytes += pageSize;
                    }
                }
            }
        }
        address = static_cast<BYTE *>(region.BaseAddress) + region.RegionSize;
        if (address <= static_cast<BYTE *>(region.BaseAddress))
            break;
    }
    if (privateBytes > 0)
        return static_cast<int>(privateBytes / 1048576);
    PROCESS_MEMORY_COUNTERS_EX counters;
    if (GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters), sizeof(counters)))
        return static_cast<int>(counters.WorkingSetSize / 1048576);
#endif
    return 0;
}

qint64 totalPhysicalMemoryMb()
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status))
        return static_cast<qint64>(status.ullTotalPhys / 1048576);
#endif
    return 0;
}

qint64 availablePhysicalMemoryMb()
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status))
        return static_cast<qint64>(status.ullAvailPhys / 1048576);
#endif
    return 0;
}

QString applicationMemoryInfo()
{
#ifdef Q_OS_WIN
    const int mb = currentProcessMemoryMb();
    return QStringLiteral("%1 МБ").arg(mb);
#else
    return QStringLiteral("—");
#endif
}

QString steamInstallPath()
{
#ifdef Q_OS_WIN
    QSettings settings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Valve\\Steam"), QSettings::NativeFormat);
    const QString path = settings.value(QStringLiteral("SteamPath")).toString();
    if (!path.isEmpty())
        return QDir::fromNativeSeparators(path);
    QSettings wow(QStringLiteral("HKEY_CURRENT_USER\\Software\\WOW6432Node\\Valve\\Steam"), QSettings::NativeFormat);
    return QDir::fromNativeSeparators(wow.value(QStringLiteral("SteamPath")).toString());
#else
    return QString();
#endif
}

bool steamRunning()
{
#ifdef Q_OS_WIN
    const ProcessResult result = runProcess(QStringLiteral("tasklist.exe"),
                                            {QStringLiteral("/FI"), QStringLiteral("IMAGENAME eq steam.exe"), QStringLiteral("/NH")},
                                            15000);
    return QString::fromLocal8Bit(result.stdoutData).contains(QStringLiteral("steam.exe"), Qt::CaseInsensitive);
#else
    return false;
#endif
}

bool steamLoggedIn()
{
#ifdef Q_OS_WIN
    QSettings settings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Valve\\Steam\\ActiveProcess"), QSettings::NativeFormat);
    return settings.value(QStringLiteral("ActiveUser")).toInt() != 0;
#else
    return false;
#endif
}

QStringList taskkillTree(qint64 pid, bool force)
{
    QStringList args{QStringLiteral("/PID"), QString::number(pid)};
    if (force) {
        args << QStringLiteral("/T") << QStringLiteral("/F");
    }
    const ProcessResult result = runProcess(QStringLiteral("taskkill.exe"), args, 15000);
    return QStringList{QString::fromLocal8Bit(result.stdoutData), QString::fromLocal8Bit(result.stderrData)};
}

} // namespace Noble
