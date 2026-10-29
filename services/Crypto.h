#pragma once

#include <QString>

namespace Noble {

/** Шифрование секрета. Windows: DPAPI (совместимо с Electron safeStorage), иначе base64. */
QString encryptSecret(const QString &plain);

struct SecretReadResult {
    QString value;
    /** false — шифртекст есть, но расшифровать не удалось (данные не теряем). */
    bool ok = true;
};

SecretReadResult tryDecryptSecret(const QString &stored);

} // namespace Noble
