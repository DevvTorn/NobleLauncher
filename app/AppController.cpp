#include "AppController.h"

#include "core/AccountManager.h"
#include "core/AchievementCatalog.h"
#include "core/BackgroundsService.h"
#include "core/E4SteamService.h"
#include "Env.h"
#include "core/FontsService.h"
#include "core/GameLauncher.h"
#include "core/InstanceManager.h"
#include "core/Invoke.h"
#include "core/JavaManager.h"
#include "core/Json.h"
#include "core/Logger.h"
#include "core/ModQueue.h"
#include "core/ModsScanner.h"
#include "core/NbFormat.h"
#include "core/ModrinthService.h"
#include "core/NewsService.h"
#include "core/NobleBuildsService.h"
#include "core/OptimizerService.h"
#include "core/Paths.h"
#include "core/ProfileService.h"
#include "ProcessUtil.h"
#include "core/Pure.h"
#include "core/Settings.h"
#include "core/SkinService.h"
#include "core/StatsService.h"
#include "core/StorageService.h"
#include "core/TaskRunner.h"
#include "core/UpdaterService.h"
#include "core/VersionManager.h"
#include "Zip.h"

#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QPointer>
#include <QProcess>
#include <QSet>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace Noble {

AppController::AppController(QObject *parent)
    : QObject(parent)
{
}

AppController::~AppController()
{
    if (m_settings)
        m_settings->saveNow();
    Logger::setSink({});
}

QString AppController::lang() const
{
    return m_settings ? m_settings->lang() : QStringLiteral("ru");
}

static bool osAnimationsEnabled()
{
#ifdef Q_OS_WIN
    BOOL enabled = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0))
        return enabled != FALSE;
#endif
    return true;
}

bool AppController::reducedMotion() const
{
    return !osAnimationsEnabled();
}

bool AppController::animationsEnabled() const
{
    return m_settings && m_settings->animations() && osAnimationsEnabled();
}

QString AppController::fontFamily() const
{
    return m_fonts ? m_fonts->family() : QStringLiteral("Segoe UI");
}

static bool isDark(const Settings *settings)
{
    return !settings || settings->theme() != QLatin1String("light");
}

QString AppController::colorBg() const { return isDark(m_settings) ? QStringLiteral("#0b0b0d") : QStringLiteral("#f2f2f5"); }
QString AppController::colorSurface() const { return isDark(m_settings) ? QStringLiteral("#121216") : QStringLiteral("#ffffff"); }
QString AppController::colorSurfaceAlt() const { return isDark(m_settings) ? QStringLiteral("#191920") : QStringLiteral("#f7f7fa"); }
QString AppController::colorBorder() const { return isDark(m_settings) ? QStringLiteral("#2a2a33") : QStringLiteral("#dcdce4"); }
QString AppController::colorText() const { return isDark(m_settings) ? QStringLiteral("#f4f4f7") : QStringLiteral("#16161b"); }
QString AppController::colorTextDim() const { return isDark(m_settings) ? QStringLiteral("#9d9daa") : QStringLiteral("#666673"); }
QString AppController::colorAccent() const
{
    const QString accent = m_settings ? m_settings->accent() : QStringLiteral("#e11d2e");
    return QColor(accent).isValid() ? accent : QStringLiteral("#e11d2e");
}
QString AppController::colorAccentHover() const
{
    QColor color(colorAccent());
    return color.lighter(115).name();
}
QString AppController::colorAccentSoft() const
{
    QColor color(colorAccent());
    color.setAlpha(isDark(m_settings) ? 40 : 28);
    return color.name(QColor::HexArgb);
}
QString AppController::colorSuccess() const { return isDark(m_settings) ? QStringLiteral("#37d39c") : QStringLiteral("#0d8f63"); }
QString AppController::colorWarn() const { return isDark(m_settings) ? QStringLiteral("#f0b429") : QStringLiteral("#a86f00"); }
QString AppController::colorError() const { return isDark(m_settings) ? QStringLiteral("#ff5c66") : QStringLiteral("#c81e2b"); }
double AppController::spacingScale() const
{
    return m_settings && m_settings->density() == QLatin1String("compact") ? 0.82 : 1.0;
}

QVariantList AppController::accounts() const { return m_accounts ? m_accounts->accounts() : QVariantList(); }
QVariantMap AppController::selectedAccount() const { return m_accounts ? m_accounts->selectedAccount() : QVariantMap(); }
QString AppController::selectedAccountId() const { return m_accounts ? m_accounts->selectedId() : QString(); }
bool AppController::accountsBusy() const { return m_accounts && m_accounts->busy(); }
QString AppController::oauthStatus() const { return m_accounts ? m_accounts->oauthStatus() : QString(); }

QVariantList AppController::instances() const { return m_instances ? m_instances->instances() : QVariantList(); }
QString AppController::selectedInstanceId() const { return m_instances ? m_instances->selectedInstanceId() : QString(); }
void AppController::setSelectedInstanceId(const QString &id)
{
    if (m_instances)
        m_instances->setSelectedInstanceId(id);
}
QVariantMap AppController::selectedInstance() const { return m_instances ? m_instances->selectedInstance() : QVariantMap(); }

QVariantList AppController::queueItems() const { return m_queue ? m_queue->items() : QVariantList(); }
double AppController::queueProgress() const { return m_queue ? m_queue->overallProgress() : 0; }
int AppController::queueActive() const { return m_queue ? m_queue->activeCount() : 0; }
QVariantList AppController::newsItems() const { return m_news ? m_news->items() : QVariantList(); }
QVariantList AppController::backgrounds() const { return m_backgrounds ? m_backgrounds->items() : QVariantList(); }
QVariantList AppController::customFonts() const { return m_fonts ? m_fonts->items() : QVariantList(); }
QVariantList AppController::nobleBuilds() const { return m_noble ? m_noble->builds() : QVariantList(); }
bool AppController::nobleLoading() const { return m_noble && m_noble->loading(); }
bool AppController::nobleFromCache() const { return m_noble && m_noble->fromCache(); }
QVariantMap AppController::steamStatus() const { return m_steam ? m_steam->status() : QVariantMap(); }
QVariantMap AppController::updateStatus() const { return m_updater ? m_updater->status() : QVariantMap(); }
QVariantMap AppController::statsSummary() const { return m_stats ? m_stats->summary() : QVariantMap(); }
QString AppController::skinSource() const { return m_skin ? m_skin->skinSource() : QStringLiteral("qrc:/assets/steve.png"); }
QString AppController::avatarSource() const { return m_skin ? m_skin->avatarSource() : QStringLiteral("qrc:/assets/steve.png"); }
QString AppController::skinName() const { return m_skin ? m_skin->skinName() : QStringLiteral("Steve"); }
bool AppController::skinLoading() const { return m_skin && m_skin->skinLoading(); }
QString AppController::skinError() const { return m_skin ? m_skin->skinError() : QString(); }
QString AppController::profileIconSource() const { return m_profile ? m_profile->profileIconSource() : QString(); }
QString AppController::gamePhase() const { return m_launcher ? m_launcher->phase() : QStringLiteral("idle"); }
qint64 AppController::gamePid() const { return m_launcher ? m_launcher->pid() : 0; }
int AppController::gameElapsed() const { return m_launcher ? m_launcher->elapsedSeconds() : 0; }
bool AppController::taskRunning() const { return m_task && m_task->running(); }
QString AppController::taskTitle() const { return m_task ? m_task->title() : QString(); }
QString AppController::taskStage() const { return m_task ? m_task->stage() : QString(); }
QString AppController::taskMessage() const { return m_task ? m_task->message() : QString(); }
double AppController::taskProgress() const { return m_task ? m_task->progress() : 0; }
bool AppController::taskCancellable() const { return m_task && m_task->cancellable(); }

QString AppController::backgroundSource() const
{
    if (!m_settings || !m_backgrounds)
        return QString();
    const QString id = m_settings->activeBackgroundId();
    return id.isEmpty() ? QString() : m_backgrounds->sourceFor(id);
}

QString AppController::envMissing() const
{
    return Env::instance().missingRequired().join(QStringLiteral(", "));
}

bool AppController::elyKeysConfigured() const
{
    const Env &env = Env::instance();
    return !env.elybyClientId().isEmpty() && !env.elybyClientSecret().isEmpty();
}

QString AppController::dataDir() const { return Paths::dataDir(); }
QString AppController::logFile() const { return Logger::filePath(); }

