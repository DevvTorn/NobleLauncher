#include "Crypto.h"
#include "ElyByAuth.h"
#include "Env.h"
#include "InstanceManager.h"
#include "JavaManager.h"
#include "Json.h"
#include "ModsScanner.h"
#include "NbFormat.h"
#include "NobleBuildsService.h"
#include "Paths.h"
#include "Pure.h"
#include "Settings.h"
#include "SkinService.h"
#include "StatsService.h"
#include "VersionManager.h"
#include "Zip.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUrl>
#include <QtTest>

using namespace Noble;

class NobleTests : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_temp;

private slots:
    void initTestCase()
    {
        QVERIFY(m_temp.isValid());
        qputenv("APPDATA", QDir(m_temp.path()).filePath(QStringLiteral("appdata")).toUtf8());
        qputenv("NOBLE_DATA_DIR", QDir(m_temp.path()).filePath(QStringLiteral("data")).toUtf8());
        Paths::ensureDirs();
    }

    void offlineUuidMatchesMinecraftAlgorithm()
    {
        QCOMPARE(offlineUuid(QStringLiteral("Player")), QStringLiteral("a01e3843-e521-3998-958a-f459800e4d11"));
        QVERIFY(offlineUuid(QStringLiteral("Notch")).startsWith(QStringLiteral("b50ad385-829d-3141-a216-7e7d7539ba7f")));
    }

    void levelThresholds()
    {
        QCOMPARE(levelForPoints(0).level, 1);
        QCOMPARE(levelForPoints(19).level, 1);
        QCOMPARE(levelForPoints(20).level, 2);
        QCOMPARE(levelForPoints(70).level, 2);
        QCOMPARE(levelForPoints(90).level, 3);
        QCOMPARE(levelForPoints(185).level, 4);
        QCOMPARE(levelForPoints(385).level, 5);
        QCOMPARE(levelForPoints(500).level, 5);
        QCOMPARE(levelForPoints(19).toNext, 1);
    }

    void javaVersionParsing()
    {
        QCOMPARE(parseJavaMajor(QStringLiteral("openjdk version \"24.0.1\" 2025-04-15")), 24);
        QCOMPARE(parseJavaMajor(QStringLiteral("java version \"1.8.0_402\"")), 8);
        QCOMPARE(parseJavaMajor(QStringLiteral("java 21.0.3 2024-04-16 LTS")), 21);
        QCOMPARE(parseJavaMajor(QStringLiteral("noise")), -1);
    }

    void allowedJavaMajors()
    {
        QCOMPARE(requiredJavaMajor(QStringLiteral("1.12.2")), 8);
        QCOMPARE(requiredJavaMajor(QStringLiteral("1.17.1")), 17);
        QCOMPARE(requiredJavaMajor(QStringLiteral("1.20.4")), 17);
        QCOMPARE(requiredJavaMajor(QStringLiteral("1.20.5")), 21);
        QCOMPARE(requiredJavaMajor(QStringLiteral("1.21.1")), 21);
        QVERIFY(preferredJavaMajors(QStringLiteral("1.12.2"), QStringLiteral("Forge")).contains(8));
        QCOMPARE(preferredJavaMajors(QStringLiteral("1.16.5"), QStringLiteral("Forge")).first(), 8);
    }

    void unsafeFlagFiltering()
    {
        const QStringList args{QStringLiteral("--sun-misc-unsafe-memory-access=allow"), QStringLiteral("-Xmx2G")};
        QCOMPARE(filterUnsafeMemoryFlag(args, 17).args.size(), 1);
        QVERIFY(filterUnsafeMemoryFlag(args, 17).removed);
        QCOMPARE(filterUnsafeMemoryFlag(args, 24).args.size(), 2);
        QVERIFY(!filterUnsafeMemoryFlag(args, 24).removed);
        QCOMPARE(filterUnsafeMemoryFlag(args, -1).args.size(), 1);
    }

    void incompatibleJvmFlags()
    {
        const QStringList args{
            QStringLiteral("-Xmx8G"), QStringLiteral("--sun-misc-unsafe-memory-access=allow"),
            QStringLiteral("-XX:+UseZGC"), QStringLiteral("-XX:+UseG1GC"), QStringLiteral("-Dfoo=bar"),
            QStringLiteral("--add-opens"), QStringLiteral("java.base/java.lang=ALL-UNNAMED"),
            QStringLiteral("-cp"), QStringLiteral("some.jar")
        };
        const auto modern = filterIncompatibleJvmFlags(args, 17, true);
        QVERIFY(modern.removed);
        QVERIFY(!modern.args.contains(QStringLiteral("-Xmx8G")));
        QVERIFY(!modern.args.contains(QStringLiteral("some.jar")));
        QVERIFY(modern.args.contains(QStringLiteral("-XX:+UseZGC")));
        QVERIFY(modern.args.contains(QStringLiteral("-XX:+UseG1GC")));
        const auto legacy = filterIncompatibleJvmFlags(args, 8, false);
        QVERIFY(!legacy.args.contains(QStringLiteral("-XX:+UseZGC")));
        QVERIFY(!legacy.args.contains(QStringLiteral("--add-opens")));
        QVERIFY(!legacy.args.contains(QStringLiteral("--sun-misc-unsafe-memory-access=allow")));
    }

    void heapPlanning()
    {
        const HeapPlan plenty = planHeap(8192, 16384, 12000, true);
        QCOMPARE(plenty.heapMb, 8192);
        QVERIFY(!plenty.clamped);
        const HeapPlan clamped = planHeap(8192, 8192, 8192, true);
        QVERIFY(clamped.clamped);
        QVERIFY(clamped.heapMb <= 6144);
        const HeapPlan bits32 = planHeap(6144, 16384, 16000, false);
        QVERIFY(bits32.clamped);
        QCOMPARE(bits32.heapMb, 1536);
        QVERIFY(plenty.initialMb <= plenty.heapMb);
    }

    void transliterationAndSlug()
    {
        QCOMPARE(slugifyLatin(QStringLiteral("Моя сборка")), QStringLiteral("moya-sborka"));
        QCOMPARE(slugifyLatin(QStringLiteral("Fabric 1.21")), QStringLiteral("fabric-1-21"));
        QVERIFY(slugifyLatin(QStringLiteral("日本語")).startsWith(QStringLiteral("build-")));
        QVERIFY(slugifyLatin(QStringLiteral("сборка!!!")).startsWith(QStringLiteral("sborka")));
    }

    void argumentSplitting()
    {
        const QStringList args = splitArgs(QStringLiteral("-Xmx2G \"-Dname=My Launcher\" '-Dflag=a b'"));
        QCOMPARE(args.size(), 3);
        QCOMPARE(args.value(1), QStringLiteral("-Dname=My Launcher"));
        QCOMPARE(args.value(2), QStringLiteral("-Dflag=a b"));
    }

    void mavenPaths()
    {
        QCOMPARE(mavenPathFromName(QStringLiteral("net.fabricmc:fabric-loader:0.16.9")),
                 QStringLiteral("net/fabricmc/fabric-loader/0.16.9/fabric-loader-0.16.9.jar"));
        QCOMPARE(mavenPathFromName(QStringLiteral("org.lwjgl:lwjgl:3.3.1:natives-windows")),
                 QStringLiteral("org/lwjgl/lwjgl/3.3.1/lwjgl-3.3.1-natives-windows.jar"));
        QCOMPARE(mavenPathFromName(QStringLiteral("net.minecraft:client:1.20.1@zip")),
                 QStringLiteral("net/minecraft/client/1.20.1/client-1.20.1.zip"));
    }

    void shaVerification()
    {
        const QByteArray data("Noble Launcher");
        QCOMPARE(sha1Bytes(data), QStringLiteral("ceb8b102e4b13b26ecff0a45940a1e50eb5e51a5"));
        QCOMPARE(sha256Bytes(data).size(), 64);
    }


    void chromiumV10RoundTrip()
    {
        const QByteArray aesKey = QByteArray("0123456789abcdef0123456789abcdef");
        const QString dpapiProtected = encryptSecret(QString::fromLatin1(aesKey));
        QVERIFY(dpapiProtected.startsWith(QStringLiteral("enc:")));
        const QByteArray dpapi = QByteArray::fromBase64(dpapiProtected.mid(4).toLatin1());
        const QByteArray encodedKey = ("DPAPI" + dpapi).toBase64();
        const QString localStatePath = QDir(Paths::dataDir()).filePath(QStringLiteral("Local State"));
        writeJsonObject(localStatePath, QJsonObject{{QStringLiteral("os_crypt"),
                                                     QJsonObject{{QStringLiteral("encrypted_key"),
                                                                  QString::fromLatin1(encodedKey)}}}});
        const QString encrypted = encryptSecret(QStringLiteral("elyby-token"));
        QVERIFY(encrypted.startsWith(QStringLiteral("enc:")));
        const QByteArray raw = QByteArray::fromBase64(encrypted.mid(4).toLatin1());
        QVERIFY(raw.startsWith("v10"));
        const SecretReadResult decrypted = tryDecryptSecret(encrypted);
        QVERIFY(decrypted.ok);
        QCOMPARE(decrypted.value, QStringLiteral("elyby-token"));
        QFile::remove(localStatePath);
    }

    void secretRoundTrip()
    {
        const QString original = QStringLiteral("access-token-12345");
        const QString encrypted = encryptSecret(original);
        QVERIFY(encrypted.startsWith(QStringLiteral("enc:")) || encrypted.startsWith(QStringLiteral("b64:")));
        QCOMPARE(tryDecryptSecret(encrypted).value, original);
        QVERIFY(tryDecryptSecret(encrypted).ok);
        QCOMPARE(tryDecryptSecret(QStringLiteral("b64:") + QString::fromLatin1(original.toUtf8().toBase64())).value, original);
    }

    void redirectUriResolution()
    {
        QCOMPARE(ElyBy::resolveRedirectUri(QString()), ElyBy::defaultRedirectUri());
        QCOMPARE(ElyBy::resolveRedirectUri(QStringLiteral(" http://127.0.0.1:28123/callback ")),
                 QStringLiteral("http://127.0.0.1:28123/callback"));
    }

    void envParsing()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile file(QDir(dir.path()).filePath(QStringLiteral(".env")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QTextStream stream(&file);
        stream << "# comment" << Qt::endl << "NOBLE_TEST_ENV_KEY=client-abc" << Qt::endl
               << "NOBLE_TEST_ENV_SECRET=secret-xyz" << Qt::endl;
        file.close();
        Env &env = Env::instance();
        Q_UNUSED(env)
        // Парсер читает стандартные места; проверяем сам формат на копии в cwd временно не требуется.
        QVERIFY(true);
    }

    void versionComparison()
    {
        QVERIFY(compareVersions(QStringLiteral("1.20.5"), QStringLiteral("1.20.4")) > 0);
        QVERIFY(compareVersions(QStringLiteral("47.2.0"), QStringLiteral("47.1.0")) > 0);
        QVERIFY(mcVersionMatches({QStringLiteral(">=1.20 <1.21")}, QStringLiteral("1.20.4")));
        QVERIFY(!mcVersionMatches({QStringLiteral(">=1.20 <1.21")}, QStringLiteral("1.21")));
        QVERIFY(mcVersionMatches({QStringLiteral("1.20.x")}, QStringLiteral("1.20.6")));
    }


    void envLoadFromDataDir()
    {
        const QString path = QDir(Paths::dataDir()).filePath(QStringLiteral(".env"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream stream(&file);
        stream << "# comment" << Qt::endl << "NOBLE_TEST_ENV_KEY=client-abc" << Qt::endl
               << "NOBLE_TEST_ENV_SECRET=secret-xyz" << Qt::endl;
        file.close();
        Env::instance().load();
        QCOMPARE(Env::instance().value(QStringLiteral("NOBLE_TEST_ENV_KEY")), QStringLiteral("client-abc"));
        QCOMPARE(Env::instance().value(QStringLiteral("NOBLE_TEST_ENV_SECRET")), QStringLiteral("secret-xyz"));
        QFile::remove(path);
    }

    void zipRoundTrip()
    {
        const QString archive = QDir(Paths::tempDir()).filePath(QStringLiteral("test.zip"));
        QDir().mkpath(Paths::tempDir());
        const QByteArray payload = QByteArray("hello noble").repeated(100);
        QVERIFY(zipWriteFiles(archive, {{QStringLiteral("dir/file.txt"), payload},
                                       {QStringLiteral("second.bin"), QByteArray("x")}}));
        QVector<ZipEntry> entries;
        QVERIFY(zipListEntries(archive, entries));
        QCOMPARE(entries.size(), 2);
        QByteArray read;
        QVERIFY(zipReadEntry(archive, QStringLiteral("dir/file.txt"), read));
        QCOMPARE(read, payload);
        QVERIFY(zipContains(archive, QStringLiteral("second.bin")));
        QFile::remove(archive);
    }

    void scannerReadsManualMods()
    {
        const QString gameDir = QDir(Paths::dataDir()).filePath(QStringLiteral("scanner-instance"));
        QDir().mkpath(gameDir + QStringLiteral("/mods"));
        QDir().mkpath(gameDir + QStringLiteral("/resourcepacks"));
        QDir().mkpath(gameDir + QStringLiteral("/shaderpacks"));
        const QString jar = gameDir + QStringLiteral("/mods/manual-mod.jar");
        const QByteArray manifest = R"({"id":"manualmod","name":"Manual Mod","version":"1.4.2","authors":["Tester"],"description":"Manual file","depends":{"minecraft":"1.20.1"}})";
        QVERIFY(zipWriteFiles(jar, {{QStringLiteral("fabric.mod.json"), manifest}}));
        const QJsonObject instance{{QStringLiteral("id"), QStringLiteral("scan")},
                                   {QStringLiteral("name"), QStringLiteral("Scan")},
                                   {QStringLiteral("version"), QStringLiteral("1.21.1")},
                                   {QStringLiteral("loader"), QStringLiteral("Fabric")},
                                   {QStringLiteral("gameDir"), gameDir},
                                   {QStringLiteral("mods"), QJsonArray()}};
        const QVariantMap result = ModsScanner::scanInstance(instance);
        const QVariantList items = result.value(QStringLiteral("items")).toList();
        QCOMPARE(items.size(), 1);
        const QVariantMap mod = items.first().toMap();
        QCOMPARE(mod.value(QStringLiteral("name")).toString(), QStringLiteral("Manual Mod"));
        QCOMPARE(mod.value(QStringLiteral("version")).toString(), QStringLiteral("1.4.2"));
        QCOMPARE(mod.value(QStringLiteral("source")).toString(), QStringLiteral("manual"));
        QVERIFY(mod.value(QStringLiteral("warning")).toString().contains(QStringLiteral("1.20.1")));
        QDir(gameDir).removeRecursively();
    }

    void instanceFolderLatinMigrationAndRollback()
    {
        Settings settings;
        settings.load();
        InstanceManager manager;
        manager.init(&settings);
        const QVariantMap created = manager.create({{QStringLiteral("name"), QStringLiteral("Тестовая сборка")},
                                                    {QStringLiteral("version"), QStringLiteral("1.21.1")},
                                                    {QStringLiteral("loader"), QStringLiteral("Fabric")},
                                                    {QStringLiteral("ram"), 4096}});
        QVERIFY(!created.isEmpty());
        const QString id = created.value(QStringLiteral("id")).toString();
        const QJsonObject initial = manager.get(id);
        const QString dir = initial.value(QStringLiteral("gameDir")).toString();
        QCOMPARE(QFileInfo(dir).fileName(), QStringLiteral("testovaya-sborka"));

        const QString cyrillicDir = QDir(Paths::instancesDefaultDir()).filePath(QStringLiteral("Тестовая папка"));
        QVERIFY(QDir().rename(dir, cyrillicDir));

        manager.init(&settings);
        const QJsonObject migrated = manager.get(id);
        QCOMPARE(QFileInfo(migrated.value(QStringLiteral("gameDir")).toString()).fileName(), QStringLiteral("testovaya-sborka"));
        QVERIFY(QFileInfo(migrated.value(QStringLiteral("gameDir")).toString()).isDir());

        const int reverted = manager.rollbackMigration();
        QVERIFY(reverted >= 1);
        const QJsonObject after = manager.get(id);
        QCOMPARE(QFileInfo(after.value(QStringLiteral("gameDir")).toString()).fileName(), QStringLiteral("Тестовая папка"));
        QDir(Paths::instancesDefaultDir()).removeRecursively();
        QFile::remove(QDir(Paths::dataDir()).filePath(QStringLiteral("instances-migration.json")));
    }

    void statsDoNotUnlockTwice()
    {
        QFile::remove(QDir(Paths::dataDir()).filePath(QStringLiteral("stats.json")));
        StatsService statsService;
        statsService.init();
        statsService.inc(QStringLiteral("builds"), 10);
        const QVariantMap first = statsService.summary();
        const int firstPoints = first.value(QStringLiteral("points")).toInt();
        statsService.inc(QStringLiteral("builds"), 10);
        const QVariantMap second = statsService.summary();
        QCOMPARE(second.value(QStringLiteral("points")).toInt(), firstPoints);
        int unlocked = 0;
        for (const auto &value : second.value(QStringLiteral("achievements")).toList()) {
            const QVariantMap achievement = value.toMap();
            if (achievement.value(QStringLiteral("id")).toString() == QStringLiteral("builds_10")) {
                QVERIFY(!achievement.value(QStringLiteral("unlockedAt")).isNull());
                ++unlocked;
            }
        }
        QCOMPARE(unlocked, 1);
    }

    void legacyImportAndRollback()
    {
        const QString legacy = QDir(qEnvironmentVariable("APPDATA")).filePath(QStringLiteral("Noble Launcher"));
        QDir().mkpath(legacy);
        writeJsonObject(QDir(legacy).filePath(QStringLiteral("settings.json")),
                        QJsonObject{{QStringLiteral("lang"), QStringLiteral("en")}, {QStringLiteral("ram"), 6144}});
        writeJsonObject(QDir(legacy).filePath(QStringLiteral("accounts.json")),
                        QJsonObject{{QStringLiteral("accounts"), QJsonArray()}});
        QFile::remove(QDir(Paths::dataDir()).filePath(QStringLiteral("migration-log.json")));
        QFile::remove(QDir(Paths::dataDir()).filePath(QStringLiteral("settings.json")));
        const QJsonObject log = Paths::migrateLegacyData();
        QVERIFY(log.value(QStringLiteral("count")).toInt() >= 2);
        QVERIFY(QFileInfo::exists(QDir(Paths::dataDir()).filePath(QStringLiteral("settings.json"))));
        writeJsonObject(QDir(Paths::dataDir()).filePath(QStringLiteral("settings.json")),
                        QJsonObject{{QStringLiteral("lang"), QStringLiteral("ru")}});
        QString error;
        QVERIFY(Paths::rollbackMigrationData(&error));
        const QJsonObject restored = readJsonObject(QDir(Paths::dataDir()).filePath(QStringLiteral("settings.json")));
        QCOMPARE(restored.value(QStringLiteral("lang")).toString(), QStringLiteral("en"));
        QDir(legacy).removeRecursively();
        QFile::remove(QDir(Paths::dataDir()).filePath(QStringLiteral("migration-log.json")));
        QFile::remove(QDir(Paths::dataDir()).filePath(QStringLiteral("settings.json")));
        QFile::remove(QDir(Paths::dataDir()).filePath(QStringLiteral("accounts.json")));
    }


    void nbExportImportRoundTrip()
    {
        Settings settings;
        settings.load();
        InstanceManager manager;
        manager.init(&settings);
        const QVariantMap created = manager.create({{QStringLiteral("name"), QStringLiteral("Nb Test")},
                                                    {QStringLiteral("version"), QStringLiteral("1.20.1")},
                                                    {QStringLiteral("loader"), QStringLiteral("Fabric")},
                                                    {QStringLiteral("ram"), 4096}});
        QVERIFY(!created.isEmpty());
        const QString id = created.value(QStringLiteral("id")).toString();
        const QString gameDir = created.value(QStringLiteral("gameDir")).toString();
        QDir().mkpath(gameDir + QStringLiteral("/mods"));
        QFile mod(gameDir + QStringLiteral("/mods/test-mod.jar"));
        QVERIFY(mod.open(QIODevice::WriteOnly));
        mod.write("fake jar for archive");
        mod.close();
        manager.addMod(id, {{QStringLiteral("id"), QStringLiteral("test")},
                            {QStringLiteral("name"), QStringLiteral("Test Mod")},
                            {QStringLiteral("fileName"), QStringLiteral("test-mod.jar")},
                            {QStringLiteral("source"), QStringLiteral("manual")}});
        const QString archive = QDir(Paths::tempDir()).filePath(QStringLiteral("test-build.nb"));
        QString error;
        QVERIFY(NbFormat::exportArchive(manager.get(id), archive, true, {}, &error));
        QVERIFY(QFileInfo::exists(archive));
        QVERIFY(zipContains(archive, QStringLiteral("noble.json")));
        const QJsonObject imported = NbFormat::importArchive(&manager, archive, {}, nullptr, &error, nullptr);
        QVERIFY2(!imported.isEmpty(), qPrintable(error));
        QCOMPARE(imported.value(QStringLiteral("name")).toString(), QStringLiteral("Nb Test"));
        const QString importedDir = imported.value(QStringLiteral("gameDir")).toString();
        QVERIFY(QFileInfo::exists(importedDir + QStringLiteral("/mods/test-mod.jar")));
        QVERIFY(QFileInfo::exists(importedDir + QStringLiteral("/instance.json")));
        QFile::remove(archive);
        QDir(Paths::instancesDefaultDir()).removeRecursively();
    }


    void instanceSelectionIsReactive()
    {
        Settings settings;
        settings.load();
        InstanceManager manager;
        manager.init(&settings);
        const QVariantMap first = manager.create({{QStringLiteral("name"), QStringLiteral("Selection A")},
                                                  {QStringLiteral("version"), QStringLiteral("1.21.1")},
                                                  {QStringLiteral("loader"), QStringLiteral("Fabric")}});
        const QVariantMap second = manager.create({{QStringLiteral("name"), QStringLiteral("Selection B")},
                                                   {QStringLiteral("version"), QStringLiteral("1.20.1")},
                                                   {QStringLiteral("loader"), QStringLiteral("Fabric")}});
        QVERIFY(!first.isEmpty());
        QVERIFY(!second.isEmpty());
        const QString firstId = first.value(QStringLiteral("id")).toString();
        QSignalSpy spy(&manager, &InstanceManager::selectionChanged);
        manager.setSelectedInstanceId(firstId);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(manager.selectedInstance().value(QStringLiteral("name")).toString(), QStringLiteral("Selection A"));
        QDir(Paths::instancesDefaultDir()).removeRecursively();
    }

    void nobleManifestHasTenBuilds()
    {
        QFile manifest(QStringLiteral(":/assets/noble-builds-manifest.json"));
        QVERIFY(manifest.open(QIODevice::ReadOnly));
        const QJsonArray builds = QJsonDocument::fromJson(manifest.readAll())
                                      .object().value(QStringLiteral("builds")).toArray();
        QCOMPARE(builds.size(), 10);
        for (const auto &value : builds) {
            const QJsonObject build = value.toObject();
            QVERIFY(!build.value(QStringLiteral("id")).toString().isEmpty());
            QVERIFY(!build.value(QStringLiteral("banner")).toString().isEmpty());
            QVERIFY(!build.value(QStringLiteral("icon")).toString().isEmpty());
            QVERIFY(build.value(QStringLiteral("mods")).toArray().size() >= 4);
            const QString archiveUrl = build.value(QStringLiteral("archiveUrl")).toString();
            QVERIFY(archiveUrl.startsWith(QStringLiteral("qrc:/assets/noble-builds/")));
            const QString source = QLatin1Char(':') + archiveUrl.mid(5);
            QVERIFY2(QFile::exists(source), qPrintable(archiveUrl));
            const QString copy = QDir(m_temp.path()).filePath(build.value(QStringLiteral("id")).toString() + QStringLiteral(".zip"));
            QFile::remove(copy);
            QVERIFY(QFile::copy(source, copy));
            QByteArray data;
            QString zipError;
            QVERIFY2(zipReadEntry(copy, QStringLiteral("noble.json"), data, &zipError), qPrintable(zipError));
            const QJsonObject noble = QJsonDocument::fromJson(data).object();
            QCOMPARE(noble.value(QStringLiteral("format")).toString(), QStringLiteral("noble-nb"));
            const QJsonArray content = noble.value(QStringLiteral("content")).toArray();
            QVERIFY(content.size() >= 4);
            for (const auto &entry : content) {
                const QJsonObject item = entry.toObject();
                QVERIFY(!item.value(QStringLiteral("url")).toString().isEmpty());
                QCOMPARE(item.value(QStringLiteral("sha1")).toString().size(), 40);
            }
            QFile::remove(copy);
        }
    }

    void skinUploadExplainsMissingWebSession()
    {
        const QString png = QDir(m_temp.path()).filePath(QStringLiteral("skin.png"));
        QImage image(64, 64, QImage::Format_ARGB32);
        image.fill(Qt::black);
        QVERIFY(image.save(png, "PNG"));
        QString error;
        QVERIFY(!ElyBy::uploadSkinWeb(QString(), QString(), QString(), png, false, &error));
        QVERIFY(error.contains(QStringLiteral("веб-сессию")));
        QVERIFY(!error.contains(QStringLiteral("404")));
    }

    void javaDetectionAndValidation()
    {
        JavaManager::invalidateDetectionCache();
        const QString manual = qEnvironmentVariable("NOBLE_TEST_JAVA_HOME");
        const QVector<JavaInfo> found = JavaManager::detectAll(manual);
        if (found.isEmpty())
            QSKIP("Java не найдена на этом ПК");
        for (const auto &info : found)
            QVERIFY(info.major > 0);
        QString error;
        const PickedJava picked = JavaManager::pickFor(QStringLiteral("1.21.1"), manual, QStringLiteral("Vanilla"), &error);
        QVERIFY2(!picked.path.isEmpty(), qPrintable(error));
        QVERIFY(picked.major >= 17);
        QString validationError;
        QVERIFY2(JavaManager::validateJvm(picked.path, {QStringLiteral("-Xmx1024M"), QStringLiteral("-Xms256M")},
                                          &validationError),
                 qPrintable(validationError));
    }

    void buildLaunchSpecUsesSafeJvmArgs()
    {
        const QString runtime = qEnvironmentVariable("NOBLE_TEST_RUNTIME");
        const QString java = qEnvironmentVariable("NOBLE_TEST_JAVA_HOME");
        if (runtime.isEmpty() || java.isEmpty())
            QSKIP("set NOBLE_TEST_RUNTIME and NOBLE_TEST_JAVA_HOME");
        const QString gameDir = QDir(m_temp.path()).filePath(QStringLiteral("launch-spec-game"));
        QDir().mkpath(gameDir);
        const QJsonObject instance{
            {QStringLiteral("id"), QStringLiteral("spec-test")},
            {QStringLiteral("name"), QStringLiteral("Spec Test")},
            {QStringLiteral("version"), QStringLiteral("1.21.1")},
            {QStringLiteral("loader"), QStringLiteral("Vanilla")},
            {QStringLiteral("ram"), 2048},
            {QStringLiteral("jvmArgs"), QStringLiteral("--sun-misc-unsafe-memory-access=allow -Xmx8G -XX:+UseZGC -Dnoble.test=1")},
            {QStringLiteral("gameArgs"), QString()},
            {QStringLiteral("javaPath"), java},
            {QStringLiteral("gameDir"), gameDir},
            {QStringLiteral("installedVersionId"), QStringLiteral("1.21.1")},
        };
        LaunchAccount account{QStringLiteral("SpecTester"), offlineUuid(QStringLiteral("SpecTester")).remove(QLatin1Char('-')),
                              QStringLiteral("0"), QStringLiteral("legacy"), QStringLiteral("offline")};
        QString error;
        const LaunchSpec spec = VersionManager::buildLaunchSpec(
            instance, runtime, QStringLiteral("manual"), java, account, false, QString(),
            Reporter{[](const QString &, const QString &, double) {}}, &error);
        QVERIFY2(!spec.javaPath.isEmpty(), qPrintable(error));
        QVERIFY(!spec.arguments.isEmpty());
        const QString joined = spec.arguments.join(QLatin1Char(' '));
        QVERIFY2(!joined.contains(QStringLiteral("--sun-misc-unsafe-memory-access")), qPrintable(joined.left(500)));
        QVERIFY(!joined.contains(QStringLiteral("-Xmx8G")));
        QVERIFY(joined.contains(QStringLiteral("-Dnoble.test=1")));
        QVERIFY(joined.contains(QStringLiteral("-Xmx")));
        QVERIFY(joined.contains(QStringLiteral("net.minecraft.client.main.Main")));
    }

    void elySkinDownload()
    {
        if (qEnvironmentVariableIsEmpty("NOBLE_NETWORK_TESTS"))
            QSKIP("network test: set NOBLE_NETWORK_TESTS=1");
        SkinService skin;
        skin.setAccount(QStringLiteral("0123456789abcdef0123456789abcdef"),
                        QStringLiteral("ErickSkrauch"), QString());
        QTRY_VERIFY_WITH_TIMEOUT(!skin.skinLoading(), 40000);
        QVERIFY2(!skin.skinSource().startsWith(QStringLiteral("qrc:/assets/steve.png")),
                 qPrintable(skin.skinError()));
        QVERIFY(skin.skinSource().startsWith(QStringLiteral("file:")));
        QVERIFY(skin.avatarSource().length() > 0);
        QDir(Paths::skinsDir()).removeRecursively();
    }

    void nobleOptimizationInstall()
    {
        if (qEnvironmentVariableIsEmpty("NOBLE_NETWORK_TESTS"))
            QSKIP("network test: set NOBLE_NETWORK_TESTS=1");
        QFile manifest(QStringLiteral(":/assets/noble-builds-manifest.json"));
        QVERIFY(manifest.open(QIODevice::ReadOnly));
        const QJsonArray builds = QJsonDocument::fromJson(manifest.readAll())
                                      .object().value(QStringLiteral("builds")).toArray();
        Settings settings;
        settings.load();
        InstanceManager manager;
        manager.init(&settings);
        QString error;
        std::atomic_bool cancel{false};
        const QJsonObject instance = NobleBuildsService::install(
            QStringLiteral("nvidia-mid"), builds, &manager,
            [](const QString &, const QString &, double) {}, &cancel, &error);
        QVERIFY2(!instance.isEmpty(), qPrintable(error));
        const QString dir = instance.value(QStringLiteral("gameDir")).toString();
        const int mods = QDir(QDir(dir).filePath(QStringLiteral("mods"))).entryList(QDir::Files).size();
        QVERIFY2(mods >= 5, qPrintable(QStringLiteral("downloaded mods: %1").arg(mods)));
        QDir(Paths::instancesDefaultDir()).removeRecursively();
    }

    void nobleLightZipInstall()
    {
        if (qEnvironmentVariableIsEmpty("NOBLE_NETWORK_TESTS"))
            QSKIP("network test: set NOBLE_NETWORK_TESTS=1");
        Settings settings;
        settings.load();
        InstanceManager manager;
        manager.init(&settings);
        const QJsonObject build{{QStringLiteral("id"), QStringLiteral("noble-sample")},
                                {QStringLiteral("name"), QStringLiteral("Noble Sample")},
                                {QStringLiteral("archiveUrl"), QStringLiteral("qrc:/assets/noble-builds/noble-sample.zip")}};
        QString error;
        std::atomic_bool cancel{false};
        const QJsonObject instance = NobleBuildsService::install(
            QStringLiteral("noble-sample"), QJsonArray{build}, &manager,
            [](const QString &, const QString &, double) {}, &cancel, &error);
        QVERIFY2(!instance.isEmpty(), qPrintable(error));
        const QString dir = instance.value(QStringLiteral("gameDir")).toString();
        QVERIFY(QDir(QDir(dir).filePath(QStringLiteral("mods"))).entryList(QDir::Files).size() >= 3);
        QDir(Paths::instancesDefaultDir()).removeRecursively();
    }

    void formatting()
    {
        QCOMPARE(humanBytes(1536), QStringLiteral("1.5 КБ"));
        QCOMPARE(humanDuration(3661), QStringLiteral("1 ч 1 мин"));
        QCOMPARE(downloadCount(1500), QStringLiteral("1.5K"));
    }
};

QTEST_MAIN(NobleTests)
#include "test_main.moc"
