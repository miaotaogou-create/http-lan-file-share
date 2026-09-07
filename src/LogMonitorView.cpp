#include "LogMonitorView.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSvgRenderer>
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

// 心跳脉冲图标：不依赖 SVG 编码，避免资源解析失败时标题左侧空白
QPixmap makePulsePixmap(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal s = size / 24.0;
    p.scale(s, s);
    QPen pen(QColor(0x81, 0x8C, 0xF8), 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QPainterPath path;
    path.moveTo(3.5, 13.5);
    path.lineTo(7.8, 13.5);
    path.lineTo(11.2, 5.5);
    path.lineTo(14.8, 19.5);
    path.lineTo(17.5, 13.5);
    path.lineTo(20.5, 13.5);
    p.drawPath(path);
    return pm;
}

// 把 IP / 路径 / 常见文件名染成青蓝等宽字
QString toRichMessage(const QString &message)
{
    QString html = message.toHtmlEscaped();
    static const QRegularExpression re(
        QStringLiteral(
            R"((?:\d{1,3}(?:\.\d{1,3}){3}(?::\d{1,5})?)"
            R"(|[A-Za-z]:[\\/][^\s<>]+)"
            R"(|[\w.\-]+\.(?:zip|tar\.gz|tgz|gz|7z|rar|exe|msi|dll|pdf|tar)))"));
    QString out;
    out.reserve(html.size() + 64);
    int last = 0;
    auto it = re.globalMatch(html);
    while (it.hasNext()) {
        const auto m = it.next();
        out += html.mid(last, m.capturedStart() - last);
        out += QStringLiteral("<span style='color:#38BDF8;font-family:Consolas,\"Cascadia Mono\",monospace;'>");
        out += m.captured();
        out += QStringLiteral("</span>");
        last = m.capturedEnd();
    }
    out += html.mid(last);
    return QStringLiteral("<span style='color:#E2E8F0;'>%1</span>").arg(out);
}

// 内嵌 SVG（纯 ASCII），避免资源编码踩坑；按 DPR 出图，高分屏不糊
QPixmap renderInlineSvg(const QByteArray &svg, int logicalSize)
{
    QSvgRenderer renderer(svg);
    if (!renderer.isValid())
        return {};
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    const int px = qMax(1, qRound(logicalSize * dpr));
    QPixmap pm(px, px);
    pm.fill(Qt::transparent);
    pm.setDevicePixelRatio(dpr);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&p, QRectF(0, 0, logicalSize, logicalSize));
    return pm;
}

QPixmap makeTypeIcon(const QString &type, int size)
{
    QByteArray svg;
    if (type == QLatin1String("download")) {
        svg = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none">
<path d="M12 3V15M12 15L7.5 10.5M12 15L16.5 10.5" stroke="#10B981" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"/>
<line x1="5.5" y1="20" x2="18.5" y2="20" stroke="#10B981" stroke-width="2.5" stroke-linecap="round"/>
</svg>)SVG";
    } else if (type == QLatin1String("upload")) {
        svg = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none">
<path d="M12 21V9M12 9L7.5 13.5M12 9L16.5 13.5" stroke="#22D3EE" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"/>
<line x1="5.5" y1="4" x2="18.5" y2="4" stroke="#22D3EE" stroke-width="2.5" stroke-linecap="round"/>
</svg>)SVG";
    } else if (type == QLatin1String("start") || type == QLatin1String("stop")) {
        const char *color = (type == QLatin1String("start")) ? "#00E5FF" : "#F87171";
        svg = QByteArray(
                  "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\">"
                  "<line x1=\"12\" y1=\"2.5\" x2=\"12\" y2=\"11.5\" stroke=\"")
              + color
              + "\" stroke-width=\"2.4\" stroke-linecap=\"round\"/>"
                "<path d=\"M18.36 6.64A8.5 8.5 0 1 1 5.64 6.64\" stroke=\""
              + color
              + "\" stroke-width=\"2.4\" stroke-linecap=\"round\" fill=\"none\"/>"
                "</svg>";
    } else if (type == QLatin1String("nic_add") || type == QLatin1String("nic_del")) {
        svg = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none">
<path d="M12 2.5L4.5 5.5V11.8C4.5 16.5 7.7 20.8 12 22C16.3 20.8 19.5 16.5 19.5 11.8V5.5L12 2.5Z" stroke="#F59E0B" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round"/>
</svg>)SVG";
    } else {
        svg = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none">
<path d="M3.5 13.5H7.5L10.8 6L14.2 19L17 13.5H20.5" stroke="#38BDF8" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round"/>
</svg>)SVG";
    }

    QPixmap pm = renderInlineSvg(svg, size);
    if (!pm.isNull())
        return pm;

    // 兜底：若 SVG 解析失败，手绘电源环仍能看见
    QPixmap fallback(size, size);
    fallback.fill(Qt::transparent);
    QPainter p(&fallback);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(0, 229, 255), 2.0, Qt::SolidLine, Qt::RoundCap));
    p.drawEllipse(QRectF(3, 3, size - 6, size - 6));
    return fallback;
}

} // namespace

