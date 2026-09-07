#include "HttpFileServer.h"

#include <QDir>
#include <QFile>
#include <QUrl>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QHostAddress>

namespace {

QString htmlEscape(const QString &s)
{
    QString o = s;
    o.replace('&', "&amp;");
    o.replace('<', "&lt;");
    o.replace('>', "&gt;");
    o.replace('"', "&quot;");
    return o;
}

QString formatBytes(qint64 n)
{
    const char *u[] = {"B", "KB", "MB", "GB", "TB"};
    double v = double(n);
    int i = 0;
    while (v >= 1024.0 && i < 4) {
        v /= 1024.0;
        ++i;
    }
    return QString::number(v, 'f', i == 0 ? 0 : 1) + ' ' + u[i];
}

} // namespace

HttpFileServer::HttpFileServer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &HttpFileServer::onNewConnection);
}

HttpFileServer::~HttpFileServer()
{
    stop();
}

bool HttpFileServer::start(const QString &rootDir, quint16 port)
{
    stop();
    QDir dir(rootDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            emit errorOccurred(QStringLiteral("无法创建共享目录: %1").arg(rootDir));
            return false;
        }
    }
    m_rootDir = QDir::cleanPath(dir.absolutePath());
    if (!m_server.listen(QHostAddress::AnyIPv4, port)) {
        emit errorOccurred(QStringLiteral("端口绑定失败: %1").arg(m_server.errorString()));
        return false;
    }
    m_port = m_server.serverPort();
    m_running = true;
    emit started(m_port);
    return true;
}

void HttpFileServer::stop()
{
    if (!m_running && !m_server.isListening())
        return;
    const auto socks = m_conns.keys();
    for (QTcpSocket *s : socks) {
        s->disconnectFromHost();
        s->deleteLater();
    }
    m_conns.clear();
    m_server.close();
    m_running = false;
    m_port = 0;
    emit stopped();
}

QList<QFileInfo> HttpFileServer::listedFiles() const
{
    QMutexLocker lock(&m_mutex);
    QList<QFileInfo> out;
    QDir dir(m_rootDir);
    if (!dir.exists())
        return out;
    const auto infos = dir.entryInfoList(QDir::Files | QDir::Readable, QDir::Time);
    for (const QFileInfo &fi : infos)
        out.append(fi);
    return out;
}

void HttpFileServer::onNewConnection()
{
    while (QTcpSocket *sock = m_server.nextPendingConnection()) {
        m_conns.insert(sock, ConnState{});
        connect(sock, &QTcpSocket::readyRead, this, &HttpFileServer::onReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &HttpFileServer::onDisconnected);
    }
}

void HttpFileServer::onDisconnected()
{
    auto *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock)
        return;
    m_conns.remove(sock);
    sock->deleteLater();
}

void HttpFileServer::onReadyRead()
{
    auto *sock = qobject_cast<QTcpSocket *>(sender());
    if (!sock || !m_conns.contains(sock))
        return;

    ConnState &st = m_conns[sock];
    st.buffer.append(sock->readAll());

    if (!st.headersDone) {
        const int sep = st.buffer.indexOf("\r\n\r\n");
        if (sep < 0)
            return;
        const QByteArray head = st.buffer.left(sep);
        st.body = st.buffer.mid(sep + 4);
        st.buffer.clear();
        st.headersDone = true;

        const QList<QByteArray> lines = head.split('\n');
        if (lines.isEmpty()) {
            sock->disconnectFromHost();
            return;
        }
        const QByteArray reqLine = lines.first().trimmed();
        const QList<QByteArray> parts = reqLine.split(' ');
        if (parts.size() < 2) {
            sock->disconnectFromHost();
            return;
        }
        st.method = QString::fromLatin1(parts[0]).toUpper();
        st.path = decodePath(QString::fromUtf8(parts[1]));

        for (int i = 1; i < lines.size(); ++i) {
            QByteArray line = lines[i].trimmed();
            const int c = line.indexOf(':');
            if (c <= 0)
                continue;
            const QString k = QString::fromLatin1(line.left(c)).trimmed().toLower();
            const QString v = QString::fromUtf8(line.mid(c + 1)).trimmed();
            st.headers.insert(k, v);
        }
        st.contentLength = st.headers.value(QStringLiteral("content-length")).toLongLong();
    } else {
        st.body.append(sock->readAll());
    }

    if (st.method == QLatin1String("POST") && st.body.size() < st.contentLength)
        return;

    handleRequest(sock, st);
}

QString HttpFileServer::decodePath(const QString &raw)
{
    QString p = raw;
    const int q = p.indexOf('?');
    if (q >= 0)
        p = p.left(q);
    return QUrl::fromPercentEncoding(p.toUtf8());
}

