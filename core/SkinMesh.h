#pragma once

#include <QQuick3DGeometry>
#include <QString>

namespace Noble {

class SkinMesh : public QQuick3DGeometry
{
    Q_OBJECT
    Q_PROPERTY(QString part READ part WRITE setPart NOTIFY partChanged)
    Q_PROPERTY(bool slim READ slim WRITE setSlim NOTIFY slimChanged)

public:
    explicit SkinMesh(QQuick3DObject *parent = nullptr);

    QString part() const { return m_part; }
    void setPart(const QString &part);
    bool slim() const { return m_slim; }
    void setSlim(bool slim);

signals:
    void partChanged();
    void slimChanged();

private:
    void rebuild();

    QString m_part = QStringLiteral("head");
    bool m_slim = false;
};

} // namespace Noble
