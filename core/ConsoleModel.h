#pragma once

#include <QAbstractListModel>
#include <QVector>
#include <QDateTime>

namespace Noble {

class ConsoleModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        TypeRole = Qt::UserRole + 1,
        TextRole,
        TimeRole,
        TimeTextRole,
    };

    explicit ConsoleModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void append(const QString &type, const QString &text);
    Q_INVOKABLE void clear();
    Q_INVOKABLE QString toText() const;

signals:
    void countChanged();

private:
    struct Line {
        QString type;
        QString text;
        qint64 time = 0;
    };
    QVector<Line> m_lines;
    static const int kMaxLines = 5000;
};

} // namespace Noble
