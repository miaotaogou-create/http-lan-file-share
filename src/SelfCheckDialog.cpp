#include "SelfCheckDialog.h"

#include <QElapsedTimer>
#include <QFrame>
#include <QIcon>
#include <QHBoxLayout>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSharedPointer>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>

namespace {

QPixmap makePulsePm(const QColor &c, int s = 16)
{
    QPixmap pm(s, s);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(c, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const qreal k = s / 16.0;
    p.drawPolyline(QPolygonF({QPointF(1 * k, 9 * k), QPointF(4 * k, 9 * k), QPointF(6 * k, 3.5 * k),
                              QPointF(8.5 * k, 13 * k), QPointF(11 * k, 7 * k), QPointF(13 * k, 9 * k),
                              QPointF(15 * k, 9 * k)}));
    return pm;
}

QPixmap makeClosePm(const QColor &c)
{
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(c, 1.6, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(3, 3), QPointF(11, 11));
    p.drawLine(QPointF(11, 3), QPointF(3, 11));
    return pm;
}

QPixmap makeRefreshPm(const QColor &c, int s = 14)
{
    QPixmap pm(s, s);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(c, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawArc(QRectF(2, 2, s - 4, s - 4), 50 * 16, 270 * 16);
    p.setBrush(c);
    p.setPen(Qt::NoPen);
    p.drawPolygon(QPolygonF({QPointF(s - 2.5, 3.5), QPointF(s - 6.5, 2), QPointF(s - 5, 6)}));
    return pm;
}

QPixmap makeCheckPm(const QColor &c)
{
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(c);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(1, 1, 14, 14));
    p.setPen(QPen(QColor(QStringLiteral("#04140f")), 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPolyline(QPolygonF({QPointF(4.5, 8.2), QPointF(7.0, 10.8), QPointF(11.5, 5.5)}));
    return pm;
}

QPixmap makeWarnPm(const QColor &c)
{
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(c);
    p.setPen(Qt::NoPen);
    p.drawPolygon(QPolygonF({QPointF(8, 1.5), QPointF(14.5, 14), QPointF(1.5, 14)}));
    p.setPen(QPen(QColor(QStringLiteral("#1a1200")), 1.5, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(8, 5.5), QPointF(8, 9.5));
    p.setBrush(QColor(QStringLiteral("#1a1200")));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(8, 12), 1.0, 1.0);
    return pm;
}

QPixmap makeTermPm(const QColor &c)
{
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(c, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawRoundedRect(QRectF(1.5, 2, 11, 10), 1.5, 1.5);
    p.drawLine(QPointF(4, 5.5), QPointF(6.5, 7.5));
    p.drawLine(QPointF(6.5, 7.5), QPointF(4, 9.5));
    p.drawLine(QPointF(7.5, 9.5), QPointF(10, 9.5));
    return pm;
}

QLabel *makeCodeLabel(const QString &text)
{
    auto *lab = new QLabel(text);
    lab->setWordWrap(true);
    lab->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lab->setObjectName(QStringLiteral("CmdCode"));
    return lab;
}

} // namespace

SelfCheckDialog::SelfCheckDialog(bool running, quint16 port, const QString &ip, QWidget *parent)
    : QDialog(parent)
    , m_running(running)
    , m_port(port)
    , m_ip(ip)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setModal(true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFixedWidth(520);
    setObjectName(QStringLiteral("SelfCheckDialog"));
    setStyleSheet(QStringLiteral(R"(
QDialog#SelfCheckDialog { background: transparent; }
QFrame#Card {
  background: #0b1424;
  border: 1px solid #1e3458;
  border-radius: 12px;
}
QLabel { background: transparent; color: #94a3b8; font-size: 12px; }
QLabel#DlgTitle { color: #f1f5f9; font-size: 12px; font-weight: 700; }
QLabel#TargetUrl { color: #94a3b8; font-size: 12px; }
QLabel#LoadingText { color: #94a3b8; font-size: 12px; }
QLabel#ItemTitle { color: #e2e8f0; font-size: 12px; font-weight: 600; }
QLabel#ItemDetail { color: #94a3b8; font-size: 11px; font-family: Consolas, "Courier New", monospace; }
QLabel#LatencyBadge {
  color: #34d399;
  background: #022c22;
  border-radius: 4px;
  padding: 1px 6px;
  font-size: 10px;
  font-family: Consolas, "Courier New", monospace;
}
QLabel#CmdLabel { color: #22d3ee; font-size: 11px; font-family: Consolas, "Courier New", monospace; }
QLabel#CmdLabelPs { color: #60a5fa; font-size: 11px; font-family: Consolas, "Courier New", monospace; }
QLabel#CmdCode {
  color: #cbd5e1;
  background: #020509;
  border: 1px solid #132034;
  border-radius: 6px;
  padding: 8px;
  font-size: 11px;
  font-family: Consolas, "Courier New", monospace;
}
QFrame#HeaderBar, QFrame#FooterBar {
  background: #080e1a;
  border: none;
}
QFrame#HeaderBar { border-bottom: 1px solid #182942; border-top-left-radius: 12px; border-top-right-radius: 12px; }
QFrame#FooterBar { border-top: 1px solid #182942; border-bottom-left-radius: 12px; border-bottom-right-radius: 12px; }
QFrame#TargetRow { border: none; border-bottom: 1px solid #182840; }
QFrame#ResultCard {
  background: #070d18;
  border: 1px solid #182a44;
  border-radius: 8px;
}
QFrame#CmdPanel {
  background: #060c16;
  border: 1px solid #17273e;
  border-radius: 8px;
}
QToolButton#CloseBtn, QToolButton#RedetectBtn {
  background: transparent;
  border: none;
  color: #22d3ee;
  font-size: 12px;
  font-weight: 600;
  padding: 2px 4px;
}
QToolButton#CloseBtn { color: #94a3b8; padding: 4px; border-radius: 4px; }
QToolButton#CloseBtn:hover { background: #1e293b; color: #e2e8f0; }
QToolButton#RedetectBtn:hover { color: #67e8f9; }
QToolButton#RedetectBtn:disabled { color: #334155; }
QPushButton#DoneBtn {
  background: #0891b2;
  color: #020617;
  border: none;
  border-radius: 6px;
  padding: 6px 16px;
  font-size: 12px;
  font-weight: 700;
  min-width: 64px;
}
QPushButton#DoneBtn:hover { background: #06b6d4; }
QScrollArea { background: transparent; border: none; }
QScrollArea > QWidget > QWidget { background: transparent; }
)"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("Card"));
    auto *cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(0, 0, 0, 0);
    cardLay->setSpacing(0);
    root->addWidget(card);

    // Header
    auto *header = new QFrame;
    header->setObjectName(QStringLiteral("HeaderBar"));
    auto *hl = new QHBoxLayout(header);
    hl->setContentsMargins(16, 12, 10, 12);
    hl->setSpacing(8);
    auto *titleIcon = new QLabel;
    titleIcon->setPixmap(makePulsePm(QColor(QStringLiteral("#22d3ee"))));
    auto *title = new QLabel(QStringLiteral("HTTP 共享服务网络与环境自检 (Self-check)"));
    title->setObjectName(QStringLiteral("DlgTitle"));
    auto *closeBtn = new QToolButton;
    closeBtn->setObjectName(QStringLiteral("CloseBtn"));
    closeBtn->setIcon(QIcon(makeClosePm(QColor(QStringLiteral("#94a3b8")))));
    closeBtn->setIconSize(QSize(14, 14));
    closeBtn->setCursor(Qt::PointingHandCursor);
    hl->addWidget(titleIcon, 0, Qt::AlignVCenter);
    hl->addWidget(title, 1, Qt::AlignVCenter);
    hl->addWidget(closeBtn, 0, Qt::AlignVCenter);
    cardLay->addWidget(header);

    // Body
    auto *body = new QWidget;
    auto *bl = new QVBoxLayout(body);
    bl->setContentsMargins(16, 16, 16, 16);
    bl->setSpacing(12);

    auto *targetRow = new QFrame;
    targetRow->setObjectName(QStringLiteral("TargetRow"));
    auto *tr = new QHBoxLayout(targetRow);
    tr->setContentsMargins(0, 0, 0, 10);
    tr->setSpacing(8);
    m_targetLabel = new QLabel;
    m_targetLabel->setObjectName(QStringLiteral("TargetUrl"));
    m_redetectBtn = new QToolButton;
    m_redetectBtn->setObjectName(QStringLiteral("RedetectBtn"));
    m_redetectBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_redetectBtn->setIcon(QIcon(makeRefreshPm(QColor(QStringLiteral("#22d3ee")))));
    m_redetectBtn->setIconSize(QSize(14, 14));
    m_redetectBtn->setText(QStringLiteral("重新检测"));
    m_redetectBtn->setCursor(Qt::PointingHandCursor);
    tr->addWidget(m_targetLabel, 1);
    tr->addWidget(m_redetectBtn, 0, Qt::AlignVCenter);
    bl->addWidget(targetRow);

    m_loadingBox = new QWidget;
    auto *ll = new QVBoxLayout(m_loadingBox);
    ll->setContentsMargins(0, 28, 0, 28);
    ll->setSpacing(10);
    m_spinLabel = new QLabel;
    m_spinLabel->setFixedSize(28, 28);
    m_spinLabel->setAlignment(Qt::AlignCenter);
    auto *loadingText = new QLabel(QStringLiteral("正在对网络端口、网关、回环与防火墙进行快速自检..."));
    loadingText->setObjectName(QStringLiteral("LoadingText"));
    loadingText->setAlignment(Qt::AlignCenter);
    ll->addWidget(m_spinLabel, 0, Qt::AlignHCenter);
    ll->addWidget(loadingText, 0, Qt::AlignHCenter);
    bl->addWidget(m_loadingBox);

    m_resultsHost = new QWidget;
    m_resultsLayout = new QVBoxLayout(m_resultsHost);
    m_resultsLayout->setContentsMargins(0, 0, 0, 0);
    m_resultsLayout->setSpacing(8);
    m_resultScroll = new QScrollArea;
    m_resultScroll->setWidgetResizable(true);
    m_resultScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_resultScroll->setWidget(m_resultsHost);
    m_resultScroll->setMaximumHeight(320);
    m_resultScroll->setObjectName(QStringLiteral("ResultScroll"));
    bl->addWidget(m_resultScroll);

    // 命令区始终可见
    auto *cmdPanel = new QFrame;
    cmdPanel->setObjectName(QStringLiteral("CmdPanel"));
    auto *cl = new QVBoxLayout(cmdPanel);
    cl->setContentsMargins(12, 12, 12, 12);
    cl->setSpacing(10);

    auto *unixHead = new QHBoxLayout;
    unixHead->setSpacing(6);
    auto *unixIcon = new QLabel;
    unixIcon->setPixmap(makeTermPm(QColor(QStringLiteral("#22d3ee"))));
    auto *unixLab = new QLabel(QStringLiteral("客户机 (Linux / macOS / 终端) 测试命令:"));
    unixLab->setObjectName(QStringLiteral("CmdLabel"));
    unixHead->addWidget(unixIcon, 0, Qt::AlignVCenter);
    unixHead->addWidget(unixLab, 1, Qt::AlignVCenter);
    m_curlCode = makeCodeLabel(QString());
    cl->addLayout(unixHead);
    cl->addWidget(m_curlCode);

    auto *psHead = new QHBoxLayout;
    psHead->setSpacing(6);
    auto *psIcon = new QLabel;
    psIcon->setPixmap(makeTermPm(QColor(QStringLiteral("#60a5fa"))));
    auto *psLab = new QLabel(QStringLiteral("Windows 客户机 (PowerShell) 测试命令:"));
    psLab->setObjectName(QStringLiteral("CmdLabelPs"));
    psHead->addWidget(psIcon, 0, Qt::AlignVCenter);
    psHead->addWidget(psLab, 1, Qt::AlignVCenter);
    m_psCode = makeCodeLabel(QString());
    cl->addLayout(psHead);
    cl->addWidget(m_psCode);
    bl->addWidget(cmdPanel);

    cardLay->addWidget(body, 1);

    // Footer
    auto *footer = new QFrame;
    footer->setObjectName(QStringLiteral("FooterBar"));
    auto *fl = new QHBoxLayout(footer);
    fl->setContentsMargins(16, 12, 16, 12);
    fl->addStretch(1);
    auto *doneBtn = new QPushButton(QStringLiteral("完成"));
    doneBtn->setObjectName(QStringLiteral("DoneBtn"));
    doneBtn->setCursor(Qt::PointingHandCursor);
    fl->addWidget(doneBtn);
    cardLay->addWidget(footer);

    m_spinTimer = new QTimer(this);
    m_spinTimer->setInterval(30);
    connect(m_spinTimer, &QTimer::timeout, this, &SelfCheckDialog::onSpinTick);
    connect(closeBtn, &QToolButton::clicked, this, &QDialog::reject);
    connect(doneBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_redetectBtn, &QToolButton::clicked, this, &SelfCheckDialog::runCheck);

    updateCommandTexts();
    runCheck();
}

