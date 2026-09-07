#pragma once

#include <QWidget>

class QPropertyAnimation;

class StatusBadgeWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal glowAlpha READ glowAlpha WRITE setGlowAlpha)

public:
    explicit StatusBadgeWidget(QWidget *parent = nullptr);

    void setRunning(bool running, const QString &customText = QString());
    bool isRunning() const { return m_running; }

    qreal glowAlpha() const { return m_glowAlpha; }
    void setGlowAlpha(qreal alpha);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_running = false;
    QString m_text = QStringLiteral("共享服务已停止");
    qreal m_glowAlpha = 0.0;
    QPropertyAnimation *m_anim = nullptr;
};
