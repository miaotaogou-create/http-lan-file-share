#include "WebDeliveryView.h"

#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSvgRenderer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QPixmap loadSvgPm(const QString &path, int size)
{
    QSvgRenderer renderer(path);
    if (!renderer.isValid())
        return {};
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&p, QRectF(0, 0, size, size));
    return pm;
}

QString iconPathForName(const QString &fileName)
{
    const QString ext = QFileInfo(fileName).suffix().toLower();
    if (ext == QLatin1String("zip") || ext == QLatin1String("rar") || ext == QLatin1String("7z")
        || ext == QLatin1String("tar") || ext == QLatin1String("gz") || ext == QLatin1String("tgz"))
        return QStringLiteral(":/icons/file_zip.svg");
    if (ext == QLatin1String("exe") || ext == QLatin1String("msi") || ext == QLatin1String("dll"))
        return QStringLiteral(":/icons/file_exe.svg");
    return QStringLiteral(":/icons/file_document.svg");
}

QString iconBorderForName(const QString &fileName)
{
    const QString ext = QFileInfo(fileName).suffix().toLower();
    if (ext == QLatin1String("zip") || ext == QLatin1String("rar") || ext == QLatin1String("7z")
        || ext == QLatin1String("tar") || ext == QLatin1String("gz") || ext == QLatin1String("tgz"))
        return QStringLiteral("#D97706");
    if (ext == QLatin1String("exe") || ext == QLatin1String("msi") || ext == QLatin1String("dll"))
        return QStringLiteral("#9333EA");
    return QStringLiteral("#0284C7");
}

// QLabel 的 QSS border-radius 在 Windows 上经常画成直角，手绘胶囊更稳
class CapsuleBadge : public QWidget
{
public:
    explicit CapsuleBadge(const QString &text, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_text(text)
    {
        setFixedHeight(24);
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        QFont f = font();
        f.setPixelSize(11);
        f.setBold(true);
        setFont(f);
        setFixedWidth(QFontMetrics(f).horizontalAdvance(m_text) + 24);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const qreal radius = r.height() / 2.0;
        p.setBrush(QColor(8, 51, 68));
        p.setPen(QPen(QColor(34, 211, 238, 140), 1));
        p.drawRoundedRect(r, radius, radius);
        p.setPen(QColor(103, 232, 249));
        p.drawText(r, Qt::AlignCenter, m_text);
    }

private:
    QString m_text;
};

} // namespace

WebDeliveryView::WebDeliveryView(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("WebDeliveryView"));
    setStyleSheet(QStringLiteral(
        "QWidget#WebDeliveryView { background-color:#070e1a; color:#F8FAFC; }"
        "QFrame#DeliveryCard {"
        "  background-color:#0b1627;"
        "  border:1px solid #1d3153;"
        "  border-radius:16px;"
        "}"
        "QScrollArea { background:transparent; border:none; }"
        "QScrollArea > QWidget > QWidget { background:transparent; }"));

    auto *root = new QHBoxLayout(this);
    // 与参考一致：左右窄边，卡片横向铺满可用区
    root->setContentsMargins(24, 14, 24, 16);
    root->setSpacing(0);

    auto *column = new QWidget;
    column->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *colLay = new QVBoxLayout(column);
    colLay->setContentsMargins(0, 0, 0, 0);
    colLay->setSpacing(12);

    colLay->addWidget(createTopNavBar(), 0);

    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("DeliveryCard"));
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(28, 22, 28, 22);
    cardLay->setSpacing(16);
    cardLay->addWidget(createHeroHeader(), 0);
    cardLay->addWidget(createSearchBar(), 0);

    // 文件列表占满卡片剩余高度，多了再滚
    auto *listScroll = new QScrollArea;
    listScroll->setWidgetResizable(true);
    listScroll->setFrameShape(QFrame::NoFrame);
    listScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *listHost = new QWidget;
    m_fileListLayout = new QVBoxLayout(listHost);
    m_fileListLayout->setContentsMargins(0, 0, 0, 0);
    m_fileListLayout->setSpacing(12);
    m_fileListLayout->addStretch(1);
    listScroll->setWidget(listHost);
    cardLay->addWidget(listScroll, 1);

    cardLay->addWidget(createBoardGuideCard(), 0);
    colLay->addWidget(card, 1);

    root->addWidget(column, 1);
}

