#include "Pure.h"

#include <QCryptographicHash>
#include <QFile>
#include <QHash>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QUrl>

#include <algorithm>

namespace Noble {

QString offlineUuid(const QString &name)
{
    QByteArray hash = QCryptographicHash::hash((QStringLiteral("OfflinePlayer:") + name).toUtf8(), QCryptographicHash::Md5);
    hash[6] = static_cast<char>((hash[6] & 0x0f) | 0x30);
    hash[8] = static_cast<char>((hash[8] & 0x3f) | 0x80);
    const QString hex = QString::fromLatin1(hash.toHex());
    return hex.left(8) + QLatin1Char('-') + hex.mid(8, 4) + QLatin1Char('-') + hex.mid(12, 4)
        + QLatin1Char('-') + hex.mid(16, 4) + QLatin1Char('-') + hex.mid(20);
}

int requiredJavaMajor(const QString &mcVersion)
{
    static const QRegularExpression re(QStringLiteral("^(\\d+)\\.(\\d+)(?:\\.(\\d+))?"));
    const auto match = re.match(mcVersion.trimmed());
    if (!match.hasMatch())
        return 17;
    const int major = match.captured(1).toInt();
    const int minor = match.captured(2).toInt();
    const int patch = match.captured(3).isEmpty() ? 0 : match.captured(3).toInt();
    if (major > 1)
        return 21;
    if (minor <= 16)
        return 8;
    if (minor == 17)
        return 17;
    if (minor <= 20 && patch <= 4)
        return 17;
    return 21;
}

QString runtimeComponent(int major)
{
    if (major <= 8)
        return QStringLiteral("jre-legacy");
    if (major <= 16)
        return QStringLiteral("java-runtime-alpha");
    if (major <= 17)
        return QStringLiteral("java-runtime-gamma");
    return QStringLiteral("java-runtime-delta");
}

QString platformKey()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows-x64");
#elif defined(Q_OS_MACOS)
    return sizeof(void *) == 8 ? QStringLiteral("mac-os-arm64") : QStringLiteral("mac-os");
#else
    return QStringLiteral("linux");
#endif
}

QString neoforgeGameVersion(const QString &version)
{
    const QStringList parts = version.split(QLatin1Char('.'));
    if (parts.size() < 2)
        return version;
    const int major = parts[0].toInt();
    if (major >= 21)
        return QStringLiteral("1.%1.%2").arg(parts[0], parts[1]);
    if (parts[1] == QLatin1String("0"))
        return QStringLiteral("1.%1").arg(parts[0]);
    return QStringLiteral("1.%1.%2").arg(parts[0], parts[1]);
}

QString loaderVersionId(const QString &loader, const QString &mcVersion, const QString &loaderVersion)
{
    if (loader == QLatin1String("Fabric"))
        return QStringLiteral("fabric-loader-%1-%2").arg(loaderVersion, mcVersion);
    if (loader == QLatin1String("Quilt"))
        return QStringLiteral("quilt-loader-%1-%2").arg(loaderVersion, mcVersion);
    if (loader == QLatin1String("Forge"))
        return QStringLiteral("%1-forge-%2").arg(mcVersion, loaderVersion);
    if (loader == QLatin1String("NeoForge"))
        return QStringLiteral("neoforge-%1").arg(loaderVersion);
    return mcVersion;
}

QStringList splitArgs(const QString &raw)
{
    QStringList args;
    static const QRegularExpression re(QStringLiteral("\"([^\"]*)\"|'([^']*)'|(\\S+)"));
    auto it = re.globalMatch(raw);
    while (it.hasNext()) {
        const auto m = it.next();
        if (!m.captured(1).isNull())
            args << m.captured(1);
        else if (!m.captured(2).isNull())
            args << m.captured(2);
        else
            args << m.captured(3);
    }
    return args;
}

