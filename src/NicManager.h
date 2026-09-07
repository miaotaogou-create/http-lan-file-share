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
    // 弹出 UAC，以管理员身份重启当前进程；成功发起返回 true
    static bool requestElevation(QString *err = nullptr);
    static bool addAddress(const QString &adapterName, const QString &ip, const QString &mask, QString *err = nullptr);
    static bool removeAddress(const QString &ip, QString *err = nullptr);
    static QString preferLanIp(const QList<NicEntry> &nics);
};
