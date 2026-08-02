# Dr.Rain — Healthcare AI Assistant

English | **[中文](README.md)**

> A production-grade C++ healthcare AI platform. Multi-model LLM · SSE streaming · MCP tools · Admin dashboard · RBAC.

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-%E2%89%A5%203.16-blue.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-Apache%202.0-green.svg)](LICENSE)
[![Version](https://img.shields.io/badge/Version-v3.0.0-orange.svg)](CHANGELOG.md)

---

## ✨ Features

| Module | Capability |
|---|---|
| **Multi-Model LLM** | Aliyun Qwen / Doubao via Strategy + Factory pattern, native Function Calling |
| **SSE Streaming** | curl `WRITEFUNCTION` token-by-token push, real-time frontend rendering |
| **MCP Tools** | JSON-RPC 2.0 `tools/list` + `tools/call`, stdio/sse remote servers |
| **Admin Dashboard** | RBAC + real-time SSR/SSE stats + user management + logs + feedback |
| **Security** | JWT HS256 + argon2id + login lockout + CSP/HSTS + Token Bucket rate limiting |
| **Structured Logging** | spdlog dual-sink (colored console + JSON file), request_id tracing |
| **Healthcare Persona** | Dr.Rain System Prompt injection + medical disclaimer |
| **Image Recognition** | ONNX Runtime + MobileNetV2 on-device inference |
| **Speech Synthesis** | Baidu TTS with token caching and fast polling |

---

## 🏗 Architecture

```
Browser / Admin / MCP Client
        │  HTTP / SSE / JSON-RPC
┌───────▼──────────┐
│  HttpServer       │  muduo Reactor, epoll, 4 IO threads
│  ├─ Middleware    │  CORS, Auth (JWT), Admin RBAC, Security, RateLimit, RequestId
│  ├─ Router        │  Exact O(1) + Regex O(n)
│  └─ Session       │  In-memory + LRU (MAX 500)
├──────────────────┤
│  AIServerCore     │  Business orchestration (Controller → Service → Repository)
├──────────────────┤
│  AIEngine         │  AI library (libaiengine.a, zero HTTP dependency)
│  ├─ llm/          │  AIHelper · AIStrategy · AIFactory
│  ├─ mcp/          │  AIToolRegistry · McpClientManager (stdio/sse)
│  └─ audio/vision/ │  AISpeechProcessor · ImageRecognizer
├──────────────────┤
│  Storage          │  Persistence (libstorage.a, Prepared Statement)
├──────────────────┤
│  Common/          │  ConfigManager, Logger(spdlog), JwtService, TokenBucket, PasswordHash ...
└──────────────────┘
        │
   ┌────▼────┐
   │  MySQL  │
   └─────────┘
```

---

## 🚀 Local Deployment

### Prerequisites

| Dependency | Purpose |
|-----------|---------|
| GCC ≥ 12, CMake ≥ 3.16 | C++17 |
| muduo, OpenSSL, libcurl | Networking + HTTPS |
| spdlog ≥ 1.12 | Logging |
| MySQL C++ Connector 8.0 | Database |
| libsodium ≥ 1.0.18 | argon2id hashing |
| OpenCV ≥ 4.x, ONNX Runtime 1.17.1 | Image recognition |

### Build

```bash
git clone git@github.com:Rain0832/RainCppAI.git && cd RainCppAI

# 1. System dependencies (Ubuntu 24.04)
sudo apt-get install -y cmake g++ make libcurl4-openssl-dev libssl-dev \
    libspdlog-dev libopencv-dev libmysqlcppconn-dev libmysqlclient-dev \
    libsodium-dev nlohmann-json3-dev clang-format

# 2. Build muduo
git clone --depth=1 https://github.com/chenshuo/muduo.git /tmp/muduo
cd /tmp/muduo && mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DMUDUO_BUILD_EXAMPLES=OFF
make -j$(nproc) && sudo make install

# 3. ONNX Runtime
curl -fsSL https://github.com/microsoft/onnxruntime/releases/download/v1.17.1/onnxruntime-linux-x64-1.17.1.tgz | tar xz -C /tmp
sudo cp -r /tmp/onnxruntime-linux-x64-1.17.1/include/* /usr/local/include/
sudo cp -r /tmp/onnxruntime-linux-x64-1.17.1/lib/* /usr/local/lib/
sudo ldconfig

# 4. Configure
cp config.json.example config.json
# Edit config.json with your DB password, JWT secret, etc.

# 5. Create database (tables auto-created on first startup)
mysql -u root -e "CREATE DATABASE IF NOT EXISTS ChatHttpServer"

# 6. Build & Run
mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)
./http_server -p 8088
```

Open `http://localhost:8088` in your browser.

> **SSE reverse proxy**: Nginx requires `proxy_buffering off;`, see [`deploy/nginx.conf`](deploy/nginx.conf).
> **Systemd service**: See [`deploy/raincppai.service`](deploy/raincppai.service).

---

## 📚 API Overview

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| `POST` | `/login` | ✗ | Login, returns JWT cookie |
| `POST` | `/register` | ✗ | Register (invite code + email verify) |
| `POST` | `/chat/send-stream` | ✓ | SSE chat (sole entry point) |
| `GET` | `/chat/sessions` | ✓ | Session list |
| `POST` | `/chat/history` | ✓ | Chat history |
| `POST` | `/api/feedback` | ✓ | Submit feedback |
| `POST` | `/mcp` | ✗ | MCP JSON-RPC 2.0 |
| `GET` | `/admin/dashboard` | admin | Admin dashboard |
| `GET` | `/admin/logs` | admin | Log viewer |
| `GET` | `/admin/api/users` | admin | User list |
| `GET` | `/admin/api/feedback` | admin | Feedback list |

---

## 📁 Project Structure

```
RainCppAI/
├── HttpServer/           # Self-developed HTTP framework (libhttpserver.a)
├── Storage/              # Persistence layer (libstorage.a)
├── AIServerCore/         # Business orchestration (controllers/services/repositories)
├── AIEngine/             # AI library (libaiengine.a, zero HTTP deps)
├── Common/               # Shared components (Config, Logging, Auth, Crypto, Mail...)
├── web/                  # Frontend (SSR templates + vanilla HTML/JS/CSS)
├── deploy/               # Deployment configs (nginx, systemd)
├── Docs/                 # Design documents (Plans)
├── Tests/                # GoogleTest unit tests
└── mcp_servers/          # Python MCP microservices
```

---

## 🤝 Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for module dependency rules, commit conventions, and development workflow.
Coding style: [`DEVELOP_STANDARD.md`](DEVELOP_STANDARD.md).

---

## 📖 References

- [muduo — Linux Multithreaded Server Programming](https://github.com/chenshuo/muduo)
- [spdlog — Fast C++ logging](https://github.com/gabime/spdlog)
- [Model Context Protocol](https://modelcontextprotocol.io/)
- [Alibaba Bailian API](https://help.aliyun.com/product/610100.html)

---

## 📚 API Reference

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/` or `/entry` | ✗ | Login / Register page |
| POST | `/login` | ✗ | Login |
| POST | `/register` | ✗ | Register |
| POST | `/user/logout` | ✓ | Logout |
| GET | `/chat` | ✓ | Chat page |
| POST | `/chat/send` | ✓ | Send message (async) |
| POST | `/chat/send-stream` | ✓ | SSE streaming send |
| POST | `/chat/send-new-session` | ✓ | Create session and send |
| GET | `/chat/sessions` | ✓ | List sessions |
| POST | `/chat/history` | ✓ | Session history |
| POST | `/chat/tts` | ✓ | Text-to-speech |
| POST | `/upload/send` | ✓ | Image recognition |
| POST | `/mcp` | ✗ | MCP Server (JSON-RPC 2.0) |

---

## 📋 Changelog

### v1.6.0 — SSE Streaming + Standard MCP Server
### v1.5.0 — Database Schema Redesign (sessions / messages / user_api_keys)
### v1.4.0 — Concurrency Bug Fixes (6 items)
### v1.3.0 — Async AI Thread Pool (IO threads never blocked)
### v1.2.0 — shared_mutex + LRU eviction
### v1.1.0 — Dynamic API Key + Frontend Rebuild
### v1.0.0 — Initial deployment

> See [中文 README](README.md) for detailed changelog.

---

## 📁 Project Structure

```
RainCppAI/
├── HttpServer/          # Self-developed HTTP framework
│   ├── include/         # http / router / middleware / session / ssl / utils
│   └── src/
├── AIApps/ChatServer/   # AI chat application
│   ├── include/         # ChatServer.h / AIUtil / handlers
│   ├── resource/        # HTML frontend
│   └── src/
├── Internview/          # Interview knowledge base (Chinese)
├── CMakeLists.txt
├── TODO.md
└── README.md            # 中文版
```

---

## 📖 References

- [muduo — Linux Multi-threaded Server Programming](https://github.com/chenshuo/muduo)
- [ONNX Runtime C++ API](https://onnxruntime.ai/docs/api/c/)
- [Model Context Protocol](https://modelcontextprotocol.io/)
- [Aliyun Bailian API](https://help.aliyun.com/product/610100.html)
