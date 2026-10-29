#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QVariantList>

#include <atomic>
#include <functional>

namespace Noble {

struct JavaInfo {
    QString path;
    QString versionLine;
    int major = -1;
    bool is64Bit = false;
};

struct PickedJava {
    QString path;
    int major = -1;
    bool is64Bit = false;
    QStringList warnings;
};

using JavaProgress = std::function<void(int done, int total)>;

class JavaManager
{
public:
    static QString executableFor(const QString &javaHomeOrExe);
    static int majorVersion(const QString &exe);
    static JavaInfo probe(const QString &exe);
    static void setRuntimeRoot(const QString &path);
    static QVector<JavaInfo> detectAll(const QString &extraPath = QString());
    static QVariantList detectAllVariant(const QString &extraPath = QString());
    static void invalidateDetectionCache();
    static PickedJava pickFor(const QString &mcVersion, const QString &manualPath, const QString &loader, QString *error);
    static bool validateJvm(const QString &exe, const QStringList &args, QString *error);
    static QString installRuntime(int major, const QString &runtimeRoot, const JavaProgress &progress,
                                  std::atomic_bool *cancel, QString *error);
};

} // namespace Noble
