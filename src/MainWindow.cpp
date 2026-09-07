#include "MainWindow.h"
#include "SelfCheckDialog.h"
#include "StatusBadgeWidget.h"
#include "DropUploadArea.h"

#include "HttpFileServer.h"
#include "NicManager.h"
#include "ActivityLogModel.h"
#include "QrCodeWidget.h"

#include <QAbstractSpinBox>
#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QEvent>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QResource>
#include <QFileSystemWatcher>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSvgRenderer>
#include <QPainter>
#include <QPushButton>
#include <QSizePolicy>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidget>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QDir>
#include <QStandardPaths>
#include <QWindow>

static QString fmtBytes(qint64 n)
{
    const char *u[] = {"B", "KB", "MB", "GB", "TB"};
    double v = double(n);
    int i = 0;
    while (v >= 1024.0 && i < 4) {
        v /= 1024.0;
        ++i;
    }
    return QString::number(v, 'f', i == 0 ? 0 : 1) + QLatin1Char(' ') + QLatin1String(u[i]);
}

static QPixmap loadSvgPixmap(const QString &resPath, int size)
{
    QSvgRenderer renderer(resPath);
    if (!renderer.isValid())
        return {};
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&p, QRectF(0, 0, size, size));
    return pm;
}

static QIcon loadSvgIcon(const QString &resPath, int size)
{
    const QPixmap pm = loadSvgPixmap(resPath, size);
    return pm.isNull() ? QIcon() : QIcon(pm);
}

static QIcon fileTypeIcon(const QString &fileName)
{
    const QString ext = QFileInfo(fileName).suffix().toLower();
    if (ext == QLatin1String("zip") || ext == QLatin1String("rar") || ext == QLatin1String("7z")
        || ext == QLatin1String("tar") || ext == QLatin1String("gz") || ext == QLatin1String("tgz"))
        return loadSvgIcon(QStringLiteral(":/icons/file_zip.svg"), 18);
    if (ext == QLatin1String("exe") || ext == QLatin1String("msi") || ext == QLatin1String("bat")
        || ext == QLatin1String("cmd") || ext == QLatin1String("dll"))
        return loadSvgIcon(QStringLiteral(":/icons/file_exe.svg"), 18);
    return loadSvgIcon(QStringLiteral(":/icons/file_document.svg"), 18);
}

static bool isArchiveName(const QString &fileName)
{
    const QString ext = QFileInfo(fileName).suffix().toLower();
    return ext == QLatin1String("zip") || ext == QLatin1String("rar") || ext == QLatin1String("7z")
        || ext == QLatin1String("tar") || ext == QLatin1String("gz") || ext == QLatin1String("tgz")
        || fileName.contains(QStringLiteral("bundle"), Qt::CaseInsensitive);
}

static QIcon makeWinChromeIcon(int kind)
{
    // 0 最小化  1 最大化  2 关闭  3 还原
    QPixmap pm(36, 28);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, false);
    const QColor c(203, 213, 225);
    p.setPen(QPen(c, 1));
    if (kind == 0) {
        p.drawLine(13, 14, 23, 14);
    } else if (kind == 1) {
        p.drawRect(13, 9, 10, 10);
    } else if (kind == 3) {
        // 还原：两层错开方框
        p.drawRect(15, 8, 9, 9);
        p.fillRect(12, 11, 9, 9, QColor(9, 17, 30)); // 标题栏底色挖空
        p.drawRect(12, 11, 9, 9);
    } else {
        p.drawLine(13, 9, 23, 19);
        p.drawLine(23, 9, 13, 19);
    }
    return QIcon(pm);
}

static QToolButton *makeWinChromeBtn(QWidget *parent, int kind)
{
    auto *b = new QToolButton(parent);
    b->setIcon(makeWinChromeIcon(kind));
    b->setIconSize(QSize(36, 28));
    b->setFixedSize(36, 28);
    b->setAutoRaise(true);
    b->setCursor(Qt::ArrowCursor);
    b->setFocusPolicy(Qt::NoFocus);
    if (kind == 2) {
        b->setObjectName(QStringLiteral("WinClose"));
        b->setStyleSheet(QStringLiteral(
            "QToolButton{background:transparent;border:none;padding:0;margin:0;}"
            "QToolButton:hover{background:#e81123;}"
            "QToolButton:pressed{background:#f1707a;}"));
    } else {
        b->setObjectName(QStringLiteral("WinBtn"));
        b->setStyleSheet(QStringLiteral(
            "QToolButton{background:transparent;border:none;padding:0;margin:0;}"
            "QToolButton:hover{background:#1e293b;}"
            "QToolButton:pressed{background:#334155;}"));
    }
    return b;
}

static QIcon makeTabIcon(int kind, const QColor &color)
{
    QString path;
    if (kind == 0)
        path = QStringLiteral(":/icons/sliders_panel.svg");
    else if (kind == 1)
        path = QStringLiteral(":/icons/mobile_device.svg");
    else if (kind == 2)
        path = QStringLiteral(":/icons/activity_monitor.svg");
    else
        return {};

    QResource res(path);
    if (!res.isValid())
        return {};
    // Qt Svg 不认 currentColor，按页签态替换成具体颜色
    QByteArray svg(reinterpret_cast<const char *>(res.data()), int(res.size()));
    svg.replace("currentColor", color.name(QColor::HexRgb).toLatin1());

    QSvgRenderer renderer(svg);
    if (!renderer.isValid())
        return {};
    QPixmap pm(15, 15);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&p, QRectF(0, 0, 15, 15));
    return QIcon(pm);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 无边框，自绘标题栏对齐参考图
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

    m_server = new HttpFileServer(this);
    m_logs = new ActivityLogModel(this);
    m_watcher = new QFileSystemWatcher(this);

    m_shareRoot = QDir::fromNativeSeparators(
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + QStringLiteral("/SharedFiles"));
    QDir().mkpath(m_shareRoot);

    buildUi();
    applyTheme();
    refreshNics();
    refreshFiles();
    updateShareUrlUi();
    setRunningUi(false);
    updateUacBadge();
    updateTabChrome(0);

    connect(m_server, &HttpFileServer::started, this, &MainWindow::onServerStarted);
    connect(m_server, &HttpFileServer::stopped, this, &MainWindow::onServerStopped);
    connect(m_server, &HttpFileServer::clientDownload, this, &MainWindow::onDownload);
    connect(m_server, &HttpFileServer::clientUpload, this, &MainWindow::onUpload);
    connect(m_server, &HttpFileServer::errorOccurred, this, [this](const QString &e) {
        addLog(QStringLiteral("check"), e);
        showToast(e);
    });
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &MainWindow::refreshFiles);

    addLog(QStringLiteral("check"), QStringLiteral("客户端已启动，等待开启 HTTP 共享"));
    resize(1180, 860);
    setWindowTitle(QStringLiteral("HTTP 局域网极速文件共享客户端"));
}

MainWindow::~MainWindow()
{
    if (m_server)
        m_server->stop();
}

