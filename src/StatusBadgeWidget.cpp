#include "StatusBadgeWidget.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>

StatusBadgeWidget::StatusBadgeWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(24);
    setAttribute(Qt::WA_OpaquePaintEvent, false);

    m_anim = new QPropertyAnimation(this, "glowAlpha", this);
    m_anim->setDuration(1200);
    m_anim->setStartValue(0.2);
    m_anim->setEndValue(0.9);
    m_anim->setEasingCurve(QEasingCurve::InOutQuad);
    m_anim->setLoopCount(-1);

    setRunning(false);
}

void StatusBadgeWidget::setGlowAlpha(qreal alpha)
{
    m_glowAlpha = alpha;
    update();
}

QSize StatusBadgeWidget::sizeHint() const
{
    QFont f = font();
    f.setPixelSize(11);
    f.setBold(true);
    const int textW = QFontMetrics(f).horizontalAdvance(m_text);
    // 左内边距 + 圆点区 + 间距 + 文字 + 右内边距
    return QSize(12 + 8 + 6 + textW + 12, 24);
}

void StatusBadgeWidget::setRunning(bool running, const QString &customText)
{
    m_running = running;
    if (!customText.isEmpty())
        m_text = customText;
    else
        m_text = m_running ? QStringLiteral("共享服务运行中") : QStringLiteral("共享服务已停止");

    if (m_running) {
        if (m_anim->state() != QAbstractAnimation::Running)
            m_anim->start();
    } else {
        m_anim->stop();
        m_glowAlpha = 0.0;
    }
    setFixedWidth(sizeHint().width());
    update();
}

void StatusBadgeWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = r.height() / 2.0;

    const QColor bgColor = m_running ? QColor(6, 78, 59, 100) : QColor(30, 41, 59, 120);
    const QColor borderColor = m_running ? QColor(16, 185, 129, 200) : QColor(100, 116, 139, 120);
    const QColor textColor = m_running ? QColor(52, 211, 153) : QColor(148, 163, 184);
    const QColor dotColor = m_running ? QColor(16, 185, 129) : QColor(100, 116, 139);

    p.setBrush(bgColor);
    p.setPen(QPen(borderColor, 1.2));
    p.drawRoundedRect(r, radius, radius);

    const qreal dotCenterX = r.left() + radius;
    const qreal dotCenterY = r.center().y();
    const qreal dotRadius = 3.5;

    if (m_running && m_glowAlpha > 0.01) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(16, 185, 129, static_cast<int>(m_glowAlpha * 120)));
        p.drawEllipse(QPointF(dotCenterX, dotCenterY), dotRadius + 2.5, dotRadius + 2.5);
    }

    p.setPen(Qt::NoPen);
    p.setBrush(dotColor);
    p.drawEllipse(QPointF(dotCenterX, dotCenterY), dotRadius, dotRadius);

    QFont f = font();
    f.setPixelSize(11);
    f.setBold(true);
    p.setFont(f);
    p.setPen(textColor);
    const QRectF textRect(dotCenterX + dotRadius + 6, r.top(),
                          r.right() - (dotCenterX + dotRadius + 6) - 8, r.height());
    p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, m_text);
}