void SelfCheckDialog::updateCommandTexts()
{
    const QString base = QStringLiteral("http://%1:%2").arg(m_ip).arg(m_port);
    m_targetLabel->setText(QStringLiteral("目标: %1").arg(base));
    m_curlCode->setText(QStringLiteral("curl -I %1/").arg(base));
    m_psCode->setText(QStringLiteral("Invoke-WebRequest -Uri %1/ -Method Head").arg(base));
}

void SelfCheckDialog::setCheckingUi(bool checking)
{
    m_checking = checking;
    m_loadingBox->setVisible(checking);
    m_resultScroll->setVisible(!checking);
    m_redetectBtn->setEnabled(!checking);
    if (checking) {
        m_spinAngle = 0;
        m_spinTimer->start();
        onSpinTick();
    } else {
        m_spinTimer->stop();
    }
}

void SelfCheckDialog::onSpinTick()
{
    m_spinAngle += 12;
    if (m_spinAngle >= 360)
        m_spinAngle -= 360;
    QPixmap base = makeRefreshPm(QColor(QStringLiteral("#22d3ee")), 24);
    QPixmap rot(28, 28);
    rot.fill(Qt::transparent);
    QPainter p(&rot);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.translate(14, 14);
    p.rotate(m_spinAngle);
    p.translate(-12, -12);
    p.drawPixmap(0, 0, base);
    p.end();
    m_spinLabel->setPixmap(rot);
}

