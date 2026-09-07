#pragma once

#include <QFrame>
#include <QStringList>

class QLabel;

class DropUploadArea : public QFrame
{
    Q_OBJECT
public:
    explicit DropUploadArea(QWidget *parent = nullptr);

signals:
    void filesDropped(const QStringList &filePaths);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void setDragHover(bool hover);
    void refreshTextStyle();

    QLabel *m_textLabel = nullptr;
    bool m_dragHover = false;
    bool m_mouseHover = false;
};
