#include "UacAuthDialog.h"

#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QShowEvent>
#include <QSvgRenderer>
#include <QVBoxLayout>
#include <QWidget>

namespace {

QPixmap loadShieldPm(int size)
{
    QSvgRenderer renderer(QStringLiteral(":/icons/shield_alert_amber.svg"));
    if (!renderer.isValid())
        return {};
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&p, QRectF(0, 0, size, size));
    return pm;
}

} // namespace

UacAuthDialog::UacAuthDialog(bool alreadyElevated, QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::Dialog)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setModal(true);
    setWindowModality(Qt::ApplicationModal);
    initUi(alreadyElevated);
}

bool UacAuthDialog::ask(bool alreadyElevated, QWidget *parent)
{
    UacAuthDialog dlg(alreadyElevated, parent);
    return dlg.exec() == QDialog::Accepted;
}

void UacAuthDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    layoutOverlay();
}

void UacAuthDialog::layoutOverlay()
{
    // 铺满父窗口（或屏幕），卡片居中
    if (QWidget *pw = parentWidget()) {
        if (QWidget *win = pw->window()) {
            setGeometry(win->geometry());
            return;
        }
    }
    if (QScreen *sc = screen())
        setGeometry(sc->availableGeometry());
}

void UacAuthDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0, 204)); // ~80% 黑蒙层
}

