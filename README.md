# Dr.Rain — 智能医疗 AI 助手

> Healthcare AI Assistant · 全栈 C++ 自研 HTTP 框架 · 多模型 LLM · SSE 流式 · MCP 协议 · 端侧视觉 · Admin 后台

[English](#english) | [中文](#chinese)

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-%E2%89%A5%203.16-blue.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-Apache%202.0-green.svg)](LICENSE)
[![Version](https://img.shields.io/badge/Version-v3.0.0-orange.svg)](CHANGELOG.md)

---

<a id="english"></a>
## 🇬🇧 English

### ✨ Core Features

| Module | Capabilities |
|--------|-------------|
| **AI Chat** | Multi-model LLM (Qwen / DouBao), SSE streaming, MCP Function Calling, Agent Loop |
| **Vision** | In-chat image upload → ONNX MobileNetV2 inference → Prompt injection → LLM response |
| **User System** | Invite-code registration, email verification (QQ SMTP), JWT auth, RBAC (admin/user/org) |
| **Security** | argon2id password hashing, login lockout, CORS / CSP / HSTS headers, error redaction |
| **Admin** | Real-time dashboard, user/invite-code/feedback management, log viewer, root auto-seeding |
| **Infrastructure** | Repository/Service layering, ConfigManager, GoogleTest, GitHub Actions CI/CD |

### 🏗 Architecture

```
Browser / Nginx
  └─► muduo EventLoop (Reactor pattern)
        └─► CorsMiddleware → RequestId → Auth → AdminAuth → RateLimit → SecurityHeaders
              └─► Router::dispatch()
                    ├─► StaticFileHandler    ← CSS / JS / images
                    ├─► ChatSseHandler        ← SSE streaming (only dialog entry)
                    ├─► ChatHistoryHandler    ← Session history from MySQL
                    ├─► ChatSessionsHandler   ← Session list
                    ├─► Admin*Handler (×8)    ← Dashboard / users / invites / logs
                    ├─► ChangePasswordHandler ← Password change
                    ├─► HealthHandler         ← GET /health
                    ├─► MetricsHandler        ← GET /metrics
                    └─► McpHandler            ← MCP JSON-RPC 2.0
                          ↓
                    AIEngine::AIHelper
                      ├─► AIModelStrategy    ← Qwen / DouBao
                      ├─► AIToolRegistry     ← MCP tools
                      ├─► McpClientManager   ← stdio/sse client
                      ├─► ImageRecognizer    ← ONNX Runtime
                      └─► AISpeechProcessor  ← TTS
                          ↓
                    Storage::MysqlUtil → MySQL (8 tables + FK)
```

### 🔧 Quick Start

```bash
# 1. Build
sudo apt install -y cmake g++ libssl-dev libcurl4-openssl-dev \
  libmysqlcppconn-dev libopencv-dev libsodium-dev
wget https://github.com/microsoft/onnxruntime/releases/download/v1.17.1/... -O onnxruntime.tgz
tar xzf onnxruntime.tgz -C /usr/local

cd RainCppAI && mkdir build && cd build
cmake .. && make -j$(nproc)

# 2. Configure
cp ../config.json.example ../config.json
# Edit config.json → set DB credentials + LLM API keys

# 3. Download models (optional — for vision)
bash ../scripts/download_models.sh

# 4. Run
./http_server -p 8088
```

### 🌍 Environment Variables

All config.json values can be overridden via environment variables (ConfigManager auto-loads):

| Config Path | Env Var |
|------------|---------|
| `db.host` | `DB_HOST` |
| `db.port` | `DB_PORT` |
| `db.user` | `DB_USER` |
| `db.password` | `DB_PASSWORD` |
| `db.name` | `DB_NAME` |
| `mail.username` | `MAIL_USERNAME` |
| `mail.password` | `MAIL_PASSWORD` |
| `jwt.secret` | `JWT_SECRET` |
| `default_api_keys.dashscope` | `DEFAULT_API_KEYS_DASHSCOPE` |
| `default_api_keys.doubao` | `DEFAULT_API_KEYS_DOUBAO` |

### 📚 API Reference

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/` `/entry` | ✗ | Login / Register page |
| GET | `/register` | ✗ | Registration page |
| POST | `/login` | ✗ | Login |
| POST | `/register` | ✗ | Register |
| GET | `/chat` | ✓ | Chat page |
| POST | `/chat/send-stream` | ✓ | **SSE streaming dialog** (only entry) |
| GET | `/chat/sessions` | ✓ | Session list |
| POST | `/chat/history` | ✓ | Session history |
| POST | `/chat/delete-session` | ✓ | Soft-delete session |
| POST | `/chat/update-title` | ✓ | Update session title |
| POST | `/chat/tts` | ✓ | Text-to-speech |
| POST | `/user/logout` | ✓ | Logout |
| GET | `/upload` | ✓ | Image recognition page |
| POST | `/upload/send` | ✓ | Image recognition upload |
| GET/POST | `/api/user/apikey` | ✓ | API Key CRUD |
| POST | `/api/user/password` | ✓ | Change password |
| POST | `/api/invite/verify` | ✗ | Verify invite code |
| POST | `/api/verify/send` | ✗ | Send email verification code |
| POST | `/api/verify/check` | ✗ | Check email verification code |
| POST | `/api/feedback` | ✓ | Submit feedback |
| GET | `/api/chat/models` | ✓ | Model registry (models.json) |
| POST | `/mcp` | ✗ | MCP JSON-RPC 2.0 |
| GET | `/health` | ✗ | Health check (MySQL SELECT 1) |
| GET | `/metrics` | ✗ | Prometheus metrics |
| GET | `/admin/dashboard` | admin | Admin panel |
| GET | `/admin/sse` | admin | Real-time dashboard SSE |
| GET | `/admin/logs` | admin | Log viewer |
| GET | `/admin/api/users` | admin | User list |
| POST | `/admin/api/users/toggle` | admin | Enable/disable user |
| GET | `/admin/api/feedback` | admin | Feedback list |
| GET | `/admin/api/invite-codes` | admin | Invite code list |
| POST | `/admin/api/invite-codes/create` | admin | Create invite code |
| POST | `/admin/api/invite-codes/toggle` | admin | Enable/disable invite code |

### 📁 Project Structure

```
RainCppAI/
├── HttpServer/          # Self-developed HTTP framework (muduo Reactor)
│   ├── include/http/    # HttpServer, HttpRequest, HttpResponse
│   ├── include/router/  # Router + RouterHandler
│   ├── include/middleware/ # Cors, Auth, AdminAuth, RateLimit, RequestId, SecurityHeaders
│   └── include/session/ # Session + SessionManager
├── AIServerCore/        # Business orchestration (30+ handlers)
│   ├── include/controller/  # All Handler headers
│   ├── include/Repository/  # Data access layer (Account/Session/Message/ApiKey...)
│   ├── include/Service/     # Business logic (Auth/Session/ApiKey/Chat)
│   ├── include/server/      # ChatServer + SessionStore
│   └── src/                 # Implementation
├── AIEngine/            # AI utility library (zero HttpServer dependency)
│   ├── include/llm/     # AIHelper, AIStrategy, AIFactory
│   ├── include/mcp/     # McpServer, McpClientManager, AIToolRegistry
│   ├── include/vision/  # ImageRecognizer (ONNX Runtime)
│   ├── include/audio/   # AISpeechProcessor (TTS)
│   └── include/common/  # Message, base64, AISessionIdGenerator
├── Storage/             # MySQL connection pool + PreparedStatement
├── Common/              # Shared: Config, Logging (spdlog), Auth (JWT), Crypto (argon2id), Mail
├── web/                 # Frontend (HTML + vanilla JS ES modules)
├── mcp_servers/         # Python MCP microservices (weather)
├── Tests/               # GoogleTest unit tests
├── deploy/              # nginx.conf + systemd service
├── scripts/             # download_models.sh
└── Docs/                # Design documents
```

### 📦 Dependencies

| Library | Version | Usage |
|---------|---------|-------|
| muduo | 2.0+ | Network I/O (Reactor) |
| MySQL Connector C++ | 8.0 | Database |
| ONNX Runtime | 1.17.1 | Image recognition |
| OpenCV | 4.x | Image preprocessing |
| libcurl | 7.x | LLM API + SMTP |
| OpenSSL | 3.x | HTTPS + JWT |
| libsodium | 1.0.18 | argon2id |
| spdlog | 1.x | Logging |
| nlohmann/json | 3.x | JSON |
| GoogleTest | 1.15.2 | Unit tests (FetchContent) |

---

<a id="chinese"></a>
## 🇨🇳 中文

### ✨ 核心能力

| 模块 | 功能 |
|------|------|
| **AI 对话** | 多模型 LLM（百炼/豆包）、SSE 流式输出、MCP Function Calling、Agent Loop |
| **视觉识别** | 聊天框内图片上传 → ONNX MobileNetV2 推理 → Prompt 注入 → LLM 自然回答 |
| **用户系统** | 邀请码注册、邮箱验证（QQ SMTP）、JWT 鉴权、RBAC 角色体系（admin/user/org） |
| **安全加固** | argon2id 密码哈希、登录失败锁定、CORS / CSP / HSTS 安全头、错误脱敏 |
| **管理后台** | 实时看板、用户/邀请码/反馈管理、日志查看器、Root 账号自动播种 |
| **工程基建** | Repository/Service 分层架构、ConfigManager 统一配置、GoogleTest、GitHub Actions CI |

### 🏗 架构（同上）

### 🔧 快速开始

```bash
# 1. 编译
sudo apt install -y cmake g++ libssl-dev libcurl4-openssl-dev \
  libmysqlcppconn-dev libopencv-dev libsodium-dev
wget https://github.com/microsoft/onnxruntime/releases/download/v1.17.1/... -O onnxruntime.tgz
tar xzf onnxruntime.tgz -C /usr/local

cd RainCppAI && mkdir build && cd build
cmake .. && make -j$(nproc)

# 2. 配置
cp ../config.json.example ../config.json
# 编辑 config.json → 设置数据库凭据 + LLM API Key

# 3. 下载模型（可选 — 启用视觉识别）
bash ../scripts/download_models.sh

# 4. 启动
./http_server -p 8088
```

### 🌍 环境变量对照

| config.json 路径 | 环境变量 |
|-----------------|---------|
| `db.host` | `DB_HOST` |
| `db.port` | `DB_PORT` |
| `db.user` | `DB_USER` |
| `db.password` | `DB_PASSWORD` |
| `db.name` | `DB_NAME` |
| `mail.username` | `MAIL_USERNAME` |
| `mail.password` | `MAIL_PASSWORD` |
| `jwt.secret` | `JWT_SECRET` |
| `default_api_keys.dashscope` | `DEFAULT_API_KEYS_DASHSCOPE` |
| `default_api_keys.doubao` | `DEFAULT_API_KEYS_DOUBAO` |

### 📚 API 路由表（同英文版）

### 📁 项目结构（同英文版）

### 📦 依赖清单（同英文版）

---

## 🤝 贡献

见 [`CONTRIBUTING.md`](CONTRIBUTING.md)（模块依赖规则、Commit 规范、开发流程）。
代码风格: [`DEVELOP_STANDARD.md`](DEVELOP_STANDARD.md)。

## 📄 许可证

[Apache License 2.0](LICENSE)

## 📖 参考资料

- [muduo — Linux 多线程服务端编程](https://github.com/chenshuo/muduo)
- [spdlog — Fast C++ logging](https://github.com/gabime/spdlog)
- [Model Context Protocol](https://modelcontextprotocol.io/)
- [ONNX Runtime C++ API](https://onnxruntime.ai/docs/api/c/)
- [阿里云百炼 API](https://help.aliyun.com/product/610100.html)
