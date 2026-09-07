#include "MainWindow.h"

#include "HttpFileServer.h"
#include "NicManager.h"
#include "ActivityLogModel.h"
#include "QrCodeWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
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

    connect(m_server, &HttpFileServer::started, this, &MainWindow::onServerStarted);
    connect(m_server, &HttpFileServer::stopped, this, &MainWindow::onServerStopped);
    connect(m_server, &HttpFileServer::clientDownload, this, &MainWindow::onDownload);
    connect(m_server, &HttpFileServer::clientUpload, this, &MainWindow::onUpload);
    connect(m_server, &HttpFileServer::errorOccurred, this, [this](const QString &e) {
        addLog(QStringLiteral("check"), e);
        showToast(e);
    });
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &MainWindow::refreshFiles);

    if (NicManager::isElevated())
        m_uacBadge->setText(QStringLiteral("UAC 已授权"));
    else
        m_uacBadge->setText(QStringLiteral("标准权限"));

    addLog(QStringLiteral("check"), QStringLiteral("客户端已启动，等待开启 HTTP 共享"));
    resize(1180, 860);
    setWindowTitle(QStringLiteral("HTTP 局域网极速文件共享客户端  ·  Win x64 · v1.0.0"));
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
QPushButton#Tab {
  background: transparent;
  border: none;
  border-radius: 6px;
  padding: 6px 14px;
  color: #94a3b8;
}
QPushButton#TabActive {
  background: rgba(16,185,129,0.2);
  border: 1px solid rgba(16,185,129,0.4);
  color: #6ee7b7;
  font-weight: 600;
}
QPushButton#TabActiveCyan {
  background: rgba(6,182,212,0.2);
  border: 1px solid rgba(6,182,212,0.4);
  color: #67e8f9;
  font-weight: 600;
}
QPushButton#TabActiveIndigo {
  background: rgba(99,102,241,0.2);
  border: 1px solid rgba(99,102,241,0.4);
  color: #a5b4fc;
  font-weight: 600;
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
  background: rgba(120,53,15,0.4);
  border: 1px solid rgba(245,158,11,0.5);
  border-radius: 6px;
  padding: 3px 8px;
  color: #fcd34d;
}
QLabel#Toast {
  background: #0e1d33;
  border: 1px solid rgba(6,182,212,0.6);
  border-radius: 12px;
  padding: 10px 16px;
  color: #f1f5f9;
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

    // 顶栏
    auto *titleBar = new QWidget;
    titleBar->setStyleSheet(QStringLiteral("background:#09111e;border-bottom:1px solid #1b2b46;"));
    auto *tb = new QHBoxLayout(titleBar);
    tb->setContentsMargins(12, 8, 12, 8);
    auto *appName = new QLabel(QStringLiteral("HTTP 局域网极速文件共享客户端"));
    appName->setStyleSheet(QStringLiteral("font-size:13px;font-weight:700;"));
    auto *ver = new QLabel(QStringLiteral("Win x64 · v1.0.0"));
    ver->setStyleSheet(QStringLiteral("color:#94a3b8;background:#14233c;border:1px solid #1f355a;border-radius:4px;padding:2px 6px;"));
    m_uacBadge = new QLabel;
    m_uacBadge->setObjectName(QStringLiteral("Uac"));
    tb->addWidget(appName);
    tb->addWidget(ver);
    tb->addWidget(m_uacBadge);
    tb->addStretch();

    auto *tabs = new QWidget;
    tabs->setStyleSheet(QStringLiteral("background:#0d182b;border:1px solid #1d3153;border-radius:8px;"));
    auto *tabsLay = new QHBoxLayout(tabs);
    tabsLay->setContentsMargins(4, 4, 4, 4);
    tabsLay->setSpacing(2);
    m_tabManager = new QPushButton(QStringLiteral("客户端控制面板"));
    m_tabPortal = new QPushButton(QStringLiteral("局域网提货 Web 端"));
    m_tabLogs = new QPushButton(QStringLiteral("实时日志与监控"));
    for (auto *b : {m_tabManager, m_tabPortal, m_tabLogs}) {
        b->setObjectName(QStringLiteral("Tab"));
        tabsLay->addWidget(b);
    }
    m_tabManager->setObjectName(QStringLiteral("TabActive"));
    tb->addWidget(tabs);
    root->addWidget(titleBar);

    connect(m_tabManager, &QPushButton::clicked, this, [this] { switchView(0); });
    connect(m_tabPortal, &QPushButton::clicked, this, [this] { switchView(1); });
    connect(m_tabLogs, &QPushButton::clicked, this, [this] { switchView(2); });

    m_stack = new QStackedWidget;
    root->addWidget(m_stack, 1);

    // ===== View 0: Manager =====
    auto *managerPage = new QWidget;
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
    nicLay->addWidget(m_nicTable);
    mv->addWidget(nicCard);

    auto *footer = new QLabel(QStringLiteral("Windows x64 Native Qt/HTTP Core  ·  局域网千兆全双工  ·  开源项目"));
    footer->setObjectName(QStringLiteral("Muted"));
    mv->addWidget(footer);

    m_stack->addWidget(managerPage);

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
    m_tabManager->setObjectName(index == 0 ? QStringLiteral("TabActive") : QStringLiteral("Tab"));
    m_tabPortal->setObjectName(index == 1 ? QStringLiteral("TabActiveCyan") : QStringLiteral("Tab"));
    m_tabLogs->setObjectName(index == 2 ? QStringLiteral("TabActiveIndigo") : QStringLiteral("Tab"));
    // 刷新样式
    for (auto *b : {m_tabManager, m_tabPortal, m_tabLogs}) {
        b->style()->unpolish(b);
        b->style()->polish(b);
    }
    if (index == 1)
        refreshFiles();
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
    m_tabPortal->setText(QStringLiteral("局域网提货 Web 端 %1").arg(shown));
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
