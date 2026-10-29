#pragma once

#include <QString>
#include <QFile>
#include <QMutex>
#include <functional>

namespace Noble {

class Logger
{
public:
    static void init();
    static void setSink(std::function<void(const QString &level, const QString &text)> sink);
    static void info(const QString &message);
    static void warn(const QString &message);
    static void error(const QString &message);
    static void debug(const QString &message);
    static QString filePath();
    static void shutdown();

private:
    static void write(const QString &level, const QString &message);
};

} // namespace Noble
