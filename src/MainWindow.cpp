#include "MainWindow.h"

#include "HttpFileServer.h"
#include "NicManager.h"
#include "ActivityLogModel.h"
#include "QrCodeWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
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
#include <QFrame>
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

static QIcon makeWinChromeIcon(int kind)
{
    // 0 最小化  1 最大化  2 关闭 — 对齐 Win11 线框风格
    QPixmap pm(46, 32);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, false);
    const QColor c(203, 213, 225); // slate-300
    p.setPen(QPen(c, 1));
    if (kind == 0) {
        p.drawLine(17, 16, 29, 16);
    } else if (kind == 1) {
        p.drawRect(17, 10, 12, 12);
    } else {
        p.drawLine(17, 10, 29, 22);
        p.drawLine(29, 10, 17, 22);
    }
    return QIcon(pm);
}

static QToolButton *makeWinChromeBtn(QWidget *parent, int kind)
{
    auto *b = new QToolButton(parent);
    b->setIcon(makeWinChromeIcon(kind));
    b->setIconSize(QSize(46, 32));
    b->setFixedSize(46, 32);
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
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (kind == 0) {
        // 滑块/控制面板
        p.drawLine(3, 4, 13, 4);
        p.drawLine(3, 8, 13, 8);
        p.drawLine(3, 12, 13, 12);
        p.setBrush(color);
        p.drawEllipse(QPointF(6, 4), 2, 2);
        p.drawEllipse(QPointF(11, 8), 2, 2);
        p.drawEllipse(QPointF(7, 12), 2, 2);
    } else if (kind == 1) {
        // 手机
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(5, 1.5, 6, 13), 1.5, 1.5);
        p.drawLine(7, 12.5, 9, 12.5);
    } else {
        // 活动波形
        p.drawPolyline(QPolygonF({QPointF(1, 10), QPointF(4, 10), QPointF(6, 4), QPointF(8, 13),
                                  QPointF(10, 7), QPointF(12, 10), QPointF(15, 10)}));
    }
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
  background: #070e1a;
  color: #e2e8f0;
  font-family: "Segoe UI", "Microsoft YaHei UI", sans-serif;
  font-size: 12px;
}
QFrame#Card {
  background: #0b1424;
  border: 1px solid #1b2b46;
  border-radius: 12px;
}
QLineEdit, QSpinBox, QComboBox {
  background: #070d18;
  border: 1px solid #1f324f;
  border-radius: 8px;
  padding: 6px 8px;
  color: #e2e8f0;
  selection-background-color: #059669;
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
}
QPushButton#Danger {
  background: #4c0519;
  border: 1px solid #fb7185;
  color: #fecdd3;
  font-weight: 700;
}
QFrame#TabStrip QPushButton {
  background: transparent;
  border: 2px solid transparent;
  border-radius: 6px;
  padding: 0px 12px;
  margin: 0px;
  min-height: 0px;
  max-height: 28px;
  color: #94a3b8;
  font-size: 12px;
  font-weight: 600;
}
QFrame#TabStrip QPushButton#TabActive {
  background: rgba(6, 78, 59, 0.55);
  border: 2px solid #34d399;
  color: #6ee7b7;
}
QFrame#TabStrip QPushButton#TabActiveCyan {
  background: rgba(8, 51, 68, 0.65);
  border: 2px solid #22d3ee;
  color: #67e8f9;
}
QFrame#TabStrip QPushButton#TabActiveIndigo {
  background: rgba(49, 46, 129, 0.55);
  border: 2px solid #818cf8;
  color: #a5b4fc;
}
QFrame#TabStrip {
  background: #0d182b;
  border: 1px solid #1d3153;
  border-radius: 8px;
}
QHeaderView::section {
  background: #0e1b2f;
  color: #94a3b8;
  border: none;
  border-bottom: 1px solid #1b2f4d;
  padding: 8px;
}
QTableWidget {
  background: #070e1b;
  border: 1px solid #1a2b45;
  border-radius: 8px;
  gridline-color: #14233a;
  selection-background-color: #0f2139;
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
QLabel#Uac {
  background: rgba(120,53,15,0.45);
  border: 1px solid rgba(245,158,11,0.55);
  border-radius: 6px;
  padding: 2px 9px 2px 6px;
  color: #fcd34d;
  font-size: 11px;
  font-weight: 600;
}
QLabel#UacOff {
  background: rgba(30,41,59,0.8);
  border: 1px solid #475569;
  border-radius: 6px;
  padding: 2px 9px 2px 6px;
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

