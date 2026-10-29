#include "Logger.h"

#include "Paths.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

namespace Noble {

namespace {
QMutex g_mutex;
QFile g_file;
std::function<void(const QString &, const QString &)> g_sink;
QString g_path;

void rotateIfNeeded(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists() || info.size() < 2 * 1024 * 1024)
        return;
    const QString previous = QDir(info.absolutePath()).filePath(QStringLiteral("launcher.prev.log"));
    QFile::remove(previous);
    QFile::rename(path, previous);
}
} // namespace

void Logger::init()
{
    QMutexLocker lock(&g_mutex);
    QDir().mkpath(Paths::logsDir());
    g_path = QDir(Paths::logsDir()).filePath(QStringLiteral("launcher.log"));
    rotateIfNeeded(g_path);
    g_file.setFileName(g_path);
    g_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    g_mutex.unlock();
    info(QStringLiteral("Запуск Noble Launcher 2.9.0"));
    g_mutex.lock();
}

void Logger::setSink(std::function<void(const QString &, const QString &)> sink)
{
    QMutexLocker lock(&g_mutex);
    g_sink = std::move(sink);
}

void Logger::write(const QString &level, const QString &message)
{
    const QString line = QStringLiteral("[%1] [%2] %3")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs), level.toUpper(), message);
    std::function<void(const QString &, const QString &)> sink;
    {
        QMutexLocker lock(&g_mutex);
        if (g_file.isOpen()) {
            g_file.write(line.toUtf8() + '\n');
            g_file.flush();
        }
        sink = g_sink;
    }
    if (level == QLatin1String("ERROR"))
        qCritical("%s", qPrintable(message));
    else if (level == QLatin1String("WARN"))
        qWarning("%s", qPrintable(message));
    else
        qDebug("%s", qPrintable(message));
    if (sink)
        sink(level.toLower(), message);
}

void Logger::info(const QString &message) { write(QStringLiteral("INFO"), message); }
void Logger::warn(const QString &message) { write(QStringLiteral("WARN"), message); }
void Logger::error(const QString &message) { write(QStringLiteral("ERROR"), message); }
void Logger::debug(const QString &message) { write(QStringLiteral("DEBUG"), message); }

QString Logger::filePath()
{
    QMutexLocker lock(&g_mutex);
    return g_path;
}

void Logger::shutdown()
{
    QMutexLocker lock(&g_mutex);
    if (g_file.isOpen())
        g_file.close();
}

} // namespace Noble
