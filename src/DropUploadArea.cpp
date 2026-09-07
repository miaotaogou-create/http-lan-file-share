#include "DropUploadArea.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
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
    setAcceptDrops(true);
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(48);

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(16, 10, 16, 10);
    lay->setSpacing(8);
    lay->setAlignment(Qt::AlignCenter);

    m_iconLabel = new QLabel;
    m_iconLabel->setFixedSize(18, 18);
    m_iconLabel->setPixmap(loadBoltPixmap(18));

    m_textLabel = new QLabel(QStringLiteral(
        "支持局域网千兆极速互传：拖拽任意本地文件到此处，或点击选择直接加入 HTTP 共享"));
    m_textLabel->setWordWrap(true);
    m_textLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    lay->addWidget(m_iconLabel, 0, Qt::AlignVCenter);
    lay->addWidget(m_textLabel, 0, Qt::AlignVCenter);

    setDragHover(false);
}

void DropUploadArea::setDragHover(bool hover)
{
    m_dragHover = hover;
    if (hover) {
        setStyleSheet(QStringLiteral(
            "QFrame#DropUploadArea {"
            "  background-color: rgba(6, 78, 59, 0.45);"
            "  border: 2px dashed #10B981;"
            "  border-radius: 8px;"
            "}"
            "QLabel { background: transparent; }"));
        m_textLabel->setStyleSheet(QStringLiteral(
            "color:#6EE7B7;font-size:12px;font-weight:700;background:transparent;"));
        m_textLabel->setText(QStringLiteral("释放文件即可加入共享目录"));
    } else {
        setStyleSheet(QStringLiteral(
            "QFrame#DropUploadArea {"
            "  background-color: #070d18;"
            "  border: 1px dashed #0284c7;"
            "  border-radius: 8px;"
            "}"
            "QFrame#DropUploadArea:hover {"
            "  background-color: #0c1c32;"
            "  border-color: #00d2ff;"
            "}"
            "QLabel { background: transparent; }"));
        m_textLabel->setStyleSheet(QStringLiteral(
            "color:#CBD5E1;font-size:12px;background:transparent;"));
        m_textLabel->setText(QStringLiteral(
            "支持局域网千兆极速互传：拖拽任意本地文件到此处，或点击选择直接加入 HTTP 共享"));
    }
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