QString slugifyLatin(const QString &name)
{
    static const QHash<QChar, QString> translit = {
        {QChar(u'а'), QStringLiteral("a")}, {QChar(u'б'), QStringLiteral("b")}, {QChar(u'в'), QStringLiteral("v")},
        {QChar(u'г'), QStringLiteral("g")}, {QChar(u'д'), QStringLiteral("d")}, {QChar(u'е'), QStringLiteral("e")},
        {QChar(u'ё'), QStringLiteral("e")}, {QChar(u'ж'), QStringLiteral("zh")}, {QChar(u'з'), QStringLiteral("z")},
        {QChar(u'и'), QStringLiteral("i")}, {QChar(u'й'), QStringLiteral("y")}, {QChar(u'к'), QStringLiteral("k")},
        {QChar(u'л'), QStringLiteral("l")}, {QChar(u'м'), QStringLiteral("m")}, {QChar(u'н'), QStringLiteral("n")},
        {QChar(u'о'), QStringLiteral("o")}, {QChar(u'п'), QStringLiteral("p")}, {QChar(u'р'), QStringLiteral("r")},
        {QChar(u'с'), QStringLiteral("s")}, {QChar(u'т'), QStringLiteral("t")}, {QChar(u'у'), QStringLiteral("u")},
        {QChar(u'ф'), QStringLiteral("f")}, {QChar(u'х'), QStringLiteral("h")}, {QChar(u'ц'), QStringLiteral("ts")},
        {QChar(u'ч'), QStringLiteral("ch")}, {QChar(u'ш'), QStringLiteral("sh")}, {QChar(u'щ'), QStringLiteral("sch")},
        {QChar(u'ъ'), QString()}, {QChar(u'ы'), QStringLiteral("y")}, {QChar(u'ь'), QString()},
        {QChar(u'э'), QStringLiteral("e")}, {QChar(u'ю'), QStringLiteral("yu")}, {QChar(u'я'), QStringLiteral("ya")},
        {QChar(u'і'), QStringLiteral("i")}, {QChar(u'ї'), QStringLiteral("yi")}, {QChar(u'є'), QStringLiteral("ye")},
        {QChar(u'ґ'), QStringLiteral("g")}, {QChar(u'ў'), QStringLiteral("u")},
    };
    QString transliterated;
    const QString source = name.trimmed();
    for (const QChar ch : source.toLower()) {
        auto it = translit.constFind(ch);
        transliterated += it != translit.constEnd() ? it.value() : QString(ch);
    }
    static const QRegularExpression junk(QStringLiteral("[^a-z0-9]+"));
    QString clean = transliterated.toLower();
    clean.replace(junk, QStringLiteral("-"));
    while (clean.startsWith(QLatin1Char('-')))
        clean.remove(0, 1);
    while (clean.endsWith(QLatin1Char('-')))
        clean.chop(1);
    clean = clean.left(40);
    while (clean.endsWith(QLatin1Char('-')))
        clean.chop(1);
    if (!clean.isEmpty())
        return clean;
    return QStringLiteral("build-") + md5Hex((source.isEmpty() ? QStringLiteral("instance") : source).toUtf8()).left(8);
}

