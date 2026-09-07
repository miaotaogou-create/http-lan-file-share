#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QFileInfo>
#include <QMutex>

class HttpFileServer : public QObject
{
    Q_OBJECT
public:
    explicit HttpFileServer(QObject *parent = nullptr);
    ~HttpFileServer() override;

    bool start(const QString &rootDir, quint16 port);
    void stop();
    bool isRunning() const { return m_running; }
    quint16 port() const { return m_port; }
    QString rootDir() const { return m_rootDir; }

    QList<QFileInfo> listedFiles() const;

signals:
    void started(quint16 port);
    void stopped();
    void clientDownload(const QString &clientIp, const QString &fileName, qint64 size);
    void clientUpload(const QString &clientIp, const QString &fileName, qint64 size);
    void errorOccurred(const QString &message);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    struct ConnState {
        QByteArray buffer;
        bool headersDone = false;
        QString method;
        QString path;
        QHash<QString, QString> headers;
        qint64 contentLength = 0;
        QByteArray body;
    };

    void handleRequest(QTcpSocket *sock, ConnState &st);
    void sendResponse(QTcpSocket *sock, int code, const QByteArray &contentType,
                      const QByteArray &body, const QList<QPair<QByteArray, QByteArray>> &extra = {});
    void sendFile(QTcpSocket *sock, const QString &absPath, const QString &clientIp,
                  const QHash<QString, QString> &reqHeaders, bool headOnly);
    void handleUpload(QTcpSocket *sock, ConnState &st, const QString &clientIp);
    QByteArray buildPortalHtml() const;
    QString safeJoin(const QString &name) const;
    static QByteArray guessMime(const QString &name);
    static QString decodePath(const QString &raw);
    static bool parseBytesRange(const QString &rangeHeader, qint64 fileSize,
                                qint64 *outStart, qint64 *outEnd);

    QTcpServer m_server;
    QString m_rootDir;
    quint16 m_port = 0;
    bool m_running = false;
    QHash<QTcpSocket *, ConnState> m_conns;
    mutable QMutex m_mutex;
};
