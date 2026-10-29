#include "FontsService.h"

#include "Logger.h"
#include "Paths.h"
#include "ProcessUtil.h"
#include "Pure.h"
#include "Settings.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonObject>
#include <QUrl>

namespace Noble {

FontsService::FontsService(QObject *parent)
    : QObject(parent)
{
}

void FontsService::init(Settings *settings)
{
    m_settings = settings;
    m_items = settings->customFonts();
    const int pixelId = QFontDatabase::addApplicationFont(QStringLiteral(":/assets/fonts/PixelifySans.ttf"));
    if (pixelId >= 0) {
        const QStringList families = QFontDatabase::applicationFontFamilies(pixelId);
        if (!families.isEmpty())
            m_pixelFamily = families.first();
    }
    if (m_pixelFamily.isEmpty())
        m_pixelFamily = QStringLiteral("Segoe UI");
    apply();
    emit changed();
}

QString FontsService::loadFontFile(const QString &path)
{
    if (m_loadedFamilies.contains(path))
        return m_loadedFamilies.value(path);
    const int id = QFontDatabase::addApplicationFont(path);
    if (id < 0)
        return QString();
    const QStringList families = QFontDatabase::applicationFontFamilies(id);
    if (families.isEmpty())
        return QString();
    m_loadedFamilies.insert(path, families.first());
    return families.first();
}

QString FontsService::familyForId(const QString &id)
{
    for (const auto &value : m_items) {
        const QJsonObject font = value.toObject();
        if (font.value(QStringLiteral("id")).toString() == id) {
            const QString file = QDir(Paths::fontsDir()).filePath(font.value(QStringLiteral("fileName")).toString());
            return loadFontFile(file);
        }
    }
    return QString();
}

void FontsService::apply()
{
    if (!m_settings)
        return;
    QString family;
    const QString mode = m_settings->fontMode();
    if (mode == QLatin1String("system")) {
        family = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
    } else if (mode == QLatin1String("custom")) {
        family = familyForId(m_settings->activeFontId());
        if (family.isEmpty()) {
            family = m_pixelFamily;
            emit toastRequested(QStringLiteral("Не удалось загрузить шрифт — применён стандартный"), QStringLiteral("warn"));
        }
    } else {
        family = m_pixelFamily;
    }
    if (family == m_family)
        return;
    m_family = family;
    QFont font = QGuiApplication::font();
    font.setFamily(m_family);
    QGuiApplication::setFont(font);
    emit familyChanged();
}

QVariantList FontsService::items() const
{
    QVariantList list;
    for (const auto &value : m_items) {
        const QJsonObject font = value.toObject();
        QVariantMap map = font.toVariantMap();
        map.insert(QStringLiteral("active"), font.value(QStringLiteral("id")).toString()
                                                  == (m_settings ? m_settings->activeFontId() : QString()));
        list.append(map);
    }
    return list;
}

void FontsService::importFiles(const QStringList &paths)
{
    if (!m_settings)
        return;
    QDir().mkpath(Paths::fontsDir());
    QJsonArray list = m_items;
    QStringList skipped;
    int processed = 0;
    for (const auto &path : paths) {
        if (++processed > 20)
            break;
        QFileInfo info(path);
        if (!info.isFile() || info.size() <= 0 || info.size() > 30 * 1024 * 1024) {
            skipped << info.fileName();
            continue;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            skipped << info.fileName();
            continue;
        }
        const QByteArray content = file.readAll();
        const QByteArray tag = content.left(4);
        QString ext;
        QString kind;
        if (content.size() >= 4 && static_cast<unsigned char>(content[0]) == 0 && static_cast<unsigned char>(content[1]) == 1
            && static_cast<unsigned char>(content[2]) == 0 && static_cast<unsigned char>(content[3]) == 0) {
            ext = QStringLiteral(".ttf");
            kind = QStringLiteral("TrueType");
        } else if (tag == "true" || tag == "typ1") {
            ext = QStringLiteral(".ttf");
            kind = QStringLiteral("TrueType");
        } else if (tag == "ttcf") {
            ext = QStringLiteral(".ttc");
            kind = QStringLiteral("TrueType Collection");
        } else if (tag == "OTTO") {
            ext = QStringLiteral(".otf");
            kind = QStringLiteral("OpenType");
        } else if (tag == "wOFF") {
            ext = QStringLiteral(".woff");
            kind = QStringLiteral("WOFF");
        } else if (tag == "wOF2") {
            ext = QStringLiteral(".woff2");
            kind = QStringLiteral("WOFF2");
        } else {
            skipped << info.fileName();
            continue;
        }
        const QString id = sha1Bytes(content).left(16);
        bool exists = false;
        for (const auto &value : list) {
            if (value.toObject().value(QStringLiteral("id")).toString() == id) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            const QString fileName = id + ext;
            QFile out(QDir(Paths::fontsDir()).filePath(fileName));
            if (!out.open(QIODevice::WriteOnly)) {
                skipped << info.fileName();
                continue;
            }
            out.write(content);
            out.close();
            list.append(QJsonObject{{QStringLiteral("id"), id},
                                    {QStringLiteral("name"), info.completeBaseName().left(60)},
                                    {QStringLiteral("fileName"), fileName},
                                    {QStringLiteral("format"), kind},
                                    {QStringLiteral("size"), static_cast<double>(content.size())},
                                    {QStringLiteral("addedAt"), static_cast<double>(QDateTime::currentMSecsSinceEpoch())}});
            Logger::info(QStringLiteral("Шрифт добавлен: %1").arg(fileName));
        }
    }
    m_items = list;
    m_settings->setCustomFonts(m_items);
    apply();
    emit changed();
    if (!skipped.isEmpty())
        emit toastRequested(QStringLiteral("Пропущены файлы: %1").arg(skipped.join(QStringLiteral(", "))), QStringLiteral("warn"));
}

void FontsService::remove(const QString &id)
{
    if (!m_settings)
        return;
    QJsonArray list;
    for (const auto &value : m_items) {
        const QJsonObject font = value.toObject();
        if (font.value(QStringLiteral("id")).toString() == id) {
            QFile::remove(QDir(Paths::fontsDir()).filePath(font.value(QStringLiteral("fileName")).toString()));
            continue;
        }
        list.append(font);
    }
    m_items = list;
    m_settings->setCustomFonts(m_items);
    if (m_settings->activeFontId() == id) {
        m_settings->setActiveFontId(QString());
        m_settings->setFontMode(QStringLiteral("minecraft"));
    }
    apply();
    emit changed();
}

void FontsService::openFolder()
{
    QDir().mkpath(Paths::fontsDir());
    openWithDefaultApp(Paths::fontsDir());
}

} // namespace Noble