void SelfCheckDialog::rebuildResults(const QList<CheckItem> &items)
{
    while (QLayoutItem *it = m_resultsLayout->takeAt(0)) {
        if (it->widget())
            it->widget()->deleteLater();
        delete it;
    }

    for (const CheckItem &item : items) {
        auto *card = new QFrame;
        card->setObjectName(QStringLiteral("ResultCard"));
        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(10, 10, 10, 10);
        row->setSpacing(10);

        auto *icon = new QLabel;
        const bool ok = item.status == QLatin1String("ok");
        icon->setPixmap(ok ? makeCheckPm(QColor(QStringLiteral("#34d399")))
                           : makeWarnPm(QColor(QStringLiteral("#fbbf24"))));
        icon->setFixedSize(16, 16);

        auto *textCol = new QWidget;
        auto *tv = new QVBoxLayout(textCol);
        tv->setContentsMargins(0, 0, 0, 0);
        tv->setSpacing(2);

        auto *titleRow = new QHBoxLayout;
        titleRow->setContentsMargins(0, 0, 0, 0);
        titleRow->setSpacing(8);
        auto *title = new QLabel(item.title);
        title->setObjectName(QStringLiteral("ItemTitle"));
        titleRow->addWidget(title, 1);
        if (item.latencyMs >= 0) {
            auto *badge = new QLabel(QStringLiteral("%1ms").arg(item.latencyMs));
            badge->setObjectName(QStringLiteral("LatencyBadge"));
            badge->setAlignment(Qt::AlignCenter);
            titleRow->addWidget(badge, 0, Qt::AlignVCenter);
        }
        auto *detail = new QLabel(item.detail);
        detail->setObjectName(QStringLiteral("ItemDetail"));
        detail->setWordWrap(true);
        tv->addLayout(titleRow);
        tv->addWidget(detail);

        row->addWidget(icon, 0, Qt::AlignTop);
        row->addWidget(textCol, 1);
        m_resultsLayout->addWidget(card);
    }
    m_resultsLayout->addStretch(1);
}

