#pragma once

#include <QWidget>
#include <QString>

class QLabel;
class QVBoxLayout;
class QScrollArea;

class LogMonitorView : public QWidget
{
    Q_OBJECT
public:
    explicit LogMonitorView(QWidget *parent = nullptr);

    void prependEntry(const QString &timestamp, const QString &type, const QString &message);
    void clearEntries();
    void setListenStatus(const QString &ipPort, bool running);

signals:
    void clearClicked();

private:
    QWidget *createHeader();
    QWidget *createFooter();
    QWidget *createRow(const QString &timestamp, const QString &type, const QString &message);
    void updateCountBadge();
    void syncEmptyState();

    QLabel *m_countBadge = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QLabel *m_listenLabel = nullptr;
    QVBoxLayout *m_rowsLayout = nullptr;
    int m_count = 0;
};
