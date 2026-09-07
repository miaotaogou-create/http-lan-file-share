#pragma once

#include <QString>
#include <QStringList>
#include <QList>

struct NicEntry {
    QString id;
    QString name;
    QString ip;
    QString subnet;
    bool isPrimary = false;
    bool isEthernet = false;
};

class NicManager
{
public:
    static QList<NicEntry> enumerate();
    static bool isElevated();
    static bool addAddress(const QString &adapterName, const QString &ip, const QString &mask, QString *err = nullptr);
    static bool removeAddress(const QString &ip, QString *err = nullptr);
    static QString preferLanIp(const QList<NicEntry> &nics);
};