QString HttpFileServer::safeJoin(const QString &name) const
{
    if (name.contains(QLatin1String("..")) || name.contains('/') || name.contains('\\')
        || name.contains(':'))
        return {};
    const QString abs = QDir(m_rootDir).filePath(name);
    const QString clean = QDir::cleanPath(abs);
    if (!clean.startsWith(m_rootDir))
        return {};
    return clean;
}

QByteArray HttpFileServer::guessMime(const QString &name)
{
    const QString lower = name.toLower();
    if (lower.endsWith(QLatin1String(".html")) || lower.endsWith(QLatin1String(".htm")))
        return "text/html; charset=utf-8";
    if (lower.endsWith(QLatin1String(".json")))
        return "application/json; charset=utf-8";
    if (lower.endsWith(QLatin1String(".txt")) || lower.endsWith(QLatin1String(".log"))
        || lower.endsWith(QLatin1String(".md")))
        return "text/plain; charset=utf-8";
    if (lower.endsWith(QLatin1String(".png")))
        return "image/png";
    if (lower.endsWith(QLatin1String(".jpg")) || lower.endsWith(QLatin1String(".jpeg")))
        return "image/jpeg";
    if (lower.endsWith(QLatin1String(".pdf")))
        return "application/pdf";
    if (lower.endsWith(QLatin1String(".zip")))
        return "application/zip";
    return "application/octet-stream";
}

void HttpFileServer::sendResponse(QTcpSocket *sock, int code, const QByteArray &contentType,
                                  const QByteArray &body,
                                  const QList<QPair<QByteArray, QByteArray>> &extra)
{
    QByteArray reason = "OK";
    if (code == 400)
        reason = "Bad Request";
    else if (code == 404)
        reason = "Not Found";
    else if (code == 405)
        reason = "Method Not Allowed";
    else if (code == 500)
        reason = "Internal Server Error";

    QByteArray resp;
    resp += "HTTP/1.1 " + QByteArray::number(code) + ' ' + reason + "\r\n";
    resp += "Content-Type: " + contentType + "\r\n";
    resp += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    resp += "Connection: close\r\n";
    resp += "Access-Control-Allow-Origin: *\r\n";
    for (const auto &p : extra)
        resp += p.first + ": " + p.second + "\r\n";
    resp += "\r\n";
    resp += body;
    sock->write(resp);
    sock->disconnectFromHost();
}

void HttpFileServer::sendFile(QTcpSocket *sock, const QString &absPath, const QString &clientIp)
{
    QFile f(absPath);
    if (!f.open(QIODevice::ReadOnly)) {
        sendResponse(sock, 404, "text/plain; charset=utf-8", "file not found");
        return;
    }
    const QByteArray data = f.readAll();
    const QFileInfo fi(absPath);
    QList<QPair<QByteArray, QByteArray>> extra;
    extra.append({QByteArray("Content-Disposition"),
                  QByteArray("attachment; filename=\"") + fi.fileName().toUtf8() + '"'});
    sendResponse(sock, 200, guessMime(fi.fileName()), data, extra);
    emit clientDownload(clientIp, fi.fileName(), fi.size());
}

void HttpFileServer::handleUpload(QTcpSocket *sock, ConnState &st, const QString &clientIp)
{
    const QString ctype = st.headers.value(QStringLiteral("content-type"));
    const QRegularExpression re(QStringLiteral("boundary=(.+)"));
    const auto m = re.match(ctype);
    if (!m.hasMatch()) {
        sendResponse(sock, 400, "text/plain; charset=utf-8", "need multipart boundary");
        return;
    }
    const QByteArray boundary = "--" + m.captured(1).trimmed().toUtf8();

    // 标准 multipart 解析：按 boundary 切分
    QList<QByteArray> chunks;
    int from = 0;
    while (true) {
        int idx = st.body.indexOf(boundary, from);
        if (idx < 0)
            break;
        if (from > 0 && idx > from)
            chunks.append(st.body.mid(from, idx - from));
        from = idx + boundary.size();
        if (from + 1 < st.body.size() && st.body[from] == '-' && st.body[from + 1] == '-')
            break;
        if (from + 1 < st.body.size() && st.body.mid(from, 2) == "\r\n")
            from += 2;
    }

    int saved = 0;
    for (QByteArray chunk : chunks) {
        if (chunk.startsWith("\r\n"))
            chunk = chunk.mid(2);
        if (chunk.endsWith("\r\n"))
            chunk.chop(2);
        const int hs = chunk.indexOf("\r\n\r\n");
        if (hs < 0)
            continue;
        const QByteArray header = chunk.left(hs);
        QByteArray fileBody = chunk.mid(hs + 4);
        if (fileBody.endsWith("\r\n"))
            fileBody.chop(2);

        QString filename;
        const QRegularExpression fnRe(QStringLiteral("filename=\"([^\"]+)\""));
        const auto fm = fnRe.match(QString::fromUtf8(header));
        if (!fm.hasMatch())
            continue;
        filename = QFileInfo(fm.captured(1)).fileName();
        const QString abs = safeJoin(filename);
        if (abs.isEmpty())
            continue;
        QFile out(abs);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
            continue;
        out.write(fileBody);
        out.close();
        ++saved;
        emit clientUpload(clientIp, filename, fileBody.size());
    }

    const QByteArray msg = QStringLiteral("{\"ok\":true,\"saved\":%1}").arg(saved).toUtf8();
    sendResponse(sock, 200, "application/json; charset=utf-8", msg);
}