void MainWindow::applyTheme()
{
    setStyleSheet(QStringLiteral(R"QSS(
QMainWindow, QWidget {
  color: #e2e8f0;
  font-family: "Segoe UI", "Microsoft YaHei UI", sans-serif;
  font-size: 12px;
}
QMainWindow, QWidget#TitleBar, QWidget#CentralRoot, QScrollArea, QScrollArea > QWidget > QWidget {
  background: #070e1a;
}
QLabel {
  background: transparent;
  border: none;
}
QFrame#Card {
  background: #0b1424;
  border: 1px solid #1b2b46;
  border-radius: 12px;
}
QComboBox {
  background: #070d18;
  border: 1px solid #1f324f;
  border-radius: 8px;
  padding: 6px 8px;
  color: #e2e8f0;
}
QLineEdit, QSpinBox {
  background: #070d18;
  border: 1px solid #1f324f;
  border-radius: 8px;
  padding: 4px 22px 4px 8px;
  color: #e2e8f0;
  selection-background-color: #059669;
  min-height: 26px;
}
QSpinBox#PortSpin {
  color: #67e8f9;
  font-family: Consolas, "Cascadia Mono", monospace;
}
QSpinBox::up-button, QSpinBox::down-button {
  background: #122035;
  border: none;
  width: 18px;
}
QSpinBox::up-button {
  subcontrol-origin: border;
  subcontrol-position: top right;
  margin: 1px 1px 0 0;
  border-top-right-radius: 7px;
}
QSpinBox::down-button {
  subcontrol-origin: border;
  subcontrol-position: bottom right;
  margin: 0 1px 1px 0;
  border-bottom-right-radius: 7px;
}
QSpinBox::up-button:hover, QSpinBox::down-button:hover {
  background: #1a2f50;
}
QSpinBox::up-arrow {
  image: url(:/icons/chevron_up.png);
  width: 9px;
  height: 9px;
}
QSpinBox::down-arrow {
  image: url(:/icons/chevron_down.png);
  width: 9px;
  height: 9px;
}
QHeaderView::section {
  background: #0e1b2f;
  color: #94a3b8;
  border: none;
  border-right: 1px solid #1b2f4d;
  border-bottom: 1px solid #1b2f4d;
  padding: 8px 10px;
  font-weight: 600;
}
QHeaderView::section:last {
  border-right: none;
}
QTableCornerButton::section {
  background: #0e1b2f;
  border: none;
}
QTableWidget {
  background: #070e1b;
  border: 1px solid #1a2b45;
  border-radius: 8px;
  gridline-color: #14233a;
  selection-background-color: #0f2139;
  color: #e2e8f0;
  outline: 0;
}
QTableWidget#FileTable {
  background: #030813;
  border: 1px solid #132338;
  border-radius: 8px;
  gridline-color: transparent;
  selection-background-color: transparent;
  outline: none;
}
QTableWidget#FileTable QHeaderView::section {
  background: transparent;
  color: #94A3B8;
  font-size: 12px;
  font-weight: 500;
  padding: 12px 14px;
  border: none;
  border-right: none;
  border-bottom: 1px solid #132338;
}
QTableWidget#FileTable::item {
  border: none;
  border-bottom: 1px solid #0B1728;
  padding: 0px;
  background: transparent;
}
QTableWidget#FileTable::item:hover {
  background: #081528;
}
QTableWidget#FileTable::item:selected {
  background: #081528;
  color: #e2e8f0;
}
QTableWidget#NicTable {
  background: #030813;
  border: 1px solid #132338;
  border-radius: 8px;
  gridline-color: transparent;
  selection-background-color: transparent;
  outline: none;
}
QTableWidget#NicTable QHeaderView::section {
  background: transparent;
  color: #94A3B8;
  font-size: 12px;
  font-weight: 500;
  padding: 10px 12px;
  border: none;
  border-right: none;
  border-bottom: 1px solid #0F2036;
}
QTableWidget#NicTable::item {
  border: none;
  border-bottom: 1px solid #0B1728;
  padding: 0px;
  background: transparent;
}
QTableWidget#NicTable::item:hover {
  background: #071526;
}
QLineEdit#NicIpEdit, QLineEdit#NicMaskEdit {
  background: #061121;
  border: 1px solid #162E4D;
  border-radius: 6px;
  color: #38BDF8;
  font-family: Consolas, "Cascadia Mono", monospace;
  font-size: 13px;
  padding: 0 10px;
  min-height: 34px;
}
QLineEdit#NicMaskEdit {
  color: #E2E8F0;
}
QLineEdit#NicIpEdit:focus, QLineEdit#NicMaskEdit:focus {
  border: 1px solid #00D2FF;
}
QWidget#NicUac, QWidget#NicUacOff {
  min-height: 28px;
  max-height: 28px;
  border-radius: 14px;
}
QWidget#NicUac {
  background-color: rgba(69, 26, 3, 0.55);
  border: 1px solid rgba(217, 119, 6, 0.75);
}
QWidget#NicUacOff {
  background-color: rgba(30, 41, 59, 0.85);
  border: 1px solid #475569;
}
QWidget#NicUac QLabel, QWidget#NicUacOff QLabel {
  background: transparent;
  border: none;
  padding: 0;
}
QWidget#NicUac QLabel#UacText {
  color: #FBBF24;
  font-size: 12px;
  font-weight: 600;
}
QWidget#NicUacOff QLabel#UacText {
  color: #94a3b8;
  font-size: 12px;
  font-weight: 600;
}
QLabel#NicTopoIcon {
  background-color: #06182C;
  border: 1px solid #0E365E;
  border-radius: 6px;
}
QPushButton {
  background: #122035;
  border: 1px solid #233857;
  border-radius: 8px;
  padding: 7px 12px;
  color: #e2e8f0;
}
QPushButton:hover { background: #182b46; }
QPushButton#Primary {
  background: #059669;
  border: 1px solid #10b981;
  color: white;
  font-weight: 700;
  padding: 8px 16px;
  border-radius: 8px;
  min-height: 36px;
}
QPushButton#Primary:hover {
  background: #10b981;
  border-color: #34d399;
}
QPushButton#AddNicBtn {
  background: #10B981;
  border: 1px solid #10B981;
  border-radius: 6px;
  color: #FFFFFF;
  font-size: 13px;
  font-weight: 700;
  padding: 0 16px;
  min-height: 36px;
  max-height: 36px;
}
QPushButton#AddNicBtn:hover {
  background: #34D399;
  border-color: #6EE7B7;
}
QPushButton#AddNicBtn:pressed {
  background: #059669;
  border-color: #047857;
}
QPushButton#UploadShareBtn {
  background: rgba(6, 78, 59, 0.35);
  border: 1px solid rgba(16, 185, 129, 0.70);
  border-radius: 17px;
  color: #34d399;
  font-size: 13px;
  font-weight: 700;
  padding: 0 16px;
  min-height: 34px;
}
QPushButton#UploadShareBtn:hover {
  background: rgba(6, 78, 59, 0.60);
  border-color: #10b981;
  color: #6ee7b7;
}
QPushButton#UploadShareBtn:pressed {
  background: rgba(6, 78, 59, 0.85);
  border-color: #059669;
}
QPushButton#Danger {
  background: #500724;
  border: 1px solid #9F1239;
  color: #ffe4e6;
  font-weight: 700;
  padding: 8px 16px;
  border-radius: 8px;
  min-height: 36px;
}
QPushButton#Danger:hover {
  background: #700A32;
  border-color: #F43F5E;
}
QPushButton#Danger:pressed {
  background: #3F051C;
  border-color: #881337;
}
QFrame#TabStrip QPushButton {
  background: transparent;
  border: 2px solid transparent;
  border-radius: 8px;
  padding: 8px 16px;
  margin: 0px;
  min-height: 0px;
  color: #94a3b8;
  font-size: 12px;
  font-weight: 600;
}
QFrame#TabStrip QPushButton#TabActive {
  background: rgba(6, 78, 59, 0.55);
  border: 2px solid #34d399;
  color: #6ee7b7;
  padding: 8px 16px;
}
QFrame#TabStrip QPushButton#TabActiveCyan {
  background: rgba(8, 51, 68, 0.65);
  border: 2px solid #22d3ee;
  color: #67e8f9;
  padding: 8px 16px;
}
QFrame#TabStrip QPushButton#TabActiveIndigo {
  background: rgba(49, 46, 129, 0.55);
  border: 2px solid #818cf8;
  color: #a5b4fc;
  padding: 8px 16px;
}
QFrame#TabStrip {
  background: #0d182b;
  border: 1px solid #1d3153;
  border-radius: 10px;
}
QPushButton#BackPortalBtn {
  background: #0b1627;
  border: 2px solid #22d3ee;
  border-radius: 8px;
  color: #67e8f9;
  font-size: 12px;
  font-weight: 600;
  padding: 8px 14px 8px 12px;
  min-height: 34px;
}
QPushButton#BackPortalBtn:hover {
  background: #0e213b;
  border: 2px solid #67e8f9;
  color: #a5f3fc;
}
QPushButton#BackPortalBtn:pressed {
  background: #083344;
  border: 2px solid #22d3ee;
}
QLabel#SectionCyan {
  color: #22d3ee;
  font-size: 12px;
  font-weight: 600;
}
QComboBox#IpCombo {
  background: #0e1b2f;
  border: 1px solid #22d3ee;
  border-radius: 6px;
  padding: 4px 28px 4px 10px;
  color: #f1f5f9;
  font-size: 12px;
  font-family: Consolas, "Cascadia Mono", monospace;
  min-height: 26px;
}
QComboBox#IpCombo:hover {
  border: 1px solid #67e8f9;
}
QComboBox#IpCombo::drop-down {
  subcontrol-origin: padding;
  subcontrol-position: center right;
  width: 28px;
  border: none;
  background: transparent;
}
QComboBox#IpCombo::down-arrow {
  image: url(:/icons/chevron_down.png);
  width: 12px;
  height: 12px;
}
QComboBox#IpCombo QAbstractItemView {
  background: #0b1424;
  border: 1px solid #94a3b8;
  outline: 0;
  selection-background-color: #2563eb;
  selection-color: #ffffff;
  color: #e2e8f0;
}
QComboBox#IpCombo QAbstractItemView::item {
  min-height: 28px;
  padding: 4px 10px;
}
QComboBox#IpCombo QAbstractItemView::item:selected {
  background: #2563eb;
  color: #ffffff;
}
QPushButton#GhostCyan {
  background: #11243b;
  border: 1px solid rgba(34, 211, 238, 0.45);
  border-radius: 6px;
  color: #67e8f9;
  font-size: 12px;
  font-weight: 600;
  padding: 6px 12px;
  min-height: 28px;
}
QPushButton#GhostCyan:hover {
  background: #163050;
  border: 1px solid #22d3ee;
}
QPushButton#GhostCyanIcon {
  background: #11243b;
  border: 1px solid rgba(34, 211, 238, 0.45);
  border-radius: 6px;
  color: #67e8f9;
  padding: 0;
  min-width: 32px;
  max-width: 32px;
  min-height: 32px;
  max-height: 32px;
}
QPushButton#GhostCyanIcon:hover {
  background: #163050;
  border: 1px solid #22d3ee;
}
QPushButton#SelfCheckBtn {
  background: transparent;
  border: 1px solid #22d3ee;
  border-radius: 6px;
  color: #22d3ee;
  font-size: 12px;
  font-weight: 600;
  padding: 6px 12px;
  min-height: 28px;
}
QPushButton#SelfCheckBtn:hover {
  background: rgba(34, 211, 238, 0.10);
  border: 1px solid #67e8f9;
  color: #67e8f9;
}
QPushButton#ResetFolderBtn {
  background: transparent;
  border: none;
  color: #22d3ee;
  font-size: 11px;
  font-weight: 600;
  padding: 0 2px;
  min-height: 18px;
}
QPushButton#ResetFolderBtn:hover {
  color: #67e8f9;
  text-decoration: underline;
}
QPushButton#BrowseFolderBtn {
  background: #122035;
  border: 1px solid #233857;
  border-radius: 8px;
  color: #e2e8f0;
  font-size: 12px;
  padding: 0 14px;
  min-height: 34px;
}
QPushButton#BrowseFolderBtn:hover {
  background: #182b46;
  border-color: #38bdf8;
}
QPushButton#FileActCli {
  background: #071526;
  border: 1px solid #1A3454;
  border-radius: 4px;
  padding: 0;
  min-width: 32px;
  max-width: 32px;
  min-height: 28px;
  max-height: 28px;
}
QPushButton#FileActCli:hover {
  background: #0F2542;
  border-color: #00D2FF;
}
QPushButton#FileActDl {
  background: rgba(6, 78, 59, 0.45);
  border: 1px solid #059669;
  border-radius: 4px;
  color: #34D399;
  font-size: 12px;
  font-weight: 700;
  padding: 0 10px;
  min-height: 28px;
  max-height: 28px;
}
QPushButton#FileActDl:hover {
  background: rgba(6, 78, 59, 0.8);
  border-color: #10B981;
  color: #6EE7B7;
}
QPushButton#FileActDl:pressed {
  background: #064E3B;
}
QPushButton#FileActDel {
  background: transparent;
  border: none;
  border-radius: 4px;
  padding: 0;
  min-width: 28px;
  max-width: 28px;
  min-height: 28px;
  max-height: 28px;
}
QPushButton#FileActDel:hover {
  background: rgba(239, 68, 68, 0.15);
}
QPushButton#PriorityDl {
  background: rgba(8, 145, 178, 0.28);
  border: 1px solid rgba(34, 211, 238, 0.40);
  border-radius: 4px;
  color: #67e8f9;
  font-size: 11px;
  font-weight: 600;
  padding: 0 8px;
  min-height: 26px;
  max-height: 26px;
}
QPushButton#PriorityDl:hover {
  background: rgba(8, 145, 178, 0.45);
  border-color: #22d3ee;
  color: #a5f3fc;
}
QFrame#PriorityCard {
  background: #070d18;
  border: 1px solid #1f324f;
  border-radius: 8px;
}
QFrame#PriorityCard:hover {
  border: 1px solid rgba(34, 211, 238, 0.45);
}
QLabel#PriorityEmpty {
  color: #64748b;
  font-size: 12px;
  background: #070d18;
  border: 1px dashed #1f324f;
  border-radius: 8px;
  padding: 14px;
}
QLabel#FileStatsPill {
  background: #13233c;
  border: 1px solid #1f375c;
  border-radius: 10px;
  padding: 2px 10px;
  font-size: 11px;
  font-family: Consolas, "Cascadia Mono", monospace;
}
QLabel#FilePathHint {
  color: #94a3b8;
  font-size: 11px;
  font-family: Consolas, "Cascadia Mono", monospace;
  background: transparent;
}
QLineEdit#FolderPathEdit {
  font-family: Consolas, "Cascadia Mono", "Courier New", monospace;
  font-size: 12px;
  min-height: 32px;
  padding-left: 4px;
}
QListWidget {
  background: #070d18;
  border: 1px solid #182840;
  border-radius: 8px;
}
QLabel#Title { font-size: 14px; font-weight: 700; color: #f1f5f9; }
QLabel#Muted { color: #94a3b8; }
QLabel#Url { color: #34d399; font-family: Consolas, monospace; font-weight: 700; font-size: 13px; }
QLabel#Badge {
  background: #083344;
  border: 1px solid rgba(34,211,238,0.4);
  border-radius: 999px;
  padding: 2px 10px;
  color: #67e8f9;
}
QWidget#Uac {
  background: rgba(69, 26, 3, 0.55);
  border: 1px solid rgba(217, 119, 6, 0.75);
  border-radius: 12px;
  min-height: 24px;
  max-height: 24px;
}
QWidget#UacOff {
  background: rgba(30, 41, 59, 0.85);
  border: 1px solid #475569;
  border-radius: 12px;
  min-height: 24px;
  max-height: 24px;
}
QWidget#Uac QLabel, QWidget#UacOff QLabel {
  background: transparent;
  border: none;
  padding: 0;
}
QWidget#Uac QLabel#UacText {
  color: #fcd34d;
  font-size: 11px;
  font-weight: 600;
}
QWidget#UacOff QLabel#UacText {
  color: #94a3b8;
  font-size: 11px;
  font-weight: 600;
}
QLabel#Toast {
  background: #0e1d33;
  border: 1px solid rgba(6,182,212,0.6);
  border-radius: 12px;
  padding: 10px 16px;
  color: #f1f5f9;
}
QWidget#TitleBar {
  background: #09111e;
  border-bottom: 1px solid #1b2b46;
}
QPushButton#WinBtn {
  background: transparent;
  border: none;
  padding: 0;
  margin: 0;
}
QToolButton#WinBtn, QToolButton#WinClose {
  background: transparent;
  border: none;
  padding: 0;
  margin: 0;
}
QLabel#PortalCount {
  background: #1e293b;
  color: #cbd5e1;
  border-radius: 8px;
  padding: 1px 6px;
  font-size: 10px;
  min-width: 16px;
}
QLabel#RunDot {
  background: #34d399;
  border-radius: 3px;
  min-width: 6px;
  max-width: 6px;
  min-height: 6px;
  max-height: 6px;
}
QScrollBar:vertical {
  background: #070e1a;
  width: 10px;
  margin: 2px;
  border: none;
}
QScrollBar::handle:vertical {
  background: #1a2f4d;
  min-height: 40px;
  border-radius: 5px;
  border: 1px solid #243b5c;
}
QScrollBar::handle:vertical:hover {
  background: #25456e;
}
QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical {
  height: 0px;
  border: none;
  background: none;
}
QScrollBar::add-page:vertical,
QScrollBar::sub-page:vertical {
  background: transparent;
}
QScrollBar:horizontal {
  background: #070e1a;
  height: 10px;
  margin: 2px;
  border: none;
}
QScrollBar::handle:horizontal {
  background: #1a2f4d;
  min-width: 40px;
  border-radius: 5px;
  border: 1px solid #243b5c;
}
QScrollBar::handle:horizontal:hover {
  background: #25456e;
}
QScrollBar::add-line:horizontal,
QScrollBar::sub-line:horizontal {
  width: 0px;
  border: none;
  background: none;
}
QScrollBar::add-page:horizontal,
QScrollBar::sub-page:horizontal {
  background: transparent;
}
QScrollArea {
  background: transparent;
  border: none;
}
QScrollArea > QWidget > QWidget {
  background: transparent;
}
)QSS"));
}

