#include "ConsoleModel.h"

namespace Noble {

ConsoleModel::ConsoleModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ConsoleModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_lines.size();
}

QVariant ConsoleModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_lines.size())
        return {};
    const Line &line = m_lines.at(index.row());
    switch (role) {
    case TypeRole:
        return line.type;
    case TextRole:
        return line.text;
    case TimeRole:
        return line.time;
    case TimeTextRole:
        return QDateTime::fromMSecsSinceEpoch(line.time).toString(QStringLiteral("HH:mm:ss"));
    default:
        return {};
    }
}

QHash<int, QByteArray> ConsoleModel::roleNames() const
{
    return {{TypeRole, "type"}, {TextRole, "text"}, {TimeRole, "time"}, {TimeTextRole, "timeText"}};
}

void ConsoleModel::append(const QString &type, const QString &text)
{
    if (text.isEmpty())
        return;
    const int overflow = m_lines.size() - kMaxLines + 1;
    if (overflow > 0) {
        beginRemoveRows(QModelIndex(), 0, overflow - 1);
        m_lines.remove(0, overflow);
        endRemoveRows();
    }
    beginInsertRows(QModelIndex(), m_lines.size(), m_lines.size());
    m_lines.append({type, text, QDateTime::currentMSecsSinceEpoch()});
    endInsertRows();
    emit countChanged();
}

void ConsoleModel::clear()
{
    if (m_lines.isEmpty())
        return;
    beginResetModel();
    m_lines.clear();
    endResetModel();
    emit countChanged();
}

QString ConsoleModel::toText() const
{
    QStringList lines;
    for (const auto &line : m_lines)
        lines << QStringLiteral("[%1] [%2] %3")
                     .arg(QDateTime::fromMSecsSinceEpoch(line.time).toString(QStringLiteral("HH:mm:ss")),
                          line.type.toUpper(), line.text);
    return lines.join(QLatin1Char('\n'));
}

} // namespace Noble