void AppController::setupConnections()
{
    connect(m_settings, &Settings::changed, this, [this] {
        m_fonts->apply();
        emit settingsChanged();
        emit appearanceChanged();
    });
    connect(m_settings, &Settings::introReplayRequested, this, [this] { emit toast(QStringLiteral("__replay_intro__"), QStringLiteral("info")); });

    connect(m_accounts, &AccountManager::accountsChanged, this, [this] {
        updateSkinForAccount();
        emit accountsChanged();
    });
    connect(m_accounts, &AccountManager::selectionChanged, this, [this] {
        updateSkinForAccount();
        emit accountsChanged();
    });
    connect(m_accounts, &AccountManager::busyChanged, this, &AppController::accountsBusyChanged);
    connect(m_accounts, &AccountManager::oauthStatusChanged, this, &AppController::oauthStatusChanged);
    connect(m_accounts, &AccountManager::toastRequested, this, [this](const QString &message, const QString &type) {
        emit toast(message, type);
        appendConsole(type == QLatin1String("error") ? QStringLiteral("error") : QStringLiteral("info"), message);
    });
    connect(m_accounts, &AccountManager::accountAdded, this, [this](const QString &, const QString &provider) {
        if (provider == QLatin1String("elyby"))
            m_stats->inc(QStringLiteral("elybyLogins"));
        else if (provider == QLatin1String("offline"))
            m_stats->inc(QStringLiteral("offlineAccounts"));
        updateSkinForAccount();
    });
    connect(m_accounts, &AccountManager::accountRefreshed, this, [this](const QString &) { updateSkinForAccount(); });

    connect(m_instances, &InstanceManager::instancesChanged, this, &AppController::instancesChanged);
    connect(m_instances, &InstanceManager::selectionChanged, this, &AppController::instancesChanged);
    connect(m_instances, &InstanceManager::toastRequested, this, [this](const QString &message, const QString &type) {
        emit toast(message, type);
    });
    connect(m_instances, &InstanceManager::migrationHappened, this, [this](int count) {
        QTimer::singleShot(1200, this, [this, count] {
            emit toast(QStringLiteral("Папки сборок переименованы в латинские (%1). Откат — в настройках.").arg(count),
                       QStringLiteral("info"));
        });
    });

    connect(m_stats, &StatsService::summaryChanged, this, &AppController::statsChanged);
    connect(m_stats, &StatsService::achievementUnlocked, this, &AppController::notifyAchievement);
    connect(m_stats, &StatsService::levelUp, this, [this](int level, const QString &titleRu, const QString &) {
        emit toast(QStringLiteral("Новый уровень %1 — %2").arg(level).arg(titleRu), QStringLiteral("success"));
        appendConsole(QStringLiteral("success"), QStringLiteral("[Достижения] Уровень %1 — %2").arg(level).arg(titleRu));
    });

    connect(m_backgrounds, &BackgroundsService::changed, this, [this] {
        emit backgroundsChanged();
        emit appearanceChanged();
    });
    connect(m_backgrounds, &BackgroundsService::toastRequested, this, [this](const QString &message, const QString &type) {
        emit toast(message, type);
    });
    connect(m_fonts, &FontsService::changed, this, [this] {
        emit fontsChanged();
        emit settingsChanged();
    });
    connect(m_fonts, &FontsService::familyChanged, this, &AppController::settingsChanged);
    connect(m_fonts, &FontsService::toastRequested, this, [this](const QString &message, const QString &type) {
        emit toast(message, type);
    });

    connect(m_profile, &ProfileService::profileIconChanged, this, &AppController::profileChanged);
    connect(m_profile, &ProfileService::toastRequested, this, [this](const QString &message, const QString &type) {
        emit toast(message, type);
    });
    connect(m_profile, &ProfileService::skinUploaded, this, [this](const QString &) {
        m_stats->inc(QStringLiteral("skinUploads"));
        QTimer::singleShot(1200, this, [this] { m_skin->refresh(); });
        QTimer::singleShot(7000, this, [this] { m_skin->refresh(); });
    });

    connect(m_skin, &SkinService::skinChanged, this, &AppController::skinChanged);
    connect(m_skin, &SkinService::loadingChanged, this, &AppController::skinLoadingChanged);
    connect(m_skin, &SkinService::toastRequested, this, [this](const QString &message, const QString &type) {
        emit toast(message, type);
    });

    connect(m_launcher, &GameLauncher::phaseChanged, this, [this](const QString &) { emit gameChanged(); });
    connect(m_launcher, &GameLauncher::pidChanged, this, &AppController::gameChanged);
    connect(m_launcher, &GameLauncher::elapsedChanged, this, &AppController::gameChanged);
    connect(m_launcher, &GameLauncher::consoleLine, this, [this](const QString &type, const QString &text) {
        appendConsole(type, text);
    });
    connect(m_launcher, &GameLauncher::gameStarted, this, [this](qint64, const QString &instanceId) {
        const QJsonObject instance = m_instances->get(instanceId);
        m_stats->gameStarted(instanceId, instance.value(QStringLiteral("name")).toString());
        m_instances->markLaunched(instanceId);
        emit instancesChanged();
        emit gameChanged();
    });
    connect(m_launcher, &GameLauncher::gameFinished, this, [this](int code, bool crashed, const QString &instanceId) {
        m_stats->gameEnded(instanceId, m_launcher->elapsedSeconds(), crashed);
        emit gameChanged();
        if (crashed)
            emit toast(QStringLiteral("Игра завершилась с ошибкой (код %1)").arg(code), QStringLiteral("warn"));
        else if (code == 0)
            emit toast(QStringLiteral("Игра завершена"), QStringLiteral("info"));
        m_instances->reload();
        emit instancesChanged();
    });

    connect(m_queue, &ModQueue::queueChanged, this, &AppController::queueChanged);
    connect(m_queue, &ModQueue::contentInstalled, this, [this](const QString &kind, int count) {
        m_stats->contentInstalled(kind, count);
    });
    connect(m_queue, &ModQueue::instanceTouched, this, [this](const QString &id) {
        emit instancesChanged();
        if (id == m_scannedInstanceId)
            scanInstance(id);
    });
    connect(m_queue, &ModQueue::toastRequested, this, [this](const QString &message, const QString &type) {
        emit toast(message, type);
        appendConsole(type == QLatin1String("error") ? QStringLiteral("error") : QStringLiteral("info"), QStringLiteral("[Загрузки] ") + message);
    });

    connect(m_news, &NewsService::itemsChanged, this, &AppController::newsChanged);
    connect(m_noble, &NobleBuildsService::buildsChanged, this, &AppController::nobleChanged);
    connect(m_noble, &NobleBuildsService::loadingChanged, this, &AppController::nobleChanged);
    connect(m_steam, &E4SteamService::statusChanged, this, &AppController::steamChanged);
    connect(m_updater, &UpdaterService::statusChanged, this, &AppController::updateChanged);

    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_scanTimer->start(); });
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_scanTimer->start(); });
    connect(m_scanTimer, &QTimer::timeout, this, &AppController::rescanWatched);
}