static QIcon makeBackArrowIcon(const QColor &color)
{
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    // ← 箭头
    p.drawLine(QPointF(12.5, 8), QPointF(4.5, 8));
    p.drawLine(QPointF(7.5, 4.5), QPointF(4.2, 8));
    p.drawLine(QPointF(7.5, 11.5), QPointF(4.2, 8));
    return QIcon(pm);
}

static QIcon makePulseIcon(const QColor &color)
{
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPolyline(QPolygonF({QPointF(1, 9), QPointF(4, 9), QPointF(6, 3.5), QPointF(8.5, 13),
                              QPointF(11, 7), QPointF(13, 9), QPointF(15, 9)}));
    return QIcon(pm);
}

static QFrame *makeCard(QWidget *parent)
{
    auto *f = new QFrame(parent);
    f->setObjectName(QStringLiteral("Card"));
    return f;
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("CentralRoot"));
    setCentralWidget(central);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // 顶栏：左品牌 | 居中页签 | 右窗控
    m_titleBar = new QWidget;
    m_titleBar->setObjectName(QStringLiteral("TitleBar"));
    m_titleBar->setFixedHeight(58);
    m_titleBar->installEventFilter(this);
    auto *tb = new QHBoxLayout(m_titleBar);
    tb->setContentsMargins(10, 4, 6, 4);
    tb->setSpacing(0);

    // 分享图标（资源 SVG）
    auto *appIcon = new QLabel;
    appIcon->setFixedSize(26, 26);
    {
        QSvgRenderer renderer(QStringLiteral(":/icons/share_app_icon.svg"));
        QPixmap pm(26, 26);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        renderer.render(&p, QRectF(0, 0, 26, 26));
        appIcon->setPixmap(pm);
    }

    auto *appName = new QLabel(QStringLiteral("HTTP 局域网极速文件共享客户端"));
    appName->setStyleSheet(QStringLiteral("font-size:13px;font-weight:700;color:#f8fafc;background:transparent;"));

    m_uacBadge = new QWidget;
    m_uacBadge->setObjectName(QStringLiteral("UacOff"));
    m_uacBadge->setCursor(Qt::PointingHandCursor);
    m_uacBadge->setToolTip(QStringLiteral("点击可请求管理员权限（真实 Windows UAC）"));
    m_uacBadge->installEventFilter(this);
    m_uacBadge->setAttribute(Qt::WA_StyledBackground, true);
    auto *uacLay = new QHBoxLayout(m_uacBadge);
    uacLay->setContentsMargins(10, 0, 12, 0);
    uacLay->setSpacing(6);
    m_uacIcon = new QLabel;
    m_uacIcon->setFixedSize(14, 14);
    m_uacIcon->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_uacText = new QLabel;
    m_uacText->setObjectName(QStringLiteral("UacText"));
    m_uacText->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    uacLay->addWidget(m_uacIcon, 0, Qt::AlignVCenter);
    uacLay->addWidget(m_uacText, 0, Qt::AlignVCenter);

    auto *leftPanel = new QWidget;
    auto *lp = new QHBoxLayout(leftPanel);
    lp->setContentsMargins(0, 0, 0, 0);
    lp->setSpacing(8);
    lp->addWidget(appIcon, 0, Qt::AlignVCenter);
    lp->addWidget(appName, 0, Qt::AlignVCenter);
    lp->addWidget(m_uacBadge, 0, Qt::AlignVCenter);
    lp->addStretch(1);

    auto *tabs = new QFrame;
    tabs->setObjectName(QStringLiteral("TabStrip"));
    tabs->setFrameShape(QFrame::NoFrame);
    tabs->setAttribute(Qt::WA_StyledBackground, true);
    // 外框只包住页签；真正加高的是选中项绿色描边框本身
    tabs->setFixedHeight(50);
    tabs->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    auto *tabsLay = new QHBoxLayout(tabs);
    tabsLay->setContentsMargins(4, 4, 4, 4);
    tabsLay->setSpacing(4);

    m_tabManager = new QPushButton(QStringLiteral("客户端控制面板"));
    m_tabPortal = new QPushButton(QStringLiteral("局域网提货 Web 端"));
    m_tabLogs = new QPushButton(QStringLiteral("实时日志与监控"));
    m_tabManager->setIcon(makeTabIcon(0, QColor(QStringLiteral("#94a3b8"))));
    m_tabPortal->setIcon(makeTabIcon(1, QColor(QStringLiteral("#94a3b8"))));
    m_tabLogs->setIcon(makeTabIcon(2, QColor(QStringLiteral("#94a3b8"))));
    for (auto *b : {m_tabManager, m_tabPortal, m_tabLogs}) {
        b->setObjectName(QStringLiteral("Tab"));
        b->setCursor(Qt::PointingHandCursor);
        b->setFlat(true);
        // 选中绿框高度由 padding + 文字撑开，这里给足竖向空间
        b->setMinimumHeight(42);
        b->setIconSize(QSize(15, 15));
        b->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    }

    m_runDot = new QLabel;
    m_runDot->setObjectName(QStringLiteral("RunDot"));
    m_runDot->setVisible(false);

    m_portalCount = new QLabel(QStringLiteral("0"));
    m_portalCount->setObjectName(QStringLiteral("PortalCount"));
    m_portalCount->setAlignment(Qt::AlignCenter);
    m_portalCount->setFixedHeight(16);

    tabsLay->addWidget(m_tabManager, 0, Qt::AlignVCenter);
    tabsLay->addWidget(m_runDot, 0, Qt::AlignVCenter);
    tabsLay->addWidget(m_tabPortal, 0, Qt::AlignVCenter);
    tabsLay->addWidget(m_portalCount, 0, Qt::AlignVCenter);
    tabsLay->addWidget(m_tabLogs, 0, Qt::AlignVCenter);

    auto *minBtn = makeWinChromeBtn(m_titleBar, 0);
    m_maxBtn = makeWinChromeBtn(m_titleBar, 1);
    auto *closeBtn = makeWinChromeBtn(m_titleBar, 2);
    auto *chrome = new QWidget;
    chrome->setFixedHeight(28);
    auto *chromeLay = new QHBoxLayout(chrome);
    chromeLay->setContentsMargins(0, 0, 0, 0);
    chromeLay->setSpacing(0);
    chromeLay->addWidget(minBtn);
    chromeLay->addWidget(m_maxBtn);
    chromeLay->addWidget(closeBtn);

    auto *rightPanel = new QWidget;
    auto *rp = new QHBoxLayout(rightPanel);
    rp->setContentsMargins(0, 0, 0, 0);
    rp->setSpacing(0);
    rp->addStretch(1);
    rp->addWidget(chrome, 0, Qt::AlignVCenter);

    // 左右等权拉伸 → 页签落在标题栏正中
    tb->addWidget(leftPanel, 1);
    tb->addWidget(tabs, 0, Qt::AlignVCenter);
    tb->addWidget(rightPanel, 1);
    root->addWidget(m_titleBar);

    connect(m_tabManager, &QPushButton::clicked, this, [this] { switchView(0); });
    connect(m_tabPortal, &QPushButton::clicked, this, [this] { switchView(1); });
    connect(m_tabLogs, &QPushButton::clicked, this, [this] { switchView(2); });
    connect(minBtn, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(m_maxBtn, &QToolButton::clicked, this, [this] {
        if (isMaximized())
            showNormal();
        else
            showMaximized();
        updateMaxButtonIcon();
    });
    connect(closeBtn, &QToolButton::clicked, this, &QWidget::close);

    m_stack = new QStackedWidget;
    root->addWidget(m_stack, 1);

    // ===== View 0: Manager（整页可滚）=====
    auto *managerScroll = new QScrollArea;
    managerScroll->setWidgetResizable(true);
    managerScroll->setFrameShape(QFrame::NoFrame);
    managerScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    managerScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto *managerPage = new QWidget;
    managerScroll->setWidget(managerPage);
    auto *mv = new QVBoxLayout(managerPage);
    mv->setContentsMargins(16, 16, 16, 16);
    mv->setSpacing(12);

    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(12);

    // 左：HTTP 配置卡
    auto *httpCard = makeCard(managerPage);
    auto *httpLay = new QVBoxLayout(httpCard);
    httpLay->setContentsMargins(16, 16, 16, 16);
    auto *httpHead = new QHBoxLayout;
    httpHead->setSpacing(14);
    auto *globeIcon = new QLabel;
    globeIcon->setFixedSize(40, 40);
    {
        QSvgRenderer renderer(QStringLiteral(":/icons/globe_container.svg"));
        QPixmap pm(40, 40);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        renderer.render(&p, QRectF(0, 0, 40, 40));
        globeIcon->setPixmap(pm);
    }
    auto *httpTitleBox = new QVBoxLayout;
    httpTitleBox->setSpacing(4);
    auto *titleRow = new QHBoxLayout;
    titleRow->setSpacing(10);
    auto *httpTitle = new QLabel(QStringLiteral("局域网 HTTP 极速文件共享服务"));
    httpTitle->setObjectName(QStringLiteral("Title"));
    m_statusPill = new StatusBadgeWidget;
    titleRow->addWidget(httpTitle, 0, Qt::AlignVCenter);
    titleRow->addWidget(m_statusPill, 0, Qt::AlignVCenter);
    titleRow->addStretch(1);
    httpTitleBox->addLayout(titleRow);
    httpTitleBox->addWidget(new QLabel(QStringLiteral("一键将本地文件夹发布为 HTTP 站点，手机/电脑/开发板均可下载。")));
    httpHead->addWidget(globeIcon, 0, Qt::AlignTop);
    httpHead->addLayout(httpTitleBox, 1);
    m_toggleBtn = new QPushButton(QStringLiteral("一键启动 HTTP 共享"));
    m_toggleBtn->setObjectName(QStringLiteral("Primary"));
    m_toggleBtn->setIcon(loadSvgIcon(QStringLiteral(":/icons/power_icon_white.svg"), 16));
    m_toggleBtn->setIconSize(QSize(16, 16));
    m_toggleBtn->setCursor(Qt::PointingHandCursor);
    m_toggleBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    httpHead->addWidget(m_toggleBtn, 0, Qt::AlignTop);
    httpLay->addLayout(httpHead);

    auto *folderHead = new QHBoxLayout;
    folderHead->setContentsMargins(0, 0, 0, 0);
    folderHead->setSpacing(6);
    auto *folderTitleIcon = new QLabel;
    folderTitleIcon->setFixedSize(15, 15);
    folderTitleIcon->setPixmap(loadSvgIcon(QStringLiteral(":/icons/folder_outline_cyan.svg"), 15).pixmap(15, 15));
    auto *folderTitle = new QLabel(QStringLiteral("本地共享文件夹路径"));
    folderTitle->setStyleSheet(QStringLiteral("color:#cbd5e1;font-size:12px;font-weight:600;background:transparent;"));
    auto *resetFolderBtn = new QPushButton(QStringLiteral("重设为默认共享目录"));
    resetFolderBtn->setObjectName(QStringLiteral("ResetFolderBtn"));
    resetFolderBtn->setIcon(loadSvgIcon(QStringLiteral(":/icons/refresh_cw_cyan.svg"), 13));
    resetFolderBtn->setIconSize(QSize(13, 13));
    resetFolderBtn->setCursor(Qt::PointingHandCursor);
    folderHead->addWidget(folderTitleIcon, 0, Qt::AlignVCenter);
    folderHead->addWidget(folderTitle, 0, Qt::AlignVCenter);
    folderHead->addStretch(1);
    folderHead->addWidget(resetFolderBtn, 0, Qt::AlignVCenter);
    httpLay->addLayout(folderHead);

    auto *folderRow = new QHBoxLayout;
    folderRow->setSpacing(8);
    m_folderEdit = new QLineEdit(m_shareRoot);
    m_folderEdit->setObjectName(QStringLiteral("FolderPathEdit"));
    m_folderEdit->setPlaceholderText(QStringLiteral("例如 D:/SharedFiles 或 C:/Users/Public/Downloads"));
    auto *folderLead = new QAction(loadSvgIcon(QStringLiteral(":/icons/folder_filled_yellow.svg"), 16), QString(), m_folderEdit);
    m_folderEdit->addAction(folderLead, QLineEdit::LeadingPosition);
    auto *browseBtn = new QPushButton(QStringLiteral("浏览..."));
    browseBtn->setObjectName(QStringLiteral("BrowseFolderBtn"));
    browseBtn->setIcon(loadSvgIcon(QStringLiteral(":/icons/search_browse.svg"), 14));
    browseBtn->setIconSize(QSize(14, 14));
    browseBtn->setCursor(Qt::PointingHandCursor);
    folderRow->addWidget(m_folderEdit, 1);
    folderRow->addWidget(browseBtn);
    httpLay->addLayout(folderRow);

    auto *portRow = new QHBoxLayout;
    portRow->addWidget(new QLabel(QStringLiteral("服务端口:")));
    m_portSpin = new QSpinBox;
    m_portSpin->setObjectName(QStringLiteral("PortSpin"));
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(8899);
    m_portSpin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    m_portSpin->setFixedWidth(110);
    portRow->addWidget(m_portSpin);
    for (int p : {8899, 8080, 8000, 9000}) {
        auto *b = new QPushButton(QString::number(p));
        connect(b, &QPushButton::clicked, this, [this, p] {
            if (!m_running)
                m_portSpin->setValue(p);
        });
        portRow->addWidget(b);
    }
    portRow->addStretch();
    httpLay->addLayout(portRow);

    auto *shareTitleRow = new QHBoxLayout;
    shareTitleRow->setSpacing(6);
    auto *shareTitleIcon = new QLabel;
    shareTitleIcon->setFixedSize(14, 14);
    shareTitleIcon->setPixmap(loadSvgPixmap(QStringLiteral(":/icons/wifi_lan.svg"), 14));
    auto *shareTitle = new QLabel(QStringLiteral("局域网直连提货地址"));
    shareTitle->setObjectName(QStringLiteral("SectionCyan"));
    shareTitleRow->addWidget(shareTitleIcon, 0, Qt::AlignVCenter);
    shareTitleRow->addWidget(shareTitle, 0, Qt::AlignVCenter);
    shareTitleRow->addStretch(1);
    httpLay->addLayout(shareTitleRow);

    auto *urlBox = makeCard(httpCard);
    urlBox->setStyleSheet(QStringLiteral("QFrame#Card{background:#070d18;border:1px solid #1f324f;border-radius:8px;}"));
    auto *urlLay = new QHBoxLayout(urlBox);
    urlLay->setContentsMargins(12, 10, 12, 10);
    urlLay->setSpacing(12);

    auto *wifiBadge = new QLabel;
    wifiBadge->setFixedSize(32, 32);
    wifiBadge->setAlignment(Qt::AlignCenter);
    wifiBadge->setStyleSheet(QStringLiteral("background:transparent;border:none;"));
    wifiBadge->setPixmap(loadSvgPixmap(QStringLiteral(":/icons/wifi_lan_box.svg"), 32));

    m_ipCombo = new QComboBox;
    m_ipCombo->setObjectName(QStringLiteral("IpCombo"));
    m_ipCombo->setMinimumWidth(260);
    m_ipCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    // 下拉列表选中条铺满行宽
    m_ipCombo->view()->setTextElideMode(Qt::ElideNone);
    m_ipCombo->setMaxVisibleItems(10);

    m_urlLabel = new QLabel(QStringLiteral("— (服务未启动)"));
    m_urlLabel->setObjectName(QStringLiteral("Url"));

    auto *ipRow = new QHBoxLayout;
    ipRow->setSpacing(8);
    auto *ipHint = new QLabel(QStringLiteral("局域网首选 IP:"));
    ipHint->setStyleSheet(QStringLiteral("color:#94a3b8;background:transparent;"));
    ipRow->addWidget(ipHint);
    ipRow->addWidget(m_ipCombo, 1);

    auto *leftUrl = new QVBoxLayout;
    leftUrl->setSpacing(6);
    leftUrl->addLayout(ipRow);
    leftUrl->addWidget(m_urlLabel);

    auto *copyBtn = new QPushButton(QStringLiteral("复制地址"));
    copyBtn->setObjectName(QStringLiteral("GhostCyan"));
    copyBtn->setIcon(loadSvgIcon(QStringLiteral(":/icons/copy_clipboard.svg"), 14));
    copyBtn->setIconSize(QSize(14, 14));
    copyBtn->setCursor(Qt::PointingHandCursor);

    auto *openBtn = new QPushButton;
    openBtn->setObjectName(QStringLiteral("GhostCyanIcon"));
    openBtn->setIcon(loadSvgIcon(QStringLiteral(":/icons/external_link.svg"), 14));
    openBtn->setIconSize(QSize(14, 14));
    openBtn->setToolTip(QStringLiteral("在浏览器打开提货页"));
    openBtn->setCursor(Qt::PointingHandCursor);

    urlLay->addWidget(wifiBadge, 0, Qt::AlignVCenter);
    urlLay->addLayout(leftUrl, 1);
    urlLay->addWidget(copyBtn, 0, Qt::AlignVCenter);
    urlLay->addWidget(openBtn, 0, Qt::AlignVCenter);
    httpLay->addWidget(urlBox);

    auto *loopRow = new QHBoxLayout;
    m_loopbackLabel = new QLabel(QStringLiteral("本机 Loopback: http://127.0.0.1:8899"));
    m_loopbackLabel->setObjectName(QStringLiteral("Muted"));
    auto *selfCheckBtn = new QPushButton(QStringLiteral("执行自检 (Self-check)"));
    selfCheckBtn->setObjectName(QStringLiteral("SelfCheckBtn"));
    selfCheckBtn->setIcon(makePulseIcon(QColor(QStringLiteral("#22d3ee"))));
    selfCheckBtn->setIconSize(QSize(14, 14));
    selfCheckBtn->setCursor(Qt::PointingHandCursor);
    loopRow->addWidget(m_loopbackLabel);
    loopRow->addStretch();
    loopRow->addWidget(selfCheckBtn);
    httpLay->addLayout(loopRow);

    // 右：二维码卡
    auto *qrCard = makeCard(managerPage);
    auto *qrLay = new QVBoxLayout(qrCard);
    qrLay->setContentsMargins(16, 16, 16, 16);
    qrLay->setSpacing(10);

    auto *qrHead = new QHBoxLayout;
    qrHead->setSpacing(8);
    auto *qrTitleIcon = new QLabel;
    qrTitleIcon->setFixedSize(16, 16);
    qrTitleIcon->setPixmap(loadSvgIcon(QStringLiteral(":/icons/qrcode_icon.svg"), 16).pixmap(16, 16));
    auto *qrTitle = new QLabel(QStringLiteral("扫码提货 / 扫码下载"));
    qrTitle->setStyleSheet(QStringLiteral(
        "color:#e2e8f0;font-size:12px;font-weight:700;letter-spacing:0.5px;background:transparent;"));
    auto *qrHintIcon = new QLabel;
    qrHintIcon->setFixedSize(14, 14);
    qrHintIcon->setPixmap(loadSvgPixmap(QStringLiteral(":/icons/smartphone.svg"), 14));
    auto *qrHint = new QLabel(QStringLiteral("摄像头对准扫码"));
    qrHint->setStyleSheet(QStringLiteral("color:#94a3b8;font-size:11px;background:transparent;"));
    auto *qrHintWrap = new QWidget;
    auto *qrHintLay = new QHBoxLayout(qrHintWrap);
    qrHintLay->setContentsMargins(0, 0, 0, 0);
    qrHintLay->setSpacing(4);
    qrHintLay->addWidget(qrHintIcon, 0, Qt::AlignVCenter);
    qrHintLay->addWidget(qrHint, 0, Qt::AlignVCenter);
    qrHead->addWidget(qrTitleIcon, 0, Qt::AlignVCenter);
    qrHead->addWidget(qrTitle, 0, Qt::AlignVCenter);
    qrHead->addStretch(1);
    qrHead->addWidget(qrHintWrap, 0, Qt::AlignVCenter);
    qrLay->addLayout(qrHead);

    m_qr = new QrCodeWidget;
    m_qr->setMinimumHeight(200);
    qrLay->addWidget(m_qr, 1);
    m_qrUrlLabel = new QLabel;
    m_qrUrlLabel->setObjectName(QStringLiteral("Url"));
    m_qrUrlLabel->setAlignment(Qt::AlignCenter);
    m_qrUrlLabel->setWordWrap(true);
    qrLay->addWidget(m_qrUrlLabel);

    auto *scanTip = new QWidget;
    auto *scanTipLay = new QHBoxLayout(scanTip);
    scanTipLay->setContentsMargins(0, 4, 0, 4);
    scanTipLay->setSpacing(8);
    auto *scanPhoneIcon = new QLabel;
    scanPhoneIcon->setFixedSize(16, 16);
    scanPhoneIcon->setAlignment(Qt::AlignCenter);
    scanPhoneIcon->setPixmap(loadSvgPixmap(QStringLiteral(":/icons/smartphone.svg"), 16));
    auto *scanTipText = new QLabel(QStringLiteral("手机扫码直连 · 手机、平板或局域网客户机扫码即可打开提货页"));
    scanTipText->setStyleSheet(QStringLiteral("color:#cbd5e1;font-size:11px;background:transparent;"));
    scanTipText->setWordWrap(true);
    scanTipLay->addWidget(scanPhoneIcon, 0, Qt::AlignVCenter);
    scanTipLay->addWidget(scanTipText, 1, Qt::AlignVCenter);
    qrLay->addWidget(scanTip);

    auto *pkgHead = new QHBoxLayout;
    pkgHead->setContentsMargins(0, 8, 0, 0);
    pkgHead->setSpacing(6);
    auto *pkgIcon = new QLabel;
    pkgIcon->setFixedSize(14, 14);
    pkgIcon->setPixmap(loadSvgPixmap(QStringLiteral(":/icons/package_box_cyan.svg"), 14));
    auto *pkgTitle = new QLabel(QStringLiteral("共享文件快捷提货"));
    pkgTitle->setStyleSheet(QStringLiteral(
        "color:#e2e8f0;font-size:12px;font-weight:600;background:transparent;"));
    auto *readyBadge = new QLabel(QStringLiteral("就绪共享"));
    readyBadge->setStyleSheet(QStringLiteral(
        "color:#34d399;background:rgba(6,78,59,0.55);border:1px solid rgba(16,185,129,0.45);"
        "border-radius:4px;padding:2px 8px;font-size:10px;font-weight:600;"));
    pkgHead->addWidget(pkgIcon, 0, Qt::AlignVCenter);
    pkgHead->addWidget(pkgTitle, 0, Qt::AlignVCenter);
    pkgHead->addStretch(1);
    pkgHead->addWidget(readyBadge, 0, Qt::AlignVCenter);
    qrLay->addLayout(pkgHead);

    m_priorityEmpty = new QLabel(QStringLiteral("暂无打包产物"));
    m_priorityEmpty->setObjectName(QStringLiteral("PriorityEmpty"));
    m_priorityEmpty->setAlignment(Qt::AlignCenter);
    qrLay->addWidget(m_priorityEmpty);

    m_priorityCard = new QFrame;
    m_priorityCard->setObjectName(QStringLiteral("PriorityCard"));
    auto *priorityLay = new QHBoxLayout(m_priorityCard);
    priorityLay->setContentsMargins(10, 10, 10, 10);
    priorityLay->setSpacing(10);

    m_priorityIcon = new QLabel;
    m_priorityIcon->setFixedSize(22, 22);
    m_priorityIcon->setPixmap(loadSvgPixmap(QStringLiteral(":/icons/box_cardboard.svg"), 22));

    auto *textCol = new QWidget;
    auto *textLay = new QVBoxLayout(textCol);
    textLay->setContentsMargins(0, 0, 0, 0);
    textLay->setSpacing(2);
    m_priorityName = new QLabel;
    m_priorityName->setStyleSheet(QStringLiteral(
        "color:#e2e8f0;font-size:12px;font-weight:600;font-family:Consolas,\"Cascadia Mono\",monospace;"
        "background:transparent;"));
    m_priorityName->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_prioritySize = new QLabel;
    m_prioritySize->setStyleSheet(QStringLiteral(
        "color:#94a3b8;font-size:10px;font-family:Consolas,\"Cascadia Mono\",monospace;background:transparent;"));
    textLay->addWidget(m_priorityName);
    textLay->addWidget(m_prioritySize);

    m_priorityActs = new QWidget;
    auto *actLay = new QHBoxLayout(m_priorityActs);
    actLay->setContentsMargins(0, 0, 0, 0);
    actLay->setSpacing(6);
    auto *cliBtn = new QPushButton;
    cliBtn->setObjectName(QStringLiteral("FileActCli"));
    cliBtn->setIcon(loadSvgIcon(QStringLiteral(":/icons/terminal_cli.svg"), 14));
    cliBtn->setIconSize(QSize(14, 14));
    cliBtn->setCursor(Qt::PointingHandCursor);
    cliBtn->setToolTip(QStringLiteral("复制 curl 下载命令"));
    auto *dlBtn = new QPushButton(QStringLiteral("下载"));
    dlBtn->setObjectName(QStringLiteral("PriorityDl"));
    dlBtn->setIcon(loadSvgIcon(QStringLiteral(":/icons/download_icon_cyan.svg"), 14));
    dlBtn->setIconSize(QSize(14, 14));
    dlBtn->setCursor(Qt::PointingHandCursor);
    dlBtn->setToolTip(QStringLiteral("直接下载此文件"));
    actLay->addWidget(cliBtn);
    actLay->addWidget(dlBtn);

    priorityLay->addWidget(m_priorityIcon, 0, Qt::AlignVCenter);
    priorityLay->addWidget(textCol, 1, Qt::AlignVCenter);
    priorityLay->addWidget(m_priorityActs, 0, Qt::AlignVCenter);
    m_priorityCard->setVisible(false);
    qrLay->addWidget(m_priorityCard);

    connect(cliBtn, &QPushButton::clicked, this, [this] {
        if (!m_priorityName->text().isEmpty())
            copyCurlForName(m_priorityName->text());
    });
    connect(dlBtn, &QPushButton::clicked, this, [this] {
        if (!m_priorityPath.isEmpty())
            downloadFileByPath(m_priorityPath, m_priorityName->text());
    });

    topRow->addWidget(httpCard, 7);
    topRow->addWidget(qrCard, 5);
    mv->addLayout(topRow);

    // 文件卡
    auto *fileCard = makeCard(managerPage);
    auto *fileLay = new QVBoxLayout(fileCard);
    fileLay->setContentsMargins(16, 16, 16, 16);
    fileLay->setSpacing(12);

    auto *fileHead = new QHBoxLayout;
    fileHead->setSpacing(10);

    auto *fileFolderIcon = new QLabel;
    fileFolderIcon->setFixedSize(32, 32);
    fileFolderIcon->setPixmap(loadSvgPixmap(QStringLiteral(":/icons/folder_open_box.svg"), 32));

    auto *fileTitleCol = new QWidget;
    auto *fileTitleLay = new QVBoxLayout(fileTitleCol);
    fileTitleLay->setContentsMargins(0, 0, 0, 0);
    fileTitleLay->setSpacing(2);

    auto *fileTitleRow = new QHBoxLayout;
    fileTitleRow->setSpacing(8);
    auto *fileTitle = new QLabel(QStringLiteral("共享文件夹内容与快速传输"));
    fileTitle->setStyleSheet(QStringLiteral(
        "color:#f1f5f9;font-size:13px;font-weight:700;background:transparent;"));
    m_fileStats = new QLabel;
    m_fileStats->setObjectName(QStringLiteral("FileStatsPill"));
    m_fileStats->setTextFormat(Qt::RichText);
    m_fileStats->setText(QStringLiteral("<span style='color:#00d2ff'>0</span>"
                                        "<span style='color:#7dd3fc'> 个文件</span>"
                                        "<span style='color:#7dd3fc'> · </span>"
                                        "<span style='color:#00d2ff'>0 B</span>"));
    fileTitleRow->addWidget(fileTitle, 0, Qt::AlignVCenter);
    fileTitleRow->addWidget(m_fileStats, 0, Qt::AlignVCenter);
    fileTitleRow->addStretch(1);

    m_filePathLabel = new QLabel;
    m_filePathLabel->setObjectName(QStringLiteral("FilePathHint"));
    m_filePathLabel->setText(QStringLiteral("当前路径: %1").arg(m_shareRoot));
    m_filePathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    fileTitleLay->addLayout(fileTitleRow);
    fileTitleLay->addWidget(m_filePathLabel);

    auto *uploadBtn = new QPushButton(QStringLiteral("上传文件到共享目录"));
    uploadBtn->setObjectName(QStringLiteral("UploadShareBtn"));
    uploadBtn->setIcon(loadSvgIcon(QStringLiteral(":/icons/upload_icon.svg"), 16));
    uploadBtn->setIconSize(QSize(16, 16));
    uploadBtn->setFixedHeight(34);
    uploadBtn->setCursor(Qt::PointingHandCursor);

    fileHead->addWidget(fileFolderIcon, 0, Qt::AlignTop);
    fileHead->addWidget(fileTitleCol, 1, Qt::AlignVCenter);
    fileHead->addWidget(uploadBtn, 0, Qt::AlignTop);
    fileLay->addLayout(fileHead);

    auto *dropArea = new DropUploadArea;
    fileLay->addWidget(dropArea);
    connect(dropArea, &DropUploadArea::filesDropped, this, &MainWindow::importFilesToShare);

    auto *filterRow = new QHBoxLayout;
    m_fileFilter = new QLineEdit;
    m_fileFilter->setPlaceholderText(QStringLiteral("搜索共享文件名..."));
    filterRow->addWidget(m_fileFilter, 1);
    fileLay->addLayout(filterRow);

    m_fileTable = new QTableWidget(0, 5);
    m_fileTable->setObjectName(QStringLiteral("FileTable"));
    m_fileTable->setHorizontalHeaderLabels(
        {QStringLiteral("文件名与产物"), QStringLiteral("文件大小"), QStringLiteral("修改/生成时间"),
         QStringLiteral("下载次数"), QStringLiteral("快捷操作")});
    m_fileTable->horizontalHeader()->setStretchLastSection(false);
    m_fileTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_fileTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_fileTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_fileTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_fileTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_fileTable->setColumnWidth(4, 180);
    m_fileTable->setIconSize(QSize(18, 18));
    m_fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fileTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fileTable->setFocusPolicy(Qt::NoFocus);
    m_fileTable->setShowGrid(false);
    m_fileTable->setFrameShape(QFrame::NoFrame);
    m_fileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fileTable->verticalHeader()->setVisible(false);
    m_fileTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_fileTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_fileTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_fileTable->setMouseTracking(true);
    fileLay->addWidget(m_fileTable);
    mv->addWidget(fileCard);

    // 网卡卡
    auto *nicCard = makeCard(managerPage);
    auto *nicLay = new QVBoxLayout(nicCard);
    nicLay->setContentsMargins(20, 20, 20, 20);
    nicLay->setSpacing(14);

    auto *nicHead = new QHBoxLayout;
    nicHead->setSpacing(10);
    auto *nicTopo = new QLabel;
    nicTopo->setObjectName(QStringLiteral("NicTopoIcon"));
    nicTopo->setFixedSize(30, 30);
    nicTopo->setAlignment(Qt::AlignCenter);
    nicTopo->setPixmap(loadSvgPixmap(QStringLiteral(":/icons/network_nodes.svg"), 20));
    auto *nicTitle = new QLabel(QStringLiteral("有线网卡高级 IP 地址绑定与管理 (NIC Manager)"));
    nicTitle->setStyleSheet(QStringLiteral("color:#FFFFFF;font-size:15px;font-weight:700;background:transparent;"));

    m_nicUacBadge = new QWidget;
    m_nicUacBadge->setObjectName(QStringLiteral("NicUacOff"));
    m_nicUacBadge->setCursor(Qt::PointingHandCursor);
    m_nicUacBadge->setAttribute(Qt::WA_StyledBackground, true);
    m_nicUacBadge->installEventFilter(this);
    auto *nicUacLay = new QHBoxLayout(m_nicUacBadge);
    nicUacLay->setContentsMargins(14, 0, 16, 0);
    nicUacLay->setSpacing(8);
    m_nicUacIcon = new QLabel;
    m_nicUacIcon->setFixedSize(14, 14);
    m_nicUacIcon->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_nicUacText = new QLabel;
    m_nicUacText->setObjectName(QStringLiteral("UacText"));
    m_nicUacText->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    nicUacLay->addWidget(m_nicUacIcon, 0, Qt::AlignVCenter);
    nicUacLay->addWidget(m_nicUacText, 0, Qt::AlignVCenter);

    nicHead->addWidget(nicTopo, 0, Qt::AlignVCenter);
    nicHead->addWidget(nicTitle, 0, Qt::AlignVCenter);
    nicHead->addStretch(1);
    nicHead->addWidget(m_nicUacBadge, 0, Qt::AlignVCenter);
    nicLay->addLayout(nicHead);

    auto *nicDesc = new QLabel(QStringLiteral(
        "支持向现有网卡快速追加辅助调试段 IP（例如 192.168.8.x），以便无缝连接不同网段的 ARM 嵌入式开发板，无需修改默认网关。"));
    nicDesc->setWordWrap(true);
    nicDesc->setStyleSheet(QStringLiteral("color:#94A3B8;font-size:12px;background:transparent;"));
    nicLay->addWidget(nicDesc);

    auto *formRow = new QHBoxLayout;
    formRow->setSpacing(16);
    auto *ipCol = new QVBoxLayout;
    ipCol->setSpacing(6);
    auto *ipLab = new QLabel(QStringLiteral("追加辅助 IP 地址:"));
    ipLab->setStyleSheet(QStringLiteral("color:#CBD5E1;font-size:12px;font-weight:700;background:transparent;"));
    m_newIpEdit = new QLineEdit(QStringLiteral("192.168.8.202"));
    m_newIpEdit->setObjectName(QStringLiteral("NicIpEdit"));
    m_newIpEdit->setFixedHeight(36);
    ipCol->addWidget(ipLab);
    ipCol->addWidget(m_newIpEdit);

    auto *maskCol = new QVBoxLayout;
    maskCol->setSpacing(6);
    auto *maskLab = new QLabel(QStringLiteral("子网掩码 (Subnet Mask):"));
    maskLab->setStyleSheet(QStringLiteral("color:#CBD5E1;font-size:12px;font-weight:700;background:transparent;"));
    m_newMaskEdit = new QLineEdit(QStringLiteral("255.255.255.0"));
    m_newMaskEdit->setObjectName(QStringLiteral("NicMaskEdit"));
    m_newMaskEdit->setFixedHeight(36);
    maskCol->addWidget(maskLab);
    maskCol->addWidget(m_newMaskEdit);

    auto *addNicBtn = new QPushButton(QStringLiteral("追加绑定 IP"));
    addNicBtn->setObjectName(QStringLiteral("AddNicBtn"));
    addNicBtn->setIcon(loadSvgIcon(QStringLiteral(":/icons/plus_add.svg"), 14));
    addNicBtn->setIconSize(QSize(14, 14));
    addNicBtn->setCursor(Qt::PointingHandCursor);
    addNicBtn->setFixedHeight(36);

    formRow->addLayout(ipCol, 4);
    formRow->addLayout(maskCol, 4);
    formRow->addWidget(addNicBtn, 0, Qt::AlignBottom);
    nicLay->addLayout(formRow);

    m_nicTable = new QTableWidget(0, 4);
    m_nicTable->setObjectName(QStringLiteral("NicTable"));
    m_nicTable->setHorizontalHeaderLabels(
        {QStringLiteral("网卡与名称"), QStringLiteral("绑定 IP 地址"), QStringLiteral("子网掩码"),
         QStringLiteral("操作")});
    m_nicTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_nicTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_nicTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_nicTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_nicTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_nicTable->setColumnWidth(3, 120);
    m_nicTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_nicTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_nicTable->setFocusPolicy(Qt::NoFocus);
    m_nicTable->setShowGrid(false);
    m_nicTable->setFrameShape(QFrame::NoFrame);
    m_nicTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_nicTable->verticalHeader()->setVisible(false);
    m_nicTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_nicTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_nicTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_nicTable->setMouseTracking(true);
    nicLay->addWidget(m_nicTable);
    mv->addWidget(nicCard);

    auto *footer = new QLabel(QStringLiteral("Windows x64 Native Qt/HTTP Core  ·  局域网千兆全双工  ·  开源项目"));
    footer->setObjectName(QStringLiteral("Muted"));
    mv->addWidget(footer);
    mv->addStretch(0);

    m_stack->addWidget(managerScroll);

    // ===== View 1: Portal preview =====
    auto *portalPage = new QWidget;
    auto *pv = new QVBoxLayout(portalPage);
    pv->setContentsMargins(24, 16, 24, 24);
    auto *backRow = new QHBoxLayout;
    auto *backBtn = new QPushButton(QStringLiteral("返回 Windows 客户端控制台"));
    backBtn->setObjectName(QStringLiteral("BackPortalBtn"));
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setIcon(makeBackArrowIcon(QColor(QStringLiteral("#67e8f9"))));
    backBtn->setIconSize(QSize(16, 16));
    backBtn->setFlat(false);
    backRow->addWidget(backBtn, 0, Qt::AlignVCenter);
    backRow->addStretch();
    auto *previewHint = new QLabel(QStringLiteral("提货专线预览 · 真实页面请用浏览器访问共享地址"));
    previewHint->setObjectName(QStringLiteral("Muted"));
    backRow->addWidget(previewHint, 0, Qt::AlignVCenter);
    pv->addLayout(backRow);

    auto *portalCard = makeCard(portalPage);
    auto *pl = new QVBoxLayout(portalCard);
    pl->setContentsMargins(20, 20, 20, 20);
    auto *ph = new QLabel(QStringLiteral("局域网极速文件共享与提货端"));
    ph->setObjectName(QStringLiteral("Title"));
    ph->setStyleSheet(QStringLiteral("font-size:20px;font-weight:700;"));
    m_portalHostLabel = new QLabel;
    m_portalHostLabel->setObjectName(QStringLiteral("Muted"));
    pl->addWidget(ph);
    pl->addWidget(m_portalHostLabel);
    m_portalTable = new QTableWidget(0, 3);
    m_portalTable->setIconSize(QSize(18, 18));
    m_portalTable->setHorizontalHeaderLabels(
        {QStringLiteral("文件名"), QStringLiteral("大小"), QStringLiteral("修改时间")});
    m_portalTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_portalTable->verticalHeader()->setVisible(false);
    m_portalTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pl->addWidget(m_portalTable, 1);
    auto *openReal = new QPushButton(QStringLiteral("在系统浏览器打开真实提货页"));
    openReal->setObjectName(QStringLiteral("Primary"));
    pl->addWidget(openReal, 0, Qt::AlignRight);
    pv->addWidget(portalCard, 1);
    m_stack->addWidget(portalPage);

    // ===== View 2: Logs =====
    auto *logsPage = new QWidget;
    auto *lv = new QVBoxLayout(logsPage);
    lv->setContentsMargins(24, 16, 24, 24);
    auto *logCard = makeCard(logsPage);
    auto *ll = new QVBoxLayout(logCard);
    ll->setContentsMargins(16, 16, 16, 16);
    auto *logHead = new QHBoxLayout;
    logHead->addWidget(new QLabel(QStringLiteral("HTTP 传输活动日志与连接监控")));
    logHead->addStretch();
    auto *clearLogBtn = new QPushButton(QStringLiteral("清空日志"));
    logHead->addWidget(clearLogBtn);
    ll->addLayout(logHead);
    m_logList = new QListWidget;
    ll->addWidget(m_logList, 1);
    lv->addWidget(logCard, 1);
    m_stack->addWidget(logsPage);

    // Toast
    m_toast = new QLabel(central);
    m_toast->setObjectName(QStringLiteral("Toast"));
    m_toast->hide();
    m_toast->setAttribute(Qt::WA_TransparentForMouseEvents);

    // 连接信号
    connect(m_toggleBtn, &QPushButton::clicked, this, &MainWindow::toggleServer);
    connect(browseBtn, &QPushButton::clicked, this, &MainWindow::browseFolder);
    connect(resetFolderBtn, &QPushButton::clicked, this, [this] {
        if (m_running) {
            showToast(QStringLiteral("请先停止共享服务再修改目录"));
            return;
        }
        const QString def = QDir::fromNativeSeparators(
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
            + QStringLiteral("/SharedFiles"));
        QDir().mkpath(def);
        m_shareRoot = def;
        m_folderEdit->setText(m_shareRoot);
        addLog(QStringLiteral("check"), QStringLiteral("已重设为默认共享目录: %1").arg(m_shareRoot));
        refreshFiles();
        showToast(QStringLiteral("已重设为默认共享目录"));
    });
    connect(copyBtn, &QPushButton::clicked, this, &MainWindow::copyShareUrl);
    connect(openBtn, &QPushButton::clicked, this, &MainWindow::openPortalInBrowser);
    connect(openReal, &QPushButton::clicked, this, &MainWindow::openPortalInBrowser);
    connect(selfCheckBtn, &QPushButton::clicked, this, &MainWindow::selfCheck);
    connect(uploadBtn, &QPushButton::clicked, this, &MainWindow::uploadLocalFiles);
    connect(addNicBtn, &QPushButton::clicked, this, &MainWindow::addNicIp);
    connect(clearLogBtn, &QPushButton::clicked, this, &MainWindow::clearLogs);
    connect(backBtn, &QPushButton::clicked, this, [this] { switchView(0); });
    connect(m_ipCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onIpSelectionChanged);
    connect(m_fileFilter, &QLineEdit::textChanged, this, &MainWindow::refreshFiles);
    connect(m_portSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { updateShareUrlUi(); });
    connect(m_folderEdit, &QLineEdit::editingFinished, this, [this] {
        m_shareRoot = QDir::fromNativeSeparators(m_folderEdit->text().trimmed());
    });
}

