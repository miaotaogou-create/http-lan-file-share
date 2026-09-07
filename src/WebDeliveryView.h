#pragma once

#include <QWidget>
#include <QString>
#include <QVector>

class QLabel;
class QLineEdit;
class QVBoxLayout;
class QScrollArea;

struct WebDeliveryItem {
    QString fileName;
    QString filePath;
    QString fileSize;
    QString modifyTime;
    int downloadCount = 0;
};

class WebDeliveryView : public QWidget
{
    Q_OBJECT
public:
    explicit WebDeliveryView(QWidget *parent = nullptr);

    void setHostInfo(const QString &url, bool running);
    void setItems(const QVector<WebDeliveryItem> &items);

signals:
    void backToDashboardClicked();
    void uploadClicked();
    void curlCopyClicked(const QString &fileName);
    void downloadItemClicked(const QString &filePath, const QString &fileName);

private slots:
    void onSearchTextChanged(const QString &text);

private:
    QWidget *createTopNavBar();
    QWidget *createHeroHeader();
    QWidget *createSearchBar();
    QWidget *createFileCard(const WebDeliveryItem &item);
    QWidget *createBoardGuideCard();
    void rebuildFileCards();
    void updateListTitle();
    void updateGuideCmds();

    QLabel *m_hostLabel = nullptr;
    QLabel *m_runningLabel = nullptr;
    QLabel *m_readyLabel = nullptr;
    QLabel *m_listTitle = nullptr;
    QLabel *m_guideCmd1 = nullptr;
    QLabel *m_guideCmd2 = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QVBoxLayout *m_fileListLayout = nullptr;
    QVector<WebDeliveryItem> m_items;
    QString m_hostUrl;
    bool m_running = false;
};