void AppController::init()
{
    Paths::ensureDirs();
    Paths::clearCache();
    const QJsonObject migration = Paths::migrateLegacyData();
    Logger::init();
    Env::instance().ensureNextToExecutable();
    Env::instance().load();
    Env::instance().ensureExample();
    Logger::info(QStringLiteral("Данные: %1").arg(Paths::dataDir()));
    if (migration.value(QStringLiteral("count")).toInt() > 0) {
        const int count = migration.value(QStringLiteral("count")).toInt();
        QTimer::singleShot(1400, this, [this, count] {
            emit toast(QStringLiteral("Данные прошлой версии импортированы (%1). Исходные папки не изменены.").arg(count),
                       QStringLiteral("info"));
        });
    }

    Logger::setSink([this](const QString &level, const QString &text) {
        QMetaObject::invokeMethod(this, [this, level, text] {
            if (!m_console)
                return;
            const QString type = level == QLatin1String("error") ? QStringLiteral("error")
                : level == QLatin1String("warn") ? QStringLiteral("warn")
                                                  : QStringLiteral("info");
            m_console->append(type, text);
        }, Qt::QueuedConnection);
    });

    m_settings = new Settings(this);
    m_settings->load();
    JavaManager::setRuntimeRoot(m_settings->runtimeDir());

    m_stats = new StatsService(this);
    m_accounts = new AccountManager(this);
    m_accounts->setSettings(m_settings);
    m_instances = new InstanceManager(this);
    m_skin = new SkinService(this);
    m_backgrounds = new BackgroundsService(this);
    m_fonts = new FontsService(this);
    m_profile = new ProfileService(this);
    m_launcher = new GameLauncher(this);
    m_queue = new ModQueue(this);
    m_news = new NewsService(this);
    m_noble = new NobleBuildsService(this);
    m_steam = new E4SteamService(this);
    m_updater = new UpdaterService(this);

    m_watcher = new QFileSystemWatcher(this);
    m_scanTimer = new QTimer(this);
    m_scanTimer->setSingleShot(true);
    m_scanTimer->setInterval(700);

    m_stats->init();
    m_accounts->load();
    m_instances->init(m_settings);
    m_backgrounds->init(m_settings);
    m_fonts->init(m_settings);
    m_profile->init(m_settings, m_accounts);
    m_queue->setInstances(m_instances);
    setupConnections();
    updateSkinForAccount();
    m_stats->noteDay();

    m_news->load();
    if (m_settings->nobleBuilds())
        m_noble->load(false);
    if (m_settings->autoUpdate())
        m_updater->check(false);

    m_memoryTimer = new QTimer(this);
    m_memoryTimer->setInterval(5000);
    connect(m_memoryTimer, &QTimer::timeout, this, [this] {
        const int value = currentProcessMemoryMb();
        if (value != m_memoryMB) {
            m_memoryMB = value;
            emit memoryChanged();
        }
    });
    m_memoryTimer->start();
    m_memoryMB = currentProcessMemoryMb();

    m_launcherTimer = new QTimer(this);
    m_launcherTimer->setInterval(60000);
    connect(m_launcherTimer, &QTimer::timeout, this, [this] { m_stats->addLauncherMinutes(1); });
    m_launcherTimer->start();

    emit miscChanged();
    appendConsole(QStringLiteral("success"), QStringLiteral("Noble Launcher 2.9.0 готов к работе"));
    Logger::info(QStringLiteral("Инициализация завершена"));
}

void AppController::updateSkinForAccount()
{
    if (!m_skin || !m_accounts)
        return;
    const QVariantMap account = m_accounts->selectedAccount();
    if (account.isEmpty() || account.value(QStringLiteral("provider")).toString() != QLatin1String("elyby")) {
        m_skin->clearAccount();
        return;
    }
    m_skin->setAccount(account.value(QStringLiteral("uuid")).toString(), account.value(QStringLiteral("name")).toString(),
                       account.value(QStringLiteral("skinUrl")).toString());
}

void AppController::appendConsole(const QString &type, const QString &text)
{
    if (!m_console)
        return;
    m_console->append(type, text);
}

void AppController::notifyAchievement(const QVariantMap &achievement)
{
    emit achievementUnlocked(achievement);
    const QString title = m_settings->lang() == QLatin1String("en") ? achievement.value(QStringLiteral("titleEn")).toString()
                                                                     : achievement.value(QStringLiteral("titleRu")).toString();
    const QString message = QStringLiteral("Достижение: «%1» (+%2)").arg(title).arg(achievement.value(QStringLiteral("points")).toInt());
    emit toast(message, QStringLiteral("success"));
    appendConsole(QStringLiteral("success"), QStringLiteral("[Достижения] ") + message);
}

void AppController::startTask(const QString &title, const std::function<void(TaskRunner *)> &fn,
                              const std::function<void(bool, const QString &)> &onFinished, bool cancellable)
{
    if (m_task && m_task->running()) {
        emit toast(QStringLiteral("Дождитесь завершения текущей операции"), QStringLiteral("warn"));
        return;
    }
    TaskRunner *task = new TaskRunner(this);
    m_task = task;
    connect(task, &TaskRunner::progressChanged, this, &AppController::taskChanged);
    connect(task, &TaskRunner::logLineEmitted, this, [this](const QString &type, const QString &text) {
        appendConsole(type, text);
    });
    connect(task, &TaskRunner::finished, this, [this, task, onFinished](bool ok, const QString &error) {
        if (onFinished)
            onFinished(ok, error);
        if (m_task == task) {
            m_task = nullptr;
            emit taskChanged();
        }
        task->deleteLater();
    });
    emit taskChanged();
    task->start(title, fn, cancellable);
}

void AppController::cancelTask()
{
    if (m_task && m_task->running()) {
        m_task->cancel();
        emit toast(QStringLiteral("Операция отменяется…"), QStringLiteral("info"));
    }
}

void AppController::selectAccount(const QString &id) { if (m_accounts) m_accounts->selectAccount(id); }
void AppController::addOfflineAccount(const QString &name) { if (m_accounts) m_accounts->addOffline(name); }
void AppController::addElyByPassword(const QString &username, const QString &password)
{
    if (m_accounts)
        m_accounts->addElyByPassword(username, password);
}
void AppController::addElyByOAuth() { if (m_accounts) m_accounts->addElyByOAuth(); }
void AppController::cancelOAuth() { if (m_accounts) m_accounts->cancelOAuth(); }
void AppController::removeAccount(const QString &id) { if (m_accounts) m_accounts->removeAccount(id); }
void AppController::refreshAccount(const QString &id) { if (m_accounts) m_accounts->refreshAccount(id); }

QVariantMap AppController::createInstance(const QVariantMap &data)
{
    if (!m_instances)
        return {};
    const QVariantMap instance = m_instances->create(data);
    if (!instance.isEmpty()) {
        m_stats->inc(QStringLiteral("builds"));
        emit toast(QStringLiteral("Сборка создана"), QStringLiteral("success"));
        emit instancesChanged();
    }
    return instance;
}

QVariantMap AppController::updateInstance(const QString &id, const QVariantMap &patch)
{
    if (!m_instances)
        return {};
    const QVariantMap instance = m_instances->update(id, patch);
    if (!instance.isEmpty()) {
        emit toast(QStringLiteral("Изменения сохранены"), QStringLiteral("success"));
        emit instancesChanged();
        if (id == m_scannedInstanceId)
            scanInstance(id);
    }
    return instance;
}

void AppController::deleteInstance(const QString &id, bool deleteFiles)
{
    if (!m_instances)
        return;
    const QVariantMap instance = m_instances->get(id).toVariantMap();
    m_instances->remove(id, deleteFiles);
    emit toast(QStringLiteral("Сборка «%1» удалена").arg(instance.value(QStringLiteral("name")).toString()),
               QStringLiteral("success"));
    emit instancesChanged();
}

void AppController::cloneInstance(const QString &id, const QString &name)
{
    if (!m_instances)
        return;
    const QVariantMap clone = m_instances->clone(id, name);
    if (!clone.isEmpty()) {
        m_stats->inc(QStringLiteral("clones"));
        emit toast(QStringLiteral("Сборка склонирована"), QStringLiteral("success"));
        emit instancesChanged();
    }
}

void AppController::exportInstance(const QString &id, const QString &targetUrl)
{
    const QString targetPath = toLocalFile(targetUrl);
    const QJsonObject instance = m_instances ? m_instances->get(id) : QJsonObject();
    if (instance.isEmpty()) {
        emit toast(QStringLiteral("Сборка не найдена"), QStringLiteral("error"));
        return;
    }
    startTask(QStringLiteral("Экспорт сборки"), [this, instance, targetPath](TaskRunner *task) {
        QString error;
        task->report(QStringLiteral("export"), QStringLiteral("Упаковка сборки…"), 0.2);
        if (!zipAddDirectory(targetPath, instance.value(QStringLiteral("gameDir")).toString(),
                             {QStringLiteral("logs"), QStringLiteral("crash-reports"), QStringLiteral(".cache")}, &error)) {
            throw std::runtime_error(error.toStdString());
        }
    }, [this, targetPath](bool ok, const QString &error) {
        if (ok) {
            m_stats->inc(QStringLiteral("exports"));
            emit toast(QStringLiteral("Сборка экспортирована: %1").arg(QFileInfo(targetPath).fileName()), QStringLiteral("success"));
        } else {
            emit toast(QStringLiteral("Экспорт не удался: %1").arg(error), QStringLiteral("error"));
        }
    });
}