void MainWindow::switchView(int index)
{
    m_stack->setCurrentIndex(index);
    updateTabChrome(index);
    if (index == 1)
        refreshFiles();
}

void MainWindow::updateTabChrome(int index)
{
    m_tabManager->setObjectName(index == 0 ? QStringLiteral("TabActive") : QStringLiteral("Tab"));
    m_tabPortal->setObjectName(index == 1 ? QStringLiteral("TabActiveCyan") : QStringLiteral("Tab"));
    m_tabLogs->setObjectName(index == 2 ? QStringLiteral("TabActiveIndigo") : QStringLiteral("Tab"));
    m_tabManager->setIcon(makeTabIcon(0, QColor(index == 0 ? QStringLiteral("#6ee7b7") : QStringLiteral("#94a3b8"))));
    m_tabPortal->setIcon(makeTabIcon(1, QColor(index == 1 ? QStringLiteral("#67e8f9") : QStringLiteral("#94a3b8"))));
    m_tabLogs->setIcon(makeTabIcon(2, QColor(index == 2 ? QStringLiteral("#a5b4fc") : QStringLiteral("#94a3b8"))));
    for (auto *b : {m_tabManager, m_tabPortal, m_tabLogs}) {
        b->style()->unpolish(b);
        b->style()->polish(b);
    }
}

