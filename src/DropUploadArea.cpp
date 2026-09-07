#include "DropUploadArea.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QSizePolicy>
#include <QSvgRenderer>
#include <QUrl>

namespace {

QPixmap loadBoltPixmap(int size)
{
    QSvgRenderer renderer(QStringLiteral(":/icons/lightning_bolt.svg"));
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

DropUploadArea::DropUploadArea(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("DropUploadArea"));
    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_StyledBackground, false);
    setAcceptDrops(true);
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(48);

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(16, 0, 16, 0);
    lay->setSpacing(8);
    lay->setAlignment(Qt::AlignCenter);

    m_iconLabel = new QLabel;
    m_iconLabel->setFixedSize(18, 18);
    m_iconLabel->setPixmap(loadBoltPixmap(18));
    m_iconLabel->setStyleSheet(QStringLiteral("background:transparent;"));

    m_textLabel = new QLabel(QStringLiteral(
        "支持局域网千兆极速互传：拖拽任意本地文件到此处，或点击选择直接加入 HTTP 共享"));
    m_textLabel->setWordWrap(false);
    m_textLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_textLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    lay->addWidget(m_iconLabel, 0, Qt::AlignVCenter);
    lay->addWidget(m_textLabel, 0, Qt::AlignVCenter);

    refreshTextStyle();
}

void DropUploadArea::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 半像素对齐，四边虚线都画全，避免 QSS 圆角虚线裁掉顶边
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = 8.0;

    QColor bg;
    QColor border;
    qreal borderW = 1.0;
    if (m_dragHover) {
        bg = QColor(6, 78, 59, 115);
        border = QColor(QStringLiteral("#10B981"));
        borderW = 2.0;
    } else if (m_mouseHover) {
        bg = QColor(QStringLiteral("#0c1c32"));
        border = QColor(QStringLiteral("#00d2ff"));
    } else {
        bg = QColor(QStringLiteral("#070d18"));
        border = QColor(QStringLiteral("#0284c7"));
    }

    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(r, radius, radius);

    QPen pen(border);
    pen.setWidthF(borderW);
    pen.setStyle(Qt::DashLine);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setBrush(Qt::NoBrush);
    p.setPen(pen);
    p.drawRoundedRect(r, radius, radius);
}

void DropUploadArea::enterEvent(QEnterEvent *event)
{
    QFrame::enterEvent(event);
    m_mouseHover = true;
    update();
}

void DropUploadArea::leaveEvent(QEvent *event)
{
    QFrame::leaveEvent(event);
    m_mouseHover = false;
    update();
}

void DropUploadArea::refreshTextStyle()
{
    if (m_dragHover) {
        m_textLabel->setStyleSheet(QStringLiteral(
            "color:#6EE7B7;font-size:13px;font-weight:700;background:transparent;"));
        m_textLabel->setText(QStringLiteral("释放文件即可加入共享目录"));
    } else {
        m_textLabel->setStyleSheet(QStringLiteral(
            "color:#CBD5E1;font-size:13px;background:transparent;"));
        m_textLabel->setText(QStringLiteral(
            "支持局域网千兆极速互传：拖拽任意本地文件到此处，或点击选择直接加入 HTTP 共享"));
    }
}

void DropUploadArea::setDragHover(bool hover)
{
    if (m_dragHover == hover)
        return;
    m_dragHover = hover;
    refreshTextStyle();
    update();
}

void DropUploadArea::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        setDragHover(true);
    }
}

void DropUploadArea::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event);
    setDragHover(false);
}

void DropUploadArea::dropEvent(QDropEvent *event)
{
    setDragHover(false);
    const QMimeData *mime = event->mimeData();
    if (!mime || !mime->hasUrls())
        return;

    QStringList files;
    for (const QUrl &url : mime->urls()) {
        if (!url.isLocalFile())
            continue;
        const QString path = url.toLocalFile();
        const QFileInfo fi(path);
        if (fi.isFile())
            files.append(path);
    }
    if (!files.isEmpty()) {
        event->acceptProposedAction();
        emit filesDropped(files);
    }
}

void DropUploadArea::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const QStringList files = QFileDialog::getOpenFileNames(
            this, QStringLiteral("选择加入共享的文件"));
        if (!files.isEmpty())
            emit filesDropped(files);
    }
    QFrame::mousePressEvent(event);
}