static QFrame *makeCard(QWidget *parent)
{
    auto *f = new QFrame(parent);
    f->setObjectName(QStringLiteral("Card"));
    return f;
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // 顶栏：左品牌 | 居中页签 | 右窗控
    m_titleBar = new QWidget;
    m_titleBar->setObjectName(QStringLiteral("TitleBar"));
    m_titleBar->setFixedHeight(48);
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

    m_uacBadge = new QLabel;
    m_uacBadge->setObjectName(QStringLiteral("UacOff"));
    m_uacBadge->setCursor(Qt::PointingHandCursor);
    m_uacBadge->setToolTip(QStringLiteral("点击可请求管理员权限（真实 Windows UAC）"));
    m_uacBadge->installEventFilter(this);

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
    // 外框略高，给 2px 霓虹描边 + 内边距留空
    tabs->setFixedHeight(36);
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
        b->setFixedHeight(28);
        b->setIconSize(QSize(14, 14));
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
    auto *maxBtn = makeWinChromeBtn(m_titleBar, 1);
    auto *closeBtn = makeWinChromeBtn(m_titleBar, 2);
    auto *chrome = new QWidget;
    chrome->setFixedHeight(32);
    auto *chromeLay = new QHBoxLayout(chrome);
    chromeLay->setContentsMargins(0, 0, 0, 0);
    chromeLay->setSpacing(0);
    chromeLay->addWidget(minBtn);
    chromeLay->addWidget(maxBtn);
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
    connect(maxBtn, &QToolButton::clicked, this, [this, maxBtn] {
        if (isMaximized()) {
            showNormal();
            maxBtn->setIcon(makeWinChromeIcon(1));
        } else {
            showMaximized();
            // 还原态画成双框感：仍用方框即可
            maxBtn->setIcon(makeWinChromeIcon(1));
        }
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
    auto *httpTitleBox = new QVBoxLayout;
    auto *httpTitle = new QLabel(QStringLiteral("局域网 HTTP 极速文件共享服务"));
    httpTitle->setObjectName(QStringLiteral("Title"));
    m_statusPill = new QLabel(QStringLiteral("共享服务已停止"));
    m_statusPill->setObjectName(QStringLiteral("Badge"));
    httpTitleBox->addWidget(httpTitle);
    httpTitleBox->addWidget(new QLabel(QStringLiteral("一键将本地文件夹发布为 HTTP 站点，手机/电脑/开发板均可下载。")));
    httpHead->addLayout(httpTitleBox, 1);
    m_toggleBtn = new QPushButton(QStringLiteral("一键启动 HTTP 共享"));
    m_toggleBtn->setObjectName(QStringLiteral("Primary"));
    httpHead->addWidget(m_toggleBtn, 0, Qt::AlignTop);
    httpLay->addLayout(httpHead);

    httpLay->addWidget(new QLabel(QStringLiteral("本地共享文件夹路径")));
    auto *folderRow = new QHBoxLayout;
    m_folderEdit = new QLineEdit(m_shareRoot);
    auto *browseBtn = new QPushButton(QStringLiteral("浏览..."));
    folderRow->addWidget(m_folderEdit, 1);
    folderRow->addWidget(browseBtn);
    httpLay->addLayout(folderRow);

    auto *portRow = new QHBoxLayout;
    portRow->addWidget(new QLabel(QStringLiteral("服务端口:")));
    m_portSpin = new QSpinBox;
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(8899);
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

    httpLay->addWidget(new QLabel(QStringLiteral("局域网直连提货地址")));
    auto *urlBox = makeCard(httpCard);
    urlBox->setStyleSheet(QStringLiteral("QFrame#Card{background:#070d18;border:1px solid #1f324f;border-radius:8px;}"));
    auto *urlLay = new QHBoxLayout(urlBox);
    m_ipCombo = new QComboBox;
    m_urlLabel = new QLabel(QStringLiteral("— (服务未启动)"));
    m_urlLabel->setObjectName(QStringLiteral("Url"));
    auto *copyBtn = new QPushButton(QStringLiteral("复制地址"));
    auto *openBtn = new QPushButton(QStringLiteral("打开"));
    auto *leftUrl = new QVBoxLayout;
    leftUrl->addWidget(m_ipCombo);
    leftUrl->addWidget(m_urlLabel);
    urlLay->addLayout(leftUrl, 1);
    urlLay->addWidget(copyBtn);
    urlLay->addWidget(openBtn);
    httpLay->addWidget(urlBox);

    auto *loopRow = new QHBoxLayout;
    loopRow->addWidget(new QLabel(QStringLiteral("本机 Loopback: http://127.0.0.1:") + QString::number(m_portSpin->value())));
    auto *selfCheckBtn = new QPushButton(QStringLiteral("执行自检 (Self-check)"));
    loopRow->addStretch();
    loopRow->addWidget(selfCheckBtn);
    httpLay->addLayout(loopRow);

    // 右：二维码卡
    auto *qrCard = makeCard(managerPage);
    auto *qrLay = new QVBoxLayout(qrCard);
    qrLay->setContentsMargins(16, 16, 16, 16);
    qrLay->addWidget(new QLabel(QStringLiteral("扫码提货 / 扫码下载")));
    m_qr = new QrCodeWidget;
    m_qr->setMinimumHeight(200);
    qrLay->addWidget(m_qr, 1);
    m_qrUrlLabel = new QLabel;
    m_qrUrlLabel->setObjectName(QStringLiteral("Url"));
    m_qrUrlLabel->setAlignment(Qt::AlignCenter);
    m_qrUrlLabel->setWordWrap(true);
    qrLay->addWidget(m_qrUrlLabel);
    qrLay->addWidget(new QLabel(QStringLiteral("共享文件快捷提货")));
    m_priorityFileLabel = new QLabel(QStringLiteral("暂无打包产物"));
    m_priorityFileLabel->setStyleSheet(QStringLiteral("background:#070d18;border:1px solid #1f324f;border-radius:8px;padding:10px;"));
    qrLay->addWidget(m_priorityFileLabel);

    topRow->addWidget(httpCard, 7);
    topRow->addWidget(qrCard, 5);
    mv->addLayout(topRow);

    // 文件卡
    auto *fileCard = makeCard(managerPage);
    auto *fileLay = new QVBoxLayout(fileCard);
    fileLay->setContentsMargins(16, 16, 16, 16);
    auto *fileHead = new QHBoxLayout;
    auto *fileTitle = new QLabel(QStringLiteral("共享文件夹内容与快速传输"));
    fileTitle->setObjectName(QStringLiteral("Title"));
    m_fileStats = new QLabel;
    m_fileStats->setObjectName(QStringLiteral("Badge"));
    auto *uploadBtn = new QPushButton(QStringLiteral("上传文件到共享目录"));
    uploadBtn->setObjectName(QStringLiteral("Primary"));
    fileHead->addWidget(fileTitle);
    fileHead->addWidget(m_fileStats);
    fileHead->addStretch();
    fileHead->addWidget(uploadBtn);
    fileLay->addLayout(fileHead);

    auto *dropHint = new QLabel(QStringLiteral("支持局域网千兆互传：使用上方按钮选择文件加入 HTTP 共享目录"));
    dropHint->setAlignment(Qt::AlignCenter);
    dropHint->setStyleSheet(QStringLiteral("border:1px dashed #1f324f;border-radius:8px;padding:12px;color:#94a3b8;background:#070d18;"));
    fileLay->addWidget(dropHint);

    auto *filterRow = new QHBoxLayout;
    m_fileFilter = new QLineEdit;
    m_fileFilter->setPlaceholderText(QStringLiteral("搜索共享文件名..."));
    filterRow->addWidget(m_fileFilter, 1);
    fileLay->addLayout(filterRow);

    m_fileTable = new QTableWidget(0, 5);
    m_fileTable->setHorizontalHeaderLabels(
        {QStringLiteral("文件名与产物"), QStringLiteral("文件大小"), QStringLiteral("修改时间"),
         QStringLiteral("类型"), QStringLiteral("快捷操作")});
    m_fileTable->horizontalHeader()->setStretchLastSection(true);
    m_fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fileTable->verticalHeader()->setVisible(false);
    m_fileTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_fileTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_fileTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    fileLay->addWidget(m_fileTable);

    auto *fileActs = new QHBoxLayout;
    auto *dlBtn = new QPushButton(QStringLiteral("下载选中"));
    auto *curlBtn = new QPushButton(QStringLiteral("复制 curl"));
    auto *delBtn = new QPushButton(QStringLiteral("删除选中"));
    fileActs->addStretch();
    fileActs->addWidget(curlBtn);
    fileActs->addWidget(dlBtn);
    fileActs->addWidget(delBtn);
    fileLay->addLayout(fileActs);
    mv->addWidget(fileCard);

    // 网卡卡
    auto *nicCard = makeCard(managerPage);
    auto *nicLay = new QVBoxLayout(nicCard);
    nicLay->setContentsMargins(16, 16, 16, 16);
    auto *nicHead = new QHBoxLayout;
    auto *nicTitle = new QLabel(QStringLiteral("有线网卡高级 IP 地址绑定与管理 (NIC Manager)"));
    nicTitle->setObjectName(QStringLiteral("Title"));
    nicHead->addWidget(nicTitle);
    nicHead->addStretch();
    auto *refreshNicBtn = new QPushButton(QStringLiteral("刷新网卡"));
    nicHead->addWidget(refreshNicBtn);
    nicLay->addLayout(nicHead);
    nicLay->addWidget(new QLabel(QStringLiteral(
        "支持向现有网卡追加辅助调试段 IP（例如 192.168.8.x），以便连接不同网段的 ARM 开发板，无需修改默认网关。")));

    auto *addRow = new QHBoxLayout;
    m_newIpEdit = new QLineEdit(QStringLiteral("192.168.8.202"));
    m_newMaskEdit = new QLineEdit(QStringLiteral("255.255.255.0"));
    auto *addNicBtn = new QPushButton(QStringLiteral("+ 追加绑定 IP"));
    addNicBtn->setObjectName(QStringLiteral("Primary"));
    addRow->addWidget(new QLabel(QStringLiteral("追加辅助 IP:")));
    addRow->addWidget(m_newIpEdit, 1);
    addRow->addWidget(new QLabel(QStringLiteral("子网掩码:")));
    addRow->addWidget(m_newMaskEdit, 1);
    addRow->addWidget(addNicBtn);
    nicLay->addLayout(addRow);

    m_nicTable = new QTableWidget(0, 4);
    m_nicTable->setHorizontalHeaderLabels(
        {QStringLiteral("网卡与名称"), QStringLiteral("绑定 IP"), QStringLiteral("子网掩码"), QStringLiteral("操作")});
    m_nicTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_nicTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_nicTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_nicTable->verticalHeader()->setVisible(false);
    m_nicTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_nicTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_nicTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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
    auto *backBtn = new QPushButton(QStringLiteral("← 返回 Windows 客户端控制台"));
    backRow->addWidget(backBtn);
    backRow->addStretch();
    backRow->addWidget(new QLabel(QStringLiteral("提货专线预览 · 真实页面请用浏览器访问共享地址")));
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
    connect(copyBtn, &QPushButton::clicked, this, &MainWindow::copyShareUrl);
    connect(openBtn, &QPushButton::clicked, this, &MainWindow::openPortalInBrowser);
    connect(openReal, &QPushButton::clicked, this, &MainWindow::openPortalInBrowser);
    connect(selfCheckBtn, &QPushButton::clicked, this, &MainWindow::selfCheck);
    connect(uploadBtn, &QPushButton::clicked, this, &MainWindow::uploadLocalFiles);
    connect(dlBtn, &QPushButton::clicked, this, &MainWindow::downloadSelectedFile);
    connect(delBtn, &QPushButton::clicked, this, &MainWindow::deleteSelectedFile);
    connect(curlBtn, &QPushButton::clicked, this, &MainWindow::copyCurlForSelected);
    connect(addNicBtn, &QPushButton::clicked, this, &MainWindow::addNicIp);
    connect(refreshNicBtn, &QPushButton::clicked, this, &MainWindow::refreshNics);
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

void MainWindow::updateUacBadge()
{
    const bool elevated = NicManager::isElevated();
    QSvgRenderer renderer(QString(elevated ? QStringLiteral(":/icons/shield_check.svg")
                                           : QStringLiteral(":/icons/shield_off.svg")));
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        renderer.render(&p, QRectF(0, 0, 16, 16));
    }

    m_uacBadge->setPixmap(pm);
    m_uacBadge->setObjectName(elevated ? QStringLiteral("Uac") : QStringLiteral("UacOff"));
    m_uacBadge->setText(elevated ? QStringLiteral("  UAC 已授权") : QStringLiteral("  UAC 未授权 · 点击提权"));
    m_uacBadge->setToolTip(elevated ? QStringLiteral("当前进程已通过 Windows UAC 提权（TokenElevation=1）")
                                    : QStringLiteral("当前为标准权限。点击将弹出系统 UAC，同意后以管理员重启。"));
    m_uacBadge->style()->unpolish(m_uacBadge);
    m_uacBadge->style()->polish(m_uacBadge);
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
        m_statusPill->setText(QStringLiteral("共享服务运行中"));
    } else {
        m_toggleBtn->setText(QStringLiteral("一键启动 HTTP 共享"));
        m_toggleBtn->setObjectName(QStringLiteral("Primary"));
        m_statusPill->setText(QStringLiteral("共享服务已停止"));
    }
    m_toggleBtn->style()->unpolish(m_toggleBtn);
    m_toggleBtn->style()->polish(m_toggleBtn);
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
    addLog(QStringLiteral("download"),
           QStringLiteral("%1 下载: %2 (%3)").arg(ip, name, fmtBytes(size)));
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
    if (!m_running) {
        QMessageBox::information(this, QStringLiteral("自检"),
                                 QStringLiteral("服务当前处于停止状态。请先启动 HTTP 共享。"));
        return;
    }
    auto *nam = new QNetworkAccessManager(this);
    const QUrl url(QStringLiteral("http://127.0.0.1:%1/").arg(m_portSpin->value()));
    QNetworkRequest req(url);
    auto *reply = nam->head(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam] {
        reply->deleteLater();
        nam->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            addLog(QStringLiteral("check"),
                   QStringLiteral("自检通过: GET http://127.0.0.1:%1/ -> HTTP 200").arg(m_portSpin->value()));
            QMessageBox::information(this, QStringLiteral("自检"),
                                     QStringLiteral("本机回环检测通过。\n局域网地址: %1").arg(currentShareUrl()));
        } else {
            addLog(QStringLiteral("check"), QStringLiteral("自检失败: %1").arg(reply->errorString()));
            QMessageBox::warning(this, QStringLiteral("自检"), reply->errorString());
        }
    });
}

