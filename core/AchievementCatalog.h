#pragma once

#include <QString>
#include <QVector>

namespace Noble {

struct AchievementDef {
    QString id;
    QString titleRu;
    QString titleEn;
    QString descRu;
    QString descEn;
    int points = 0;
    QString stat;
    qint64 threshold = 0;
};

const QVector<AchievementDef> &achievements();

QString statKeyRu(const QString &key);
QString statKeyEn(const QString &key);

} // namespace Noble