QWidget *WebDeliveryView::createTopNavBar()
{
    auto *w = new QWidget;
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);

    auto *backBtn = new QPushButton(QStringLiteral(" 返回 Windows 客户端控制台"));
    backBtn->setIcon(QIcon(loadSvgPm(QStringLiteral(":/icons/arrow_left.svg"), 16)));
    backBtn->setIconSize(QSize(16, 16));
    backBtn->setFixedHeight(36);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color:#06182C; border:1px solid #0E365E; border-radius:8px;"
        "  color:#38BDF8; font-size:13px; font-weight:700; padding:0 16px;"
        "}"
        "QPushButton:hover {"
        "  background-color:#0C2849; border-color:#00D2FF; color:#E0F2FE;"
        "}"));
    connect(backBtn, &QPushButton::clicked, this, &WebDeliveryView::backToDashboardClicked);

    m_readyLabel = new QLabel;
    m_readyLabel->setTextFormat(Qt::RichText);
    m_readyLabel->setStyleSheet(QStringLiteral("color:#94A3B8;font-size:13px;background:transparent;"));

    lay->addWidget(backBtn, 0, Qt::AlignVCenter);
    lay->addStretch(1);
    lay->addWidget(m_readyLabel, 0, Qt::AlignVCenter);
    return w;
}

QWidget *WebDeliveryView::createHeroHeader()
{
    auto *w = new QWidget;
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 8, 0, 8);
    lay->setSpacing(16);

    auto *left = new QVBoxLayout;
    left->setSpacing(8);

    auto *badgeRow = new QHBoxLayout;
    badgeRow->setSpacing(10);
    auto *serviceBadge = new CapsuleBadge(QStringLiteral("HTTP LAN File Delivery"));
    m_runningLabel = new QLabel;
    m_runningLabel->setTextFormat(Qt::RichText);
    m_runningLabel->setStyleSheet(QStringLiteral("font-size:12px;font-weight:700;background:transparent;"));
    badgeRow->addWidget(serviceBadge, 0, Qt::AlignVCenter);
    badgeRow->addWidget(m_runningLabel, 0, Qt::AlignVCenter);
    badgeRow->addStretch(1);
    left->addLayout(badgeRow);

    auto *title = new QLabel(QStringLiteral("局域网极速文件共享与提货端"));
    title->setStyleSheet(QStringLiteral("color:#FFFFFF;font-size:24px;font-weight:900;background:transparent;"));
    left->addWidget(title);

    m_hostLabel = new QLabel;
    m_hostLabel->setTextFormat(Qt::RichText);
    m_hostLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_hostLabel->setStyleSheet(QStringLiteral("color:#94A3B8;font-size:13px;background:transparent;"));
    left->addWidget(m_hostLabel);

    lay->addLayout(left, 1);

    auto *uploadBtn = new QPushButton(QStringLiteral(" 从本机/手机上传文件到电脑"));
    uploadBtn->setIcon(QIcon(loadSvgPm(QStringLiteral(":/icons/upload_icon.svg"), 16)));
    uploadBtn->setIconSize(QSize(16, 16));
    uploadBtn->setFixedHeight(42);
    uploadBtn->setCursor(Qt::PointingHandCursor);
    uploadBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color:#059669; border:1px solid #10B981; border-radius:8px;"
        "  color:#FFFFFF; font-size:14px; font-weight:700; padding:0 20px;"
        "}"
        "QPushButton:hover { background-color:#10B981; border-color:#34D399; }"
        "QPushButton:pressed { background-color:#047857; }"));
    connect(uploadBtn, &QPushButton::clicked, this, &WebDeliveryView::uploadClicked);
    lay->addWidget(uploadBtn, 0, Qt::AlignVCenter);
    return w;
}

QWidget *WebDeliveryView::createSearchBar()
{
    auto *w = new QWidget;
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 4, 0, 4);

    m_listTitle = new QLabel;
    m_listTitle->setTextFormat(Qt::RichText);
    m_listTitle->setStyleSheet(QStringLiteral("color:#F1F5F9;font-size:15px;font-weight:700;background:transparent;"));

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(QStringLiteral("快速过滤文件名..."));
    m_searchEdit->setFixedWidth(280);
    m_searchEdit->setFixedHeight(36);
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background-color:#061121; border:1px solid #152945; border-radius:6px;"
        "  color:#E2E8F0; font-size:13px; padding:0 12px;"
        "}"
        "QLineEdit:focus { border:1px solid #00D2FF; }"));
    connect(m_searchEdit, &QLineEdit::textChanged, this, &WebDeliveryView::onSearchTextChanged);

    lay->addWidget(m_listTitle, 0, Qt::AlignVCenter);
    lay->addStretch(1);
    lay->addWidget(m_searchEdit, 0, Qt::AlignVCenter);
    return w;
}