void MainWindow::fitTableHeight(QTableWidget *table)
{
    if (!table)
        return;
    int h = table->horizontalHeader()->height() + 2;
    const int rows = table->rowCount();
    if (rows == 0) {
        h += 48;
    } else {
        for (int i = 0; i < rows; ++i)
            h += qMax(table->rowHeight(i), 28);
    }
    // 边框余量
    h += 4;
    table->setFixedHeight(h);
}

void MainWindow::updateMaxButtonIcon()
{
    if (!m_maxBtn)
        return;
    m_maxBtn->setIcon(makeWinChromeIcon(isMaximized() ? 3 : 1));
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange)
        updateMaxButtonIcon();
}

void MainWindow::applyUacChip(QWidget *badge, QLabel *icon, QLabel *text, bool elevated, bool nicStyle)
{
    if (!badge || !icon || !text)
        return;
    icon->setPixmap(loadSvgPixmap(
        elevated ? QStringLiteral(":/icons/shield_check.svg") : QStringLiteral(":/icons/shield_off.svg"), 14));
    if (nicStyle) {
        badge->setObjectName(elevated ? QStringLiteral("NicUac") : QStringLiteral("NicUacOff"));
        text->setText(elevated ? QStringLiteral("Windows UAC 授权已生效")
                               : QStringLiteral("Windows UAC 授权管理"));
    } else {
        badge->setObjectName(elevated ? QStringLiteral("Uac") : QStringLiteral("UacOff"));
        text->setText(elevated ? QStringLiteral("UAC 已授权") : QStringLiteral("UAC 未授权 · 点击提权"));
    }
    badge->setToolTip(elevated ? QStringLiteral("当前进程已通过 Windows UAC 提权（TokenElevation=1）")
                               : QStringLiteral("当前为标准权限。点击将弹出系统 UAC，同意后以管理员重启。"));
    badge->style()->unpolish(badge);
    badge->style()->polish(badge);
}