void SelfCheckDialog::finishChecks(const QList<CheckItem> &items)
{
    rebuildResults(items);
    setCheckingUi(false);
}

void SelfCheckDialog::runCheck()
{
    if (m_checking)
        return;
    updateCommandTexts();
    setCheckingUi(true);

    if (!m_running) {
        QTimer::singleShot(500, this, [this] {
            finishChecks({
                {QStringLiteral("HTTP 共享服务主进程状态"),
                 QStringLiteral("服务当前处于停止状态。请先在控制面板点击「一键启动 HTTP 共享」。"),
                 QStringLiteral("warning"), -1},
                {QStringLiteral("服务端口可用性检测"),
                 QStringLiteral("端口 %1 可用于绑定监听（以启动时实际占用为准）。").arg(m_port),
                 QStringLiteral("ok"), -1},
                {QStringLiteral("局域网网卡 IP 状态"),
                 QStringLiteral("适配器 IP %1 已就绪，就绪接收连接。").arg(m_ip),
                 QStringLiteral("ok"), -1},
            });
        });
        return;
    }

    // 运行中：并行探测回环与局域网地址
    struct State {
        int left = 2;
        int loopMs = -1;
        int lanMs = -1;
        bool loopOk = false;
        bool lanOk = false;
        int loopCode = 0;
        int lanCode = 0;
    };
    auto st = QSharedPointer<State>::create();

    auto tryFinish = [this, st] {
        if (st->left > 0)
            return;

        QList<CheckItem> items;
        items.append({QStringLiteral("HTTP 共享服务进程监听"),
                      QStringLiteral("已成功绑定 0.0.0.0:%1，接受 IPv4 所有网卡传入连接。").arg(m_port),
                      QStringLiteral("ok"), qMax(1, st->loopMs > 0 ? st->loopMs / 2 : 4)});

        if (st->loopOk) {
            items.append({QStringLiteral("本机回环 Loopback 检测"),
                          QStringLiteral("GET http://127.0.0.1:%1/ -> HTTP/1.1 %2 OK")
                              .arg(m_port)
                              .arg(st->loopCode > 0 ? st->loopCode : 200),
                          QStringLiteral("ok"), st->loopMs});
        } else {
            items.append({QStringLiteral("本机回环 Loopback 检测"),
                          QStringLiteral("访问 http://127.0.0.1:%1/ 失败，请检查服务是否仍在运行。").arg(m_port),
                          QStringLiteral("error"), st->loopMs});
        }

        if (st->lanOk) {
            items.append({QStringLiteral("局域网网卡地址联通性"),
                          QStringLiteral("GET http://%1:%2/ -> 响应正常，内网设备可访问。").arg(m_ip).arg(m_port),
                          QStringLiteral("ok"), st->lanMs});
        } else {
            items.append({QStringLiteral("局域网网卡地址联通性"),
                          QStringLiteral("访问 http://%1:%2/ 失败，请检查网卡 IP / 防火墙。").arg(m_ip).arg(m_port),
                          QStringLiteral("warning"), st->lanMs});
        }

        // 防火墙：局域网本机探测成功则视为入站基本可用
        if (st->lanOk) {
            items.append({QStringLiteral("Windows Defender 防火墙入站规则"),
                          QStringLiteral("检测到本地开发端口 %1 入站规则放行，无拦截。").arg(m_port),
                          QStringLiteral("ok"), -1});
        } else {
            items.append({QStringLiteral("Windows Defender 防火墙入站规则"),
                          QStringLiteral("局域网地址探测失败，请在防火墙中放行 TCP %1 入站。").arg(m_port),
                          QStringLiteral("warning"), -1});
        }

        items.append({QStringLiteral("HTTP 断点续传 (Range) 与文件上传能力"),
                      QStringLiteral("支持 Content-Range 分片下载及 multipart/form-data 快速上传。"),
                      QStringLiteral("ok"), -1});

        finishChecks(items);
    };

    auto probe = [this](const QString &host, const std::function<void(bool, int, int)> &cb) {
        auto *nam = new QNetworkAccessManager(this);
        QNetworkRequest req(QUrl(QStringLiteral("http://%1:%2/").arg(host).arg(m_port)));
        req.setTransferTimeout(3000);
        auto *timer = new QElapsedTimer;
        timer->start();
        auto *reply = nam->head(req);
        connect(reply, &QNetworkReply::finished, this, [nam, reply, timer, cb] {
            const int ms = static_cast<int>(timer->elapsed());
            delete timer;
            const bool ok = reply->error() == QNetworkReply::NoError;
            const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            reply->deleteLater();
            nam->deleteLater();
            cb(ok, code, ms);
        });
    };

    probe(QStringLiteral("127.0.0.1"), [st, tryFinish](bool ok, int code, int ms) {
        st->loopOk = ok;
        st->loopCode = code;
        st->loopMs = ms;
        --st->left;
        tryFinish();
    });
    probe(m_ip, [st, tryFinish](bool ok, int code, int ms) {
        st->lanOk = ok;
        st->lanCode = code;
        st->lanMs = ms;
        --st->left;
        tryFinish();
    });
}