void AppController::importInstance(const QString &zipUrl)
{
    const QString zipPath = toLocalFile(zipUrl);
    const bool nb = zipPath.endsWith(QLatin1String(".nb"), Qt::CaseInsensitive);
    auto warningsOut = std::make_shared<QStringList>();
    startTask(QStringLiteral("Импорт сборки"), [this, zipPath, nb, warningsOut](TaskRunner *task) {
        QString error;
        QJsonObject instance;
        if (nb) {
            instance = NbFormat::importArchive(m_instances, zipPath,
                                               [&](const QString &stage, const QString &message, double progress) {
                                                   task->report(stage, message, progress);
                                               },
                                               task->cancelFlag(), &error, warningsOut.get());
        } else {
            QVariantMap imported = m_instances->importArchive(zipPath);
            instance = QJsonObject::fromVariantMap(imported);
        }
        if (instance.isEmpty())
            throw std::runtime_error(error.isEmpty() ? QStringLiteral("Не удалось импортировать сборку").toStdString() : error.toStdString());
        task->setProperty("importedId", instance.value(QStringLiteral("id")).toString());
        task->setProperty("importedWarnings", *warningsOut);
    }, [this, warningsOut](bool ok, const QString &error) {
        if (ok) {
            m_stats->inc(QStringLiteral("imports"));
            m_stats->inc(QStringLiteral("builds"));
            emit toast(QStringLiteral("Сборка импортирована"), QStringLiteral("success"));
            if (!warningsOut->isEmpty())
                emit toast(QStringLiteral("Пропущено файлов: %1").arg(warningsOut->size()), QStringLiteral("warn"));
            emit instancesChanged();
        } else {
            emit toast(QStringLiteral("Импорт не удался: %1").arg(error), QStringLiteral("error"));
        }
    });
}

void AppController::exportInstanceNb(const QString &id, const QString &targetUrl, bool embedFiles)
{
    const QString targetPath = toLocalFile(targetUrl);
    const QJsonObject instance = m_instances ? m_instances->get(id) : QJsonObject();
    if (instance.isEmpty()) {
        emit toast(QStringLiteral("Сборка не найдена"), QStringLiteral("error"));
        return;
    }
    startTask(QStringLiteral("Экспорт .Nb"), [this, instance, targetPath, embedFiles](TaskRunner *task) {
        QString error;
        if (!NbFormat::exportArchive(instance, targetPath, embedFiles,
                                     [&](const QString &stage, const QString &message, double progress) {
                                         task->report(stage, message, progress);
                                     },
                                     &error))
            throw std::runtime_error(error.toStdString());
    }, [this, targetPath](bool ok, const QString &error) {
        if (ok) {
            m_stats->inc(QStringLiteral("exports"));
            emit toast(QStringLiteral("Сборка экспортирована: %1").arg(QFileInfo(targetPath).fileName()), QStringLiteral("success"));
        } else {
            emit toast(QStringLiteral("Экспорт не удался: %1").arg(error), QStringLiteral("error"));
        }
    });
}

void AppController::openInstanceFolder(const QString &id)
{
    if (!m_instances)
        return;
    const QJsonObject instance = m_instances->get(id);
    if (instance.isEmpty())
        return;
    const QString dir = instance.value(QStringLiteral("gameDir")).toString();
    QDir().mkpath(dir);
    openWithDefaultApp(dir);
}

void AppController::setInstanceIcon(const QString &id, const QString &sourcePath)
{
    if (!m_instances)
        return;
    const QString path = toLocalFile(sourcePath);
    m_instances->setIcon(id, path);
    emit toast(QStringLiteral("Иконка обновлена"), QStringLiteral("success"));
    emit instancesChanged();
}

void AppController::installInstanceVersion(const QString &id)
{
    if (!m_instances)
        return;
    const QJsonObject instance = m_instances->get(id);
    if (instance.isEmpty()) {
        emit toast(QStringLiteral("Сборка не найдена"), QStringLiteral("error"));
        return;
    }
    const QString runtime = m_settings->runtimeDir();
    const QString javaMode = m_settings->javaMode();
    const QString javaPath = m_settings->javaPath();
    startTask(QStringLiteral("Установка версии"), [this, instance, runtime, javaMode, javaPath, id](TaskRunner *task) {
        QString error;
        const QString installed = VersionManager::ensureInstanceVersion(
            instance, runtime, javaMode, javaPath,
            Reporter{[&](const QString &stage, const QString &message, double progress) {
                task->report(stage, message, progress);
            }},
            task->cancelFlag(), &error);
        if (installed.isEmpty())
            throw std::runtime_error(error.isEmpty() ? QStringLiteral("Установка не удалась").toStdString() : error.toStdString());
        QVariantMap patch{{QStringLiteral("installedVersionId"), installed}};
        invokeBlocking(m_instances, [this, id, patch] {
            m_instances->update(id, patch);
            return true;
        });
    }, [this](bool ok, const QString &error) {
        if (ok) {
            emit toast(QStringLiteral("Версия установлена"), QStringLiteral("success"));
            emit instancesChanged();
        } else {
            emit toast(QStringLiteral("Установка не удалась: %1").arg(error), QStringLiteral("error"));
            appendConsole(QStringLiteral("error"), QStringLiteral("Установка версии: %1").arg(error));
        }
    });
}

void AppController::uninstallInstanceVersion(const QString &id)
{
    if (!m_instances)
        return;
    const QJsonObject instance = m_instances->get(id);
    if (instance.isEmpty())
        return;
    const QString runtime = m_settings->runtimeDir();
    QStringList dirs{instance.value(QStringLiteral("version")).toString()};
    const QString loader = instance.value(QStringLiteral("loader")).toString();
    if (loader != QLatin1String("Vanilla") && !instance.value(QStringLiteral("loaderVersion")).toString().isEmpty())
        dirs << loaderVersionId(loader, instance.value(QStringLiteral("version")).toString(),
                                instance.value(QStringLiteral("loaderVersion")).toString());
    for (const auto &version : dirs)
        QDir(QDir(runtime).filePath(QStringLiteral("versions/") + version)).removeRecursively();
    m_instances->update(id, {{QStringLiteral("installedVersionId"), QVariant()}});
    emit toast(QStringLiteral("Версия удалена из общего каталога"), QStringLiteral("info"));
    emit instancesChanged();
}

int AppController::rollbackInstanceMigration()
{
    if (!m_instances)
        return 0;
    const int count = m_instances->rollbackMigration();
    emit toast(count > 0 ? QStringLiteral("Папки сборок возвращены (%1)").arg(count)
                         : QStringLiteral("Нечего откатывать"),
               count > 0 ? QStringLiteral("success") : QStringLiteral("info"));
    emit instancesChanged();
    return count;
}

void AppController::loadMinecraftVersions()
{
    if (!m_minecraftVersions.isEmpty()) {
        emit versionsChanged();
        return;
    }
    QPointer<AppController> guard(this);
    QtConcurrent::run([guard] {
        QVariantList list;
        for (const auto &version : VersionManager::listVersions()) {
            list.append(QVariantMap{{QStringLiteral("id"), version.id},
                                    {QStringLiteral("type"), version.type},
                                    {QStringLiteral("releaseTime"), version.releaseTime}});
        }
        if (!guard)
            return;
        QMetaObject::invokeMethod(guard, [guard, list] {
            if (!guard)
                return;
            guard->m_minecraftVersions = list;
            emit guard->versionsChanged();
        }, Qt::QueuedConnection);
    });
}

void AppController::loadLoaderVersions(const QString &loader, const QString &mcVersion)
{
    QPointer<AppController> guard(this);
    QtConcurrent::run([guard, loader, mcVersion] {
        QVariantList list;
        for (const auto &version : VersionManager::listLoaderVersions(loader, mcVersion)) {
            list.append(QVariantMap{{QStringLiteral("version"), version.version},
                                    {QStringLiteral("gameVersion"), version.gameVersion},
                                    {QStringLiteral("stable"), version.stable}});
        }
        if (!guard)
            return;
        QMetaObject::invokeMethod(guard, [guard, list] {
            if (!guard)
                return;
            guard->m_loaderVersions = list;
            emit guard->versionsChanged();
        }, Qt::QueuedConnection);
    });
}

void AppController::detectJava()
{
    QPointer<AppController> guard(this);
    const QString manual = m_settings->javaPath();
    QtConcurrent::run([guard, manual] {
        JavaManager::invalidateDetectionCache();
        const QVariantList list = JavaManager::detectAllVariant(manual);
        if (!guard)
            return;
        QMetaObject::invokeMethod(guard, [guard, list] {
            if (!guard)
                return;
            guard->m_javaList = list;
            emit guard->javaChanged();
        }, Qt::QueuedConnection);
    });
}

