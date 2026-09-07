#pragma once

#include <QAbstractListModel>
#include <QVector>

struct ActivityLog {
    QString id;
    QString timestamp;
    QString type; // start stop upload download nic_add nic_del check
    QString message;
};

class ActivityLogModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles { TimestampRole = Qt::UserRole + 1, TypeRole, MessageRole };

    explicit ActivityLogModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void prepend(const QString &type, const QString &message);
    void clear();
    const QVector<ActivityLog> &items() const { return m_items; }

private:
    QVector<ActivityLog> m_items;
};