int parseJavaMajor(const QString &versionOutput)
{
    static const QRegularExpression quoted(QStringLiteral("version\\s+\"([0-9][^\"]*)\""), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression loose(QStringLiteral("version\\s+\"?([0-9][^\\s\"]*)\"?"), QRegularExpression::CaseInsensitiveOption);
    QString version;
    auto m = quoted.match(versionOutput);
    if (m.hasMatch())
        version = m.captured(1);
    if (version.isEmpty()) {
        m = loose.match(versionOutput);
        if (m.hasMatch())
            version = m.captured(1);
    }
    if (version.isEmpty()) {
        const QStringList parts = versionOutput.trimmed().split(QRegularExpression(QStringLiteral("\\s+")));
        for (const auto &part : parts) {
            if (!part.isEmpty() && part.at(0).isDigit()) {
                version = part;
                break;
            }
        }
    }
    if (version.isEmpty())
        return -1;
    const QStringList pieces = version.split(QRegularExpression(QStringLiteral("[._-]")));
    bool ok = false;
    const int first = pieces.value(0).toInt(&ok);
    if (!ok)
        return -1;
    if (first == 1) {
        if (pieces.size() < 2)
            return -1;
        const int second = pieces.at(1).toInt(&ok);
        return ok ? second : -1;
    }
    return first;
}

QList<int> preferredJavaMajors(const QString &mcVersion, const QString &loader)
{
    const int base = requiredJavaMajor(mcVersion);
    if ((loader == QLatin1String("Forge") || loader == QLatin1String("NeoForge")) && base == 8)
        return {8};
    if (base == 8)
        return {8, 17};
    if (base == 17)
        return {17, 21};
    return {21};
}

FilteredArgs filterUnsafeMemoryFlag(const QStringList &args, int javaMajor)
{
    const QString flag = QStringLiteral("--sun-misc-unsafe-memory-access");
    if (javaMajor >= 24)
        return {args, false};
    QStringList kept;
    for (const auto &arg : args) {
        if (!arg.startsWith(flag))
            kept << arg;
    }
    return {kept, kept.size() != args.size()};
}

FilteredArgs filterIncompatibleJvmFlags(const QStringList &args, int javaMajor, bool is64Bit)
{
    QStringList kept;
    bool removed = false;
    for (int i = 0; i < args.size(); ++i) {
        const QString arg = args.at(i).trimmed();
        if (arg.isEmpty())
            continue;
        const QString lower = arg.toLower();
        auto drop = [&] { removed = true; };
        if (lower.startsWith(QLatin1String("-xmx")) || lower.startsWith(QLatin1String("-xms"))) {
            drop();
            continue;
        }
        if (lower == QLatin1String("-cp") || lower == QLatin1String("-classpath")) {
            drop();
            ++i;
            continue;
        }
        if (lower.startsWith(QLatin1String("--sun-misc-unsafe-memory-access")) && javaMajor < 24) {
            drop();
            continue;
        }
        if (javaMajor < 9
            && (lower.startsWith(QLatin1String("--add-opens"))
                || lower.startsWith(QLatin1String("--add-exports"))
                || lower.startsWith(QLatin1String("--add-modules"))
                || lower.startsWith(QLatin1String("--illegal-access"))
                || lower.startsWith(QLatin1String("--enable-native-access")))) {
            drop();
            continue;
        }
        if (javaMajor < 21 && lower.startsWith(QLatin1String("--enable-native-access"))) {
            drop();
            continue;
        }
        if (lower.startsWith(QLatin1String("-xx:+usecms"))
            || lower.startsWith(QLatin1String("-xx:+useparnewgc"))
            || lower.startsWith(QLatin1String("-xx:+useconcmarksweepgc"))) {
            if (javaMajor < 0 || javaMajor >= 14) {
                drop();
                continue;
            }
        }
        if (lower.startsWith(QLatin1String("-xx:+usezgc"))
            || lower.startsWith(QLatin1String("-xx:+zgenerational"))) {
            if (!is64Bit || javaMajor < 15
                || (lower.contains(QLatin1String("zgenerational")) && javaMajor < 21)) {
                drop();
                continue;
            }
        }
        if (lower.startsWith(QLatin1String("-xx:+useshenandoah"))) {
            if (!is64Bit || javaMajor < 12) {
                drop();
                continue;
            }
        }
        if (lower.startsWith(QLatin1String("-xx:+usepsilon"))) {
            if (javaMajor < 11) {
                drop();
                continue;
            }
        }
        if (lower.startsWith(QLatin1String("-xx:+usejvmcicompiler"))) {
            if (javaMajor < 9) {
                drop();
                continue;
            }
        }
        kept << arg;
    }
    return {kept, removed};
}

HeapPlan planHeap(int requestedMb, qint64 totalMb, qint64 availableMb, bool is64Bit)
{
    HeapPlan plan;
    plan.heapMb = std::clamp(requestedMb, 512, 32768);
    if (plan.heapMb != requestedMb)
        plan.clamped = true;
    if (totalMb > 0) {
        const qint64 byTotal = std::max<qint64>(1024, totalMb * 3 / 4);
        const qint64 byAvailable = availableMb > 0
            ? std::max<qint64>(1024, availableMb - 1024)
            : byTotal;
        qint64 limit = std::min(byTotal, byAvailable);
        if (!is64Bit)
            limit = std::min<qint64>(limit, 1536);
        if (plan.heapMb > limit) {
            plan.heapMb = static_cast<int>(std::max<qint64>(512, limit));
            plan.clamped = true;
            plan.reason = !is64Bit
                ? QStringLiteral("32-битная Java не может выделить больше %1 МБ — значение уменьшено").arg(plan.heapMb)
                : QStringLiteral("Недостаточно свободной RAM — память уменьшена до %1 МБ").arg(plan.heapMb);
        }
    } else if (!is64Bit && plan.heapMb > 1536) {
        plan.heapMb = 1536;
        plan.clamped = true;
        plan.reason = QStringLiteral("32-битная Java не может выделить больше 1536 МБ — значение уменьшено");
    }
    plan.initialMb = std::max(256, std::min(512, plan.heapMb / 2));
    return plan;
}

static QList<int> versionParts(const QString &v)
{
    QList<int> parts;
    for (const auto &p : v.split(QRegularExpression(QStringLiteral("[.\\-+]")))) {
        bool ok = false;
        const int n = p.toInt(&ok);
        parts << (ok ? n : 0);
    }
    return parts;
}

int compareVersions(const QString &a, const QString &b)
{
    const auto pa = versionParts(a);
    const auto pb = versionParts(b);
    const int count = std::max(pa.size(), pb.size());
    for (int i = 0; i < count; ++i) {
        const int diff = pa.value(i) - pb.value(i);
        if (diff != 0)
            return diff > 0 ? 1 : -1;
    }
    return 0;
}

static bool matchesSingleRange(const QString &raw, const QString &version)
{
    if (raw.trimmed().isEmpty() || raw.trimmed() == QLatin1String("*"))
        return true;
    const auto parts = raw.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    static const QRegularExpression re(QStringLiteral("^(>=|<=|>|<|\\^|~|=)?(.+)$"));
    for (const auto &part : parts) {
        const auto m = re.match(part);
        if (!m.hasMatch())
            continue;
        const QString op = m.captured(1);
        QString target = m.captured(2);
        if (target.endsWith(QLatin1String(".x"), Qt::CaseInsensitive))
            target.chop(2);
        const int cmp = compareVersions(version, target);
        if (op == QLatin1String(">=") && cmp < 0) return false;
        if (op == QLatin1String("<=") && cmp > 0) return false;
        if (op == QLatin1String(">") && cmp <= 0) return false;
        if (op == QLatin1String("<") && cmp >= 0) return false;
        if ((op.isEmpty() || op == QLatin1String("=")) && !version.startsWith(target)) return false;
        if (op == QLatin1String("^")) {
            const auto pv = versionParts(version);
            const auto pt = versionParts(target);
            if (cmp < 0 || pv.value(1) != pt.value(1)) return false;
        }
        if (op == QLatin1String("~")) {
            if (cmp < 0 || compareVersions(version, target + QStringLiteral(".999")) > 0) return false;
        }
    }
    return true;
}

bool mcVersionMatches(const QStringList &ranges, const QString &version)
{
    if (ranges.isEmpty())
        return true;
    for (const auto &r : ranges) {
        if (matchesSingleRange(r, version))
            return true;
    }
    return false;
}

LevelInfo levelForPoints(int points)
{
    static const QList<int> steps = {20, 70, 95, 200};
    static const QList<QPair<QString, QString>> titles = {
        {QStringLiteral("Новичок"), QStringLiteral("Novice")},
        {QStringLiteral("Любитель"), QStringLiteral("Amateur")},
        {QStringLiteral("Опытный"), QStringLiteral("Experienced")},
        {QStringLiteral("Мастер"), QStringLiteral("Master")},
        {QStringLiteral("Легенда"), QStringLiteral("Legend")},
        {QStringLiteral("Легенда"), QStringLiteral("Legend")},
    };
    int level = 1;
    int required = 0;
    for (int step : steps) {
        required += step;
        if (points >= required)
            ++level;
        else
            break;
    }
    LevelInfo info;
    info.level = level;
    const int idx = std::min(level - 1, static_cast<int>(titles.size()) - 1);
    info.titleRu = titles[idx].first;
    info.titleEn = titles[idx].second;
    if (level <= steps.size()) {
        int nextAt = 0;
        for (int i = 0; i < level; ++i)
            nextAt += steps[i];
        info.nextAt = nextAt;
        info.toNext = std::max(0, nextAt - points);
    } else {
        info.nextAt = -1;
        info.toNext = -1;
    }
    return info;
}

static QString hashFile(const QString &path, QCryptographicHash::Algorithm algo, qint64 *size)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (size)
            *size = 0;
        return QString();
    }
    if (size)
        *size = file.size();
    QCryptographicHash hash(algo);
    if (!hash.addData(&file))
        return QString();
    return QString::fromLatin1(hash.result().toHex());
}