QWidget *WebDeliveryView::createFileCard(const WebDeliveryItem &item)
{
    auto *card = new QWidget;
    card->setObjectName(QStringLiteral("fileCard"));
    card->setProperty("fileName", item.fileName);
    card->setFixedHeight(72);
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setStyleSheet(QStringLiteral(
        "QWidget#fileCard {"
        "  background-color:#040E1E; border:1px solid #10233B; border-radius:8px;"
        "}"
        "QWidget#fileCard:hover {"
        "  background-color:#07152B; border-color:#1A3A63;"
        "}"));

    auto *lay = new QHBoxLayout(card);
    lay->setContentsMargins(16, 0, 16, 0);
    lay->setSpacing(16);

    auto *iconBox = new QLabel;
    iconBox->setFixedSize(40, 40);
    iconBox->setAlignment(Qt::AlignCenter);
    iconBox->setStyleSheet(QStringLiteral(
                               "background-color:#061326;border:1px solid %1;border-radius:8px;")
                               .arg(iconBorderForName(item.fileName)));
    iconBox->setPixmap(loadSvgPm(iconPathForName(item.fileName), 22));
    lay->addWidget(iconBox, 0, Qt::AlignVCenter);

    auto *info = new QVBoxLayout;
    info->setSpacing(4);
    info->setContentsMargins(0, 0, 0, 0);
    auto *name = new QLabel(item.fileName);
    name->setStyleSheet(QStringLiteral("color:#FFFFFF;font-size:14px;font-weight:700;background:transparent;"));
    auto *meta = new QLabel(QStringLiteral(
                                "<span style='color:#38BDF8;font-weight:700;'>%1</span>"
                                "  <span style='color:#475569;'>·</span>  "
                                "<span style='color:#94A3B8;'>%2</span>"
                                "  <span style='color:#475569;'>·</span>  "
                                "<span style='color:#64748B;'>已下载 %3 次</span>")
                                .arg(item.fileSize, item.modifyTime)
                                .arg(item.downloadCount));
    meta->setTextFormat(Qt::RichText);
    meta->setStyleSheet(QStringLiteral("font-size:12px;background:transparent;"));
    info->addWidget(name);
    info->addWidget(meta);
    lay->addLayout(info, 1);

    auto *curlBtn = new QPushButton(QStringLiteral(" >_  curl"));
    curlBtn->setFixedSize(76, 32);
    curlBtn->setCursor(Qt::PointingHandCursor);
    curlBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color:#07192F; border:1px solid #16365C; border-radius:6px;"
        "  color:#38BDF8; font-size:12px; font-weight:700;"
        "}"
        "QPushButton:hover { background-color:#0D2748; border-color:#00D2FF; color:#E0F2FE; }"));
    connect(curlBtn, &QPushButton::clicked, this, [this, name = item.fileName] {
        emit curlCopyClicked(name);
    });
    lay->addWidget(curlBtn, 0, Qt::AlignVCenter);

    auto *dlBtn = new QPushButton(QStringLiteral(" ↓ 高速下载"));
    dlBtn->setFixedSize(104, 32);
    dlBtn->setCursor(Qt::PointingHandCursor);
    dlBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color:#0284C7; border:1px solid #38BDF8; border-radius:6px;"
        "  color:#FFFFFF; font-size:12px; font-weight:700;"
        "}"
        "QPushButton:hover { background-color:#0369A1; border-color:#7DD3FC; }"
        "QPushButton:pressed { background-color:#075985; }"));
    connect(dlBtn, &QPushButton::clicked, this,
            [this, path = item.filePath, name = item.fileName] {
                emit downloadItemClicked(path, name);
            });
    lay->addWidget(dlBtn, 0, Qt::AlignVCenter);
    return card;
}

