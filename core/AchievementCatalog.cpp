#include "AchievementCatalog.h"

#include <QHash>

namespace Noble {

static AchievementDef make(const char *id, const char *titleRu, const char *titleEn, const char *descRu,
                           const char *descEn, int points, const char *stat, qint64 threshold)
{
    return {QString::fromUtf8(id), QString::fromUtf8(titleRu), QString::fromUtf8(titleEn), QString::fromUtf8(descRu),
            QString::fromUtf8(descEn), points, QString::fromUtf8(stat), threshold};
}

const QVector<AchievementDef> &achievements()
{
    static const QVector<AchievementDef> list = {
        make("builds_10", "Игрок-любитель", "Amateur Player", "Создать 10 сборок", "Create 10 builds", 5, "builds", 10),
        make("builds_25", "Сборщик", "Build Maker", "Создать 25 сборок", "Create 25 builds", 10, "builds", 25),
        make("builds_85", "Большой профессионал", "Big Professional", "Создать 85 сборок", "Create 85 builds", 50, "builds", 85),
        make("builds_100", "Терабайтный босс", "Terabyte Boss", "Создать 100 сборок", "Create 100 builds", 200, "builds", 100),

        make("first_launch", "Первый запуск", "First Launch", "Запустить игру впервые", "Launch the game for the first time", 5, "gameLaunches", 1),
        make("launches_10", "Постоянный игрок", "Regular Player", "Запустить игру 10 раз", "Launch the game 10 times", 5, "gameLaunches", 10),
        make("launches_50", "Верный лаунчеру", "Loyal User", "Запустить игру 50 раз", "Launch the game 50 times", 10, "gameLaunches", 50),
        make("launches_200", "Ветеран", "Veteran", "Запустить игру 200 раз", "Launch the game 200 times", 25, "gameLaunches", 200),
        make("launches_500", "Неутомимый", "Tireless", "Запустить игру 500 раз", "Launch the game 500 times", 50, "gameLaunches", 500),
        make("time_1h", "Первый час", "First Hour", "Провести в игре 1 час", "Play for 1 hour", 5, "gameSeconds", 3600),
        make("time_10h", "Десять часов", "Ten Hours", "Провести в игре 10 часов", "Play for 10 hours", 10, "gameSeconds", 36000),
        make("time_100h", "Сотка", "Hundred Hours", "Провести в игре 100 часов", "Play for 100 hours", 25, "gameSeconds", 360000),
        make("time_500h", "Полтысячи", "Five Hundred", "Провести в игре 500 часов", "Play for 500 hours", 50, "gameSeconds", 1800000),
        make("time_1000h", "Тысяча часов", "Thousand Hours", "Провести в игре 1000 часов", "Play for 1000 hours", 100, "gameSeconds", 3600000),
        make("launcher_1h", "В лаунчере час", "Launcher Hour", "Провести в лаунчере 1 час", "Spend 1 hour in the launcher", 5, "launcherMinutes", 60),
        make("launcher_10h", "Лаунчер как дом", "Launcher At Home", "Провести в лаунчере 10 часов", "Spend 10 hours in the launcher", 10, "launcherMinutes", 600),
        make("build_hour", "Час на сборке", "An Hour On A Build", "Наиграть час на одной сборке", "Play 1 hour on a single build", 10, "buildsWithHour", 1),
        make("build_hour_5", "Пять любимых", "Five Favourites", "Наиграть по часу на 5 сборках", "Play 1 hour on 5 builds", 20, "buildsWithHour", 5),
        make("build_master_10h", "Марафон", "Marathon", "Наиграть 10 часов на одной сборке", "Play 10 hours on a single build", 25, "bestBuildSeconds", 36000),
        make("mods_10", "Мододел-новичок", "Mod Beginner", "Установить 10 модов", "Install 10 mods", 5, "modsInstalled", 10),
        make("mods_50", "Коллекционер модов", "Mod Collector", "Установить 50 модов", "Install 50 mods", 10, "modsInstalled", 50),
        make("mods_100", "Сто модов", "Hundred Mods", "Установить 100 модов", "Install 100 mods", 25, "modsInstalled", 100),
        make("mods_300", "Мод-магнат", "Mod Magnate", "Установить 300 модов", "Install 300 mods", 50, "modsInstalled", 300),
        make("mods_500", "Пятьсот модов", "Five Hundred Mods", "Установить 500 модов", "Install 500 mods", 100, "modsInstalled", 500),
        make("packs_1", "Первый ресурспак", "First Resource Pack", "Скачать ресурспак", "Download a resource pack", 5, "resourcepacksInstalled", 1),
        make("packs_10", "Стилист", "Stylist", "Скачать 10 ресурспаков", "Download 10 resource packs", 10, "resourcepacksInstalled", 10),
        make("packs_50", "Графический гурман", "Graphics Gourmet", "Скачать 50 ресурспаков", "Download 50 resource packs", 25, "resourcepacksInstalled", 50),
        make("packs_100", "Коллекция текстур", "Texture Collection", "Скачать 100 ресурспаков", "Download 100 resource packs", 50, "resourcepacksInstalled", 100),
        make("shaders_1", "Первый шейдер", "First Shader", "Скачать шейдеры", "Download a shader", 5, "shaderpacksInstalled", 1),
        make("shaders_10", "Любитель графики", "Graphics Fan", "Скачать 10 шейдеров", "Download 10 shaders", 10, "shaderpacksInstalled", 10),
        make("shaders_50", "RTX-энтузиаст", "RTX Enthusiast", "Скачать 50 шейдеров", "Download 50 shaders", 25, "shaderpacksInstalled", 50),
        make("queue_10", "Конвейер", "Conveyor", "Установить 10 элементов через очередь", "Install 10 items through the queue", 10, "queueInstalls", 10),
        make("queue_50", "Поток загрузок", "Download Stream", "Установить 50 элементов через очередь", "Install 50 items through the queue", 25, "queueInstalls", 50),
        make("skin_viewer", "Рассмотрел себя", "Look At Yourself", "Открыть 3D-просмотр скина", "Open the 3D skin viewer", 5, "skinViewerOpens", 1),
        make("skin_viewer_10", "Модельер", "Fashion Designer", "Открыть 3D-скин 10 раз", "Open the 3D skin viewer 10 times", 10, "skinViewerOpens", 10),
        make("skin_upload", "Новый образ", "New Look", "Сменить скин через лаунчер", "Change your skin from the launcher", 15, "skinUploads", 1),
        make("skin_upload_5", "Гардероб", "Wardrobe", "Сменить скин 5 раз", "Change your skin 5 times", 25, "skinUploads", 5),
        make("profile_icon", "Лицо профиля", "Profile Face", "Установить иконку профиля", "Set a profile icon", 5, "profileIconChanges", 1),
        make("bg_1", "Свой стиль", "Own Style", "Включить свой фон", "Enable a custom background", 5, "backgroundChanges", 1),
        make("bg_5", "Декоратор", "Decorator", "Сменить фон 5 раз", "Change the background 5 times", 10, "backgroundChanges", 5),
        make("bg_10", "Дизайнер интерьера", "Interior Designer", "Сменить фон 10 раз", "Change the background 10 times", 20, "backgroundChanges", 10),
        make("font_custom", "Типограф", "Typographer", "Добавить свой шрифт", "Add a custom font", 10, "customFontsAdded", 1),
        make("console_1", "Внутри системы", "Inside The System", "Открыть консоль", "Open the console", 5, "consoleOpens", 1),
        make("console_25", "Диагност", "Diagnostician", "Открыть консоль 25 раз", "Open the console 25 times", 10, "consoleOpens", 25),
        make("optimize_1", "Разгон", "Overclock", "Применить оптимизацию игры", "Apply game optimization", 10, "optimizationsApplied", 1),
        make("optimize_3", "Тюнинг", "Tuning", "Применить оптимизацию 3 раза", "Apply game optimization 3 times", 20, "optimizationsApplied", 3),
        make("friend_invite", "Позвал друга", "Invited A Friend", "Пригласить друга через Steam", "Invite a friend via Steam", 15, "friendsInvites", 1),
        make("friend_invite_5", "Душа компании", "Party Soul", "Пригласить друзей 5 раз", "Invite friends 5 times", 30, "friendsInvites", 5),
        make("elyby_login", "Свой аккаунт", "Own Account", "Войти через Ely.by", "Sign in via Ely.by", 10, "elybyLogins", 1),
        make("offline_account", "Автономный", "Standalone", "Добавить офлайн-аккаунт", "Add an offline account", 5, "offlineAccounts", 1),
        make("disk_move", "Переезд", "Relocation", "Перенести данные на другой диск", "Move launcher data to another drive", 15, "diskMoves", 1),
        make("export_1", "Экспортёр", "Exporter", "Экспортировать сборку", "Export a build", 10, "exports", 1),
        make("import_1", "Импортёр", "Importer", "Импортировать сборку", "Import a build", 10, "imports", 1),
        make("clone_1", "Клон-мастер", "Clone Master", "Клонировать сборку", "Clone a build", 5, "clones", 1),
        make("crash_1", "Бывает", "It Happens", "Пережить краш игры", "Survive a game crash", 5, "crashesSeen", 1),
        make("java_install", "Своя Java", "Own Java", "Скачать Java через лаунчер", "Download Java via the launcher", 5, "javaInstalls", 1),
        make("noble_build", "Готовое решение", "Ready Solution", "Установить сборку от Noble", "Install a Noble build", 15, "nobleBuildsInstalled", 1),
        make("season_winter", "Зимний игрок", "Winter Player", "Играть в декабре–феврале", "Play in December–February", 5, "seasonWinter", 1),
        make("season_halloween", "Хэллоуин", "Halloween", "Играть в октябре", "Play in October", 5, "seasonHalloween", 1),
        make("season_newyear", "Новый год", "New Year", "Запустить игру 1 января", "Launch the game on January 1", 10, "seasonNewYear", 1),
        make("days_7", "Неделя с лаунчером", "A Week Together", "Заходить 7 дней подряд", "Open the launcher 7 days in a row", 25, "consecutiveDays", 7),
        make("days_30", "Месяц верности", "A Month Of Loyalty", "Заходить 30 дней подряд", "Open the launcher 30 days in a row", 75, "consecutiveDays", 30),
    };
    return list;
}

QString statKeyRu(const QString &key)
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("builds"), QStringLiteral("сборок")},
        {QStringLiteral("gameSeconds"), QStringLiteral("в игре")},
        {QStringLiteral("launcherMinutes"), QStringLiteral("в лаунчере")},
        {QStringLiteral("modsInstalled"), QStringLiteral("модов")},
        {QStringLiteral("gameLaunches"), QStringLiteral("запусков")},
        {QStringLiteral("skinViewerOpens"), QStringLiteral("просмотров скина")},
        {QStringLiteral("backgroundChanges"), QStringLiteral("смен фона")},
        {QStringLiteral("consoleOpens"), QStringLiteral("открытий консоли")},
        {QStringLiteral("optimizationsApplied"), QStringLiteral("оптимизаций")},
    };
    return map.value(key, key);
}

QString statKeyEn(const QString &key)
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("builds"), QStringLiteral("builds")},
        {QStringLiteral("gameSeconds"), QStringLiteral("in game")},
        {QStringLiteral("launcherMinutes"), QStringLiteral("in launcher")},
        {QStringLiteral("modsInstalled"), QStringLiteral("mods")},
        {QStringLiteral("gameLaunches"), QStringLiteral("launches")},
        {QStringLiteral("skinViewerOpens"), QStringLiteral("skin views")},
        {QStringLiteral("backgroundChanges"), QStringLiteral("background changes")},
        {QStringLiteral("consoleOpens"), QStringLiteral("console opens")},
        {QStringLiteral("optimizationsApplied"), QStringLiteral("optimizations")},
    };
    return map.value(key, key);
}

} // namespace Noble