QString sha1File(const QString &path, qint64 *size) { return hashFile(path, QCryptographicHash::Sha1, size); }
QString sha512File(const QString &path, qint64 *size) { return hashFile(path, QCryptographicHash::Sha512, size); }
QString sha256File(const QString &path, qint64 *size) { return hashFile(path, QCryptographicHash::Sha256, size); }
QString sha1Bytes(const QByteArray &data) { return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex()); }
QString sha256Bytes(const QByteArray &data) { return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()); }
QString md5Hex(const QByteArray &data) { return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex()); }

QString humanBytes(qint64 bytes)
{
    static const QStringList units = {QStringLiteral("Б"), QStringLiteral("КБ"), QStringLiteral("МБ"),
                                      QStringLiteral("ГБ"), QStringLiteral("ТБ")};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    return QString::number(value, 'f', unit == 0 ? 0 : 1) + QLatin1Char(' ') + units.at(unit);
}

QString humanSpeed(qint64 bps)
{
    if (bps <= 0)
        return QStringLiteral("—");
    return humanBytes(bps) + QStringLiteral("/с");
}

QString humanDuration(qint64 seconds)
{
    if (seconds <= 0)
        return QStringLiteral("0 мин");
    const qint64 hours = seconds / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    if (hours > 0)
        return QStringLiteral("%1 ч %2 мин").arg(hours).arg(minutes);
    if (minutes > 0)
        return QStringLiteral("%1 мин").arg(minutes);
    return QStringLiteral("%1 сек").arg(seconds);
}