void MainWindow::refreshNics()
{
    const QString keepIp = selectedIp();
    const auto nics = NicManager::enumerate();
    m_ipCombo->blockSignals(true);
    m_ipCombo->clear();
    m_nicTable->setRowCount(0);

    int row = 0;
    for (const NicEntry &n : nics) {
        m_ipCombo->addItem(QStringLiteral("%1 (%2)").arg(n.name, n.ip), n.ip);

        m_nicTable->insertRow(row);
        auto *nameItem = new QTableWidgetItem(n.name);
        auto *ipItem = new QTableWidgetItem(n.isPrimary ? QStringLiteral("Primary  %1").arg(n.ip) : n.ip);
        auto *maskItem = new QTableWidgetItem(n.subnet);
        auto *actItem = new QTableWidgetItem(n.isPrimary ? QStringLiteral("主网卡保护") : QStringLiteral("可解绑"));
        if (n.isPrimary)
            actItem->setFlags(actItem->flags() & ~Qt::ItemIsSelectable);
        m_nicTable->setItem(row, 0, nameItem);
        m_nicTable->setItem(row, 1, ipItem);
        m_nicTable->setItem(row, 2, maskItem);
        m_nicTable->setItem(row, 3, actItem);
        m_nicTable->item(row, 1)->setData(Qt::UserRole, n.ip);
        m_nicTable->item(row, 1)->setData(Qt::UserRole + 1, n.isPrimary);
        ++row;
    }
    m_ipCombo->blockSignals(false);

    int idx = m_ipCombo->findData(keepIp);
    if (idx < 0)
        idx = 0;
    if (m_ipCombo->count() > 0)
        m_ipCombo->setCurrentIndex(idx);

    // 双击非主 IP 解绑
    m_nicTable->disconnect();
    connect(m_nicTable, &QTableWidget::cellDoubleClicked, this, [this](int r, int) {
        if (r < 0)
            return;
        const bool primary = m_nicTable->item(r, 1)->data(Qt::UserRole + 1).toBool();
        if (primary)
            return;
        m_nicTable->selectRow(r);
        deleteSelectedNic();
    });
    connect(m_nicTable, &QTableWidget::itemSelectionChanged, this, [this] {
        const auto ranges = m_nicTable->selectedRanges();
        if (ranges.isEmpty())
            return;
        const int r = ranges.first().topRow();
        const QString ip = m_nicTable->item(r, 1)->data(Qt::UserRole).toString();
        const int idx = m_ipCombo->findData(ip);
        if (idx >= 0)
            m_ipCombo->setCurrentIndex(idx);
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
    if (m_nicTable->item(r, 1)->data(Qt::UserRole + 1).toBool()) {
        showToast(QStringLiteral("主网卡受保护，不能解绑"));
        return;
    }
    if (!NicManager::isElevated()) {
        QMessageBox::warning(this, QStringLiteral("需要管理员权限"),
                             QStringLiteral("解绑 IP 需要以管理员身份运行本程序。"));
        return;
    }
    const QString ip = m_nicTable->item(r, 1)->data(Qt::UserRole).toString();
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
    QString priority;

    for (const QFileInfo &fi : infos) {
        if (!filter.isEmpty() && !fi.fileName().contains(filter, Qt::CaseInsensitive))
            continue;
        total += fi.size();
        const int r = m_fileTable->rowCount();
        m_fileTable->insertRow(r);
        m_fileTable->setItem(r, 0, new QTableWidgetItem(fi.fileName()));
        m_fileTable->setItem(r, 1, new QTableWidgetItem(fmtBytes(fi.size())));
        m_fileTable->setItem(r, 2, new QTableWidgetItem(fi.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
        m_fileTable->setItem(r, 3, new QTableWidgetItem(fi.suffix().toUpper()));
        m_fileTable->setItem(r, 4, new QTableWidgetItem(QStringLiteral("下载 / curl / 删除")));
        m_fileTable->item(r, 0)->setData(Qt::UserRole, fi.absoluteFilePath());

        const int pr = m_portalTable->rowCount();
        m_portalTable->insertRow(pr);
        m_portalTable->setItem(pr, 0, new QTableWidgetItem(fi.fileName()));
        m_portalTable->setItem(pr, 1, new QTableWidgetItem(fmtBytes(fi.size())));
        m_portalTable->setItem(pr, 2, new QTableWidgetItem(fi.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));

        if (priority.isEmpty()
            || fi.fileName().contains(QStringLiteral("bundle"), Qt::CaseInsensitive)
            || fi.suffix().compare(QStringLiteral("zip"), Qt::CaseInsensitive) == 0
            || fi.suffix().compare(QStringLiteral("gz"), Qt::CaseInsensitive) == 0)
            priority = QStringLiteral("%1  (%2)").arg(fi.fileName(), fmtBytes(fi.size()));
        ++shown;
    }

    m_fileStats->setText(QStringLiteral("%1 个文件 · %2").arg(shown).arg(fmtBytes(total)));
    m_priorityFileLabel->setText(priority.isEmpty() ? QStringLiteral("暂无打包产物") : priority);
    if (m_portalCount)
        m_portalCount->setText(QString::number(shown));
    fitTableHeight(m_fileTable);
}

void MainWindow::uploadLocalFiles()
{
    const QStringList paths = QFileDialog::getOpenFileNames(this, QStringLiteral("选择要加入共享的文件"));
    if (paths.isEmpty())
        return;
    const QString root = QDir::fromNativeSeparators(m_folderEdit->text().trimmed());
    QDir().mkpath(root);
    int n = 0;
    for (const QString &p : paths) {
        const QFileInfo fi(p);
        const QString dest = QDir(root).filePath(fi.fileName());
        if (QFile::exists(dest))
            QFile::remove(dest);
        if (QFile::copy(p, dest))
            ++n;
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
    const QString src = m_fileTable->item(r, 0)->data(Qt::UserRole).toString();
    const QString name = m_fileTable->item(r, 0)->text();
    const QString dest = QFileDialog::getSaveFileName(this, QStringLiteral("保存文件"), name);
    if (dest.isEmpty())
        return;
    if (QFile::exists(dest))
        QFile::remove(dest);
    if (QFile::copy(src, dest)) {
        addLog(QStringLiteral("download"), QStringLiteral("本机导出: %1").arg(name));
        showToast(QStringLiteral("已保存"));
    }
}

void MainWindow::deleteSelectedFile()
{
    const auto items = m_fileTable->selectedItems();
    if (items.isEmpty())
        return;
    const int r = items.first()->row();
    const QString path = m_fileTable->item(r, 0)->data(Qt::UserRole).toString();
    const QString name = m_fileTable->item(r, 0)->text();
    if (QMessageBox::question(this, QStringLiteral("确认"),
                              QStringLiteral("确定从共享文件夹移除 %1 吗？").arg(name))
        != QMessageBox::Yes)
        return;
    if (QFile::remove(path)) {
        addLog(QStringLiteral("stop"), QStringLiteral("已从共享列表移除: %1").arg(name));
        refreshFiles();
    }
}

void MainWindow::copyCurlForSelected()
{
    const auto items = m_fileTable->selectedItems();
    if (items.isEmpty())
        return;
    const QString name = m_fileTable->item(items.first()->row(), 0)->text();
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
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_uacBadge && event->type() == QEvent::MouseButtonRelease) {
        if (NicManager::isElevated()) {
            updateUacBadge();
            showToast(QStringLiteral("当前已是管理员权限（真实 UAC 状态）"));
            return true;
        }
        const auto ret = QMessageBox::question(
            this, QStringLiteral("请求管理员权限"),
            QStringLiteral("追加/解绑网卡 IP 需要管理员权限。\n"
                           "将弹出 Windows UAC 对话框，同意后以管理员身份重新启动本程序。\n\n"
                           "是否继续？"));
        if (ret != QMessageBox::Yes)
            return true;
        QString err;
        if (NicManager::requestElevation(&err)) {
            // 新的管理员实例已启动，退出当前标准权限进程
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        } else {
            showToast(err.isEmpty() ? QStringLiteral("提权失败") : err);
            updateUacBadge();
        }
        return true;
    }

    if (watched == m_titleBar) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (event->type() == QEvent::MouseButtonDblClick && me->button() == Qt::LeftButton) {
            if (isMaximized())
                showNormal();
            else
                showMaximized();
            return true;
        }
        if (event->type() == QEvent::MouseButtonPress && me->button() == Qt::LeftButton) {
            QWidget *hit = m_titleBar->childAt(me->pos());
            for (QWidget *w = hit; w && w != m_titleBar; w = w->parentWidget()) {
                if (qobject_cast<QPushButton *>(w) || qobject_cast<QToolButton *>(w)
                    || w->objectName() == QLatin1String("TabStrip")
                    || w->objectName() == QLatin1String("PortalCount")
                    || w->objectName() == QLatin1String("RunDot")
                    || w->objectName() == QLatin1String("Uac"))
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