LogMonitorView::LogMonitorView(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("LogMonitorView"));
    setStyleSheet(QStringLiteral(
        "QWidget#LogMonitorView { background-color:#070e1a; }"
        "QFrame#LogCard {"
        "  background-color:#0b1424;"
        "  border:1px solid #1b2b46;"
        "  border-radius:10px;"
        "}"
        "QScrollArea { background:transparent; border:none; }"
        "QScrollArea > QWidget > QWidget { background:transparent; }"
        "QScrollBar:vertical {"
        "  background:transparent; width:6px; margin:4px 2px 4px 0;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background:#334155; border-radius:3px; min-height:24px;"
        "}"
        "QScrollBar::handle:vertical:hover { background:#475569; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"));

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(24, 14, 24, 16);
    root->setSpacing(0);

    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("LogCard"));
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(20, 18, 20, 16);
    cardLay->setSpacing(14);
    cardLay->addWidget(createHeader(), 0);

    auto *console = new QFrame;
    console->setObjectName(QStringLiteral("LogConsole"));
    console->setAttribute(Qt::WA_StyledBackground, true);
    console->setStyleSheet(QStringLiteral(
        "QFrame#LogConsole {"
        "  background-color:#070d18;"
        "  border:1px solid #182840;"
        "  border-radius:8px;"
        "}"));
    auto *consoleLay = new QVBoxLayout(console);
    consoleLay->setContentsMargins(0, 0, 0, 0);
    consoleLay->setSpacing(0);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *host = new QWidget;
    m_rowsLayout = new QVBoxLayout(host);
    m_rowsLayout->setContentsMargins(12, 8, 12, 8);
    m_rowsLayout->setSpacing(0);

    m_emptyLabel = new QLabel(QStringLiteral(
        "暂无传输事件记录，启动服务或在局域网进行文件互传时将在此实时打印"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setMinimumHeight(120);
    m_emptyLabel->setStyleSheet(QStringLiteral(
        "color:#64748B;font-size:12px;background:transparent;padding:24px;"));
    m_rowsLayout->addWidget(m_emptyLabel);
    m_rowsLayout->addStretch(1);

    scroll->setWidget(host);
    consoleLay->addWidget(scroll, 1);
    cardLay->addWidget(console, 1);
    cardLay->addWidget(createFooter(), 0);

    root->addWidget(card, 1);
}

QWidget *LogMonitorView::createHeader()
{
    auto *w = new QWidget;
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    auto *pulse = new QLabel;
    pulse->setFixedSize(18, 18);
    pulse->setAlignment(Qt::AlignCenter);
    pulse->setStyleSheet(QStringLiteral("background:transparent;border:none;"));
    pulse->setPixmap(makePulsePixmap(18));
    lay->addWidget(pulse, 0, Qt::AlignVCenter);

    auto *title = new QLabel(QStringLiteral("HTTP 传输活动日志与连接监控"));
    title->setStyleSheet(QStringLiteral(
        "color:#FFFFFF;font-size:14px;font-weight:700;background:transparent;"));
    lay->addWidget(title, 0, Qt::AlignVCenter);

    m_countBadge = new QLabel(QStringLiteral("0 条记录"));
    m_countBadge->setAttribute(Qt::WA_StyledBackground, true);
    m_countBadge->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background-color:#1e293b; border:1px solid #334155; border-radius:4px;"
        "  color:#94A3B8; font-size:11px; padding:1px 6px;"
        "}"));
    lay->addWidget(m_countBadge, 0, Qt::AlignVCenter);
    lay->addStretch(1);

    auto *clearBtn = new QPushButton(QStringLiteral(" 清空日志"));
    clearBtn->setIcon(QIcon(loadSvgPm(QStringLiteral(":/icons/trash_delete.svg"), 13)));
    clearBtn->setIconSize(QSize(13, 13));
    clearBtn->setCursor(Qt::PointingHandCursor);
    clearBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background:transparent; border:none; color:#94A3B8;"
        "  font-size:12px; padding:4px 8px; border-radius:4px;"
        "}"
        "QPushButton:hover {"
        "  background-color:rgba(239,68,68,0.15); color:#F87171;"
        "}"));
    connect(clearBtn, &QPushButton::clicked, this, &LogMonitorView::clearClicked);
    lay->addWidget(clearBtn, 0, Qt::AlignVCenter);
    return w;
}

