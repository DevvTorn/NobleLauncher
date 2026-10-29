#include "OptimizerService.h"

#include "InstanceManager.h"
#include "JavaManager.h"
#include "Json.h"
#include "Logger.h"
#include "Paths.h"
#include "ProcessUtil.h"
#include "Settings.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>

namespace Noble {

static const QString kHighPerformanceScheme = QStringLiteral("8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c");

namespace {

QString ps(const QString &value)
{
    return QStringLiteral("'") + QString(value).replace(QLatin1Char('\''), QStringLiteral("''")) + QStringLiteral("'");
}

QString backupFile()
{
    return QDir(Paths::optimizerDir()).filePath(QStringLiteral("backup.json"));
}

QString stateFile()
{
    return QDir(Paths::optimizerDir()).filePath(QStringLiteral("state.json"));
}

QString resultFile()
{
    return QDir(Paths::optimizerDir()).filePath(QStringLiteral("result.json"));
}

QJsonObject readStateObject()
{
    return readJsonObject(stateFile());
}

QStringList toStringList(const QJsonValue &value)
{
    QStringList result;
    if (value.isString()) {
        result << value.toString();
    } else if (value.isArray()) {
        for (const auto &item : value.toArray()) {
            if (item.isString())
                result << item.toString();
        }
    }
    return result;
}

QVariantMap resultFromJson(const QJsonObject &json, bool cancelled)
{
    QVariantMap result;
    result.insert(QStringLiteral("applied"), toStringList(json.value(QStringLiteral("applied"))));
    result.insert(QStringLiteral("errors"), toStringList(json.value(QStringLiteral("errors"))));
    result.insert(QStringLiteral("log"), toStringList(json.value(QStringLiteral("log"))));
    result.insert(QStringLiteral("cancelled"), cancelled);
    return result;
}

QVariantMap runScript(const QString &script, const QString &gameExe, Settings *settings)
{
    QDir().mkpath(Paths::optimizerDir());
    const QString scriptPath = QDir(Paths::optimizerDir()).filePath(QStringLiteral("task.ps1"));
    QFile file(scriptPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {{QStringLiteral("applied"), QStringList()},
                {QStringLiteral("errors"), QStringList{QStringLiteral("Не удалось записать скрипт оптимизации")}},
                {QStringLiteral("log"), QStringList()}};
    }
    file.write((QString(QChar(0xFEFF)) + script).toUtf8());
    file.close();
    QFile::remove(resultFile());
    const bool elevated = runPowerShellElevated(scriptPath, 180000);
    Q_UNUSED(gameExe)
    Q_UNUSED(settings)
    if (!elevated && !QFileInfo::exists(resultFile())) {
        Logger::info(QStringLiteral("Оптимизация: запрос прав администратора отклонён"));
        return {{QStringLiteral("applied"), QStringList()},
                {QStringLiteral("errors"), QStringList()},
                {QStringLiteral("log"), QStringList{QStringLiteral("Запрос прав администратора отклонён.")}},
                {QStringLiteral("cancelled"), true}};
    }
    QFile result(resultFile());
    if (!result.open(QIODevice::ReadOnly)) {
        return {{QStringLiteral("applied"), QStringList()},
                {QStringLiteral("errors"), QStringList{QStringLiteral("Не удалось прочитать результат оптимизации")}},
                {QStringLiteral("log"), QStringList()}};
    }
    QByteArray data = result.readAll();
    if (data.startsWith("\xEF\xBB\xBF"))
        data.remove(0, 3);
    QJsonObject json = QJsonDocument::fromJson(data).object();
    return resultFromJson(json, false);
}

QStringList defaultOps(const QVariantList &instances, Settings *settings)
{
    Q_UNUSED(instances)
    Q_UNUSED(settings)
    return {QStringLiteral("gpu"), QStringLiteral("priority"), QStringLiteral("fullscreen"), QStringLiteral("gamemode"),
            QStringLiteral("power")};
}

} // namespace

QString OptimizerService::gameExecutable(Settings *settings, const QVariantList &instances)
{
    for (const auto &value : instances) {
        const QVariantMap instance = value.toMap();
        const QString mc = instance.value(QStringLiteral("version")).toString();
        const QString manual = instance.value(QStringLiteral("javaPath")).toString().isEmpty()
            ? ((settings && settings->javaMode() == QLatin1String("manual")) ? settings->javaPath() : QString())
            : instance.value(QStringLiteral("javaPath")).toString();
        QString error;
        const PickedJava picked = JavaManager::pickFor(mc, manual, instance.value(QStringLiteral("loader")).toString(), &error);
        if (!picked.path.isEmpty() && QFileInfo::exists(picked.path))
            return picked.path;
    }
    return QString();
}

QVariantMap OptimizerService::state()
{
    const QJsonObject data = readStateObject();
    return {{QStringLiteral("lastAppliedAt"), static_cast<double>(data.value(QStringLiteral("lastAppliedAt")).toDouble())},
            {QStringLiteral("lastLog"), data.value(QStringLiteral("lastLog")).toArray().toVariantList()},
            {QStringLiteral("backupAt"), static_cast<double>(data.value(QStringLiteral("backupAt")).toDouble())}};
}

QVariantMap OptimizerService::inspect(Settings *settings, const QVariantList &instances)
{
    QVariantMap result;
    const QString exe = gameExecutable(settings, instances);
    const bool hasExe = !exe.isEmpty();
    const QJsonObject state = readStateObject();

    bool gpuApplied = false;
    bool fsApplied = false;
    bool gmApplied = false;
    bool powerApplied = false;
#ifdef Q_OS_WIN
    if (hasExe) {
        QSettings gpu(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\DirectX\\UserGpuPreferences"), QSettings::NativeFormat);
        gpuApplied = gpu.value(exe).toString().contains(QLatin1String("GpuPreference=2"));
        QSettings layers(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers"),
                         QSettings::NativeFormat);
        fsApplied = layers.value(exe).toString().contains(QLatin1String("DISABLEDXMAXIMIZEDWINDOWEDMODE"));
        QSettings gameBar(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\GameBar"), QSettings::NativeFormat);
        gmApplied = gameBar.value(QStringLiteral("AutoGameModeEnabled")).toInt() == 1;
    }
    const ProcessResult scheme = runProcess(QStringLiteral("powercfg.exe"), {QStringLiteral("/getactivescheme")}, 15000);
    const QRegularExpression re(QStringLiteral("([0-9a-fA-F-]{36})"));
    const auto match = re.match(QString::fromLocal8Bit(scheme.stdoutData));
    if (match.hasMatch())
        powerApplied = match.captured(1).toLower() == kHighPerformanceScheme.toLower();
#endif

    QVariantList ops;
    auto addOp = [&](const QString &id, const QString &titleRu, const QString &titleEn, const QString &detailRu,
                     const QString &detailEn, const QString &stateText, bool optional, bool admin) {
        ops.append(QVariantMap{{QStringLiteral("id"), id},
                               {QStringLiteral("titleRu"), titleRu},
                               {QStringLiteral("titleEn"), titleEn},
                               {QStringLiteral("detailRu"), detailRu},
                               {QStringLiteral("detailEn"), detailEn},
                               {QStringLiteral("state"), stateText},
                               {QStringLiteral("optional"), optional},
                               {QStringLiteral("admin"), admin}});
    };
    addOp(QStringLiteral("gpu"), QStringLiteral("Высокая производительность GPU"),
          QStringLiteral("High GPU performance"),
          hasExe ? QStringLiteral("Windows будет запускать Minecraft на дискретной видеокарте, а не на встроенной.")
                 : QStringLiteral("Сначала установите версию игры (нужен javaw.exe сборки)."),
          hasExe ? QStringLiteral("Windows will run Minecraft on the dedicated GPU instead of the integrated one.")
                 : QStringLiteral("Install a game version first (a build javaw.exe is required)."),
          !hasExe ? QStringLiteral("unsupported") : (gpuApplied ? QStringLiteral("applied") : QStringLiteral("pending")),
          false, false);
    addOp(QStringLiteral("priority"), QStringLiteral("Высокий приоритет процесса игры"),
          QStringLiteral("High game process priority"),
          QStringLiteral("Лаунчер будет поднимать приоритет javaw.exe при каждом запуске игры."),
          QStringLiteral("The launcher will raise javaw.exe priority on every game start."),
          settings && settings->optimizeHighPriority() ? QStringLiteral("applied") : QStringLiteral("pending"), false, false);
    addOp(QStringLiteral("fullscreen"), QStringLiteral("Отключить fullscreen optimizations"),
          QStringLiteral("Disable fullscreen optimizations"),
          hasExe ? QStringLiteral("Убирает перехват полноэкранного режима Windows — меньше микрофризов.")
                 : QStringLiteral("Сначала установите версию игры (нужен javaw.exe сборки)."),
          hasExe ? QStringLiteral("Removes the Windows fullscreen interception — fewer micro stutters.")
                 : QStringLiteral("Install a game version first (a build javaw.exe is required)."),
          !hasExe ? QStringLiteral("unsupported") : (fsApplied ? QStringLiteral("applied") : QStringLiteral("pending")),
          false, false);
    addOp(QStringLiteral("gamemode"), QStringLiteral("Игровой режим Windows"), QStringLiteral("Windows Game Mode"),
          QStringLiteral("Включает Game Mode: система отдаёт ресурсы игре и не мешает фоновыми задачами."),
          QStringLiteral("Enables Game Mode: the system prioritizes the game over background tasks."),
          gmApplied ? QStringLiteral("applied") : QStringLiteral("pending"), false, false);
    addOp(QStringLiteral("power"), QStringLiteral("План питания «Высокая производительность»"),
          QStringLiteral("High performance power plan"),
          QStringLiteral("Запрещает процессору сбрасывать частоты во время игры (на ноутбуках может расходовать батарею)."),
          QStringLiteral("Keeps the CPU from downclocking during the game (may drain laptop battery)."),
          powerApplied ? QStringLiteral("applied") : QStringLiteral("pending"), true, true);

    result.insert(QStringLiteral("supported"), true);
    result.insert(QStringLiteral("platform"), QStringLiteral("windows"));
    result.insert(QStringLiteral("gameExe"), exe);
    result.insert(QStringLiteral("ops"), ops);
    result.insert(QStringLiteral("backupAt"), static_cast<double>(state.value(QStringLiteral("backupAt")).toDouble()));
    result.insert(QStringLiteral("lastAppliedAt"), static_cast<double>(state.value(QStringLiteral("lastAppliedAt")).toDouble()));
    result.insert(QStringLiteral("lastLog"), state.value(QStringLiteral("lastLog")).toArray().toVariantList());
    return result;
}

QVariantList OptimizerService::processes()
{
    QVariantList list;
    for (const auto &process : listBackgroundProcesses()) {
        list.append(QVariantMap{{QStringLiteral("pid"), static_cast<double>(process.pid)},
                                {QStringLiteral("name"), process.name},
                                {QStringLiteral("title"), process.title},
                                {QStringLiteral("memoryMb"), process.memoryMb}});
    }
    return list;
}

static QString applyScript(const QStringList &ops, const QString &exe)
{
    const bool applyGpu = ops.contains(QStringLiteral("gpu")) && !exe.isEmpty();
    const bool applyFs = ops.contains(QStringLiteral("fullscreen")) && !exe.isEmpty();
    const bool applyGm = ops.contains(QStringLiteral("gamemode"));
    const bool applyPower = ops.contains(QStringLiteral("power"));
    QString script = QStringLiteral(R"PS(
$ErrorActionPreference = 'Continue'
$log = New-Object System.Collections.Generic.List[string]
$applied = New-Object System.Collections.Generic.List[string]
$errors = New-Object System.Collections.Generic.List[string]
$backupPath = %1
$resultPath = %2
$exe = %3

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$admin = (New-Object Security.Principal.WindowsPrincipal($identity)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $admin) {
  $errors.Add('Нужны права администратора, но процесс запущен без них.')
  @{ applied = @(); errors = $errors; log = $log } | ConvertTo-Json -Depth 5 | Set-Content -Path $resultPath -Encoding UTF8
  exit 1
}

$backup = $null
if (Test-Path $backupPath) {
  try { $backup = Get-Content -Raw -Path $backupPath | ConvertFrom-Json } catch { $backup = $null }
}
if (-not $backup) {
  $b = [ordered]@{ createdAt = (Get-Date).ToUniversalTime().ToString('o'); app = 'Noble Launcher' }
  $k = Get-ItemProperty -Path 'HKCU:\Software\Microsoft\DirectX\UserGpuPreferences' -ErrorAction SilentlyContinue
  $existed = $false; $old = $null
  if ($exe -and $k -and ($k.PSObject.Properties.Name -contains $exe)) { $existed = $true; $old = [string]$k.$exe }
  $b.gpu = @{ name = $exe; existed = $existed; old = $old }
  $l = Get-ItemProperty -Path 'HKCU:\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers' -ErrorAction SilentlyContinue
  $existed = $false; $old = $null
  if ($exe -and $l -and ($l.PSObject.Properties.Name -contains $exe)) { $existed = $true; $old = [string]$l.$exe }
  $b.fullscreen = @{ name = $exe; existed = $existed; old = $old }
  $g = Get-ItemProperty -Path 'HKCU:\Software\Microsoft\GameBar' -ErrorAction SilentlyContinue
  $b.gamemode = @{
    auto = @{ existed = [bool]($g -and $null -ne $g.AutoGameModeEnabled); old = if ($g) { $g.AutoGameModeEnabled } else { $null } }
    allow = @{ existed = [bool]($g -and $null -ne $g.AllowAutoGameMode); old = if ($g) { $g.AllowAutoGameMode } else { $null } }
  }
  $scheme = (powercfg /getactivescheme) 2>$null
  $oldGuid = $null
  if ($scheme -match '([0-9a-fA-F-]{36})') { $oldGuid = $Matches[1] }
  $b.power = @{ oldGuid = $oldGuid }
  New-Item -ItemType Directory -Force -Path (Split-Path $backupPath) | Out-Null
  $b | ConvertTo-Json -Depth 6 | Set-Content -Path $backupPath -Encoding UTF8
  $log.Add('Сохранена точка отката.')
} else {
  $log.Add('Точка отката уже существует — исходные значения сохранены ранее.')
}
)PS").arg(ps(backupFile()), ps(resultFile()), ps(exe));

    if (applyGpu) {
        script += QStringLiteral(R"PS(
try {
  New-Item -Path 'HKCU:\Software\Microsoft\DirectX\UserGpuPreferences' -Force | Out-Null
  Set-ItemProperty -Path 'HKCU:\Software\Microsoft\DirectX\UserGpuPreferences' -Name $exe -Value 'GpuPreference=2;' -Type String
  $applied.Add('gpu'); $log.Add('GPU: высокая производительность для javaw.exe.')
} catch { $errors.Add("GPU: $($_.Exception.Message)") }
)PS");
    }
    if (applyFs) {
        script += QStringLiteral(R"PS(
try {
  New-Item -Path 'HKCU:\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers' -Force | Out-Null
  Set-ItemProperty -Path 'HKCU:\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers' -Name $exe -Value '~ DISABLEDXMAXIMIZEDWINDOWEDMODE' -Type String
  $applied.Add('fullscreen'); $log.Add('Отключены fullscreen optimizations.')
} catch { $errors.Add("Fullscreen: $($_.Exception.Message)") }
)PS");
    }
    if (applyGm) {
        script += QStringLiteral(R"PS(
try {
  New-Item -Path 'HKCU:\Software\Microsoft\GameBar' -Force | Out-Null
  Set-ItemProperty -Path 'HKCU:\Software\Microsoft\GameBar' -Name 'AutoGameModeEnabled' -Value 1 -Type DWord
  Set-ItemProperty -Path 'HKCU:\Software\Microsoft\GameBar' -Name 'AllowAutoGameMode' -Value 1 -Type DWord
  $applied.Add('gamemode'); $log.Add('Включён игровой режим Windows.')
} catch { $errors.Add("Game Mode: $($_.Exception.Message)") }
)PS");
    }
    if (applyPower) {
        script += QStringLiteral(R"PS(
try {
  powercfg /setactive %1 | Out-Null
  if ($LASTEXITCODE -ne 0) { $errors.Add('План «Высокая производительность» недоступен на этом устройстве.') }
  else { $applied.Add('power'); $log.Add('Включён план питания «Высокая производительность».') }
} catch { $errors.Add("Питание: $($_.Exception.Message)") }
)PS").arg(kHighPerformanceScheme);
    }
    script += QStringLiteral(R"PS(
@{ applied = @($applied); errors = @($errors); log = @($log) } | ConvertTo-Json -Depth 5 | Set-Content -Path $resultPath -Encoding UTF8
exit 0
)PS");
    return script;
}

static QString rollbackScript()
{
    return QStringLiteral(R"PS(
$ErrorActionPreference = 'Continue'
$log = New-Object System.Collections.Generic.List[string]
$applied = New-Object System.Collections.Generic.List[string]
$errors = New-Object System.Collections.Generic.List[string]
$backupPath = %1
$resultPath = %2

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$admin = (New-Object Security.Principal.WindowsPrincipal($identity)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $admin) {
  $errors.Add('Нужны права администратора, но процесс запущен без них.')
  @{ applied = @(); errors = $errors; log = $log } | ConvertTo-Json -Depth 5 | Set-Content -Path $resultPath -Encoding UTF8
  exit 1
}
if (-not (Test-Path $backupPath)) {
  $errors.Add('Точка отката не найдена — вероятно, оптимизация ещё не применялась.')
  @{ applied = @(); errors = $errors; log = $log } | ConvertTo-Json -Depth 5 | Set-Content -Path $resultPath -Encoding UTF8
  exit 1
}
try { $b = Get-Content -Raw -Path $backupPath | ConvertFrom-Json } catch {
  $errors.Add('Не удалось прочитать точку отката.')
  @{ applied = @(); errors = $errors; log = $log } | ConvertTo-Json -Depth 5 | Set-Content -Path $resultPath -Encoding UTF8
  exit 1
}
if ($b.gpu -and $b.gpu.name) {
  try {
    if ($b.gpu.existed) { Set-ItemProperty -Path 'HKCU:\Software\Microsoft\DirectX\UserGpuPreferences' -Name $b.gpu.name -Value $b.gpu.old -Type String; $log.Add('GPU: возвращено прежнее значение.') }
    else { Remove-ItemProperty -Path 'HKCU:\Software\Microsoft\DirectX\UserGpuPreferences' -Name $b.gpu.name -ErrorAction SilentlyContinue; $log.Add('GPU: настройка удалена (как было).') }
    $applied.Add('gpu')
  } catch { $errors.Add("GPU: $($_.Exception.Message)") }
}
if ($b.fullscreen -and $b.fullscreen.name) {
  try {
    if ($b.fullscreen.existed) { Set-ItemProperty -Path 'HKCU:\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers' -Name $b.fullscreen.name -Value $b.fullscreen.old -Type String; $log.Add('Fullscreen optimizations: возвращено прежнее значение.') }
    else { Remove-ItemProperty -Path 'HKCU:\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers' -Name $b.fullscreen.name -ErrorAction SilentlyContinue; $log.Add('Fullscreen optimizations: настройка удалена (как было).') }
    $applied.Add('fullscreen')
  } catch { $errors.Add("Fullscreen: $($_.Exception.Message)") }
}
if ($b.gamemode) {
  try {
    if ($b.gamemode.auto.existed) { Set-ItemProperty -Path 'HKCU:\Software\Microsoft\GameBar' -Name 'AutoGameModeEnabled' -Value ([int]$b.gamemode.auto.old) -Type DWord }
    else { Remove-ItemProperty -Path 'HKCU:\Software\Microsoft\GameBar' -Name 'AutoGameModeEnabled' -ErrorAction SilentlyContinue }
    if ($b.gamemode.allow.existed) { Set-ItemProperty -Path 'HKCU:\Software\Microsoft\GameBar' -Name 'AllowAutoGameMode' -Value ([int]$b.gamemode.allow.old) -Type DWord }
    else { Remove-ItemProperty -Path 'HKCU:\Software\Microsoft\GameBar' -Name 'AllowAutoGameMode' -ErrorAction SilentlyContinue }
    $log.Add('Игровой режим: возвращены прежние значения.')
    $applied.Add('gamemode')
  } catch { $errors.Add("Game Mode: $($_.Exception.Message)") }
}
if ($b.power -and $b.power.oldGuid) {
  try {
    powercfg /setactive $b.power.oldGuid | Out-Null
    if ($LASTEXITCODE -ne 0) { $errors.Add('Не удалось вернуть прежний план питания.') }
    else { $log.Add('План питания: возвращён прежний.'); $applied.Add('power') }
  } catch { $errors.Add("Питание: $($_.Exception.Message)") }
}
Remove-Item -Path $backupPath -ErrorAction SilentlyContinue
$log.Add('Точка отката удалена.')
@{ applied = @($applied); errors = @($errors); log = @($log) } | ConvertTo-Json -Depth 5 | Set-Content -Path $resultPath -Encoding UTF8
exit 0
)PS").arg(ps(backupFile()), ps(resultFile()));
}

QVariantMap OptimizerService::apply(const QStringList &ops, const QList<qint64> &pids, const QVariantList &instances,
                                    Settings *settings)
{
    QStringList effective;
    for (const auto &op : ops) {
        if (op == QLatin1String("priority"))
            continue;
        effective << op;
    }
    const QString exe = gameExecutable(settings, instances);
    QVariantMap result;
    QStringList applied;
    QStringList errors;
    QStringList log;
    if (!effective.isEmpty()) {
        QVariantMap scriptResult = runScript(applyScript(effective, exe), exe, settings);
        if (scriptResult.value(QStringLiteral("cancelled")).toBool())
            return scriptResult;
        applied = scriptResult.value(QStringLiteral("applied")).toStringList();
        errors = scriptResult.value(QStringLiteral("errors")).toStringList();
        log = scriptResult.value(QStringLiteral("log")).toStringList();
    }
    if (ops.contains(QStringLiteral("priority"))) {
        applied << QStringLiteral("priority");
        log << QStringLiteral("Высокий приоритет игры будет применяться при каждом запуске.");
    }
    const QStringList killed = killBackgroundProcesses(pids);
    if (!killed.isEmpty())
        log << QStringLiteral("Закрыты процессы: %1").arg(killed.join(QStringLiteral(", ")));

    QJsonObject state = readStateObject();
    state.insert(QStringLiteral("lastAppliedAt"), static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    QJsonArray logArray;
    for (const auto &entry : log)
        logArray.append(entry);
    for (const auto &entry : errors)
        logArray.append(QStringLiteral("Ошибка: ") + entry);
    state.insert(QStringLiteral("lastLog"), logArray);
    state.insert(QStringLiteral("backupAt"), QFileInfo::exists(backupFile())
                                                  ? state.value(QStringLiteral("backupAt")).toDouble(QDateTime::currentMSecsSinceEpoch())
                                                  : QJsonValue::Null);
    writeJsonObject(stateFile(), state);
    Logger::info(QStringLiteral("Оптимизация применена: %1").arg(applied.join(QStringLiteral(", "))));

    result.insert(QStringLiteral("applied"), applied);
    result.insert(QStringLiteral("errors"), errors);
    result.insert(QStringLiteral("log"), log);
    result.insert(QStringLiteral("cancelled"), false);
    return result;
}

QVariantMap OptimizerService::rollback(Settings *settings)
{
    if (!QFileInfo::exists(backupFile())) {
        return {{QStringLiteral("applied"), QStringList()},
                {QStringLiteral("errors"), QStringList{QStringLiteral("Точка отката не найдена — оптимизация ещё не применялась.")}},
                {QStringLiteral("log"), QStringList()}};
    }
    QVariantMap scriptResult = runScript(rollbackScript(), QString(), settings);
    if (scriptResult.value(QStringLiteral("cancelled")).toBool())
        return scriptResult;
    QStringList log = scriptResult.value(QStringLiteral("log")).toStringList();
    log << QStringLiteral("Высокий приоритет игры выключен.");
    QStringList applied = scriptResult.value(QStringLiteral("applied")).toStringList();
    QJsonObject state = readStateObject();
    state.insert(QStringLiteral("lastAppliedAt"), QJsonValue::Null);
    state.insert(QStringLiteral("backupAt"), QJsonValue::Null);
    state.insert(QStringLiteral("lastLog"), QJsonArray::fromStringList(log));
    writeJsonObject(stateFile(), state);
    Logger::info(QStringLiteral("Оптимизация откатана: %1").arg(applied.join(QStringLiteral(", "))));
    return {{QStringLiteral("applied"), applied},
            {QStringLiteral("errors"), scriptResult.value(QStringLiteral("errors"))},
            {QStringLiteral("log"), log},
            {QStringLiteral("cancelled"), false}};
}

} // namespace Noble