void AppController::installJava(int major)
{
    const QString runtime = m_settings->runtimeDir();
    startTask(QStringLiteral("Загрузка Java %1").arg(major), [this, major, runtime](TaskRunner *task) {
        QString error;
        const QString path = JavaManager::installRuntime(major, runtime, [&](int done, int total) {
            task->report(QStringLiteral("java"), QStringLiteral("Java %1: %2/%3").arg(major).arg(done).arg(total),
                         total > 0 ? static_cast<double>(done) / total : 0.1);
        }, task->cancelFlag(), &error);
        if (path.isEmpty())
            throw std::runtime_error(error.toStdString());
    }, [this, major](bool ok, const QString &error) {
        if (ok) {
            m_stats->inc(QStringLiteral("javaInstalls"));
            JavaManager::invalidateDetectionCache();
            emit toast(QStringLiteral("Java %1 установлена").arg(major), QStringLiteral("success"));
            detectJava();
        } else {
            emit toast(QStringLiteral("Java не установлена: %1").arg(error), QStringLiteral("error"));
        }
    });
}

void AppController::searchMods(const QString &query, const QString &projectType, const QString &gameVersion,
                               const QString &loader, int offset)
{
    m_lastSearchQuery = query;
    m_lastSearchType = projectType.isEmpty() ? QStringLiteral("mod") : projectType;
    m_lastSearchVersion = gameVersion;
    m_lastSearchLoader = loader;
    m_modSearchLoading = true;
    emit modSearchChanged();
    QPointer<AppController> guard(this);
    QtConcurrent::run([guard, query, projectType, gameVersion, loader, offset] {
        const QVariantList results = ModrinthService::search(query, projectType, gameVersion, loader, offset, 20);
        if (!guard)
            return;
        QMetaObject::invokeMethod(guard, [guard, results, offset] {
            if (!guard)
                return;
            if (offset > 0)
                guard->m_modSearchResults.append(results);
            else
                guard->m_modSearchResults = results;
            guard->m_modSearchLoading = false;
            emit guard->modSearchChanged();
        }, Qt::QueuedConnection);
    });
}

void AppController::loadMoreMods()
{
    if (m_modSearchLoading)
        return;
    searchMods(m_lastSearchQuery, m_lastSearchType, m_lastSearchVersion, m_lastSearchLoader, m_modSearchResults.size());
}

void AppController::loadModVersions(const QString &projectId, const QString &projectType)
{
    const QString gameVersion = selectedInstance().value(QStringLiteral("version")).toString();
    const QString loader = selectedInstance().value(QStringLiteral("loader")).toString();
    QPointer<AppController> guard(this);
    QtConcurrent::run([guard, projectId, gameVersion, loader, projectType] {
        const QVariantList list = ModrinthService::versionsFor(projectId, gameVersion, loader, projectType);
        if (!guard)
            return;
        QMetaObject::invokeMethod(guard, [guard, list] {
            if (!guard)
                return;
            guard->m_modVersions = list;
            emit guard->modVersionsChanged();
        }, Qt::QueuedConnection);
    });
}

void AppController::enqueueMod(const QString &projectId, const QString &versionId, const QString &kind,
                               const QString &title)
{
    if (!m_instances || !m_queue)
        return;
    const QJsonObject instance = m_instances->get(m_instances->selectedInstanceId());
    if (instance.isEmpty()) {
        emit toast(QStringLiteral("Сначала выберите сборку"), QStringLiteral("warn"));
        return;
    }
    const QVariantMap entry{{QStringLiteral("instanceId"), instance.value(QStringLiteral("id")).toString()},
                            {QStringLiteral("instanceName"), instance.value(QStringLiteral("name")).toString()},
                            {QStringLiteral("kind"), kind},
                            {QStringLiteral("projectId"), projectId},
                            {QStringLiteral("versionId"), versionId},
                            {QStringLiteral("title"), title}};
    m_queue->enqueue({entry});
}

void AppController::installFromUrl(const QString &url)
{
    const QString instanceId = selectedInstanceId();
    startTask(QStringLiteral("Установка по ссылке"), [this, instanceId, url](TaskRunner *task) {
        QString error;
        QString message;
        QJsonObject newInstance = ModrinthService::installFromUrl(
            m_instances, instanceId, url,
            ContentInstallReport{[&](const QString &stage, const QString &msg, double progress, qint64, qint64) {
                task->report(stage, msg, progress);
            }},
            task->cancelFlag(), &error, &message);
        if (!error.isEmpty())
            throw std::runtime_error(error.toStdString());
        task->setProperty("resultMessage", message);
        task->setProperty("newInstanceId", newInstance.value(QStringLiteral("id")).toString());
    }, [this](bool ok, const QString &error) {
        if (ok) {
            emit toast(QStringLiteral("Установка завершена"), QStringLiteral("success"));
            emit instancesChanged();
            scanInstance(selectedInstanceId());
        } else {
            emit toast(QStringLiteral("Установка не удалась: %1").arg(error), QStringLiteral("error"));
        }
    });
}

void AppController::cancelQueueItem(const QString &id) { if (m_queue) m_queue->cancel(id); }
void AppController::retryQueueItem(const QString &id) { if (m_queue) m_queue->retry(id); }
void AppController::clearQueue() { if (m_queue) m_queue->clearFinished(); }

void AppController::scanInstance(const QString &id)
{
    if (!m_instances || id.isEmpty())
        return;
    const QJsonObject instance = m_instances->get(id);
    if (instance.isEmpty())
        return;
    m_scannedInstanceId = id;
    QPointer<AppController> guard(this);
    QtConcurrent::run([guard, instance] {
        const QVariantMap scan = ModsScanner::scanInstance(instance);
        if (!guard)
            return;
        QMetaObject::invokeMethod(guard, [guard, scan, instance] {
            if (!guard)
                return;
            guard->m_modsScan = scan;
            emit guard->modsScanChanged();
            guard->setupWatchersFor(instance, scan);
        }, Qt::QueuedConnection);
    });
}

void AppController::setupWatchersFor(const QJsonObject &instance, const QVariantMap &scan)
{
    Q_UNUSED(instance)
    const QStringList watched = m_watcher->directories();
    if (!watched.isEmpty())
        m_watcher->removePaths(watched);
    for (const auto &value : scan.value(QStringLiteral("dirs")).toList()) {
        const QString dir = value.toMap().value(QStringLiteral("path")).toString();
        if (QFileInfo(dir).isDir() && !m_watcher->directories().contains(dir))
            m_watcher->addPath(dir);
    }
}

void AppController::rescanWatched()
{
    if (!m_scannedInstanceId.isEmpty())
        scanInstance(m_scannedInstanceId);
}

void AppController::modFileAction(const QString &instanceId, const QString &kind, const QString &fileName,
                                  const QString &action)
{
    if (!m_instances)
        return;
    QJsonObject instance = m_instances->get(instanceId);
    if (instance.isEmpty())
        return;
    if (fileName.isEmpty() || fileName.contains(QLatin1String("..")) || fileName.contains(QLatin1Char('/'))
        || fileName.contains(QLatin1Char('\\'))) {
        emit toast(QStringLiteral("Некорректное имя файла"), QStringLiteral("error"));
        return;
    }
    const QString dirName = ModsScanner::kindDir(kind);
    const QString dir = QDir(instance.value(QStringLiteral("gameDir")).toString()).filePath(dirName);
    const QString full = QDir(dir).filePath(fileName);
    const bool disabled = fileName.endsWith(QLatin1String(".disabled"));
    const QString baseName = disabled ? fileName.left(fileName.size() - 9) : fileName;
    if (action == QLatin1String("toggle")) {
        const QString target = disabled ? QDir(dir).filePath(baseName) : QDir(dir).filePath(fileName + QStringLiteral(".disabled"));
        QFile::rename(full, target);
    } else if (action == QLatin1String("delete")) {
        QFile::remove(full);
    } else {
        emit toast(QStringLiteral("Неизвестное действие"), QStringLiteral("error"));
        return;
    }
    QJsonArray mods = instance.value(QStringLiteral("mods")).toArray();
    QJsonArray next;
    for (const auto &value : mods) {
        QJsonObject mod = value.toObject();
        QString tracked = mod.value(QStringLiteral("fileName")).toString();
        if (tracked.endsWith(QLatin1String(".disabled")))
            tracked.chop(9);
        if (tracked == baseName) {
            if (action == QLatin1String("delete"))
                continue;
            mod.insert(QStringLiteral("enabled"), disabled);
            mod.insert(QStringLiteral("fileName"), disabled ? baseName : baseName + QStringLiteral(".disabled"));
        }
        next.append(mod);
    }
    m_instances->replaceMods(instanceId, next.toVariantList());
    emit toast(action == QLatin1String("delete") ? QStringLiteral("Файл удалён") : QStringLiteral("Статус изменён"),
               QStringLiteral("info"));
    scanInstance(instanceId);
    emit instancesChanged();
}

