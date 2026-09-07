#pragma once

#include <QColor>
#include <QWidget>

class QPropertyAnimation;

class BreathingBoltIcon : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal breathPhase READ breathPhase WRITE setBreathPhase)

public:
    explicit BreathingBoltIcon(QWidget *parent = nullptr);

    qreal breathPhase() const { return m_breathPhase; }
    void setBreathPhase(qreal phase);

    void setGlowColor(const QColor &color);

    QSize sizeHint() const override { return QSize(22, 22); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal m_breathPhase = 0.5;
    QColor m_baseColor = QColor(0, 229, 255); // #00E5FF
    QPropertyAnimation *m_anim = nullptr;
};
