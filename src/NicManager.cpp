#include "NicManager.h"

#include <QNetworkInterface>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

static QString prefixToMask(int prefix)
{
    if (prefix < 0 || prefix > 32)
        return QStringLiteral("255.255.255.0");
    quint32 m = prefix == 0 ? 0u : (0xFFFFFFFFu << (32 - prefix));
    return QStringLiteral("%1.%2.%3.%4")
        .arg((m >> 24) & 0xFF)
        .arg((m >> 16) & 0xFF)
        .arg((m >> 8) & 0xFF)
        .arg(m & 0xFF);
}

static int scoreIp(const QString &ip)
{
    const auto parts = ip.split('.');
    if (parts.size() != 4)
        return -100;
    const int a = parts[0].toInt();
    const int b = parts[1].toInt();
    if (ip.startsWith(QLatin1String("127.")) || ip.startsWith(QLatin1String("169.254.")))
        return -100;
    int score = 0;
    if (a == 192 && b == 168)
        score += 40;
    else if (a == 10)
        score += 45;
    else if (a == 172 && b >= 16 && b <= 31)
        score += 35;
    return score;
}

QList<NicEntry> NicManager::enumerate()
{
    QList<NicEntry> out;
    QSet<QString> seen;
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : ifaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp))
            continue;
        if (iface.flags() & QNetworkInterface::IsLoopBack)
            continue;
        const QString name = iface.humanReadableName();
        const bool eth = name.contains(QStringLiteral("以太网"), Qt::CaseInsensitive)
            || name.contains(QStringLiteral("Ethernet"), Qt::CaseInsensitive)
            || name.contains(QStringLiteral("eth"), Qt::CaseInsensitive);

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol)
                continue;
            const QString ip = entry.ip().toString();
            if (seen.contains(ip))
                continue;
            seen.insert(ip);
            NicEntry n;
            n.id = iface.name() + QLatin1Char('/') + ip;
            n.name = name.isEmpty() ? iface.name() : name;
            n.ip = ip;
            n.subnet = entry.netmask().toString();
            if (n.subnet.isEmpty() || n.subnet == QLatin1String("0.0.0.0"))
                n.subnet = prefixToMask(entry.prefixLength());
            n.isEthernet = eth;
            out.append(n);
        }
    }

    std::sort(out.begin(), out.end(), [](const NicEntry &a, const NicEntry &b) {
        return scoreIp(a.ip) > scoreIp(b.ip);
    });
    if (!out.isEmpty()) {
        out[0].isPrimary = true;
        if (!out[0].name.contains(QLatin1String("[Primary]")))
            out[0].name += QStringLiteral(" [Primary]");
    }
    return out;
}

bool NicManager::isElevated()
{
#ifdef Q_OS_WIN
    BOOL elev = FALSE;
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION te{};
        DWORD sz = sizeof(te);
        if (GetTokenInformation(token, TokenElevation, &te, sizeof(te), &sz))
            elev = te.TokenIsElevated;
        CloseHandle(token);
    }
    return elev == TRUE;
#else
    return false;
#endif
}

bool NicManager::addAddress(const QString &adapterName, const QString &ip, const QString &mask, QString *err)
{
#ifdef Q_OS_WIN
    // ponytail: 走 netsh；失败时提示用户以管理员运行。升级路径：Win32 IP Helper API。
    QProcess p;
    p.start(QStringLiteral("netsh"),
            {QStringLiteral("interface"), QStringLiteral("ipv4"), QStringLiteral("add"),
             QStringLiteral("address"), QStringLiteral("name=") + adapterName,
             QStringLiteral("addr=") + ip, QStringLiteral("mask=") + mask});
    if (!p.waitForFinished(8000)) {
        if (err)
            *err = QStringLiteral("netsh 超时");
        return false;
    }
    if (p.exitCode() != 0) {
        if (err)
            *err = QString::fromLocal8Bit(p.readAllStandardOutput() + p.readAllStandardError()).trimmed();
        return false;
    }
    return true;
#else
    Q_UNUSED(adapterName);
    Q_UNUSED(ip);
    Q_UNUSED(mask);
    if (err)
        *err = QStringLiteral("仅 Windows 支持追加 IP");
    return false;
#endif
}

bool NicManager::removeAddress(const QString &ip, QString *err)
{
#ifdef Q_OS_WIN
    QProcess p;
    p.start(QStringLiteral("netsh"),
            {QStringLiteral("interface"), QStringLiteral("ipv4"), QStringLiteral("delete"),
             QStringLiteral("address"), QStringLiteral("addr=") + ip});
    // 有的系统需要带 name=；再试一次按接口枚举后删除
    if (!p.waitForFinished(8000) || p.exitCode() != 0) {
        // 找到包含该 IP 的适配器名再删
        for (const NicEntry &n : enumerate()) {
            if (n.ip != ip)
                continue;
            QString ifaceName = n.name;
            ifaceName.remove(QStringLiteral(" [Primary]"));
            QProcess p2;
            p2.start(QStringLiteral("netsh"),
                     {QStringLiteral("interface"), QStringLiteral("ipv4"), QStringLiteral("delete"),
                      QStringLiteral("address"), QStringLiteral("name=") + ifaceName,
                      QStringLiteral("addr=") + ip});
            if (!p2.waitForFinished(8000) || p2.exitCode() != 0) {
                if (err)
                    *err = QString::fromLocal8Bit(p2.readAllStandardOutput() + p2.readAllStandardError())
                               .trimmed();
                return false;
            }
            return true;
        }
        if (err)
            *err = QString::fromLocal8Bit(p.readAllStandardOutput() + p.readAllStandardError()).trimmed();
        return false;
    }
    return true;
#else
    Q_UNUSED(ip);
    if (err)
        *err = QStringLiteral("仅 Windows 支持解绑 IP");
    return false;
#endif
}

QString NicManager::preferLanIp(const QList<NicEntry> &nics)
{
    if (nics.isEmpty())
        return QStringLiteral("127.0.0.1");
    return nics.first().ip;
}