void AppController::launchGame(const QString &instanceId, const QString &accountId)
{
    if (!m_launcher || m_launcher->isRunning() || (m_task && m_task->running()))
        return;
    const QJsonObject instance = m_instances->get(instanceId);
    if (instance.isEmpty()) {
        emit toast(QStringLiteral("Сборка не найдена"), QStringLiteral("error"));
        return;
    }
    const QString accId = accountId.isEmpty() ? m_accounts->selectedId() : accountId;
    QJsonObject account = m_accounts->snapshot(accId);
    if (account.isEmpty()) {
        account = QJsonObject{{QStringLiteral("provider"), QStringLiteral("offline")},
                              {QStringLiteral("name"), QStringLiteral("Player")},
                              {QStringLiteral("uuid"), offlineUuid(QStringLiteral("Player"))},
                              {QStringLiteral("accessToken"), QStringLiteral("0")}};
    }
    const QString runtime = m_settings->runtimeDir();
    const QString javaMode = m_settings->javaMode();
    const QString javaPath = m_settings->javaPath();
    const QString clientId = Env::instance().elybyClientId();
    const QString clientSecret = Env::instance().elybyClientSecret();
    auto spec = std::make_shared<LaunchSpec>();
    m_pendingSpec = spec;

    startTask(QStringLiteral("Запуск игры"), [this, instance, account, runtime, javaMode, javaPath, clientId, clientSecret, spec](TaskRunner *task) {
        QString error;
        QJsonObject updated;
        const LaunchCredentials credentials = AccountManager::resolveCredentials(account, clientId, clientSecret, &error, &updated);
        if (credentials.name.isEmpty()) {
            throw std::runtime_error(error.isEmpty() ? QStringLiteral("Не удалось получить данные аккаунта").toStdString() : error.toStdString());
        }
        if (!updated.isEmpty()) {
            const QString id = account.value(QStringLiteral("id")).toString();
            invokeBlocking(m_accounts, [this, id, updated] {
                m_accounts->applyRefreshedSnapshot(id, updated);
                return true;
            });
        }
        LaunchAccount launchAccount{credentials.name, credentials.uuid, credentials.accessToken, credentials.userType,
                                    credentials.provider};
        task->report(QStringLiteral("launch"), QStringLiteral("Подготовка игры…"), 0.05);
        QString authlibJar;
        if (credentials.provider == QLatin1String("elyby")) {
            QString authlibError;
            if (VersionManager::ensureAuthlib(runtime, Reporter{[&](const QString &stage, const QString &message, double progress) {
                    task->report(stage, message, 0.05 + progress * 0.1);
                }}, &authlibJar, &authlibError)) {
                task->logLine(QStringLiteral("info"), QStringLiteral("authlib-injector подключён — онлайн-режим Ely.by доступен"));
            } else {
                task->logLine(QStringLiteral("warn"), QStringLiteral("authlib-injector не подключён (%1) — только одиночный режим").arg(authlibError));
            }
        }
        *spec = VersionManager::buildLaunchSpec(
            instance, runtime, javaMode, javaPath, launchAccount, credentials.provider == QLatin1String("elyby"), authlibJar,
            Reporter{[&](const QString &stage, const QString &message, double progress) {
                task->report(stage, message, progress);
            }},
            &error);
        if (spec->javaPath.isEmpty())
            throw std::runtime_error(error.isEmpty() ? QStringLiteral("Не удалось подготовить запуск").toStdString() : error.toStdString());
    }, [this](bool ok, const QString &error) {
        if (!ok) {
            m_pendingSpec.reset();
            emit toast(QStringLiteral("Запуск не удался: %1").arg(error), QStringLiteral("error"));
            appendConsole(QStringLiteral("error"), QStringLiteral("Запуск: ") + error);
            emit gameChanged();
            return;
        }
        if (!m_pendingSpec) {
            emit toast(QStringLiteral("Не удалось подготовить запуск"), QStringLiteral("error"));
            return;
        }
        if (m_settings->showConsole()) {
            setConsoleEnabled(true);
            emit consoleRequested();
        }
        appendConsole(QStringLiteral("info"), QStringLiteral("Запуск %1…").arg(m_pendingSpec->versionId));
        m_launcher->start(*m_pendingSpec, m_settings->optimizeHighPriority(), m_console != nullptr);
        m_pendingSpec.reset();
        emit gameChanged();
    });
}

void AppController::stopGame()
{
    if (m_launcher)
        m_launcher->stop();
}

void AppController::setConsoleEnabled(bool enabled)
{
    if (enabled) {
        if (m_console)
            return;
        m_console = new ConsoleModel(this);
        m_console->append(QStringLiteral("info"), QStringLiteral("Консоль Noble Launcher 2.9.0"));
        if (m_launcher)
            m_launcher->setConsoleEnabled(true);
        m_stats->inc(QStringLiteral("consoleOpens"));
        emit consoleChanged();
    } else {
        if (!m_console)
            return;
        ConsoleModel *model = m_console;
        m_console = nullptr;
        if (m_launcher)
            m_launcher->setConsoleEnabled(false);
        model->deleteLater();
        emit consoleChanged();
    }
}

QString AppController::consoleText() const
{
    return m_console ? m_console->toText() : QString();
}

void AppController::clearConsole()
{
    if (m_console)
        m_console->clear();
}

void AppController::saveConsoleToFile(const QString &path)
{
    const QString target = toLocalFile(path);
    if (target.isEmpty() || !m_console)
        return;
    QFile file(target);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit toast(QStringLiteral("Не удалось сохранить журнал"), QStringLiteral("error"));
        return;
    }
    file.write(m_console->toText().toUtf8());
    file.close();
    emit toast(QStringLiteral("Журнал сохранён"), QStringLiteral("success"));
}

void AppController::copyText(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
    emit toast(QStringLiteral("Скопировано"), QStringLiteral("info"));
}

void AppController::importBackgrounds(const QStringList &urls)
{
    QStringList paths;
    for (const auto &url : urls)
        paths << toLocalFile(url);
    m_backgrounds->importFiles(paths);
}

void AppController::removeBackground(const QString &id) { m_backgrounds->remove(id); }

void AppController::setBackground(const QString &id)
{
    if (!m_settings)
        return;
    const QString current = m_settings->activeBackgroundId();
    if (current == id)
        return;
    m_settings->setActiveBackgroundId(id);
    m_stats->inc(QStringLiteral("backgroundChanges"));
    emit appearanceChanged();
    emit toast(id.isEmpty() ? QStringLiteral("Фон отключён") : QStringLiteral("Фон применён"), QStringLiteral("info"));
}

void AppController::openBackgroundsFolder() { m_backgrounds->openFolder(); }

void AppController::importFonts(const QStringList &urls)
{
    const int before = m_settings->customFonts().size();
    QStringList paths;
    for (const auto &url : urls)
        paths << toLocalFile(url);
    m_fonts->importFiles(paths);
    if (m_settings->customFonts().size() > before)
        m_stats->inc(QStringLiteral("customFontsAdded"));
}

void AppController::removeFont(const QString &id) { m_fonts->remove(id); }

void AppController::applyFontMode(const QString &mode, const QString &fontId)
{
    m_settings->setActiveFontId(fontId);
    m_settings->setFontMode(mode);
    m_fonts->apply();
}

void AppController::inspectStorage(const QString &kind, const QString &path)
{
    QPointer<AppController> guard(this);
    QtConcurrent::run([guard, kind, path, settings = m_settings] {
        const QVariantMap info = StorageService::inspect(kind, path, settings);
        if (!guard)
            return;
        QMetaObject::invokeMethod(guard, [guard, info] {
            if (!guard)
                return;
            guard->m_storageInfo = info;
            emit guard->storageChanged();
            emit guard->storageInspected(info);
        }, Qt::QueuedConnection);
    });
}