QString humanTimeShort(qint64 seconds)
{
    const qint64 hours = seconds / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    if (hours > 0)
        return QStringLiteral("%1:%2").arg(hours).arg(minutes, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1 мин").arg(minutes);
}

QString downloadCount(qint64 count)
{
    if (count >= 1000000)
        return QString::number(count / 1000000.0, 'f', 1) + QStringLiteral("M");
    if (count >= 1000)
        return QString::number(count / 1000.0, 'f', 1) + QStringLiteral("K");
    return QString::number(count);
}

QString defaultUserAgent()
{
    return QStringLiteral("noble-launcher/2.9.0");
}

QString mavenPathFromName(const QString &name)
{
    QString base = name;
    QString ext = QStringLiteral("jar");
    const int at = base.indexOf(QLatin1Char('@'));
    if (at >= 0) {
        ext = base.mid(at + 1);
        base = base.left(at);
    }
    const auto parts = base.split(QLatin1Char(':'));
    if (parts.size() < 3)
        return QString();
    const QString group = parts[0];
    const QString artifact = parts[1];
    const QString version = parts[2];
    const QString classifier = parts.size() > 3 ? parts[3] : QString();
    QString path = group;
    path.replace(QLatin1Char('.'), QLatin1Char('/'));
    path += QLatin1Char('/') + artifact + QLatin1Char('/') + version + QLatin1Char('/') + artifact + QLatin1Char('-') + version;
    if (!classifier.isEmpty())
        path += QLatin1Char('-') + classifier;
    path += QLatin1Char('.') + ext;
    return path;
}

QString safeFileName(const QString &name)
{
    QString result = name;
    result.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    return result.trimmed();
}

QString randomHex(int chars)
{
    QString result;
    static const QString hex = QStringLiteral("0123456789abcdef");
    for (int i = 0; i < chars; ++i)
        result += hex.at(QRandomGenerator::global()->bounded(16));
    return result;
}

QString detectImageExt(const QByteArray &head)
{
    if (head.size() >= 8 && static_cast<unsigned char>(head[0]) == 0x89 && head[1] == 'P' && head[2] == 'N' && head[3] == 'G')
        return QStringLiteral(".png");
    if (head.size() >= 3 && static_cast<unsigned char>(head[0]) == 0xff && static_cast<unsigned char>(head[1]) == 0xd8
        && static_cast<unsigned char>(head[2]) == 0xff)
        return QStringLiteral(".jpg");
    if (head.size() >= 12 && head.mid(0, 4) == "RIFF" && head.mid(8, 4) == "WEBP")
        return QStringLiteral(".webp");
    return QString();
}

} // namespace Noble
