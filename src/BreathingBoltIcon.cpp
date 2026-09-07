#include "BreathingBoltIcon.h"

#include <QEasingCurve>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPropertyAnimation>

BreathingBoltIcon::BreathingBoltIcon(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(22, 22);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);

    // 半周期亮、半周期暗，避免 loop 回跳造成闪断
    m_anim = new QPropertyAnimation(this, "breathPhase", this);
    m_anim->setDuration(1500);
    m_anim->setStartValue(0.15);
    m_anim->setKeyValueAt(0.5, 1.0);
    m_anim->setEndValue(0.15);
    m_anim->setEasingCurve(QEasingCurve::InOutSine);
    m_anim->setLoopCount(-1);
    m_anim->start();
}

void BreathingBoltIcon::setBreathPhase(qreal phase)
{
    if (qFuzzyCompare(m_breathPhase, phase))
        return;
    m_breathPhase = phase;
    update();
}

void BreathingBoltIcon::setGlowColor(const QColor &color)
{
    m_baseColor = color;
    update();
}

void BreathingBoltIcon::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const qreal side = qMin(width(), height());
    const qreal scale = side / 24.0;
    p.translate(width() / 2.0, height() / 2.0);
    p.scale(scale, scale);
    p.translate(-12.0, -12.0);

    // 倾斜约 45°、圆角空心闪电
    QPainterPath boltPath;
    boltPath.moveTo(13.2, 2.8);
    boltPath.lineTo(5.0, 11.8);
    boltPath.lineTo(10.5, 12.2);
    boltPath.lineTo(10.8, 21.2);
    boltPath.lineTo(19.0, 12.2);
    boltPath.lineTo(13.5, 11.8);
    boltPath.closeSubpath();

    const int glowAlphaOuter = static_cast<int>(35 * m_breathPhase);
    const int glowAlphaMid = static_cast<int>(75 * m_breathPhase);

    QPen outerGlowPen(QColor(m_baseColor.red(), m_baseColor.green(), m_baseColor.blue(), glowAlphaOuter),
                      6.0 + 2.0 * m_breathPhase, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(outerGlowPen);
    p.setBrush(Qt::NoBrush);
    p.drawPath(boltPath);

    QPen midGlowPen(QColor(m_baseColor.red(), m_baseColor.green(), m_baseColor.blue(), glowAlphaMid),
                    3.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(midGlowPen);
    p.drawPath(boltPath);

    QPen corePen(m_baseColor, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(corePen);
    p.setBrush(QColor(m_baseColor.red(), m_baseColor.green(), m_baseColor.blue(),
                       static_cast<int>(40 * m_breathPhase)));
    p.drawPath(boltPath);

    if (m_breathPhase > 0.6) {
        const int highlightAlpha = static_cast<int>(180 * (m_breathPhase - 0.6) / 0.4);
        QPen highlightPen(QColor(220, 250, 255, highlightAlpha), 1.0, Qt::SolidLine,
                          Qt::RoundCap, Qt::RoundJoin);
        p.setPen(highlightPen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(boltPath);
    }
}