void AppController::moveStorage(const QString &kind, const QString &path, const QString &mode)
{
    const QString instancesDir = m_settings->instancesDir();
    const QString runtimeDir = m_settings->runtimeDir();
    startTask(QStringLiteral("Перенос данных"), [this, kind, path, mode, instancesDir, runtimeDir](TaskRunner *task) {
        QString error;
        const QVariantMap result = StorageService::move(kind, path, mode, m_settings,
                                                        [&](const QString &message, double progress) {
                                                            task->report(QStringLiteral("storage"), message, progress);
                                                        }, task->cancelFlag(), &error);
        if (result.isEmpty())
            throw std::runtime_error(error.toStdString());
        const QString target = result.value(QStringLiteral("path")).toString();
        invokeBlocking(this, [this, kind, target, instancesDir, runtimeDir] {
            if (kind == QLatin1String("runtime")) {
                m_settings->setDirectories(instancesDir, target);
                JavaManager::setRuntimeRoot(target);
            } else {
                m_settings->setDirectories(target, runtimeDir);
            }
            return true;
        });
    }, [this, kind](bool ok, const QString &error) {
        if (ok) {
            m_stats->inc(QStringLiteral("diskMoves"));
            if (kind == QLatin1String("instances")) {
                m_instances->init(m_settings);
                emit instancesChanged();
            }
            emit toast(QStringLiteral("Данные перенесены в новую папку"), QStringLiteral("success"));
            emit settingsChanged();
        } else {
            emit toast(QStringLiteral("Перенос не удался: %1").arg(error), QStringLiteral("error"));
        }
    });
}

void AppController::refreshOptimizer()
{
    if (m_optimizerBusy)
        return;
    m_optimizerBusy = true;
    emit optimizerChanged();
    QPointer<AppController> guard(this);
    const QVariantList instances = m_instances->instances();
    QtConcurrent::run([guard, instances, settings = m_settings] {
        const QVariantMap status = OptimizerService::inspect(settings, instances);
        if (!guard)
            return;
        QMetaObject::invokeMethod(guard, [guard, status] {
            if (!guard)
                return;
            guard->m_optimizerStatus = status;
            guard->m_optimizerBusy = false;
            emit guard->optimizerChanged();
        }, Qt::QueuedConnection);
    });
}

void AppController::refreshOptimizerProcesses()
{
    QPointer<AppController> guard(this);
    QtConcurrent::run([guard] {
        const QVariantList list = OptimizerService::processes();
        if (!guard)
            return;
        QMetaObject::invokeMethod(guard, [guard, list] {
            if (!guard)
                return;
            guard->m_optimizerProcesses = list;
            emit guard->optimizerChanged();
        }, Qt::QueuedConnection);
    });
}

void AppController::applyOptimization(const QStringList &ops, const QVariantList &pids)
{
    if (m_optimizerBusy)
        return;
    m_optimizerBusy = true;
    emit optimizerChanged();
    QList<qint64> pidList;
    for (const auto &value : pids)
        pidList << static_cast<qint64>(value.toDouble());
    const QVariantList instances = m_instances->instances();
    auto result = std::make_shared<QVariantMap>();
    startTask(QStringLiteral("Оптимизация игры"), [this, ops, pidList, instances, result](TaskRunner *task) {
        task->report(QStringLiteral("optimize"), QStringLiteral("Применение настроек Windows…"), 0.3);
        *result = OptimizerService::apply(ops, pidList, instances, m_settings);
        for (const auto &line : result->value(QStringLiteral("log")).toStringList())
            task->logLine(QStringLiteral("info"), line);
        for (const auto &line : result->value(QStringLiteral("errors")).toStringList())
            task->logLine(QStringLiteral("warn"), line);
    }, [this, result, ops](bool ok, const QString &error) {
        m_optimizerBusy = false;
        emit optimizerChanged();
        if (!ok) {
            emit toast(QStringLiteral("Оптимизация не удалась: %1").arg(error), QStringLiteral("error"));
            return;
        }
        if (result->value(QStringLiteral("cancelled")).toBool()) {
            emit toast(QStringLiteral("Запрос прав администратора отклонён"), QStringLiteral("warn"));
        } else {
            if (ops.contains(QStringLiteral("priority")))
                m_settings->setOptimizeHighPriority(true);
            const QStringList applied = result->value(QStringLiteral("applied")).toStringList();
            if (!applied.isEmpty())
                m_stats->inc(QStringLiteral("optimizationsApplied"));
            const QStringList errors = result->value(QStringLiteral("errors")).toStringList();
            emit toast(errors.isEmpty() ? QStringLiteral("Оптимизация применена")
                                        : QStringLiteral("Оптимизация применена с замечаниями — смотрите журнал"),
                       errors.isEmpty() ? QStringLiteral("success") : QStringLiteral("warn"));
        }
        emit optimizerFinished(*result);
        refreshOptimizer();
    });
}

void AppController::rollbackOptimization()
{
    if (m_optimizerBusy)
        return;
    m_optimizerBusy = true;
    emit optimizerChanged();
    auto result = std::make_shared<QVariantMap>();
    startTask(QStringLiteral("Откат оптимизации"), [this, result](TaskRunner *task) {
        *result = OptimizerService::rollback(m_settings);
        for (const auto &line : result->value(QStringLiteral("log")).toStringList())
            task->logLine(QStringLiteral("info"), line);
    }, [this, result](bool ok, const QString &error) {
        m_optimizerBusy = false;
        emit optimizerChanged();
        if (!ok) {
            emit toast(QStringLiteral("Откат не удался: %1").arg(error), QStringLiteral("error"));
            return;
        }
        if (result->value(QStringLiteral("cancelled")).toBool()) {
            emit toast(QStringLiteral("Запрос прав администратора отклонён"), QStringLiteral("warn"));
        } else {
            m_settings->setOptimizeHighPriority(false);
            emit toast(QStringLiteral("Изменения откатаны"), QStringLiteral("success"));
        }
        emit optimizerFinished(*result);
        refreshOptimizer();
    });
}

void AppController::setProfileIcon(const QString &urlOrPath)
{
    const QString path = toLocalFile(urlOrPath);
    m_profile->setProfileIcon(path);
    m_stats->inc(QStringLiteral("profileIconChanges"));
    emit toast(QStringLiteral("Иконка профиля обновлена"), QStringLiteral("success"));
}

void AppController::removeProfileIcon()
{
    m_profile->setProfileIcon(QString());
    emit toast(QStringLiteral("Иконка профиля убрана"), QStringLiteral("info"));
}

void AppController::uploadSkin(const QString &urlOrPath, bool slim)
{
    m_profile->uploadSkin(m_accounts->selectedId(), toLocalFile(urlOrPath), slim);
}

void AppController::refreshSkin() { m_skin->refresh(); }

void AppController::refreshSteam()
{
    m_steam->refreshStatus();
    emit steamChanged();
}

void AppController::installE4Steam(const QString &instanceId)
{
    startTask(QStringLiteral("Друзья через Steam"), [this, instanceId](TaskRunner *task) {
        QString error;
        const QVariantMap result = E4SteamService::installForInstance(
            m_instances, instanceId,
            [&](const QString &stage, const QString &message, double progress) { task->report(stage, message, progress); },
            task->cancelFlag(), &error);
        if (result.isEmpty())
            throw std::runtime_error(error.toStdString());
    }, [this](bool ok, const QString &error) {
        if (ok) {
            emit toast(QStringLiteral("e4steam установлен"), QStringLiteral("success"));
            emit instancesChanged();
        } else {
            emit toast(QStringLiteral("Не удалось установить e4steam: %1").arg(error), QStringLiteral("error"));
        }
    });
}

void AppController::inviteFriends()
{
    openExternal(QStringLiteral("steam://open/friends"));
    m_stats->inc(QStringLiteral("friendsInvites"));
    emit toast(QStringLiteral("Открыто окно друзей Steam"), QStringLiteral("info"));
}

void AppController::openSteamStore()
{
    openExternal(QStringLiteral("https://store.steampowered.com/about/"));
}

void AppController::loadNobleBuilds(bool force)
{
    if (m_noble)
        m_noble->load(force);
}

void AppController::installNobleBuild(const QString &id)
{
    const QJsonArray builds = QJsonArray::fromVariantList(m_noble->builds());
    auto newInstanceId = std::make_shared<QString>();
    startTask(QStringLiteral("Установка сборки Noble"), [this, id, builds, newInstanceId](TaskRunner *task) {
        QString error;
        const QJsonObject instance = NobleBuildsService::install(
            id, builds, m_instances,
            [&](const QString &stage, const QString &message, double progress) { task->report(stage, message, progress); },
            task->cancelFlag(), &error);
        if (instance.isEmpty())
            throw std::runtime_error(error.toStdString());
        *newInstanceId = instance.value(QStringLiteral("id")).toString();
    }, [this, newInstanceId](bool ok, const QString &error) {
        if (ok) {
            m_stats->inc(QStringLiteral("nobleBuildsInstalled"));
            m_stats->inc(QStringLiteral("builds"));
            if (!newInstanceId->isEmpty()) {
                m_instances->reload();
                m_instances->setSelectedInstanceId(*newInstanceId);
                scanInstance(*newInstanceId);
            }
            emit toast(QStringLiteral("Сборка установлена — проверьте версию и нажмите «Запуск»"),
                       QStringLiteral("success"));
            emit instancesChanged();
        } else {
            emit toast(QStringLiteral("Установка сборки не удалась: %1").arg(error), QStringLiteral("error"));
        }
    });
}

