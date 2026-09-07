#include "MainWindow.h"

#include <QApplication>
#include <QFont>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("HttpLanFileShare"));
    QApplication::setApplicationDisplayName(QStringLiteral("HTTP 局域网极速文件共享客户端"));
    QApplication::setOrganizationName(QStringLiteral("HttpLanFileShare"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app_icon.ico")));

    QFont font = app.font();
    font.setFamily(QStringLiteral("Microsoft YaHei UI"));
    font.setPointSize(10);
    app.setFont(font);

    MainWindow w;
    w.setWindowIcon(app.windowIcon());
    w.show();
    return app.exec();
}
