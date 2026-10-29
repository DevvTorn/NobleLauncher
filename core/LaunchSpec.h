#pragma once

#include <QString>
#include <QStringList>

namespace Noble {

struct LaunchSpec {
    QString javaPath;
    QStringList arguments;
    QString workingDir;
    QString instanceId;
    QString versionId;
    int javaMajor = -1;
    bool java64Bit = false;
    QStringList warnings;
};

struct LaunchAccount {
    QString name;
    QString uuid;
    QString accessToken;
    QString userType;
    QString provider;
};

} // namespace Noble