void MainWindow::updateUacBadge()
{
    const bool elevated = NicManager::isElevated();
    applyUacChip(m_uacBadge, m_uacIcon, m_uacText, elevated, false);
    applyUacChip(m_nicUacBadge, m_nicUacIcon, m_nicUacText, elevated, true);
}

void MainWindow::requestUacElevation()
{
    if (NicManager::isElevated()) {
        updateUacBadge();
        showToast(QStringLiteral("当前已是管理员权限（真实 UAC 状态）"));
        return;
    }
    const auto ret = QMessageBox::question(
        this, QStringLiteral("请求管理员权限"),
        QStringLiteral("追加/解绑网卡 IP 需要管理员权限。\n"
                       "将弹出 Windows UAC 对话框，同意后以管理员身份重新启动本程序。\n\n"
                       "是否继续？"));
    if (ret != QMessageBox::Yes)
        return;
    QString err;
    if (NicManager::requestElevation(&err)) {
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
    } else {
        showToast(err.isEmpty() ? QStringLiteral("提权失败") : err);
        updateUacBadge();
    }
}

QString MainWindow::selectedIp() const
{
    return m_ipCombo->currentData().toString().isEmpty() ? m_ipCombo->currentText()
                                                         : m_ipCombo->currentData().toString();
}

QString MainWindow::currentShareUrl() const
{
    return QStringLiteral("http://%1:%2").arg(selectedIp()).arg(m_portSpin->value());
}

void MainWindow::updateShareUrlUi()
{
    if (m_loopbackLabel)
        m_loopbackLabel->setText(QStringLiteral("本机 Loopback: http://127.0.0.1:%1").arg(m_portSpin->value()));

    if (m_running) {
        const QString url = currentShareUrl();
        m_urlLabel->setText(url);
        m_qr->setText(url);
        m_qrUrlLabel->setText(url);
        m_portalHostLabel->setText(QStringLiteral("宿主机节点: %1").arg(url));
    } else {
        m_urlLabel->setText(QStringLiteral("— (服务未启动)"));
        m_qr->setText({});
        m_qrUrlLabel->clear();
        m_portalHostLabel->setText(QStringLiteral("服务未启动"));
    }
}

void MainWindow::setRunningUi(bool running)
{
    m_running = running;
    m_portSpin->setEnabled(!running);
    m_folderEdit->setEnabled(!running);
    if (running) {
        m_toggleBtn->setText(QStringLiteral("停止 HTTP 共享"));
        m_toggleBtn->setObjectName(QStringLiteral("Danger"));
        m_toggleBtn->setIcon(loadSvgIcon(QStringLiteral(":/icons/power_icon.svg"), 16));
        m_statusPill->setRunning(true);
    } else {
        m_toggleBtn->setText(QStringLiteral("一键启动 HTTP 共享"));
        m_toggleBtn->setObjectName(QStringLiteral("Primary"));
        m_toggleBtn->setIcon(loadSvgIcon(QStringLiteral(":/icons/power_icon_white.svg"), 16));
        m_statusPill->setRunning(false);
    }
    m_toggleBtn->setIconSize(QSize(16, 16));
    m_toggleBtn->style()->unpolish(m_toggleBtn);
    m_toggleBtn->style()->polish(m_toggleBtn);
    m_toggleBtn->updateGeometry();
    if (m_runDot)
        m_runDot->setVisible(running);
    updateShareUrlUi();
}

void MainWindow::addLog(const QString &type, const QString &msg)
{
    m_logs->prepend(type, msg);
    m_logList->insertItem(0, QStringLiteral("[%1] %2")
                                 .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), msg));
}

void MainWindow::showToast(const QString &msg)
{
    m_toast->setText(msg);
    m_toast->adjustSize();
    m_toast->move(width() - m_toast->width() - 24, height() - m_toast->height() - 24);
    m_toast->show();
    m_toast->raise();
    QTimer::singleShot(2800, m_toast, &QWidget::hide);
}

void MainWindow::toggleServer()
{
    if (m_running) {
        m_server->stop();
        return;
    }
    m_shareRoot = QDir::fromNativeSeparators(m_folderEdit->text().trimmed());
    if (m_shareRoot.isEmpty()) {
        showToast(QStringLiteral("请先设置共享目录"));
        return;
    }
    if (!m_server->start(m_shareRoot, quint16(m_portSpin->value())))
        return;
}

void MainWindow::onServerStarted(quint16 port)
{
    Q_UNUSED(port);
    setRunningUi(true);
    if (!m_watcher->directories().contains(m_shareRoot))
        m_watcher->addPath(m_shareRoot);
    addLog(QStringLiteral("start"),
           QStringLiteral("HTTP 共享服务进程启动，成功绑定 0.0.0.0:%1").arg(m_portSpin->value()));
    addLog(QStringLiteral("check"), QStringLiteral("挂载本地共享目录: %1 (就绪)").arg(m_shareRoot));
    showToast(QStringLiteral("HTTP 共享服务已启动: %1").arg(currentShareUrl()));
    refreshFiles();
}

