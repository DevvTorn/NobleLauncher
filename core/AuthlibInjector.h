#pragma once

#include <QString>

namespace Noble {

namespace AuthlibInjector {
extern const char *kServerUrl;
QString ensure(const QString &runtime, const std::function<void(const QString &stage, const QString &message, double progress)> &report,
               QString *error);
}

} // namespace Noble
