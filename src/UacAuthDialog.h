#pragma once

#include <QDialog>

class UacAuthDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UacAuthDialog(bool alreadyElevated, QWidget *parent = nullptr);

    // 返回 true 表示用户点了授权/保持
    static bool ask(bool alreadyElevated, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void initUi(bool alreadyElevated);
    void layoutOverlay();
};