QWidget *WebDeliveryView::createBoardGuideCard()
{
    auto *w = new QWidget;
    w->setObjectName(QStringLiteral("guideBox"));
    w->setAttribute(Qt::WA_StyledBackground, true);
    w->setStyleSheet(QStringLiteral(
        "QWidget#guideBox {"
        "  background-color:#030814; border:1px solid #0F2036; border-radius:8px;"
        "}"));

    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(18, 16, 18, 18);
    lay->setSpacing(12);

    auto *title = new QLabel(QStringLiteral(
        "<span style='color:#38BDF8;font-weight:700;'>&gt;_</span> "
        "<b>开发板 (飞腾/树莓派/RK3588/麒麟 Linux) 命令行一键提货指南:</b>"));
    title->setTextFormat(Qt::RichText);
    title->setStyleSheet(QStringLiteral("color:#38BDF8;font-size:13px;background:transparent;"));
    lay->addWidget(title);

    auto *cmds = new QHBoxLayout;
    cmds->setSpacing(14);
    const QString boxCss = QStringLiteral(
        "background-color:#02060D;border:1px solid #0B1728;border-radius:6px;"
        "font-family:Consolas,'Cascadia Mono',monospace;font-size:12px;padding:10px 14px;");

    m_guideCmd1 = new QLabel;
    m_guideCmd1->setTextFormat(Qt::RichText);
    m_guideCmd1->setWordWrap(true);
    m_guideCmd1->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_guideCmd1->setStyleSheet(boxCss);

    m_guideCmd2 = new QLabel;
    m_guideCmd2->setTextFormat(Qt::RichText);
    m_guideCmd2->setWordWrap(true);
    m_guideCmd2->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_guideCmd2->setStyleSheet(boxCss);

    cmds->addWidget(m_guideCmd1, 1);
    cmds->addWidget(m_guideCmd2, 1);
    lay->addLayout(cmds);
    return w;
}

void WebDeliveryView::setHostInfo(const QString &url, bool running)
{
    m_hostUrl = url;
    m_running = running;
    if (running) {
        m_readyLabel->setText(QStringLiteral(
            "<span style='color:#10B981;'>●</span> 提货专线已就绪 · 局域网直连"));
        m_runningLabel->setText(QStringLiteral(
            "<span style='color:#10B981;'>●</span> <span style='color:#34D399;'>服务运行中</span>"));
        m_hostLabel->setText(QStringLiteral(
                                 "宿主机节点: <span style='color:#38BDF8;font-family:Consolas;'>%1</span>")
                                 .arg(url.toHtmlEscaped()));
    } else {
        m_readyLabel->setText(QStringLiteral(
            "<span style='color:#64748B;'>●</span> 提货专线未启动 · 请先开启共享"));
        m_runningLabel->setText(QStringLiteral(
            "<span style='color:#64748B;'>●</span> <span style='color:#94A3B8;'>服务已停止</span>"));
        m_hostLabel->setText(QStringLiteral("宿主机节点: <span style='color:#64748B;'>— (服务未启动)</span>"));
    }
    updateGuideCmds();
}

void WebDeliveryView::setItems(const QVector<WebDeliveryItem> &items)
{
    m_items = items;
    rebuildFileCards();
    updateListTitle();
    onSearchTextChanged(m_searchEdit ? m_searchEdit->text() : QString());
}

void WebDeliveryView::rebuildFileCards()
{
    if (!m_fileListLayout)
        return;
    while (QLayoutItem *it = m_fileListLayout->takeAt(0)) {
        if (it->widget())
            it->widget()->deleteLater();
        delete it;
    }
    for (const WebDeliveryItem &item : m_items)
        m_fileListLayout->addWidget(createFileCard(item));
    m_fileListLayout->addStretch(1);
}

void WebDeliveryView::updateListTitle()
{
    if (!m_listTitle)
        return;
    m_listTitle->setText(QStringLiteral(
                             "可供提货的文件清单 <span style='color:#38BDF8;'>(%1 项)</span>")
                             .arg(m_items.size()));
}

void WebDeliveryView::updateGuideCmds()
{
    const QString base = m_hostUrl.isEmpty() ? QStringLiteral("http://127.0.0.1:8899") : m_hostUrl;
    const QString sample = base + QStringLiteral("/download/qt_cross_smoke_bundle.tar.gz");
    if (m_guideCmd1) {
        m_guideCmd1->setText(QStringLiteral(
                                 "<span style='color:#64748B;'># 方式一: 使用 wget 高速获取产物</span><br/>"
                                 "<span style='color:#10B981;'>wget %1</span>")
                                 .arg(sample.toHtmlEscaped()));
    }
    if (m_guideCmd2) {
        m_guideCmd2->setText(QStringLiteral(
                                 "<span style='color:#64748B;'># 方式二: 使用 curl 下载并实时解压</span><br/>"
                                 "<span style='color:#10B981;'>curl -s %1 | tar -xz</span>")
                                 .arg(sample.toHtmlEscaped()));
    }
}

void WebDeliveryView::onSearchTextChanged(const QString &text)
{
    const QString keyword = text.trimmed().toLower();
    if (!m_fileListLayout)
        return;
    for (int i = 0; i < m_fileListLayout->count(); ++i) {
        QWidget *card = m_fileListLayout->itemAt(i)->widget();
        if (!card)
            continue;
        const QString name = card->property("fileName").toString().toLower();
        card->setVisible(keyword.isEmpty() || name.contains(keyword));
    }
}
