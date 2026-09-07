#pragma once

#include <QMainWindow>
#include <QPointer>

class HttpFileServer;
class ActivityLogModel;
class QrCodeWidget;
class QLabel;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QPushButton;
class QTableWidget;
class QListWidget;
class QStackedWidget;
class QFileSystemWatcher;
class QCheckBox;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

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
    QString currentShareUrl() const;
    QString selectedIp() const;
    QString adapterNameForIp(const QString &ip) const;
    void setRunningUi(bool running);

    HttpFileServer *m_server = nullptr;
    ActivityLogModel *m_logs = nullptr;
    QFileSystemWatcher *m_watcher = nullptr;

    QStackedWidget *m_stack = nullptr;
    QPushButton *m_tabManager = nullptr;
    QPushButton *m_tabPortal = nullptr;
    QPushButton *m_tabLogs = nullptr;

    QLabel *m_uacBadge = nullptr;
    QLabel *m_statusPill = nullptr;
    QPushButton *m_toggleBtn = nullptr;
    QLineEdit *m_folderEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QComboBox *m_ipCombo = nullptr;
    QLabel *m_urlLabel = nullptr;
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
