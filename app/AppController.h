#pragma once

#include "core/Settings.h"
#include "core/ConsoleModel.h"
#include "core/LaunchSpec.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QStringList>
#include <QPointer>
#include <QJsonObject>

#include <memory>

class QQuickWindow;
class QFileSystemWatcher;
class QTimer;

namespace Noble {

class AccountManager;
class InstanceManager;
class GameLauncher;
class ModQueue;
class StatsService;
class BackgroundsService;
class FontsService;
class ProfileService;
class SkinService;
class NewsService;
class NobleBuildsService;
class E4SteamService;
class UpdaterService;
class TaskRunner;

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Settings *settings READ settings CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString lang READ lang NOTIFY settingsChanged)
    Q_PROPERTY(bool animationsEnabled READ animationsEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion NOTIFY settingsChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY settingsChanged)
    Q_PROPERTY(QString colorBg READ colorBg NOTIFY settingsChanged)
    Q_PROPERTY(QString colorSurface READ colorSurface NOTIFY settingsChanged)
    Q_PROPERTY(QString colorSurfaceAlt READ colorSurfaceAlt NOTIFY settingsChanged)
    Q_PROPERTY(QString colorBorder READ colorBorder NOTIFY settingsChanged)
    Q_PROPERTY(QString colorText READ colorText NOTIFY settingsChanged)
    Q_PROPERTY(QString colorTextDim READ colorTextDim NOTIFY settingsChanged)
    Q_PROPERTY(QString colorAccent READ colorAccent NOTIFY settingsChanged)
    Q_PROPERTY(QString colorAccentHover READ colorAccentHover NOTIFY settingsChanged)
    Q_PROPERTY(QString colorAccentSoft READ colorAccentSoft NOTIFY settingsChanged)
    Q_PROPERTY(QString colorSuccess READ colorSuccess NOTIFY settingsChanged)
    Q_PROPERTY(QString colorWarn READ colorWarn NOTIFY settingsChanged)
    Q_PROPERTY(QString colorError READ colorError NOTIFY settingsChanged)
    Q_PROPERTY(double spacingScale READ spacingScale NOTIFY settingsChanged)

    Q_PROPERTY(QVariantList accounts READ accounts NOTIFY accountsChanged)
    Q_PROPERTY(QVariantMap selectedAccount READ selectedAccount NOTIFY accountsChanged)
    Q_PROPERTY(QString selectedAccountId READ selectedAccountId NOTIFY accountsChanged)
    Q_PROPERTY(bool accountsBusy READ accountsBusy NOTIFY accountsBusyChanged)
    Q_PROPERTY(QString oauthStatus READ oauthStatus NOTIFY oauthStatusChanged)

    Q_PROPERTY(QVariantList instances READ instances NOTIFY instancesChanged)
    Q_PROPERTY(QString selectedInstanceId READ selectedInstanceId WRITE setSelectedInstanceId NOTIFY instancesChanged)
    Q_PROPERTY(QVariantMap selectedInstance READ selectedInstance NOTIFY instancesChanged)

    Q_PROPERTY(QVariantList minecraftVersions READ minecraftVersions NOTIFY versionsChanged)
    Q_PROPERTY(QVariantList loaderVersions READ loaderVersions NOTIFY versionsChanged)
    Q_PROPERTY(QVariantList javaList READ javaList NOTIFY javaChanged)
    Q_PROPERTY(QVariantList modSearchResults READ modSearchResults NOTIFY modSearchChanged)
    Q_PROPERTY(bool modSearchLoading READ modSearchLoading NOTIFY modSearchChanged)
    Q_PROPERTY(QVariantList modVersions READ modVersions NOTIFY modVersionsChanged)
    Q_PROPERTY(QVariantList queueItems READ queueItems NOTIFY queueChanged)
    Q_PROPERTY(double queueProgress READ queueProgress NOTIFY queueChanged)
    Q_PROPERTY(int queueActive READ queueActive NOTIFY queueChanged)
    Q_PROPERTY(QVariantList newsItems READ newsItems NOTIFY newsChanged)
    Q_PROPERTY(QVariantList backgrounds READ backgrounds NOTIFY backgroundsChanged)
    Q_PROPERTY(QVariantList customFonts READ customFonts NOTIFY fontsChanged)
    Q_PROPERTY(QString backgroundSource READ backgroundSource NOTIFY appearanceChanged)
    Q_PROPERTY(QVariantMap modsScan READ modsScan NOTIFY modsScanChanged)
    Q_PROPERTY(QVariantList nobleBuilds READ nobleBuilds NOTIFY nobleChanged)
    Q_PROPERTY(bool nobleLoading READ nobleLoading NOTIFY nobleChanged)
    Q_PROPERTY(bool nobleFromCache READ nobleFromCache NOTIFY nobleChanged)
    Q_PROPERTY(QVariantMap steamStatus READ steamStatus NOTIFY steamChanged)
    Q_PROPERTY(QVariantMap updateStatus READ updateStatus NOTIFY updateChanged)
    Q_PROPERTY(QVariantMap statsSummary READ statsSummary NOTIFY statsChanged)
    Q_PROPERTY(QString skinSource READ skinSource NOTIFY skinChanged)
    Q_PROPERTY(QString avatarSource READ avatarSource NOTIFY skinChanged)
    Q_PROPERTY(QString skinName READ skinName NOTIFY skinChanged)
    Q_PROPERTY(bool skinLoading READ skinLoading NOTIFY skinLoadingChanged)
    Q_PROPERTY(QString skinError READ skinError NOTIFY skinLoadingChanged)
    Q_PROPERTY(QString profileIconSource READ profileIconSource NOTIFY profileChanged)
    Q_PROPERTY(QString gamePhase READ gamePhase NOTIFY gameChanged)
    Q_PROPERTY(qint64 gamePid READ gamePid NOTIFY gameChanged)
    Q_PROPERTY(int gameElapsed READ gameElapsed NOTIFY gameChanged)
    Q_PROPERTY(bool taskRunning READ taskRunning NOTIFY taskChanged)
    Q_PROPERTY(QString taskTitle READ taskTitle NOTIFY taskChanged)
    Q_PROPERTY(QString taskStage READ taskStage NOTIFY taskChanged)
    Q_PROPERTY(QString taskMessage READ taskMessage NOTIFY taskChanged)
    Q_PROPERTY(double taskProgress READ taskProgress NOTIFY taskChanged)
    Q_PROPERTY(bool taskCancellable READ taskCancellable NOTIFY taskChanged)
    Q_PROPERTY(QVariantMap optimizerStatus READ optimizerStatus NOTIFY optimizerChanged)
    Q_PROPERTY(QVariantList optimizerProcesses READ optimizerProcesses NOTIFY optimizerChanged)
    Q_PROPERTY(bool optimizerBusy READ optimizerBusy NOTIFY optimizerChanged)
    Q_PROPERTY(int memoryMB READ memoryMB NOTIFY memoryChanged)
    Q_PROPERTY(ConsoleModel *consoleModel READ consoleModel NOTIFY consoleChanged)
    Q_PROPERTY(bool consoleEnabled READ consoleEnabled NOTIFY consoleChanged)
    Q_PROPERTY(QString envMissing READ envMissing NOTIFY miscChanged)
    Q_PROPERTY(bool elyKeysConfigured READ elyKeysConfigured NOTIFY miscChanged)
    Q_PROPERTY(QVariantMap storageInfo READ storageInfo NOTIFY storageChanged)
    Q_PROPERTY(QString telegramUrl READ telegramUrl CONSTANT)
    Q_PROPERTY(QString siteUrl READ siteUrl CONSTANT)
    Q_PROPERTY(QString githubUrl READ githubUrl CONSTANT)
    Q_PROPERTY(QString dataDir READ dataDir CONSTANT)
    Q_PROPERTY(QString logFile READ logFile CONSTANT)
    Q_PROPERTY(bool screenshotMode READ screenshotMode NOTIFY miscChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    void init();
    void setMainWindow(QQuickWindow *window) { m_window = window; }
    void startScreenshotMode(const QString &dir);

    Settings *settings() const { return m_settings; }
    QString version() const { return QStringLiteral("2.9.0"); }
    QString lang() const;
    bool animationsEnabled() const;
    bool reducedMotion() const;
    QString fontFamily() const;
    QString colorBg() const;
    QString colorSurface() const;
    QString colorSurfaceAlt() const;
    QString colorBorder() const;
    QString colorText() const;
    QString colorTextDim() const;
    QString colorAccent() const;
    QString colorAccentHover() const;
    QString colorAccentSoft() const;
    QString colorSuccess() const;
    QString colorWarn() const;
    QString colorError() const;
    double spacingScale() const;

    QVariantList accounts() const;
    QVariantMap selectedAccount() const;
    QString selectedAccountId() const;
    bool accountsBusy() const;
    QString oauthStatus() const;

    QVariantList instances() const;
    QString selectedInstanceId() const;
    void setSelectedInstanceId(const QString &id);
    QVariantMap selectedInstance() const;

    QVariantList minecraftVersions() const { return m_minecraftVersions; }
    QVariantList loaderVersions() const { return m_loaderVersions; }
    QVariantList javaList() const { return m_javaList; }
    QVariantList modSearchResults() const { return m_modSearchResults; }
    bool modSearchLoading() const { return m_modSearchLoading; }
    QVariantList modVersions() const { return m_modVersions; }
    QVariantList queueItems() const;
    double queueProgress() const;
    int queueActive() const;
    QVariantList newsItems() const;
    QVariantList backgrounds() const;
    QVariantList customFonts() const;
    QString backgroundSource() const;
    QVariantMap modsScan() const { return m_modsScan; }
    QVariantList nobleBuilds() const;
    bool nobleLoading() const;
    bool nobleFromCache() const;
    QVariantMap steamStatus() const;
    QVariantMap updateStatus() const;
    QVariantMap statsSummary() const;
    QString skinSource() const;
    QString avatarSource() const;
    QString skinName() const;
    bool skinLoading() const;
    QString skinError() const;
    QString profileIconSource() const;
    QString gamePhase() const;
    qint64 gamePid() const;
    int gameElapsed() const;
    bool taskRunning() const;
    QString taskTitle() const;
    QString taskStage() const;
    QString taskMessage() const;
    double taskProgress() const;
    bool taskCancellable() const;
    QVariantMap optimizerStatus() const { return m_optimizerStatus; }
    QVariantList optimizerProcesses() const { return m_optimizerProcesses; }
    bool optimizerBusy() const { return m_optimizerBusy; }
    int memoryMB() const { return m_memoryMB; }
    ConsoleModel *consoleModel() const { return m_console; }
    bool consoleEnabled() const { return m_console != nullptr; }
    QString envMissing() const;
    bool elyKeysConfigured() const;
    QVariantMap storageInfo() const { return m_storageInfo; }
    QString telegramUrl() const { return QStringLiteral("https://t.me/noblelauncher"); }
    QString siteUrl() const { return QStringLiteral("https://noblelauncher.ru/"); }
    QString githubUrl() const { return QStringLiteral("https://github.com/DevvTorn/NobleLauncher"); }
    QString dataDir() const;
    QString logFile() const;
    bool screenshotMode() const { return m_screenshotMode; }

    Q_INVOKABLE void selectAccount(const QString &id);
    Q_INVOKABLE void addOfflineAccount(const QString &name);
    Q_INVOKABLE void addElyByPassword(const QString &username, const QString &password);
    Q_INVOKABLE void addElyByOAuth();
    Q_INVOKABLE void cancelOAuth();
    Q_INVOKABLE void removeAccount(const QString &id);
    Q_INVOKABLE void refreshAccount(const QString &id);

    Q_INVOKABLE QVariantMap createInstance(const QVariantMap &data);
    Q_INVOKABLE QVariantMap updateInstance(const QString &id, const QVariantMap &patch);
    Q_INVOKABLE void deleteInstance(const QString &id, bool deleteFiles);
    Q_INVOKABLE void cloneInstance(const QString &id, const QString &name);
    Q_INVOKABLE void exportInstance(const QString &id, const QString &targetPath);
    Q_INVOKABLE void exportInstanceNb(const QString &id, const QString &targetPath, bool embedFiles);
    Q_INVOKABLE void importInstance(const QString &zipPath);
    Q_INVOKABLE void openInstanceFolder(const QString &id);
    Q_INVOKABLE void setInstanceIcon(const QString &id, const QString &sourcePath);
    Q_INVOKABLE void installInstanceVersion(const QString &id);
    Q_INVOKABLE void uninstallInstanceVersion(const QString &id);
    Q_INVOKABLE int rollbackInstanceMigration();

    Q_INVOKABLE void loadMinecraftVersions();
    Q_INVOKABLE void loadLoaderVersions(const QString &loader, const QString &mcVersion);
    Q_INVOKABLE void detectJava();
    Q_INVOKABLE void installJava(int major);

    Q_INVOKABLE void searchMods(const QString &query, const QString &projectType, const QString &gameVersion,
                                const QString &loader, int offset);
    Q_INVOKABLE void loadMoreMods();
    Q_INVOKABLE void loadModVersions(const QString &projectId, const QString &projectType = QStringLiteral("mod"));
    Q_INVOKABLE void enqueueMod(const QString &projectId, const QString &versionId, const QString &kind,
                               const QString &title);
    Q_INVOKABLE void installFromUrl(const QString &url);
    Q_INVOKABLE void cancelQueueItem(const QString &id);
    Q_INVOKABLE void retryQueueItem(const QString &id);
    Q_INVOKABLE void clearQueue();

    Q_INVOKABLE void scanInstance(const QString &id);
    Q_INVOKABLE void modFileAction(const QString &instanceId, const QString &kind, const QString &fileName,
                                   const QString &action);

    Q_INVOKABLE void launchGame(const QString &instanceId, const QString &accountId);
    Q_INVOKABLE void stopGame();
    Q_INVOKABLE void cancelTask();

    Q_INVOKABLE void setConsoleEnabled(bool enabled);
    Q_INVOKABLE QString consoleText() const;
    Q_INVOKABLE void clearConsole();
    Q_INVOKABLE void saveConsoleToFile(const QString &path);
    Q_INVOKABLE void copyText(const QString &text);

    Q_INVOKABLE void importBackgrounds(const QStringList &urls);
    Q_INVOKABLE void removeBackground(const QString &id);
    Q_INVOKABLE void setBackground(const QString &id);
    Q_INVOKABLE void openBackgroundsFolder();
    Q_INVOKABLE void importFonts(const QStringList &urls);
    Q_INVOKABLE void removeFont(const QString &id);
    Q_INVOKABLE void applyFontMode(const QString &mode, const QString &fontId);

    Q_INVOKABLE void inspectStorage(const QString &kind, const QString &path);
    Q_INVOKABLE void moveStorage(const QString &kind, const QString &path, const QString &mode);

    Q_INVOKABLE void refreshOptimizer();
    Q_INVOKABLE void refreshOptimizerProcesses();
    Q_INVOKABLE void applyOptimization(const QStringList &ops, const QVariantList &pids);
    Q_INVOKABLE void rollbackOptimization();

    Q_INVOKABLE void setProfileIcon(const QString &urlOrPath);
    Q_INVOKABLE void removeProfileIcon();
    Q_INVOKABLE void uploadSkin(const QString &urlOrPath, bool slim);
    Q_INVOKABLE void refreshSkin();

    Q_INVOKABLE void refreshSteam();
    Q_INVOKABLE void installE4Steam(const QString &instanceId);
    Q_INVOKABLE void inviteFriends();
    Q_INVOKABLE void openSteamStore();

    Q_INVOKABLE void loadNobleBuilds(bool force);
    Q_INVOKABLE void installNobleBuild(const QString &id);

    Q_INVOKABLE void checkUpdates();
    Q_INVOKABLE void openUpdatePage();
    Q_INVOKABLE void openExternal(const QString &url);
    Q_INVOKABLE void openLogs();
    Q_INVOKABLE void openDataFolder();
    Q_INVOKABLE void resetSettings();
    Q_INVOKABLE void replayIntro();
    Q_INVOKABLE void clearAppCache();
    Q_INVOKABLE void rollbackDataMigration();
    Q_INVOKABLE void profileEvent(const QString &key);
    Q_INVOKABLE void showToast(const QString &message, const QString &type);

    Q_INVOKABLE QString formatBytes(double bytes) const;
    Q_INVOKABLE QString formatSpeed(double bps) const;
    Q_INVOKABLE QString formatDuration(double seconds) const;
    Q_INVOKABLE QString formatCount(double count) const;
    Q_INVOKABLE QString toLocalFile(const QString &urlOrPath) const;
    Q_INVOKABLE int fs(int base) const;
    Q_INVOKABLE void navigateForScreenshot(const QString &page, const QString &section);

signals:
    void settingsChanged();
    void appearanceChanged();
    void accountsChanged();
    void accountsBusyChanged();
    void oauthStatusChanged();
    void instancesChanged();
    void versionsChanged();
    void javaChanged();
    void modSearchChanged();
    void modVersionsChanged();
    void queueChanged();
    void newsChanged();
    void backgroundsChanged();
    void fontsChanged();
    void modsScanChanged();
    void nobleChanged();
    void steamChanged();
    void updateChanged();
    void statsChanged();
    void skinChanged();
    void skinLoadingChanged();
    void profileChanged();
    void gameChanged();
    void taskChanged();
    void optimizerChanged();
    void memoryChanged();
    void consoleChanged();
    void miscChanged();
    void storageChanged();
    void storageInspected(const QVariantMap &info);
    void optimizerFinished(const QVariantMap &result);
    void toast(const QString &message, const QString &type);
    void achievementUnlocked(const QVariantMap &achievement);
    void consoleRequested();
    void navigationRequested(const QString &page, const QString &section);

private:
    void setupConnections();
    void updateSkinForAccount();
    void appendConsole(const QString &type, const QString &text);
    void startTask(const QString &title, const std::function<void(TaskRunner *)> &fn,
                   const std::function<void(bool, const QString &)> &onFinished = {}, bool cancellable = true);
    void notifyAchievement(const QVariantMap &achievement);
    void rescanWatched();
    void setupWatchersFor(const QJsonObject &instance, const QVariantMap &scan);

    Settings *m_settings = nullptr;
    AccountManager *m_accounts = nullptr;
    InstanceManager *m_instances = nullptr;
    GameLauncher *m_launcher = nullptr;
    ModQueue *m_queue = nullptr;
    StatsService *m_stats = nullptr;
    BackgroundsService *m_backgrounds = nullptr;
    FontsService *m_fonts = nullptr;
    ProfileService *m_profile = nullptr;
    SkinService *m_skin = nullptr;
    NewsService *m_news = nullptr;
    NobleBuildsService *m_noble = nullptr;
    E4SteamService *m_steam = nullptr;
    UpdaterService *m_updater = nullptr;
    TaskRunner *m_task = nullptr;
    ConsoleModel *m_console = nullptr;

    QVariantList m_minecraftVersions;
    QVariantList m_loaderVersions;
    QVariantList m_javaList;
    QVariantList m_modSearchResults;
    QVariantList m_modVersions;
    QVariantMap m_modsScan;
    QVariantMap m_optimizerStatus;
    QVariantList m_optimizerProcesses;
    QVariantMap m_storageInfo;
    bool m_modSearchLoading = false;
    bool m_optimizerBusy = false;
    int m_memoryMB = 0;
    QString m_lastSearchQuery;
    QString m_lastSearchType = QStringLiteral("mod");
    QString m_lastSearchVersion;
    QString m_lastSearchLoader;
    QString m_scannedInstanceId;
    std::shared_ptr<LaunchSpec> m_pendingSpec;
    QQuickWindow *m_window = nullptr;
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_scanTimer = nullptr;
    QTimer *m_memoryTimer = nullptr;
    QTimer *m_launcherTimer = nullptr;
    bool m_screenshotMode = false;
    QString m_screenshotDir;
};

} // namespace Noble
