#pragma once

#include <QWidget>
#include <QString>
#include <QPixmap>

class QrCodeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit QrCodeWidget(QWidget *parent = nullptr);
    void setText(const QString &text);
    QString text() const { return m_text; }

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize sizeHint() const override { return QSize(180, 180); }
    QSize minimumSizeHint() const override { return QSize(120, 120); }

private:
    void rebuild();
    QString m_text;
    QPixmap m_pix;
};
