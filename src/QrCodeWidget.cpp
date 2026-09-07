#include "QrCodeWidget.h"

#include <QPainter>
#include <QPaintEvent>

#include "qrcodegen.hpp"

QrCodeWidget::QrCodeWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(140, 140);
}

void QrCodeWidget::setText(const QString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    rebuild();
    update();
}

void QrCodeWidget::rebuild()
{
    m_pix = QPixmap();
    if (m_text.isEmpty())
        return;
    try {
        using qrcodegen::QrCode;
        const QrCode qr = QrCode::encodeText(m_text.toUtf8().constData(), QrCode::Ecc::MEDIUM);
        const int n = qr.getSize();
        const int scale = 4;
        const int border = 2;
        const int dim = (n + border * 2) * scale;
        QImage img(dim, dim, QImage::Format_RGB32);
        img.fill(Qt::white);
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                if (!qr.getModule(x, y))
                    continue;
                for (int dy = 0; dy < scale; ++dy) {
                    for (int dx = 0; dx < scale; ++dx) {
                        img.setPixel((x + border) * scale + dx, (y + border) * scale + dy,
                                     qRgb(7, 13, 24));
                    }
                }
            }
        }
        m_pix = QPixmap::fromImage(img);
    } catch (...) {
        m_pix = QPixmap();
    }
}

void QrCodeWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(QStringLiteral("#070d18")));
    if (m_pix.isNull()) {
        p.setPen(QColor(QStringLiteral("#64748b")));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("启动共享后显示二维码"));
        return;
    }
    const QSize target = m_pix.size().scaled(size() - QSize(16, 16), Qt::KeepAspectRatio);
    const QRect r(QPoint((width() - target.width()) / 2, (height() - target.height()) / 2), target);
    p.fillRect(r.adjusted(-6, -6, 6, 6), Qt::white);
    p.drawPixmap(r, m_pix);
}
