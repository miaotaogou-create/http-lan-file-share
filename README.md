# HTTP 局域网极速文件共享客户端

Windows 原生 Qt 客户端：把本机文件夹一键发布成局域网 HTTP 站点，手机 / 电脑 / 嵌入式开发板可用浏览器或 `wget` / `curl` 提货下载。

![License](https://img.shields.io/badge/license-MIT-green)
![Qt](https://img.shields.io/badge/Qt-6.x-41CD52)
![Platform](https://img.shields.io/badge/platform-Windows%20x64-blue)

## 功能

- 一键启动 / 停止 HTTP 共享（默认端口 `8899`）
- 自定义共享目录；本地上传文件到共享目录
- 自动枚举网卡 IPv4，选择局域网提货地址
- 动态二维码，手机扫码打开提货页
- 内置提货 Web 页：列表、下载、网页上传
- 复制 `curl` / `wget` 命令，方便板端拉取
- 活动日志；本机回环自检
- （可选，需管理员）通过 `netsh` 追加 / 解绑辅助调试 IP

## 截图对应界面

| 面板 | 说明 |
|------|------|
| 客户端控制面板 | 服务配置、二维码、文件表、网卡管理 |
| 局域网提货 Web 端 | 内嵌预览；真实页面访问 `http://IP:端口/` |
| 实时日志与监控 | 启停、上传下载、网卡变更记录 |

## 构建（Windows）

依赖：

- Qt 6.x（Widgets + Network），本仓库默认路径示例：`C:/Qt6_10/6.10.1/msvc2022_64`
- Visual Studio 2022/2026（MSVC x64）
- CMake ≥ 3.16

```bat
build.bat
```

或手动：

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DCMAKE_PREFIX_PATH=C:/Qt6_10/6.10.1/msvc2022_64
cmake --build build --config Release
```

产物：`build\Release\HttpLanFileShare.exe`

运行前请保证 Qt 的 `bin` 在 `PATH` 中，或使用 `windeployqt`：

```bat
C:\Qt6_10\6.10.1\msvc2022_64\bin\windeployqt.exe build\Release\HttpLanFileShare.exe
```

## 使用

1. 启动程序，确认共享目录与端口
2. 点击「一键启动 HTTP 共享」
3. 同网段设备浏览器打开显示的地址，或扫码
4. 板端示例：

```bash
curl -O "http://192.168.x.x:8899/download/your_file.tar.gz"
wget "http://192.168.x.x:8899/download/your_file.tar.gz" -O your_file.tar.gz
```

追加网卡辅助 IP 时，请右键「以管理员身份运行」。

## HTTP 接口

| 路径 | 说明 |
|------|------|
| `GET /` | 提货 Web 页 |
| `GET /download/<文件名>` | 下载文件 |
| `GET /api/files` | JSON 文件列表 |
| `POST /upload` | `multipart/form-data` 上传 |
| `HEAD /` | 自检用 |

## 技术说明

- UI：Qt Widgets + QSS（深色工业风）
- 服务：`QTcpServer` 自研轻量 HTTP（未依赖 Qt HttpServer 模块）
- 二维码：第三方 [nayuki/QR-Code-generator](https://github.com/nayuki/QR-Code-generator)（MIT）
- 网卡：`QNetworkInterface`；改 IP 走 Windows `netsh`

## 开源协议

MIT。欢迎 Issue / PR。

## 致谢

界面布局参考了早期 Web 原型的交互设计；二维码生成使用 Nayuki QR Code generator。
