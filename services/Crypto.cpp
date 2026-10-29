#include "Crypto.h"

#include "Json.h"
#include "Logger.h"
#include "Paths.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#endif

namespace Noble {

#ifdef Q_OS_WIN
static QByteArray protect(const QByteArray &plain, bool *ok)
{
    DATA_BLOB in;
    in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(plain.constData()));
    in.cbData = static_cast<DWORD>(plain.size());
    DATA_BLOB out{};
    const BOOL result = CryptProtectData(&in, L"NobleLauncher", nullptr, nullptr, nullptr,
                                         CRYPTPROTECT_UI_FORBIDDEN, &out);
    if (!result) {
        if (ok)
            *ok = false;
        return {};
    }
    const QByteArray data(reinterpret_cast<const char *>(out.pbData), static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    if (ok)
        *ok = true;
    return data;
}

static QByteArray unprotect(const QByteArray &cipher, bool *ok)
{
    DATA_BLOB in;
    in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(cipher.constData()));
    in.cbData = static_cast<DWORD>(cipher.size());
    DATA_BLOB out{};
    const BOOL result = CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
                                           CRYPTPROTECT_UI_FORBIDDEN, &out);
    if (!result) {
        if (ok)
            *ok = false;
        return {};
    }
    const QByteArray data(reinterpret_cast<const char *>(out.pbData), static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    if (ok)
        *ok = true;
    return data;
}

static bool aesGcmDecrypt(const QByteArray &key, const QByteArray &nonce, const QByteArray &ciphertext,
                          const QByteArray &tag, QByteArray *plain)
{
    if (key.size() != 32 || nonce.size() != 12 || tag.size() != 16)
        return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_KEY_HANDLE keyHandle = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0) < 0)
        return false;
    bool result = false;
    if (BCryptSetProperty(algorithm, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_GCM)),
                          sizeof(BCRYPT_CHAIN_MODE_GCM), 0) >= 0
        && BCryptGenerateSymmetricKey(algorithm, &keyHandle, nullptr, 0,
                                      reinterpret_cast<PUCHAR>(const_cast<char *>(key.constData())),
                                      static_cast<ULONG>(key.size()), 0) >= 0) {
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
        BCRYPT_INIT_AUTH_MODE_INFO(info);
        info.pbNonce = reinterpret_cast<PUCHAR>(const_cast<char *>(nonce.constData()));
        info.cbNonce = static_cast<ULONG>(nonce.size());
        info.pbTag = reinterpret_cast<PUCHAR>(const_cast<char *>(tag.constData()));
        info.cbTag = static_cast<ULONG>(tag.size());
        QByteArray output(ciphertext.size(), Qt::Uninitialized);
        ULONG written = 0;
        if (BCryptDecrypt(keyHandle, reinterpret_cast<PUCHAR>(const_cast<char *>(ciphertext.constData())),
                          static_cast<ULONG>(ciphertext.size()), &info, nullptr, 0,
                          reinterpret_cast<PUCHAR>(output.data()), static_cast<ULONG>(output.size()), &written, 0) >= 0) {
            output.resize(static_cast<int>(written));
            *plain = output;
            result = true;
        }
    }
    if (keyHandle)
        BCryptDestroyKey(keyHandle);
    if (algorithm)
        BCryptCloseAlgorithmProvider(algorithm, 0);
    return result;
}

static bool aesGcmEncrypt(const QByteArray &key, const QByteArray &nonce, const QByteArray &plain,
                          QByteArray *ciphertext, QByteArray *tag)
{
    if (key.size() != 32 || nonce.size() != 12)
        return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_KEY_HANDLE keyHandle = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0) < 0)
        return false;
    bool result = false;
    if (BCryptSetProperty(algorithm, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_GCM)),
                          sizeof(BCRYPT_CHAIN_MODE_GCM), 0) >= 0
        && BCryptGenerateSymmetricKey(algorithm, &keyHandle, nullptr, 0,
                                      reinterpret_cast<PUCHAR>(const_cast<char *>(key.constData())),
                                      static_cast<ULONG>(key.size()), 0) >= 0) {
        QByteArray tagBuffer(16, Qt::Uninitialized);
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
        BCRYPT_INIT_AUTH_MODE_INFO(info);
        info.pbNonce = reinterpret_cast<PUCHAR>(const_cast<char *>(nonce.constData()));
        info.cbNonce = static_cast<ULONG>(nonce.size());
        info.pbTag = reinterpret_cast<PUCHAR>(tagBuffer.data());
        info.cbTag = static_cast<ULONG>(tagBuffer.size());
        QByteArray output(plain.size(), Qt::Uninitialized);
        ULONG written = 0;
        if (BCryptEncrypt(keyHandle, reinterpret_cast<PUCHAR>(const_cast<char *>(plain.constData())),
                          static_cast<ULONG>(plain.size()), &info, nullptr, 0,
                          reinterpret_cast<PUCHAR>(output.data()), static_cast<ULONG>(output.size()), &written, 0) >= 0) {
            output.resize(static_cast<int>(written));
            *ciphertext = output;
            *tag = tagBuffer;
            result = true;
        }
    }
    if (keyHandle)
        BCryptDestroyKey(keyHandle);
    if (algorithm)
        BCryptCloseAlgorithmProvider(algorithm, 0);
    return result;
}