void MainWindow::onServerStopped()
{
    setRunningUi(false);
    addLog(QStringLiteral("stop"), QStringLiteral("HTTP 共享服务已停止 (端口释放)"));
    showToast(QStringLiteral("HTTP 共享服务已关闭"));
}

void MainWindow::onDownload(const QString &ip, const QString &name, qint64 size)
{
    m_downloadCounts[name] = m_downloadCounts.value(name, 0) + 1;
    addLog(QStringLiteral("download"),
           QStringLiteral("%1 下载: %2 (%3)").arg(ip, name, fmtBytes(size)));
    refreshFiles();
}

void MainWindow::onUpload(const QString &ip, const QString &name, qint64 size)
{
    addLog(QStringLiteral("upload"),
           QStringLiteral("%1 上传: %2 (%3)").arg(ip, name, fmtBytes(size)));
    refreshFiles();
}

void MainWindow::browseFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择共享目录"), m_shareRoot);
    if (dir.isEmpty())
        return;
    m_shareRoot = QDir::fromNativeSeparators(dir);
    m_folderEdit->setText(m_shareRoot);
    addLog(QStringLiteral("check"), QStringLiteral("已选择新的本地共享目录: %1").arg(m_shareRoot));
    refreshFiles();
}

void MainWindow::copyShareUrl()
{
    if (!m_running) {
        showToast(QStringLiteral("请先启动共享服务"));
        return;
    }
    QApplication::clipboard()->setText(currentShareUrl());
    addLog(QStringLiteral("check"), QStringLiteral("已复制共享地址: %1").arg(currentShareUrl()));
    showToast(QStringLiteral("已复制共享地址"));
}

void MainWindow::openPortalInBrowser()
{
    if (!m_running) {
        showToast(QStringLiteral("请先启动共享服务"));
        return;
    }
    QDesktopServices::openUrl(QUrl(currentShareUrl()));
}

void MainWindow::selfCheck()
{
    SelfCheckDialog dlg(m_running, static_cast<quint16>(m_portSpin->value()), selectedIp(), this);
    dlg.exec();
    if (m_running)
        addLog(QStringLiteral("check"), QStringLiteral("已执行网络与环境自检"));
}

void MainWindow::refreshNics()
{
    const QString keepIp = selectedIp();
    const QString httpIp = keepIp;
    const auto nics = NicManager::enumerate();
    m_ipCombo->blockSignals(true);
    m_ipCombo->clear();
    m_nicTable->setRowCount(0);

    int row = 0;
    for (const NicEntry &n : nics) {
        m_ipCombo->addItem(QStringLiteral("%1 (%2)").arg(n.name, n.ip), n.ip);

        m_nicTable->insertRow(row);
        m_nicTable->setRowHeight(row, 46);

        auto *meta = new QTableWidgetItem;
        meta->setData(Qt::UserRole, n.ip);
        meta->setData(Qt::UserRole + 1, n.isPrimary);
        meta->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_nicTable->setItem(row, 0, meta);

        QString displayName = n.name;
        if (n.isPrimary && !displayName.contains(QStringLiteral("[Primary]")))
            displayName = QStringLiteral("%1 [Primary]").arg(displayName);

        const bool httpActive = (!httpIp.isEmpty() && httpIp == n.ip);
        m_nicTable->setCellWidget(row, 0, makeNicNameCell(displayName, n.isPrimary, httpActive));
        m_nicTable->setCellWidget(row, 1, makeNicIpCell(n.ip, n.isPrimary));

        auto *maskItem = new QTableWidgetItem(n.subnet);
        maskItem->setForeground(QColor(QStringLiteral("#94A3B8")));
        QFont mf = maskItem->font();
        mf.setFamily(QStringLiteral("Consolas"));
        maskItem->setFont(mf);
        maskItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        maskItem->setData(Qt::UserRole, n.ip);
        maskItem->setData(Qt::UserRole + 1, n.isPrimary);
        m_nicTable->setItem(row, 2, maskItem);

        m_nicTable->setCellWidget(row, 3, makeNicActionCell(n.ip, n.isPrimary));
        ++row;
    }
    m_ipCombo->blockSignals(false);

    int idx = m_ipCombo->findData(keepIp);
    if (idx < 0)
        idx = 0;
    if (m_ipCombo->count() > 0)
        m_ipCombo->setCurrentIndex(idx);

    m_nicTable->disconnect();
    connect(m_nicTable, &QTableWidget::cellClicked, this, [this](int r, int) {
        if (r < 0)
            return;
        auto *it = m_nicTable->item(r, 0);
        if (!it)
            it = m_nicTable->item(r, 2);
        if (!it)
            return;
        const QString ip = it->data(Qt::UserRole).toString();
        const int cidx = m_ipCombo->findData(ip);
        if (cidx >= 0 && cidx != m_ipCombo->currentIndex())
            m_ipCombo->setCurrentIndex(cidx);
    });

    updateShareUrlUi();
    fitTableHeight(m_nicTable);
}

QString MainWindow::adapterNameForIp(const QString &ip) const
{
    for (const NicEntry &n : NicManager::enumerate()) {
        if (n.ip == ip) {
            QString name = n.name;
            name.remove(QStringLiteral(" [Primary]"));
            return name;
        }
    }
    // 退回：用当前枚举第一张网卡名
    const auto nics = NicManager::enumerate();
    if (nics.isEmpty())
        return {};
    QString name = nics.first().name;
    name.remove(QStringLiteral(" [Primary]"));
    return name;
}

void MainWindow::addNicIp()
{
    if (!NicManager::isElevated()) {
        QMessageBox::warning(this, QStringLiteral("需要管理员权限"),
                             QStringLiteral("追加网卡 IP 需要以管理员身份运行本程序（UAC）。"));
        return;
    }
    const QString ip = m_newIpEdit->text().trimmed();
    const QString mask = m_newMaskEdit->text().trimmed();
    const QString adapter = adapterNameForIp(selectedIp());
    if (adapter.isEmpty()) {
        showToast(QStringLiteral("未找到可用网卡"));
        return;
    }
    QString err;
    if (!NicManager::addAddress(adapter, ip, mask, &err)) {
        QMessageBox::warning(this, QStringLiteral("追加失败"), err.isEmpty() ? QStringLiteral("netsh 失败") : err);
        return;
    }
    addLog(QStringLiteral("nic_add"), QStringLiteral("网卡追加辅助 IP: %1 / %2").arg(ip, mask));
    showToast(QStringLiteral("已追加绑定: %1").arg(ip));
    refreshNics();
}

void MainWindow::deleteSelectedNic()
{
    const auto ranges = m_nicTable->selectedRanges();
    if (ranges.isEmpty())
        return;
    const int r = ranges.first().topRow();
    auto *it = m_nicTable->item(r, 0);
    if (!it)
        it = m_nicTable->item(r, 2);
    if (!it)
        return;
    if (it->data(Qt::UserRole + 1).toBool()) {
        showToast(QStringLiteral("主网卡受保护，不能解绑"));
        return;
    }
    deleteNicIp(it->data(Qt::UserRole).toString());
}

void MainWindow::deleteNicIp(const QString &ip)
{
    if (ip.isEmpty())
        return;
    if (!NicManager::isElevated()) {
        QMessageBox::warning(this, QStringLiteral("需要管理员权限"),
                             QStringLiteral("解绑 IP 需要以管理员身份运行本程序。"));
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("确认"),
                              QStringLiteral("确认解绑辅助 IP: %1 ?").arg(ip))
        != QMessageBox::Yes)
        return;
    QString err;
    if (!NicManager::removeAddress(ip, &err)) {
        QMessageBox::warning(this, QStringLiteral("解绑失败"), err);
        return;
    }
    addLog(QStringLiteral("nic_del"), QStringLiteral("已解绑辅助调试 IP: %1").arg(ip));
    refreshNics();
}

void MainWindow::refreshFiles()
{
    const QString root = QDir::fromNativeSeparators(m_folderEdit->text().trimmed());
    QDir dir(root.isEmpty() ? m_shareRoot : root);
    const QString filter = m_fileFilter ? m_fileFilter->text().trimmed() : QString();

    const auto infos = dir.entryInfoList(QDir::Files | QDir::Readable, QDir::Time);
    m_fileTable->setRowCount(0);
    m_portalTable->setRowCount(0);
    qint64 total = 0;
    int shown = 0;
    QString priorityPath;
    QString priorityName;
    qint64 prioritySize = 0;

    for (const QFileInfo &fi : infos) {
        if (!filter.isEmpty() && !fi.fileName().contains(filter, Qt::CaseInsensitive))
            continue;
        total += fi.size();
        const int r = m_fileTable->rowCount();
        m_fileTable->insertRow(r);
        m_fileTable->setRowHeight(r, 50);

        auto *pathItem = new QTableWidgetItem;
        pathItem->setData(Qt::UserRole, fi.absoluteFilePath());
        pathItem->setData(Qt::UserRole + 1, fi.fileName());
        pathItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_fileTable->setItem(r, 0, pathItem);
        m_fileTable->setCellWidget(r, 0, makeFileNameCell(fi.fileName()));

        auto *sizeItem = new QTableWidgetItem(fmtBytes(fi.size()));
        sizeItem->setForeground(QColor(QStringLiteral("#38BDF8")));
        QFont sizeFont = sizeItem->font();
        sizeFont.setBold(true);
        sizeItem->setFont(sizeFont);
        sizeItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_fileTable->setItem(r, 1, sizeItem);

        auto *timeItem = new QTableWidgetItem(
            fi.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        timeItem->setForeground(QColor(QStringLiteral("#94A3B8")));
        timeItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_fileTable->setItem(r, 2, timeItem);

        const int dlCount = m_downloadCounts.value(fi.fileName(), 0);
        m_fileTable->setCellWidget(r, 3, makeDownloadCountCell(dlCount));
        m_fileTable->setCellWidget(r, 4, makeFileActionBar(fi.absoluteFilePath(), fi.fileName()));

        const int pr = m_portalTable->rowCount();
        m_portalTable->insertRow(pr);
        auto *portalName = new QTableWidgetItem(fi.fileName());
        portalName->setIcon(fileTypeIcon(fi.fileName()));
        m_portalTable->setItem(pr, 0, portalName);
        m_portalTable->setItem(pr, 1, new QTableWidgetItem(fmtBytes(fi.size())));
        m_portalTable->setItem(pr, 2, new QTableWidgetItem(fi.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));

        if (priorityPath.isEmpty() || isArchiveName(fi.fileName())) {
            priorityPath = fi.absoluteFilePath();
            priorityName = fi.fileName();
            prioritySize = fi.size();
        }
        ++shown;
    }

    m_fileStats->setText(QStringLiteral(
                             "<span style='color:#00d2ff'>%1</span>"
                             "<span style='color:#7dd3fc'> 个文件</span>"
                             "<span style='color:#7dd3fc'> · </span>"
                             "<span style='color:#00d2ff'>%2</span>")
                             .arg(shown)
                             .arg(fmtBytes(total)));
    if (m_filePathLabel)
        m_filePathLabel->setText(QStringLiteral("当前路径: %1").arg(dir.absolutePath()));
    updatePriorityPickup(priorityPath, priorityName, prioritySize);
    if (m_portalCount)
        m_portalCount->setText(QString::number(shown));
    fitTableHeight(m_fileTable);
}

void MainWindow::updatePriorityPickup(const QString &path, const QString &name, qint64 size)
{
    m_priorityPath = path;
    const bool has = !path.isEmpty();
    if (m_priorityEmpty)
        m_priorityEmpty->setVisible(!has);
    if (m_priorityCard)
        m_priorityCard->setVisible(has);
    if (!has)
        return;
    m_priorityName->setText(name);
    m_prioritySize->setText(fmtBytes(size));
    m_priorityIcon->setPixmap(loadSvgPixmap(QStringLiteral(":/icons/box_cardboard.svg"), 22));
}

QWidget *MainWindow::makeNicNameCell(const QString &name, bool isPrimary, bool isHttpActive)
{
    auto *w = new QWidget;
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(12, 0, 8, 0);
    lay->setSpacing(8);

    auto *dot = new QLabel(QStringLiteral("●"));
    dot->setStyleSheet(isPrimary ? QStringLiteral("color:#10B981;font-size:11px;background:transparent;")
                                 : QStringLiteral("color:#64748B;font-size:10px;background:transparent;"));

    auto *text = new QLabel(name);
    text->setStyleSheet(isPrimary
                            ? QStringLiteral("color:#FFFFFF;font-weight:700;font-size:13px;background:transparent;")
                            : QStringLiteral("color:#E2E8F0;font-size:13px;background:transparent;"));

    lay->addWidget(dot, 0, Qt::AlignVCenter);
    lay->addWidget(text, 0, Qt::AlignVCenter);
    if (isHttpActive) {
        auto *httpTag = new QLabel(QStringLiteral("HTTP 当前选用"));
        httpTag->setStyleSheet(QStringLiteral(
            "QLabel {"
            "  background-color: rgba(6, 182, 212, 0.15);"
            "  color: #38BDF8;"
            "  border: 1px solid rgba(56, 189, 248, 0.5);"
            "  border-radius: 4px;"
            "  font-size: 11px;"
            "  font-weight: 500;"
            "  padding: 1px 6px;"
            "}"));
        lay->addWidget(httpTag, 0, Qt::AlignVCenter);
    }
    lay->addStretch(1);
    return w;
}

QWidget *MainWindow::makeNicIpCell(const QString &ip, bool isPrimary)
{
    auto *w = new QWidget;
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(8, 0, 8, 0);
    lay->setSpacing(8);
    if (isPrimary) {
        auto *badge = new QLabel(QStringLiteral("Primary"));
        badge->setStyleSheet(QStringLiteral(
            "QLabel {"
            "  background-color: #06182C;"
            "  color: #38BDF8;"
            "  border: 1px solid #0E3A5F;"
            "  border-radius: 4px;"
            "  font-size: 11px;"
            "  padding: 1px 6px;"
            "}"));
        lay->addWidget(badge, 0, Qt::AlignVCenter);
    }
    auto *ipText = new QLabel(ip);
    ipText->setStyleSheet(QStringLiteral(
        "color:#38BDF8;font-family:Consolas,'Cascadia Mono',monospace;"
        "font-size:13px;font-weight:700;background:transparent;"));
    lay->addWidget(ipText, 0, Qt::AlignVCenter);
    lay->addStretch(1);
    return w;
}

QWidget *MainWindow::makeNicActionCell(const QString &ip, bool isPrimary)
{
    auto *w = new QWidget;
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(4, 0, 10, 0);
    lay->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (isPrimary) {
        auto *lockIcon = new QLabel;
        lockIcon->setFixedSize(14, 14);
        lockIcon->setPixmap(loadSvgPixmap(QStringLiteral(":/icons/lock_protected.svg"), 14));
        auto *lab = new QLabel(QStringLiteral("主网卡保护"));
        lab->setStyleSheet(QStringLiteral("color:#64748B;font-size:12px;background:transparent;"));
        lay->addWidget(lockIcon, 0, Qt::AlignVCenter);
        lay->addWidget(lab, 0, Qt::AlignVCenter);
    } else {
        auto *delBtn = new QPushButton;
        delBtn->setObjectName(QStringLiteral("FileActDel"));
        delBtn->setFixedSize(28, 28);
        delBtn->setIcon(loadSvgIcon(QStringLiteral(":/icons/trash_delete.svg"), 15));
        delBtn->setIconSize(QSize(15, 15));
        delBtn->setCursor(Qt::PointingHandCursor);
        delBtn->setToolTip(QStringLiteral("删除该绑定 IP"));
        connect(delBtn, &QPushButton::clicked, this, [this, ip] { deleteNicIp(ip); });
        lay->addWidget(delBtn);
    }
    return w;
}

QWidget *MainWindow::makeFileNameCell(const QString &fileName)
{
    auto *w = new QWidget;
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(12, 0, 8, 0);
    lay->setSpacing(8);

    auto *icon = new QLabel;
    icon->setFixedSize(18, 18);
    icon->setPixmap(fileTypeIcon(fileName).pixmap(18, 18));
    icon->setStyleSheet(QStringLiteral("background:transparent;"));

    auto *name = new QLabel(fileName);
    name->setStyleSheet(QStringLiteral(
        "color:#F1F5F9;font-size:13px;font-weight:500;background:transparent;"));
    name->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *badge = new QLabel(QStringLiteral("Build"));
    badge->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background-color:#111E30;"
        "  color:#94A3B8;"
        "  font-size:11px;"
        "  border:1px solid #1E3A5F;"
        "  border-radius:4px;"
        "  padding:1px 6px;"
        "}"));

    lay->addWidget(icon, 0, Qt::AlignVCenter);
    lay->addWidget(name, 0, Qt::AlignVCenter);
    lay->addWidget(badge, 0, Qt::AlignVCenter);
    lay->addStretch(1);
    return w;
}