QWidget *LogMonitorView::createFooter()
{
    auto *w = new QWidget;
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    m_listenLabel = new QLabel;
    m_listenLabel->setTextFormat(Qt::RichText);
    m_listenLabel->setStyleSheet(QStringLiteral("color:#64748B;font-size:12px;background:transparent;"));
    setListenStatus(QStringLiteral("—"), false);

    auto *speed = new QLabel(QStringLiteral("LAN Speed: 1000Mbps Duplex Ready"));
    speed->setStyleSheet(QStringLiteral(
        "color:#10B981;font-family:Consolas,'Cascadia Mono',monospace;"
        "font-size:12px;font-weight:500;background:transparent;"));

    lay->addWidget(m_listenLabel, 0, Qt::AlignVCenter);
    lay->addStretch(1);
    lay->addWidget(speed, 0, Qt::AlignVCenter);
    return w;
}

QWidget *LogMonitorView::createRow(const QString &timestamp, const QString &type, const QString &message)
{
    auto *row = new QWidget;
    row->setObjectName(QStringLiteral("logRowItem"));
    row->setFixedHeight(36);
    row->setAttribute(Qt::WA_Hover, true);
    row->setAttribute(Qt::WA_StyledBackground, true);
    row->setStyleSheet(QStringLiteral(
        "QWidget#logRowItem {"
        "  background-color:transparent;"
        "  border-bottom:1px solid #1a2d48;"
        "}"
        "QWidget#logRowItem:hover { background-color:#0c182b; }"));

    auto *lay = new QHBoxLayout(row);
    lay->setContentsMargins(8, 0, 8, 0);
    lay->setSpacing(10);

    auto *clock = new QLabel;
    clock->setFixedSize(12, 12);
    clock->setPixmap(loadSvgPm(QStringLiteral(":/icons/clock_outline.svg"), 12));
    lay->addWidget(clock, 0, Qt::AlignVCenter);

    auto *time = new QLabel(timestamp);
    time->setStyleSheet(QStringLiteral(
        "color:#64748B;font-family:Consolas,'Cascadia Mono',monospace;"
        "font-size:12px;background:transparent;"));
    lay->addWidget(time, 0, Qt::AlignVCenter);

    auto *typeIcon = new QLabel;
    typeIcon->setFixedSize(20, 20);
    typeIcon->setAlignment(Qt::AlignCenter);
    typeIcon->setStyleSheet(QStringLiteral("background:transparent;border:none;"));
    typeIcon->setPixmap(makeTypeIcon(type, 20));
    lay->addWidget(typeIcon, 0, Qt::AlignVCenter);

    auto *content = new QLabel(toRichMessage(message));
    content->setTextFormat(Qt::RichText);
    content->setTextInteractionFlags(Qt::TextSelectableByMouse);
    content->setWordWrap(false);
    content->setStyleSheet(QStringLiteral("font-size:12px;background:transparent;"));
    lay->addWidget(content, 1, Qt::AlignVCenter);
    return row;
}

void LogMonitorView::prependEntry(const QString &timestamp, const QString &type, const QString &message)
{
    if (!m_rowsLayout)
        return;
    // 空态在 index 0，真正行插在 stretch 之前；有日志时隐藏空态
    m_rowsLayout->insertWidget(0, createRow(timestamp, type, message));
    ++m_count;
    syncEmptyState();
    updateCountBadge();
}

void LogMonitorView::clearEntries()
{
    if (!m_rowsLayout)
        return;
    while (QLayoutItem *it = m_rowsLayout->takeAt(0)) {
        if (QWidget *w = it->widget()) {
            if (w != m_emptyLabel)
                w->deleteLater();
        }
        delete it;
    }
    m_rowsLayout->addWidget(m_emptyLabel);
    m_rowsLayout->addStretch(1);
    m_count = 0;
    syncEmptyState();
    updateCountBadge();
}

void LogMonitorView::setListenStatus(const QString &ipPort, bool running)
{
    if (!m_listenLabel)
        return;
    const QString target = ipPort.isEmpty() ? QStringLiteral("—") : ipPort;
    const QString state = running ? QStringLiteral("HTTP Server Active") : QStringLiteral("Stopped");
    m_listenLabel->setText(QStringLiteral(
                               "监听目标: <span style='color:#E2E8F0;font-family:Consolas,\"Cascadia Mono\",monospace;'>%1</span>"
                               " &nbsp;<span style='color:#475569;'>·</span>&nbsp; "
                               "状态: <span style='color:#E2E8F0;'>%2</span>")
                               .arg(target.toHtmlEscaped(), state));
}

void LogMonitorView::updateCountBadge()
{
    if (m_countBadge)
        m_countBadge->setText(QStringLiteral("%1 条记录").arg(m_count));
}

void LogMonitorView::syncEmptyState()
{
    if (!m_emptyLabel)
        return;
    m_emptyLabel->setVisible(m_count == 0);
}
