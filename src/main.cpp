#include "MainWindow.h"

#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QRectF>
#include <QSvgRenderer>

static QIcon loadAppIcon()
{
    // 用 UI 友好 SVG 生成多尺寸图标，避免从 qrc 解析 ICO 触发 Qt 崩溃
    QSvgRenderer renderer(QStringLiteral(":/icons/app_icon_ui.svg"));
    QIcon icon;
    if (!renderer.isValid())
        return icon;
    for (int s : {16, 24, 32, 48, 64, 128, 256}) {
        QPixmap pm(s, s);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        renderer.render(&p, QRectF(0, 0, s, s));
        p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("HttpLanFileShare"));
    QApplication::setApplicationDisplayName(QStringLiteral("HTTP 局域网极速文件共享客户端"));
    QApplication::setOrganizationName(QStringLiteral("HttpLanFileShare"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    const QIcon appIcon = loadAppIcon();
    app.setWindowIcon(appIcon);

    QFont font = app.font();
    font.setFamily(QStringLiteral("Microsoft YaHei UI"));
    font.setPointSize(10);
    app.setFont(font);

    MainWindow w;
    w.setWindowIcon(appIcon);
    w.show();
    return app.exec();
}