QWidget *MainWindow::makeDownloadCountCell(int count)
{
    auto *w = new QWidget;
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(10, 0, 4, 0);
    lay->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto *badge = new QLabel(QStringLiteral("%1 次").arg(count));
    badge->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background-color:#081424;"
        "  color:#94A3B8;"
        "  font-size:12px;"
        "  border:1px solid #122842;"
        "  border-radius:4px;"
        "  padding:3px 8px;"
        "}"));
    lay->addWidget(badge);
    return w;
}

QWidget *MainWindow::makeFileActionBar(const QString &path, const QString &name)
{
    auto *bar = new QWidget;
    auto *lay = new QHBoxLayout(bar);
    lay->setContentsMargins(4, 0, 10, 0);
    lay->setSpacing(6);
    lay->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto *cliBtn = new QPushButton;
    cliBtn->setObjectName(QStringLiteral("FileActCli"));
    cliBtn->setIcon(loadSvgIcon(QStringLiteral(":/icons/terminal_cli.svg"), 15));
    cliBtn->setIconSize(QSize(15, 15));
    cliBtn->setCursor(Qt::PointingHandCursor);
    cliBtn->setToolTip(QStringLiteral("复制 curl/wget 终端提货命令"));

    auto *dlBtn = new QPushButton(QStringLiteral(" 下载"));
    dlBtn->setObjectName(QStringLiteral("FileActDl"));
    dlBtn->setIcon(loadSvgIcon(QStringLiteral(":/icons/download_icon.svg"), 14));
    dlBtn->setIconSize(QSize(14, 14));
    dlBtn->setCursor(Qt::PointingHandCursor);
    dlBtn->setToolTip(QStringLiteral("直接下载此文件"));

    auto *delBtn = new QPushButton;
    delBtn->setObjectName(QStringLiteral("FileActDel"));
    delBtn->setIcon(loadSvgIcon(QStringLiteral(":/icons/trash_delete.svg"), 14));
    delBtn->setIconSize(QSize(14, 14));
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setToolTip(QStringLiteral("删除文件"));

    lay->addWidget(cliBtn);
    lay->addWidget(dlBtn);
    lay->addWidget(delBtn);

    connect(cliBtn, &QPushButton::clicked, this, [this, name] { copyCurlForName(name); });
    connect(dlBtn, &QPushButton::clicked, this, [this, path, name] { downloadFileByPath(path, name); });
    connect(delBtn, &QPushButton::clicked, this, [this, path, name] { deleteFileByPath(path, name); });
    return bar;
}

void MainWindow::uploadLocalFiles()
{
    const QStringList paths = QFileDialog::getOpenFileNames(this, QStringLiteral("选择要加入共享的文件"));
    importFilesToShare(paths);
}

void MainWindow::importFilesToShare(const QStringList &paths)
{
    if (paths.isEmpty())
        return;
    const QString root = QDir::fromNativeSeparators(m_folderEdit->text().trimmed());
    if (root.isEmpty()) {
        showToast(QStringLiteral("请先设置共享目录"));
        return;
    }
    QDir().mkpath(root);
    int n = 0;
    for (const QString &p : paths) {
        const QFileInfo fi(p);
        if (!fi.isFile())
            continue;
        const QString dest = QDir(root).filePath(fi.fileName());
        if (QFile::exists(dest))
            QFile::remove(dest);
        if (QFile::copy(p, dest))
            ++n;
    }
    if (n <= 0) {
        showToast(QStringLiteral("没有可加入的文件"));
        return;
    }
    addLog(QStringLiteral("upload"), QStringLiteral("本地添加了 %1 个文件到共享目录").arg(n));
    showToast(QStringLiteral("成功添加 %1 个文件").arg(n));
    refreshFiles();
}

void MainWindow::downloadSelectedFile()
{
    const auto items = m_fileTable->selectedItems();
    if (items.isEmpty())
        return;
    const int r = items.first()->row();
    auto *it = m_fileTable->item(r, 0);
    if (!it)
        return;
    downloadFileByPath(it->data(Qt::UserRole).toString(),
                       it->data(Qt::UserRole + 1).toString());
}

void MainWindow::downloadFileByPath(const QString &src, const QString &name)
{
    if (src.isEmpty())
        return;
    const QString dest = QFileDialog::getSaveFileName(this, QStringLiteral("保存文件"), name);
    if (dest.isEmpty())
        return;
    if (QFile::exists(dest))
        QFile::remove(dest);
    if (QFile::copy(src, dest)) {
        m_downloadCounts[name] = m_downloadCounts.value(name, 0) + 1;
        addLog(QStringLiteral("download"), QStringLiteral("本机导出: %1").arg(name));
        showToast(QStringLiteral("已保存"));
        refreshFiles();
    }
}

void MainWindow::deleteSelectedFile()
{
    const auto items = m_fileTable->selectedItems();
    if (items.isEmpty())
        return;
    const int r = items.first()->row();
    auto *it = m_fileTable->item(r, 0);
    if (!it)
        return;
    deleteFileByPath(it->data(Qt::UserRole).toString(),
                     it->data(Qt::UserRole + 1).toString());
}

void MainWindow::deleteFileByPath(const QString &path, const QString &name)
{
    if (path.isEmpty())
        return;
    if (QMessageBox::question(this, QStringLiteral("确认"),
                              QStringLiteral("确定从共享文件夹移除 %1 吗？").arg(name))
        != QMessageBox::Yes)
        return;
    if (QFile::remove(path)) {
        m_downloadCounts.remove(name);
        addLog(QStringLiteral("stop"), QStringLiteral("已从共享列表移除: %1").arg(name));
        refreshFiles();
    }
}

void MainWindow::copyCurlForSelected()
{
    const auto items = m_fileTable->selectedItems();
    if (items.isEmpty())
        return;
    auto *it = m_fileTable->item(items.first()->row(), 0);
    if (!it)
        return;
    copyCurlForName(it->data(Qt::UserRole + 1).toString());
}

void MainWindow::copyCurlForName(const QString &name)
{
    const QString cmd = QStringLiteral("curl -O \"%1/download/%2\"")
                            .arg(currentShareUrl(), QString::fromUtf8(QUrl::toPercentEncoding(name)));
    QApplication::clipboard()->setText(cmd);
    addLog(QStringLiteral("check"), QStringLiteral("已复制 curl: %1").arg(cmd));
    showToast(QStringLiteral("已复制 curl 命令"));
}

void MainWindow::clearLogs()
{
    m_logs->clear();
    m_logList->clear();
}

void MainWindow::onIpSelectionChanged(int)
{
    updateShareUrlUi();
    // ponytail: 全表重刷以更新「HTTP 当前选用」；用标志防重入
    static bool reenter = false;
    if (reenter || !m_nicTable)
        return;
    reenter = true;
    refreshNics();
    reenter = false;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_uacBadge || watched == m_nicUacBadge)
        && event->type() == QEvent::MouseButtonRelease) {
        requestUacElevation();
        return true;
    }

    if (watched == m_titleBar) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (event->type() == QEvent::MouseButtonDblClick && me->button() == Qt::LeftButton) {
            if (isMaximized())
                showNormal();
            else
                showMaximized();
            updateMaxButtonIcon();
            return true;
        }
        if (event->type() == QEvent::MouseButtonPress && me->button() == Qt::LeftButton) {
            QWidget *hit = m_titleBar->childAt(me->pos());
            for (QWidget *w = hit; w && w != m_titleBar; w = w->parentWidget()) {
                if (qobject_cast<QPushButton *>(w) || qobject_cast<QToolButton *>(w)
                    || w->objectName() == QLatin1String("TabStrip")
                    || w->objectName() == QLatin1String("PortalCount")
                    || w->objectName() == QLatin1String("RunDot")
                    || w->objectName() == QLatin1String("Uac")
                    || w->objectName() == QLatin1String("UacOff")
                    || w->objectName() == QLatin1String("UacText")
                    || w->objectName() == QLatin1String("NicUac")
                    || w->objectName() == QLatin1String("NicUacOff"))
                    return QMainWindow::eventFilter(watched, event);
            }
            m_dragging = true;
            m_dragPos = me->globalPosition().toPoint() - frameGeometry().topLeft();
            return true;
        }
        if (event->type() == QEvent::MouseMove && m_dragging) {
            if (!isMaximized())
                move(me->globalPosition().toPoint() - m_dragPos);
            return true;
        }
        if (event->type() == QEvent::MouseButtonRelease)
            m_dragging = false;
    }
    return QMainWindow::eventFilter(watched, event);
}