void AppController::checkUpdates()
{
    if (m_updater)
        m_updater->check(true);
}

void AppController::openUpdatePage()
{
    const QVariantMap status = updateStatus();
    QString url = status.value(QStringLiteral("url")).toString();
    if (url.isEmpty())
        url = QStringLiteral("https://github.com/DevvTorn/NobleLauncher/releases");
    openExternal(url);
}

void AppController::openExternal(const QString &url)
{
    if (!openExternalUrl(url))
        emit toast(QStringLiteral("Ссылка заблокирована"), QStringLiteral("warn"));
}

void AppController::openLogs()
{
    openPathInExplorer(Logger::filePath());
}

void AppController::openDataFolder()
{
    openWithDefaultApp(Paths::dataDir());
}

void AppController::resetSettings()
{
    m_settings->reset();
    m_fonts->apply();
    emit settingsChanged();
    emit appearanceChanged();
    emit toast(QStringLiteral("Настройки сброшены"), QStringLiteral("success"));
}

void AppController::replayIntro()
{
    if (m_settings)
        m_settings->replayIntro();
    emit toast(QStringLiteral("__replay_intro__"), QStringLiteral("info"));
}

void AppController::clearAppCache()
{
    Paths::clearCache();
    emit toast(QStringLiteral("Кэш очищен"), QStringLiteral("success"));
}

void AppController::rollbackDataMigration()
{
    QString error;
    if (Paths::rollbackMigrationData(&error)) {
        emit toast(QStringLiteral("Данные старой версии восстановлены. Перезапустите лаунчер."), QStringLiteral("success"));
    } else {
        emit toast(error.isEmpty() ? QStringLiteral("Нечего откатывать") : error, QStringLiteral("warn"));
    }
}

void AppController::profileEvent(const QString &key)
{
    static const QSet<QString> allowed = {QStringLiteral("skinViewerOpens"), QStringLiteral("consoleOpens"),
                                          QStringLiteral("friendsInvites"), QStringLiteral("profileIconChanges")};
    if (!allowed.contains(key))
        return;
    m_stats->inc(key);
}

void AppController::showToast(const QString &message, const QString &type)
{
    emit toast(message, type);
}

QString AppController::formatBytes(double bytes) const { return Noble::humanBytes(static_cast<qint64>(bytes)); }
QString AppController::formatSpeed(double bps) const { return Noble::humanSpeed(static_cast<qint64>(bps)); }
QString AppController::formatDuration(double seconds) const { return Noble::humanDuration(static_cast<qint64>(seconds)); }
QString AppController::formatCount(double count) const { return Noble::downloadCount(static_cast<qint64>(count)); }

QString AppController::toLocalFile(const QString &urlOrPath) const
{
    if (urlOrPath.startsWith(QLatin1String("file:"), Qt::CaseInsensitive))
        return QUrl(urlOrPath).toLocalFile();
    if (urlOrPath.contains(QLatin1String("://")))
        return QString();
    return urlOrPath;
}

int AppController::fs(int base) const
{
    const double scale = m_settings ? m_settings->fontScale() : 1.0;
    return std::max(9, static_cast<int>(std::round(base * scale)));
}

void AppController::navigateForScreenshot(const QString &page, const QString &section)
{
    emit navigationRequested(page, section);
}

void AppController::startScreenshotMode(const QString &dir)
{
    m_screenshotMode = true;
    m_screenshotDir = dir;
    QDir().mkpath(dir);
    m_settings->setIntroEnabled(false);
    emit miscChanged();

    struct Step {
        QString page;
        QString section;
        QString lang;
        QString file;
        bool console;
    };
    if (m_queue)
        m_queue->seedDemoItems();
    const QVector<Step> steps = {
        {QStringLiteral("home"), QString(), QStringLiteral("ru"), QStringLiteral("main-ru.png"), false},
        {QStringLiteral("home"), QString(), QStringLiteral("en"), QStringLiteral("main-en.png"), false},
        {QStringLiteral("profile"), QString(), QStringLiteral("ru"), QStringLiteral("skins-3d.png"), false},
        {QStringLiteral("builds"), QString(), QStringLiteral("ru"), QStringLiteral("builds.png"), false},
        {QStringLiteral("builds"), QStringLiteral("noble"), QStringLiteral("ru"), QStringLiteral("builds-noble.png"), false},
        {QStringLiteral("mods"), QString(), QStringLiteral("ru"), QStringLiteral("mods-queue.png"), false},
        {QStringLiteral("profile"), QString(), QStringLiteral("ru"), QStringLiteral("profile.png"), false},
        {QStringLiteral("profile"), QString(), QStringLiteral("ru"), QStringLiteral("achievements.png"), false},
        {QStringLiteral("settings"), QStringLiteral("appearance"), QStringLiteral("ru"), QStringLiteral("backgrounds.png"), false},
        {QStringLiteral("settings"), QStringLiteral("font"), QStringLiteral("ru"), QStringLiteral("font-settings.png"), false},
        {QStringLiteral("settings"), QStringLiteral("optimize"), QStringLiteral("ru"), QStringLiteral("optimization.png"), false},
        {QStringLiteral("settings"), QStringLiteral("friends"), QStringLiteral("ru"), QStringLiteral("friends.png"), false},
        {QStringLiteral("settings"), QStringLiteral("about"), QStringLiteral("ru"), QStringLiteral("about.png"), false},
        {QStringLiteral("home"), QString(), QStringLiteral("ru"), QStringLiteral("console.png"), true},
    };
    auto index = std::make_shared<int>(0);
    auto capture = std::make_shared<std::function<void()>>();
    *capture = [this, steps, index, capture] {
        if (*index >= steps.size()) {
            if (m_console)
                setConsoleEnabled(false);
            QTimer::singleShot(300, qApp, &QCoreApplication::quit);
            return;
        }
        const Step step = steps.at(*index);
        m_settings->setLang(step.lang);
        if (step.console) {
            setConsoleEnabled(true);
            m_console->append(QStringLiteral("info"), QStringLiteral("[10:24:01] Проверка Java 21…"));
            m_console->append(QStringLiteral("info"), QStringLiteral("[10:24:02] Java найдена: C:/Program Files/Java/jdk-21/bin/javaw.exe"));
            m_console->append(QStringLiteral("success"), QStringLiteral("[10:24:03] Библиотеки проверены — 214 файлов"));
            m_console->append(QStringLiteral("info"), QStringLiteral("[10:24:05] Запуск 1.21.1-neoforge-21.1.80…"));
            m_console->append(QStringLiteral("game"), QStringLiteral("[10:24:08] [Render thread/INFO] Setting user: Steve"));
            m_console->append(QStringLiteral("game"), QStringLiteral("[10:24:11] [Render thread/INFO] Narrator library for x64 successfully loaded"));
            m_console->append(QStringLiteral("game"), QStringLiteral("[10:24:14] [Render thread/INFO] OpenGL 4.6 NVIDIA GeForce RTX 4060"));
            m_console->append(QStringLiteral("warn"), QStringLiteral("[10:24:18] [Render thread/WARN] Unable to load chunk cache, rebuilding"));
            m_console->append(QStringLiteral("game"), QStringLiteral("[10:24:21] [Render thread/INFO] Loaded 42 mods, 128 resource packs"));
            m_console->append(QStringLiteral("success"), QStringLiteral("[10:24:24] Игра запущена (PID 8244)"));
            emit consoleRequested();
        } else if (m_console) {
            setConsoleEnabled(false);
        }
        emit navigationRequested(step.page, step.section);
        QTimer::singleShot(1500, this, [this, step, index, capture] {
            if (m_window) {
                const QImage image = m_window->grabWindow();
                image.save(QDir(m_screenshotDir).filePath(step.file));
                Logger::info(QStringLiteral("Screenshot %1: память %2 МБ").arg(step.file).arg(currentProcessMemoryMb()));
            }
            ++*index;
            (*capture)();
        });
    };
    QTimer::singleShot(1200, this, [capture] { (*capture)(); });
}

} // namespace Noble
