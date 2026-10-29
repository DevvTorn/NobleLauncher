#include "Env.h"

#include "Logger.h"
#include "Paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

namespace Noble {

static const char *kExampleTemplate = R"(# ============================================================================
#  Noble Launcher — внешние настройки приложения (.env)
#  Положите этот файл (переименовав в .env) рядом с .exe, в папку запуска
#  или в %APPDATA%\NobleLauncher\. Правьте ключи здесь — пересборка не нужна.
#  Файл предназначен администратору: обычный пользователь ключей не видит.
# ============================================================================

# --- Ely.by (вход в аккаунт) -------------------------------------------------
# Создайте приложение: https://account.ely.by/dev/apps
# Redirect URI приложения должен совпадать со значением ELYBY_REDIRECT_URI
# (по умолчанию лаунчер использует встроенный адрес колбэка).
ELYBY_CLIENT_ID=
ELYBY_CLIENT_SECRET=
# Необязательно: переопределить Redirect URI приложения.
ELYBY_REDIRECT_URI=
)";

const QStringList &Env::requiredKeys()
{
    static const QStringList keys = {QStringLiteral("ELYBY_CLIENT_ID"), QStringLiteral("ELYBY_CLIENT_SECRET")};
    return keys;
}

Env &Env::instance()
{
    static Env env;
    return env;
}

void Env::load()
{
    m_values = QJsonObject();
    m_files = Paths::envSearchPaths();
    for (const auto &path : m_files) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        while (!stream.atEnd()) {
            const QString raw = stream.readLine().trimmed();
            if (raw.isEmpty() || raw.startsWith(QLatin1Char('#')))
                continue;
            const int eq = raw.indexOf(QLatin1Char('='));
            if (eq < 0)
                continue;
            QString key = raw.left(eq).trimmed();
            if (key.startsWith(QLatin1String("export ")))
                key = key.mid(7).trimmed();
            static const QRegularExpression keyRe(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
            if (!keyRe.match(key).hasMatch())
                continue;
            QString value = raw.mid(eq + 1).trimmed();
            if (value.size() >= 2
                && ((value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
                    || (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\'')))))
                value = value.mid(1, value.size() - 2);
            if (!m_values.contains(key) || m_values.value(key).toString().isEmpty())
                m_values.insert(key, value);
        }
    }
    const QStringList missing = missingRequired();
    if (missing.isEmpty())
        Logger::info(QStringLiteral(".env: обязательные ключи найдены"));
    else
        Logger::warn(QStringLiteral(".env: отсутствуют переменные (%1)").arg(missing.join(QStringLiteral(", "))));
}

QString Env::value(const QString &key) const
{
    return m_values.value(key).toString();
}

QString Env::trimmed(const QString &key) const
{
    return value(key).trimmed();
}

bool Env::has(const QString &key) const
{
    return !trimmed(key).isEmpty();
}

QStringList Env::missingRequired() const
{
    QStringList missing;
    for (const auto &key : requiredKeys()) {
        if (!has(key))
            missing << key;
    }
    return missing;
}

bool Env::anyFileExists() const
{
    for (const auto &path : m_files) {
        if (QFileInfo::exists(path))
            return true;
    }
    return false;
}

QStringList Env::searchedFiles() const
{
    return m_files;
}

QString Env::ensureNextToExecutable()
{
    const QString target = QDir(Paths::exeDir()).filePath(QStringLiteral(".env"));
    if (QFileInfo::exists(target))
        return target;
    QStringList candidates;
    candidates << QDir(Paths::dataDir()).filePath(QStringLiteral(".env"));
    for (const auto &legacy : Paths::legacyDataDirs())
        candidates << QDir(legacy).filePath(QStringLiteral(".env"));
    candidates << QDir(QDir::currentPath()).filePath(QStringLiteral(".env"));
    for (const auto &source : candidates) {
        if (!QFileInfo::exists(source) || QDir(source).absolutePath() == QDir(target).absolutePath())
            continue;
        if (QFile::copy(source, target)) {
            Logger::info(QStringLiteral(".env скопирован рядом с лаунчером"));
            return target;
        }
    }
    QFile file(target);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(kExampleTemplate);
        file.close();
        Logger::info(QStringLiteral(".env создан рядом с лаунчером (ключи не заданы)"));
        const QString example = QDir(Paths::exeDir()).filePath(QStringLiteral(".env.example"));
        if (!QFileInfo::exists(example)) {
            QFile exampleFile(example);
            if (exampleFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                exampleFile.write(kExampleTemplate);
                exampleFile.close();
            }
        }
        return target;
    }
    return ensureExample();
}

QString Env::ensureExample()
{
    if (anyFileExists())
        return m_examplePath;
    if (!m_examplePath.isEmpty())
        return m_examplePath;
    QStringList candidates;
    candidates << Paths::exeDir();
    candidates << Paths::dataDir();
    candidates << QDir::currentPath();
    for (const auto &dir : candidates) {
        const QString target = QDir(dir).filePath(QStringLiteral(".env.example"));
        if (QFileInfo::exists(target)) {
            m_examplePath = target;
            return target;
        }
        QDir().mkpath(dir);
        QFile file(target);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(kExampleTemplate);
            file.close();
            m_examplePath = target;
            Logger::info(QStringLiteral(".env не найден — создан шаблон .env.example"));
            return target;
        }
    }
    return QString();
}

} // namespace Noble