QByteArray HttpFileServer::buildPortalHtml() const
{
    QString rows;
    const auto files = listedFiles();
    for (const QFileInfo &fi : files) {
        const QString name = fi.fileName();
        const QString enc = QString::fromUtf8(QUrl::toPercentEncoding(name));
        rows += QStringLiteral(
                    "<div class='card'>"
                    "<div class='meta'><div class='name'>%1</div>"
                    "<div class='sub'>%2 · %3</div></div>"
                    "<div class='acts'>"
                    "<button onclick=\"copyCurl('%4')\">curl</button>"
                    "<a class='dl' href='/download/%5'>高速下载</a>"
                    "</div></div>")
                    .arg(htmlEscape(name),
                         formatBytes(fi.size()),
                         fi.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                         htmlEscape(name),
                         enc);
    }
    if (rows.isEmpty())
        rows = QStringLiteral("<div class='empty'>共享目录暂无文件</div>");

    const QString html = QStringLiteral(R"HTML(<!DOCTYPE html>
<html lang="zh-CN"><head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>局域网极速文件共享与提货端</title>
<style>
body{margin:0;font-family:Segoe UI,system-ui,sans-serif;background:#070e1a;color:#e2e8f0}
.wrap{max-width:900px;margin:24px auto;padding:0 16px}
.box{background:#0b1627;border:1px solid #1d3153;border-radius:16px;overflow:hidden}
.head{padding:24px;border-bottom:1px solid #1b2f4f;background:linear-gradient(90deg,#0d1c33,#0e213b,#0d1c33)}
.badge{display:inline-block;padding:2px 10px;border-radius:999px;border:1px solid rgba(34,211,238,.4);
 background:#083344;color:#67e8f9;font:11px ui-monospace,Consolas,monospace}
h1{margin:8px 0 4px;font-size:22px}
.sub{color:#94a3b8;font:12px ui-monospace,Consolas,monospace}
.bar{display:flex;justify-content:space-between;align-items:center;padding:12px 24px;
 background:#08101e;border-bottom:1px solid #182842;font-size:12px;gap:12px;flex-wrap:wrap}
input{background:#0b1627;border:1px solid #1e3458;border-radius:8px;color:#e2e8f0;padding:6px 10px;min-width:200px}
.list{padding:20px;display:flex;flex-direction:column;gap:12px}
.card{display:flex;justify-content:space-between;gap:12px;align-items:center;padding:14px;
 background:#08101e;border:1px solid #1a2d48;border-radius:12px}
.name{font:14px ui-monospace,Consolas,monospace;font-weight:600}
.acts{display:flex;gap:8px}
button,.dl{border-radius:8px;border:1px solid #20395e;background:#11233c;color:#cbd5e1;
 padding:8px 12px;font-size:12px;text-decoration:none;cursor:pointer}
.dl{background:#0891b2;border-color:#0891b2;color:#020617;font-weight:700}
.foot{padding:16px 24px;border-top:1px solid #182842;background:#080e1a;font-size:12px;color:#94a3b8}
code{display:block;background:#050912;border:1px solid #16273e;border-radius:8px;padding:10px;
 color:#34d399;font:11px ui-monospace,Consolas,monospace;margin-top:6px;word-break:break-all}
.empty{text-align:center;color:#64748b;padding:40px}
.upload{float:right;background:#059669;border:none;color:#fff;font-weight:700;padding:10px 16px;border-radius:12px;cursor:pointer}
</style></head><body>
<div class="wrap"><div class="box">
<div class="head">
  <span class="badge">HTTP LAN File Delivery</span>
  <span style="color:#34d399;font-size:12px;margin-left:8px">● 服务运行中</span>
  <label class="upload">从本机/手机上传文件到电脑
    <input id="up" type="file" multiple hidden/>
  </label>
  <h1>局域网极速文件共享与提货端</h1>
  <div class="sub">宿主机节点: 本机 HTTP 共享服务</div>
</div>
<div class="bar">
  <div>可供提货的文件清单 (%1 项)</div>
  <input id="q" placeholder="快速过滤文件名..." oninput="filterFiles()"/>
</div>
<div class="list" id="list">%2</div>
<div class="foot">
  <div style="color:#67e8f9;font-weight:600;margin-bottom:8px">开发板命令行一键提货指南</div>
  <div>方式一：wget</div>
  <code id="wget">wget "http://HOST:PORT/download/文件名" -O "文件名"</code>
  <div style="margin-top:8px">方式二：curl</div>
  <code id="curl">curl -O "http://HOST:PORT/download/文件名"</code>
</div>
</div></div>
<script>
const host = location.origin;
document.getElementById('wget').textContent = 'wget "'+host+'/download/example.bin" -O "example.bin"';
document.getElementById('curl').textContent = 'curl -O "'+host+'/download/example.bin"';
function copyCurl(name){
  const cmd = 'curl -O "'+host+'/download/'+encodeURIComponent(name)+'"';
  navigator.clipboard.writeText(cmd);
}
function filterFiles(){
  const q = document.getElementById('q').value.toLowerCase();
  document.querySelectorAll('.card').forEach(c=>{
    const n = c.querySelector('.name').textContent.toLowerCase();
    c.style.display = n.includes(q) ? 'flex' : 'none';
  });
}
document.getElementById('up').addEventListener('change', async (e)=>{
  const files = e.target.files;
  if(!files || !files.length) return;
  const fd = new FormData();
  for (const f of files) fd.append('file', f, f.name);
  await fetch('/upload', {method:'POST', body: fd});
  location.reload();
});
</script>
</body></html>)HTML")
                        .arg(files.size())
                        .arg(rows);
    return html.toUtf8();
}

void HttpFileServer::handleRequest(QTcpSocket *sock, ConnState &st)
{
    const QString clientIp = sock->peerAddress().toString();

    if (st.method == QLatin1String("OPTIONS")) {
        sendResponse(sock, 200, "text/plain", {},
                     {{"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
                      {"Access-Control-Allow-Headers", "Content-Type"}});
        return;
    }

    if (st.method == QLatin1String("HEAD") || st.method == QLatin1String("GET")) {
        if (st.path == QLatin1String("/") || st.path.isEmpty()) {
            sendResponse(sock, 200, "text/html; charset=utf-8", buildPortalHtml());
            return;
        }
        if (st.path == QLatin1String("/api/files")) {
            QJsonArray arr;
            for (const QFileInfo &fi : listedFiles()) {
                QJsonObject o;
                o.insert(QStringLiteral("name"), fi.fileName());
                o.insert(QStringLiteral("size"), fi.size());
                o.insert(QStringLiteral("mtime"),
                         fi.lastModified().toString(Qt::ISODate));
                arr.append(o);
            }
            sendResponse(sock, 200, "application/json; charset=utf-8",
                         QJsonDocument(arr).toJson(QJsonDocument::Compact));
            return;
        }
        if (st.path.startsWith(QLatin1String("/download/"))) {
            QString name = st.path.mid(QStringLiteral("/download/").size());
            name = QUrl::fromPercentEncoding(name.toUtf8());
            const QString abs = safeJoin(QFileInfo(name).fileName());
            if (abs.isEmpty() || !QFileInfo::exists(abs)) {
                sendResponse(sock, 404, "text/plain; charset=utf-8", "not found");
                return;
            }
            if (st.method == QLatin1String("HEAD")) {
                QFileInfo fi(abs);
                sendResponse(sock, 200, guessMime(fi.fileName()), {},
                             {{"Content-Length", QByteArray::number(fi.size())}});
                return;
            }
            sendFile(sock, abs, clientIp);
            return;
        }
        // 直接按文件名访问
        QString name = st.path.mid(1);
        name = QUrl::fromPercentEncoding(name.toUtf8());
        const QString abs = safeJoin(QFileInfo(name).fileName());
        if (!abs.isEmpty() && QFileInfo::exists(abs)) {
            if (st.method == QLatin1String("HEAD")) {
                QFileInfo fi(abs);
                sendResponse(sock, 200, guessMime(fi.fileName()), {},
                             {{"Content-Length", QByteArray::number(fi.size())}});
                return;
            }
            sendFile(sock, abs, clientIp);
            return;
        }
        sendResponse(sock, 404, "text/plain; charset=utf-8", "not found");
        return;
    }

    if (st.method == QLatin1String("POST") && st.path == QLatin1String("/upload")) {
        handleUpload(sock, st, clientIp);
        return;
    }

    sendResponse(sock, 405, "text/plain; charset=utf-8", "method not allowed");
}
