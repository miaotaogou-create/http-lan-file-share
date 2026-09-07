#pragma once

#include <QDialog>
#include <QList>
#include <QString>

class QLabel;
class QScrollArea;
class QToolButton;
class QTimer;
class QVBoxLayout;
class QWidget;

class SelfCheckDialog : public QDialog
{
    Q_OBJECT
public:
    SelfCheckDialog(bool running, quint16 port, const QString &ip, QWidget *parent = nullptr);

private slots:
    void runCheck();
    void onSpinTick();

private:
    struct CheckItem {
        QString title;
        QString detail;
        QString status; // ok | warning | error
        int latencyMs = -1;
    };

    void rebuildResults(const QList<CheckItem> &items);
    void setCheckingUi(bool checking);
    void updateCommandTexts();
    void finishChecks(const QList<CheckItem> &items);

    bool m_running = false;
    quint16 m_port = 0;
    QString m_ip;

    QWidget *m_resultsHost = nullptr;
    QVBoxLayout *m_resultsLayout = nullptr;
    QScrollArea *m_resultScroll = nullptr;
    QWidget *m_loadingBox = nullptr;
    QLabel *m_spinLabel = nullptr;
    QLabel *m_targetLabel = nullptr;
    QLabel *m_curlCode = nullptr;
    QLabel *m_psCode = nullptr;
    QToolButton *m_redetectBtn = nullptr;
    QTimer *m_spinTimer = nullptr;
    qreal m_spinAngle = 0;
    bool m_checking = false;
};