void UacAuthDialog::initUi(bool alreadyElevated)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addStretch(1);

    auto *row = new QHBoxLayout;
    row->setContentsMargins(24, 0, 24, 0);
    row->addStretch(1);

    auto *cardWrap = new QWidget;
    cardWrap->setFixedWidth(520);
    auto *wrapLay = new QVBoxLayout(cardWrap);
    wrapLay->setContentsMargins(8, 8, 8, 8); // 给阴影留边

    auto *container = new QWidget;
    container->setObjectName(QStringLiteral("dialogContainer"));
    container->setAttribute(Qt::WA_StyledBackground, true);
    container->setStyleSheet(QStringLiteral(
        "QWidget#dialogContainer {"
        "  background-color:#050B14;"
        "  border:1.5px solid #B45309;"
        "  border-radius:10px;"
        "}"));
    auto *shadow = new QGraphicsDropShadowEffect(container);
    shadow->setBlurRadius(24);
    shadow->setColor(QColor(180, 83, 9, 100));
    shadow->setOffset(0, 4);
    container->setGraphicsEffect(shadow);

    auto *containerLay = new QVBoxLayout(container);
    containerLay->setContentsMargins(0, 0, 0, 0);
    containerLay->setSpacing(0);

    // ---- 顶部预警标头 ----
    auto *header = new QWidget;
    header->setObjectName(QStringLiteral("headerWidget"));
    header->setAttribute(Qt::WA_StyledBackground, true);
    header->setStyleSheet(QStringLiteral(
        "QWidget#headerWidget {"
        "  background-color:#261B0E;"
        "  border-top-left-radius:9px;"
        "  border-top-right-radius:9px;"
        "  border-bottom:1px solid #452D10;"
        "}"));
    auto *headerLay = new QHBoxLayout(header);
    headerLay->setContentsMargins(18, 16, 18, 16);
    headerLay->setSpacing(14);

    auto *shieldIcon = new QLabel;
    shieldIcon->setFixedSize(38, 38);
    shieldIcon->setPixmap(loadShieldPm(38));
    headerLay->addWidget(shieldIcon, 0, Qt::AlignVCenter);

    auto *titleCol = new QVBoxLayout;
    titleCol->setSpacing(4);
    auto *title = new QLabel(QStringLiteral("用户账户控制 (User Account Control)"));
    title->setStyleSheet(QStringLiteral(
        "color:#FBBF24;font-size:15px;font-weight:700;background:transparent;"));
    auto *sub = new QLabel(QStringLiteral("你要允许此应用对你的设备进行更改吗?"));
    sub->setStyleSheet(QStringLiteral(
        "color:#FDE68A;font-size:13px;font-weight:500;background:transparent;"));
    titleCol->addWidget(title);
    titleCol->addWidget(sub);
    headerLay->addLayout(titleCol, 1);
    containerLay->addWidget(header);

    // ---- 中部内容 ----
    auto *body = new QWidget;
    auto *bodyLay = new QVBoxLayout(body);
    bodyLay->setContentsMargins(20, 16, 20, 16);
    bodyLay->setSpacing(12);

    auto makeRow = [](const QString &key, const QString &valHtml) -> QWidget * {
        auto *rowW = new QWidget;
        auto *l = new QHBoxLayout(rowW);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(12);
        auto *k = new QLabel(key);
        k->setStyleSheet(QStringLiteral("color:#94A3B8;font-size:13px;background:transparent;"));
        auto *v = new QLabel(valHtml);
        v->setTextFormat(Qt::RichText);
        v->setWordWrap(true);
        v->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        v->setStyleSheet(QStringLiteral("font-size:13px;background:transparent;"));
        l->addWidget(k, 0, Qt::AlignLeft | Qt::AlignVCenter);
        l->addWidget(v, 1, Qt::AlignRight | Qt::AlignVCenter);
        return rowW;
    };

    bodyLay->addWidget(makeRow(
        QStringLiteral("程序名称:"),
        QStringLiteral("<b style='color:#FFFFFF;'>HTTP 局域网极速文件共享客户端</b>")));
    bodyLay->addWidget(makeRow(
        QStringLiteral("执行文件:"),
        QStringLiteral("<span style='color:#38BDF8;font-family:Consolas,\"Cascadia Mono\",monospace;'>"
                       "HttpFileShare.exe</span>")));
    bodyLay->addWidget(makeRow(
        QStringLiteral("请求权限:"),
        QStringLiteral("<b style='color:#F59E0B;'>高级网络适配器 (NIC) IP 别名动态绑定"
                       "与端口 1~65535 监听</b>")));

    auto *divider = new QFrame;
    divider->setFrameShape(QFrame::HLine);
    divider->setFixedHeight(1);
    divider->setStyleSheet(QStringLiteral("background-color:#182842;border:none;"));
    bodyLay->addSpacing(4);
    bodyLay->addWidget(divider);
    bodyLay->addSpacing(4);

    auto *detail = new QLabel(QStringLiteral(
        "授权后，本工具可自动调用 Windows netsh interface ipv4 add address 接口，"
        "向以太网卡追加 192.168.8.x 等嵌入式开发板直连调试段 IP，无需每次手动修改网络属性。"));
    detail->setWordWrap(true);
    detail->setStyleSheet(QStringLiteral(
        "color:#94A3B8;font-size:12px;background:transparent;"));
    bodyLay->addWidget(detail);
    bodyLay->addSpacing(8);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(12);
    btnRow->addStretch(1);

    auto *noBtn = new QPushButton(QStringLiteral("否 (No)"));
    noBtn->setFixedSize(96, 36);
    noBtn->setCursor(Qt::PointingHandCursor);
    noBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color:#172437; border:1px solid #2A3E59; border-radius:6px;"
        "  color:#CBD5E1; font-size:13px; font-weight:700;"
        "}"
        "QPushButton:hover {"
        "  background-color:#223550; border-color:#475569; color:#FFFFFF;"
        "}"
        "QPushButton:pressed { background-color:#0F172A; }"));
    connect(noBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(noBtn);

    const QString yesText = alreadyElevated ? QStringLiteral("保持授权 / 刷新")
                                            : QStringLiteral("是 (Yes - 授予管理员权限)");
    auto *yesBtn = new QPushButton(yesText);
    yesBtn->setMinimumHeight(36);
    yesBtn->setCursor(Qt::PointingHandCursor);
    yesBtn->setDefault(true);
    yesBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color:#F59E0B; border:1px solid #FBBF24; border-radius:6px;"
        "  color:#000000; font-size:13px; font-weight:900; padding:0 18px;"
        "}"
        "QPushButton:hover { background-color:#D97706; border-color:#F59E0B; }"
        "QPushButton:pressed { background-color:#B45309; }"));
    connect(yesBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(yesBtn);

    bodyLay->addLayout(btnRow);
    containerLay->addWidget(body);

    wrapLay->addWidget(container);
    row->addWidget(cardWrap, 0);
    row->addStretch(1);
    root->addLayout(row);
    root->addStretch(1);
}
