#include "ActivityLogModel.h"

#include <QDateTime>
#include <QUuid>

ActivityLogModel::ActivityLogModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ActivityLogModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.size();
}

QVariant ActivityLogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};
    const ActivityLog &log = m_items.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return QStringLiteral("[%1] %2").arg(log.timestamp, log.message);
    case TimestampRole:
        return log.timestamp;
    case TypeRole:
        return log.type;
    case MessageRole:
        return log.message;
    default:
        return {};
    }
}

QHash<int, QByteArray> ActivityLogModel::roleNames() const
{
    return {{TimestampRole, "timestamp"}, {TypeRole, "type"}, {MessageRole, "message"}};
}

void ActivityLogModel::prepend(const QString &type, const QString &message)
{
    beginInsertRows({}, 0, 0);
    ActivityLog log;
    log.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    log.timestamp = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
    log.type = type;
    log.message = message;
    m_items.prepend(log);
    endInsertRows();
}

void ActivityLogModel::clear()
{
    beginResetModel();
    m_items.clear();
    endResetModel();
}
