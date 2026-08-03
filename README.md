# Dr.Rain — 智能医疗 AI 助手

> 基于自研 C++ HTTP 框架的 AI 医疗健康平台。多模型 LLM · SSE 流式 · MCP 工具调用 · 管理后台 · RBAC 鉴权。

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-%E2%89%A5%203.16-blue.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-Apache%202.0-green.svg)](LICENSE)
[![Version](https://img.shields.io/badge/Version-v3.0.0-orange.svg)](CHANGELOG.md)

---

## ✨ 核心能力

| 模块 | 功能 |
|---|---|
| **多模型 LLM** | Aliyun Qwen / Doubao，Strategy + Factory 模式热切换，原生 Function Calling |
| **SSE 流式** | curl `WRITEFUNCTION` 逐 token 实时推送，打字机渲染 |
| **MCP 工具调用** | JSON-RPC 2.0 `tools/list` + `tools/call`，stdio/sse 远端 Server |
| **Admin 后台** | RBAC 鉴权 + 实时看板 (SSR+SSE) + 用户管理 + 日志查看 + 反馈管理 |
| **安全防护** | JWT HS256 + argon2id + 登录锁定 + CSP/HSTS + Token Bucket 限流 |
| **结构化日志** | spdlog 双通道 (控制台彩色 + JSON 文件)，全链路 request_id |
| **医疗人设** | System Prompt Dr.Rain 角色 + 免责声明 |
| **图像识别** | ONNX Runtime + MobileNetV2 端侧推理 |
| **语音合成** | 百度 TTS Token 缓存 + 快速轮询 |

---

## 🏗 架构

```
Browser / Admin / MCP Client
        │  HTTP / SSE / JSON-RPC
┌───────▼──────────┐
│  HttpServer       │  muduo Reactor, epoll, 4 IO 线程
│  ├─ Middleware    │  CORS, Auth (JWT), Admin RBAC, Security, RateLimit, RequestId
│  ├─ Router        │  精确 O(1) + 正则 O(n)
│  └─ Session       │  内存存储 + LRU (MAX 500)
├──────────────────┤
│  AIServerCore     │  业务编排层 (Controller → Service → Repository)
├──────────────────┤
│  AIEngine         │  AI 工具库 (libaiengine.a, 零 HTTP 依赖)
│  ├─ llm/          │  AIHelper · AIStrategy · AIFactory
│  ├─ mcp/          │  AIToolRegistry · McpClientManager (stdio/sse)
│  └─ audio/vision/ │  AISpeechProcessor · ImageRecognizer
├──────────────────┤
│  Storage          │  持久化层 (libstorage.a, Prepared Statement)
├──────────────────┤
│  Common/          │  ConfigManager, Logger(spdlog), JwtService, TokenBucket, PasswordHash, MailSender ...
└──────────────────┘
        │
   ┌────▼────┐
   │  MySQL  │
   └─────────┘
```

---

## 🚀 本地部署

### 环境要求

| 依赖 | 用途 |
|------|------|
| GCC ≥ 12, CMake ≥ 3.16 | C++17 |
| muduo, OpenSSL, libcurl | 网络 + HTTPS |
| spdlog ≥ 1.12 | 日志 |
| MySQL C++ Connector 8.0 | 数据库 |
| libsodium ≥ 1.0.18 | argon2id 哈希 |
| OpenCV ≥ 4.x, ONNX Runtime 1.17.1 | 图像识别 |

### 构建

```bash
git clone git@github.com:Rain0832/RainCppAI.git && cd RainCppAI

# 1. 安装系统依赖 (Ubuntu 24.04)
sudo apt-get install -y cmake g++ make libcurl4-openssl-dev libssl-dev \
    libspdlog-dev libopencv-dev libmysqlcppconn-dev libmysqlclient-dev \
    libsodium-dev nlohmann-json3-dev clang-format

# 2. 编译安装 muduo
git clone --depth=1 https://github.com/chenshuo/muduo.git /tmp/muduo
cd /tmp/muduo && mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DMUDUO_BUILD_EXAMPLES=OFF
make -j$(nproc) && sudo make install

# 3. 安装 ONNX Runtime
curl -fsSL https://github.com/microsoft/onnxruntime/releases/download/v1.17.1/onnxruntime-linux-x64-1.17.1.tgz | tar xz -C /tmp
sudo cp -r /tmp/onnxruntime-linux-x64-1.17.1/include/* /usr/local/include/
sudo cp -r /tmp/onnxruntime-linux-x64-1.17.1/lib/* /usr/local/lib/
sudo ldconfig

# 4. 配置
cp config.json.example config.json
# 编辑 config.json 填入数据库密码、JWT secret 等

# 5. 创建数据库 (表结构首次启动自动创建)
mysql -u root -e "CREATE DATABASE IF NOT EXISTS ChatHttpServer"

# 6. 构建 & 运行
mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)
./http_server -p 8088
```

浏览器打开 `http://localhost:8088`。

> **SSE 反向代理**：Nginx 需配置 `proxy_buffering off;`，参考 [`deploy/nginx.conf`](deploy/nginx.conf)。
> **Systemd 部署**：参考 [`deploy/raincppai.service`](deploy/raincppai.service)。

---

## 📚 API

| Method | Path | Auth | 说明 |
|--------|------|------|------|
| `GET` | `/` `/entry` | ✗ | 登录/注册入口 |
| `POST` | `/login` | ✗ | 登录，返回 JWT cookie |
| `POST` | `/register` | ✗ | 注册 (邀请码+邮箱验证) |
| `POST` | `/chat/send-stream` | ✓ | SSE 流式对话 (唯一入口) |
| `GET` | `/chat/sessions` | ✓ | 会话列表 |
| `POST` | `/chat/history` | ✓ | 会话历史 |
| `POST` | `/api/feedback` | ✓ | 用户反馈 |
| `POST` | `/mcp` | ✗ | MCP JSON-RPC 2.0 |
| `GET` | `/admin/dashboard` | admin | 管理后台 |
| `GET` | `/admin/logs` | admin | 日志查看 |
| `GET` | `/admin/api/users` | admin | 用户列表 |
| `GET` | `/admin/api/feedback` | admin | 反馈列表 |

> 完整 API 表见 [CONTRIBUTING.md](CONTRIBUTING.md) §一。

---

## 📁 项目结构

```
RainCppAI/
├── HttpServer/           # 自研 HTTP 框架 (静态库 libhttpserver.a)
├── Storage/              # 持久化层 (静态库 libstorage.a)
├── AIServerCore/         # 业务编排层
│   ├── controller/       # HTTP Handlers
│   ├── Service/          # 业务服务
│   └── Repository/       # 数据访问 (Prepared Statement)
├── AIEngine/             # AI 工具库 (静态库 libaiengine.a, 零 HTTP 依赖)
├── Common/               # 公共组件 (Config, Logging, Auth, Crypto, Mail ...)
├── web/                  # 前端资源 (SSR 模板 + 原生 HTML/JS/CSS)
├── deploy/               # 部署配置 (nginx.conf, raincppai.service)
├── Docs/                 # 设计文档 (Plan)
├── Tests/                # GoogleTest 单元测试
└── mcp_servers/          # Python MCP 微服务
```

---

## 🤝 贡献

请阅读 [`CONTRIBUTING.md`](CONTRIBUTING.md) 了解模块依赖、提交规范与开发流程。
代码风格见 [`DEVELOP_STANDARD.md`](DEVELOP_STANDARD.md)。

---

## 📖 参考

- [muduo — Linux 多线程服务端编程](https://github.com/chenshuo/muduo)
- [spdlog — Fast C++ logging](https://github.com/gabime/spdlog)
- [Model Context Protocol](https://modelcontextprotocol.io/)
- [阿里云百炼 API](https://help.aliyun.com/product/610100.html)
