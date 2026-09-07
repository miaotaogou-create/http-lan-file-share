#pragma once

#include <QMainWindow>
#include <QPoint>

class HttpFileServer;
class ActivityLogModel;
class QrCodeWidget;
class StatusBadgeWidget;
class QLabel;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QPushButton;
class QTableWidget;
class QListWidget;
class QStackedWidget;
class QToolButton;
class QFileSystemWatcher;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void toggleServer();
    void refreshNics();
    void refreshFiles();
    void browseFolder();
    void copyShareUrl();
    void openPortalInBrowser();
    void selfCheck();
    void addNicIp();
    void deleteSelectedNic();
    void uploadLocalFiles();
    void downloadSelectedFile();
    void deleteSelectedFile();
    void copyCurlForSelected();
    void downloadFileByPath(const QString &src, const QString &name);
    void deleteFileByPath(const QString &path, const QString &name);
    void copyCurlForName(const QString &name);
    void clearLogs();
    void onServerStarted(quint16 port);
    void onServerStopped();
    void onDownload(const QString &ip, const QString &name, qint64 size);
    void onUpload(const QString &ip, const QString &name, qint64 size);
    void onIpSelectionChanged(int index);
    void switchView(int index);

private:
    void buildUi();
    void applyTheme();
    void addLog(const QString &type, const QString &msg);
    void updateShareUrlUi();
    void showToast(const QString &msg);
    void updateTabChrome(int index);
    void updateUacBadge();
    void updateMaxButtonIcon();
    void fitTableHeight(QTableWidget *table);
    QWidget *makeFileActionBar(const QString &path, const QString &name);
    QString currentShareUrl() const;
    QString selectedIp() const;
    QString adapterNameForIp(const QString &ip) const;
    void setRunningUi(bool running);

    HttpFileServer *m_server = nullptr;
    ActivityLogModel *m_logs = nullptr;
    QFileSystemWatcher *m_watcher = nullptr;

    QWidget *m_titleBar = nullptr;
    QToolButton *m_maxBtn = nullptr;
    QStackedWidget *m_stack = nullptr;
    QPushButton *m_tabManager = nullptr;
    QPushButton *m_tabPortal = nullptr;
    QPushButton *m_tabLogs = nullptr;
    QLabel *m_runDot = nullptr;
    QLabel *m_portalCount = nullptr;

    QLabel *m_uacBadge = nullptr;
    QPoint m_dragPos;
    bool m_dragging = false;
    StatusBadgeWidget *m_statusPill = nullptr;
    QPushButton *m_toggleBtn = nullptr;
    QLineEdit *m_folderEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QComboBox *m_ipCombo = nullptr;
    QLabel *m_urlLabel = nullptr;
    QLabel *m_loopbackLabel = nullptr;
    QrCodeWidget *m_qr = nullptr;
    QLabel *m_qrUrlLabel = nullptr;
    QLabel *m_priorityFileLabel = nullptr;
    QTableWidget *m_fileTable = nullptr;
    QTableWidget *m_nicTable = nullptr;
    QLineEdit *m_newIpEdit = nullptr;
    QLineEdit *m_newMaskEdit = nullptr;
    QListWidget *m_logList = nullptr;
    QLabel *m_toast = nullptr;
    QLabel *m_fileStats = nullptr;
    QLineEdit *m_fileFilter = nullptr;

    // 内嵌提货预览
    QTableWidget *m_portalTable = nullptr;
    QLabel *m_portalHostLabel = nullptr;

    QString m_shareRoot;
    bool m_running = false;
};