/**
 * Ключ Chromium/Electron OSCrypt: Local State → os_crypt.encrypted_key
 * (base64, префикс DPAPI) → DPAPI → 32-байтный AES-256-GCM ключ.
 * Нужен для чтения токенов Electron 2.8 (формат "v10") и совместимой записи.
 */
static QByteArray chromiumKey()
{
    static QByteArray key;
    if (!key.isEmpty())
        return key;

    QStringList candidates;
    candidates << QDir(Paths::dataDir()).filePath(QStringLiteral("Local State"));
    for (const auto &legacy : Paths::legacyDataDirs())
        candidates << QDir(legacy).filePath(QStringLiteral("Local State"));

    for (const auto &path : candidates) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
        const QString encoded = object.value(QStringLiteral("os_crypt")).toObject()
                                    .value(QStringLiteral("encrypted_key")).toString();
        if (encoded.isEmpty())
            continue;
        QByteArray encrypted = QByteArray::fromBase64(encoded.toLatin1());
        if (encrypted.startsWith("DPAPI"))
            encrypted = encrypted.mid(5);
        bool ok = false;
        const QByteArray decrypted = unprotect(encrypted, &ok);
        if (ok && (decrypted.size() == 32 || decrypted.size() == 16 || decrypted.size() == 24)) {
            key = decrypted;
            Logger::info(QStringLiteral("Crypto: найден ключ Electron OSCrypt — токены расшифровываются"));
            break;
        }
    }
    return key;
}

static bool decryptChromiumV10(const QByteArray &raw, QByteArray *plain)
{
    if (raw.size() < 3 + 12 + 16)
        return false;
    if (!(raw.startsWith("v10") || raw.startsWith("v11")))
        return false;
    const QByteArray key = chromiumKey();
    if (key.isEmpty())
        return false;
    const QByteArray ciphertext = raw.mid(15, raw.size() - 15 - 16);
    return aesGcmDecrypt(key, raw.mid(3, 12), ciphertext, raw.right(16), plain);
}
#endif

QString encryptSecret(const QString &plain)
{
    if (plain.isEmpty())
        return QString();
#ifdef Q_OS_WIN
    // Если рядом есть Electron Local State — пишем в совместимом формате v10,
    // чтобы откат на старую версию не терял аккаунты.
    const QByteArray key = chromiumKey();
    if (!key.isEmpty()) {
        QByteArray nonce(12, Qt::Uninitialized);
        if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(nonce.data()), static_cast<ULONG>(nonce.size()),
                            BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0) {
            QByteArray cipher;
            QByteArray tag;
            if (aesGcmEncrypt(key, nonce, plain.toUtf8(), &cipher, &tag))
                return QStringLiteral("enc:") + QString::fromLatin1((QByteArray("v10") + nonce + cipher + tag).toBase64());
        }
    }
    bool ok = false;
    const QByteArray cipher = protect(plain.toUtf8(), &ok);
    if (ok)
        return QStringLiteral("enc:") + QString::fromLatin1(cipher.toBase64());
#endif
    return QStringLiteral("b64:") + QString::fromLatin1(plain.toUtf8().toBase64());
}

SecretReadResult tryDecryptSecret(const QString &stored)
{
    if (stored.isEmpty())
        return {QString(), true};

    if (stored.startsWith(QLatin1String("enc:"))) {
#ifdef Q_OS_WIN
        const QByteArray raw = QByteArray::fromBase64(stored.mid(4).toLatin1());
        QByteArray chromiumPlain;
        if (decryptChromiumV10(raw, &chromiumPlain))
            return {QString::fromUtf8(chromiumPlain), true};
        bool ok = false;
        const QByteArray plain = unprotect(raw, &ok);
        if (ok)
            return {QString::fromUtf8(plain), true};
        return {QString(), false};
#else
        return {QString(), false};
#endif
    }

    if (stored.startsWith(QLatin1String("b64:")))
        return {QString::fromUtf8(QByteArray::fromBase64(stored.mid(4).toLatin1())), true};

    // Совместимость со старыми записями без префикса.
#ifdef Q_OS_WIN
    const QByteArray raw = QByteArray::fromBase64(stored.toLatin1());
    QByteArray chromiumPlain;
    if (decryptChromiumV10(raw, &chromiumPlain))
        return {QString::fromUtf8(chromiumPlain), true};
    bool ok = false;
    const QByteArray plain = unprotect(raw, &ok);
    if (ok)
        return {QString::fromUtf8(plain), true};
    return {QString(), false};
#else
    return {QString(), false};
#endif
}

} // namespace Noble
