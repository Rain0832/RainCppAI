# RainCppAI

> A production-grade C++ AI application platform built on a self-developed HTTP framework, supporting multi-model LLM chat, RAG knowledge base, MCP tool calling, image recognition, and speech synthesis.

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License](https://img.shields.io/badge/License-Apache2.0-green.svg)](LICENSE)
[![Version](https://img.shields.io/badge/Version-1.3.0-orange.svg)](CHANGELOG)

---

## ✨ Features

- **Multi-Model LLM** — Strategy + Factory pattern, hot-swap between Aliyun Qwen, Doubao, RAG, and MCP models
- **MCP Tool Calling** — Two-stage inference (intent detection → tool execution → answer), aligning with Model Context Protocol
- **RAG Knowledge Base** — Aliyun Bailian knowledge base integration with citation-based answers
- **Image Recognition** — ONNX Runtime + MobileNetV2 on-device inference via OpenCV
- **Speech Synthesis / ASR** — Baidu TTS with token caching and fast polling (sub-second response)
- **Async Architecture** — AI API calls offloaded to a dedicated thread pool (8 threads), IO threads never blocked
- **Thread-Safe Session Store** — `shared_mutex` read-write lock + LRU eviction (MAX 500 sessions in memory)
- **Async DB Writes** — RabbitMQ decouples chat persistence from the request path
- **Self-Developed HTTP Framework** — Reactor model (muduo), state-machine HTTP parser, middleware chain, regex router, session manager, SSL/TLS

---

## 🏗 Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                   Client (Browser / curl)                   │
└────────────────────────────┬────────────────────────────────┘
                             │ HTTP / HTTPS
┌────────────────────────────▼────────────────────────────────┐
│  ① Network     muduo TcpServer — Reactor, epoll, 4 threads  │
├─────────────────────────────────────────────────────────────┤
│  ② Protocol    HttpContext state machine                    │
├─────────────────────────────────────────────────────────────┤
│  ③ Middleware  MiddlewareChain (CORS, ...)                   │
├─────────────────────────────────────────────────────────────┤
│  ④ Router      unordered_map O(1) + regex O(n)              │
├─────────────────────────────────────────────────────────────┤
│  ⑤ Business    ChatServer (13 Handlers)                     │
│    ├── AIHelper ──► ThreadPool(8) ──► LLM API (curl)        │
│    │   ├── AIStrategy (Aliyun / Doubao / RAG / MCP)         │
│    │   ├── AIConfig + AIToolRegistry (MCP tools)            │
│    │   └── MQManager ──► RabbitMQ ──► MySQL                 │
│    ├── ImageRecognizer (ONNX Runtime + OpenCV)               │
│    ├── AISpeechProcessor (Baidu TTS/ASR, token cache)        │
│    └── SessionManager (Cookie + memory store)                │
└─────────────────────────────────────────────────────────────┘
        │ async write          │ sync read
   ┌────▼─────┐          ┌─────▼────┐
   │ RabbitMQ │          │  MySQL   │
   │ (MQ Pool)│          │ (DB Pool)│
   └──────────┘          └──────────┘
```

---

## 🚀 Quick Start

### Prerequisites

| Dependency | Version | Notes |
|-----------|---------|-------|
| GCC | ≥ 12 | C++17 required |
| CMake | ≥ 3.16 | |
| muduo | latest | Build from source |
| OpenSSL | ≥ 1.1 | |
| libcurl | ≥ 7.x | |
| OpenCV | ≥ 4.x | |
| ONNX Runtime | 1.17.1 | Pre-built binary |
| MySQL C++ Connector | 8.0 | JDBC header path `/usr/local/include/jdbc` |
| SimpleAmqpClient | latest | rabbitmq-c dependency |
| nlohmann/json | 3.11+ | Header-only |

### Linux (TencentOS / Ubuntu / Debian)

```bash
# 1. Clone
git clone git@github.com:Rain0832/RainCppAI.git && cd RainCppAI

# 2. Install dependencies (TencentOS / CentOS)
sudo yum install -y cmake make openssl-devel libcurl-devel mysql-devel opencv-devel boost-devel git

# Install muduo (build from source)
git clone https://github.com/chenshuo/muduo.git /tmp/muduo
cd /tmp/muduo && mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DMUDUO_BUILD_EXAMPLES=OFF
make -j$(nproc) && sudo make install

# Install ONNX Runtime
curl -L https://github.com/microsoft/onnxruntime/releases/download/v1.17.1/onnxruntime-linux-x64-1.17.1.tgz \
  | tar xz -C /tmp
sudo cp -r /tmp/onnxruntime-linux-x64-1.17.1/include/* /usr/local/include/
sudo cp -r /tmp/onnxruntime-linux-x64-1.17.1/lib/* /usr/local/lib/

# Install MySQL C++ Connector
curl -L https://dev.mysql.com/get/Downloads/Connector-C++/mysql-connector-c++-8.0.33-linux-glibc2.28-x86-64bit.tar.gz \
  | tar xz -C /tmp
sudo cp -r /tmp/mysql-connector-c++-8.0.33-*/include/* /usr/local/include/
sudo cp -r /tmp/mysql-connector-c++-8.0.33-*/lib64/* /usr/local/lib64/

# Install nlohmann/json
sudo mkdir -p /usr/local/include/nlohmann
sudo curl -sL https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp \
  -o /usr/local/include/nlohmann/json.hpp

sudo ldconfig

# 3. Set up MySQL
mysqladmin -u root --port=3307 create ChatHttpServer
mysql -u root --port=3307 ChatHttpServer <<'SQL'
CREATE TABLE users (
  id INT AUTO_INCREMENT PRIMARY KEY,
  username VARCHAR(255) NOT NULL UNIQUE,
  password VARCHAR(255) NOT NULL
) CHARSET=utf8mb4;

CREATE TABLE chat_message (
  id INT NOT NULL,
  username VARCHAR(255) NOT NULL,
  session_id VARCHAR(255) NOT NULL DEFAULT '',
  is_user TINYINT NOT NULL DEFAULT 1,
  content TEXT NOT NULL,
  ts BIGINT NOT NULL DEFAULT 0,
  KEY idx_ts (ts)
) CHARSET=utf8mb4;

CREATE USER IF NOT EXISTS 'chat'@'%' IDENTIFIED BY '123456';
GRANT ALL PRIVILEGES ON ChatHttpServer.* TO 'chat'@'%';
FLUSH PRIVILEGES;
SQL

# 4. Start RabbitMQ
sudo rabbitmq-server -detached

# 5. Configure API Keys (in browser UI after startup, or via env for server-side fallback)
export DASHSCOPE_API_KEY=sk-xxxx      # Aliyun Qwen / RAG / MCP
export DOUBAO_API_KEY=xxxx            # Doubao (Volcengine)
export BAIDU_CLIENT_ID=xxxx           # Baidu TTS/ASR
export BAIDU_CLIENT_SECRET=xxxx

# 6. Build
mkdir -p build && cd build
cmake .. && make -j$(nproc)

# 7. Run (default port 80, use -p to specify)
export LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64:$LD_LIBRARY_PATH
./http_server -p 8088
```

Open `http://localhost:8088` in your browser.

### API Keys via Web UI

After login, go to **Personal Center → API Key** tab to configure keys per model. Keys are stored in browser `localStorage` and sent with each request — no server restart required.

---

## 📚 API Reference

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | `/` or `/entry` | ✗ | Login / Register page |
| POST | `/login` | ✗ | Login, returns `userId` |
| POST | `/register` | ✗ | Register new user |
| POST | `/user/logout` | ✓ | Logout |
| GET | `/menu` | ✓ | Main menu page |
| GET | `/chat` | ✓ | Chat page |
| POST | `/chat/send` | ✓ | Send message (async AI call) |
| POST | `/chat/send-new-session` | ✓ | Create new session and send |
| GET | `/chat/sessions` | ✓ | List user sessions |
| POST | `/chat/history` | ✓ | Get session history |
| POST | `/chat/tts` | ✓ | Text-to-speech |
| GET | `/upload` | ✓ | Image recognition page |
| POST | `/upload/send` | ✓ | Submit image for recognition |

---

## 🗃 Database Schema

```sql
-- Users
CREATE TABLE users (
  id       INT AUTO_INCREMENT PRIMARY KEY,
  username VARCHAR(255) NOT NULL UNIQUE,
  password VARCHAR(255) NOT NULL
) CHARSET=utf8mb4;

-- Chat messages (ordered by ts ASC for replay)
CREATE TABLE chat_message (
  id         INT NOT NULL,          -- user_id (from users.id)
  username   VARCHAR(255) NOT NULL,
  session_id VARCHAR(255) NOT NULL DEFAULT '',
  is_user    TINYINT NOT NULL DEFAULT 1,  -- 1=user, 0=AI
  content    TEXT NOT NULL,
  ts         BIGINT NOT NULL DEFAULT 0,   -- millisecond timestamp
  KEY idx_ts (ts)
) CHARSET=utf8mb4;
```

---

## 📋 Changelog

### v1.4.0 (2026-04-03)
**Phase 1 — Concurrency Bug Fixes (6 items)**
- `AIHelper::messages_` now protected by `msgMutex_` (was completely unprotected)
- `Message` struct with explicit `role` field (`"user"` / `"assistant"`) — eliminates the fragile even/odd index heuristic for role detection
- `atomic<bool> processing_` prevents concurrent requests to the same session from corrupting message order; second request returns an instant error instead of racing
- `ChatHistoryHandler`: `GetMessages()` called after `chatInfo` lock is released, using AIHelper's own `msgMutex_` — fixes data race between history reads and thread-pool writes
- RabbitMQ consumer: `DeclareQueue exclusive=false` — both consumer threads can now share the queue without `ACCESS_REFUSED`
- DB connection pool: `cv_.wait_for(3s)` replaces infinite `cv_.wait` — threads now fail fast under high load instead of hanging indefinitely

### v1.3.0 (2026-04-02)
**Async AI Thread Pool — IO threads never blocked**
- New `ThreadPool` class (std::thread + queue + condition_variable + future)
- `HttpResponse` supports deferred (async) mode with `TcpConnectionPtr` injection
- `HttpServer::onRequest` conditionally skips auto-send for async handlers
- `ChatSendHandler` and `ChatCreateAndSendHandler` submit AI calls to thread pool
- AI concurrent capacity: 4 (IO threads) → **8** (thread pool), IO threads unblocked

### v1.2.0 (2026-04-01)
**Thread Safety — `shared_mutex` + LRU eviction**
- All `std::mutex` → `std::shared_mutex` (C++17 read-write lock)
- Read ops use `shared_lock`, write ops use `unique_lock`
- LRU eviction: `std::list` + `unordered_map`, O(1) touch/evict, MAX_SESSIONS=500
- Covers: `chatInformation`, `onlineUsers`, `sessionsIdsMap`, `ImageRecognizerMap`

### v1.1.0 (2026-04-01)
**Dynamic API Key + Frontend Rebuild**
- `AIStrategy::setApiKey()` allows runtime key override (no env var required)
- Frontend passes API key from `localStorage` with each request
- Full frontend rebuild: light/dark theme, typewriter effect, personal center, session management

### v1.0.0 (2026-04-01)
Initial deployment on Linux (TencentOS 4.4).  
Self-developed HTTP framework + multi-model AI chat + RAG + MCP + image recognition + speech.

---

## 🗺 Roadmap

### v1.5.0 — SSE Streaming Output
- `HttpResponse` supports `Transfer-Encoding: chunked`
- `AIHelper::chat()` uses curl `WRITEFUNCTION` callback for token-by-token forwarding
- Frontend `EventSource` replaces typewriter simulation with real streaming

### v2.0.0 — DB Schema Redesign + Standard MCP + Observability
- New table schema: `sessions` table, `messages` table with explicit `role` column, `user_api_keys` table
- Standard MCP Server (JSON-RPC 2.0 / stdio transport)
- Structured logging (JSON format + request ID)
- Prometheus metrics (QPS, latency, pool utilization)

---

## 📁 Project Structure

```
RainCppAI/
├── HttpServer/          # Self-developed HTTP framework
│   ├── include/
│   │   ├── http/        # HttpServer, HttpRequest, HttpResponse
│   │   ├── router/      # Router (exact + regex)
│   │   ├── middleware/  # MiddlewareChain, CorsMiddleware
│   │   ├── session/     # SessionManager, SessionStorage
│   │   ├── ssl/         # SslContext, SslConnection
│   │   └── utils/       # MysqlUtil, ThreadPool, DbConnectionPool
│   └── src/
├── AIApps/ChatServer/   # AI chat application
│   ├── include/
│   │   ├── ChatServer.h
│   │   ├── AIUtil/      # AIHelper, AIStrategy, AIFactory, MQManager...
│   │   └── handlers/    # 13 HTTP handlers
│   ├── resource/        # HTML frontend (entry, menu, AI, upload)
│   └── src/
├── Internview/          # Interview knowledge base
│   ├── 00-项目详解/
│   ├── 01-AI-Agent基础/
│   ├── 02-网络基础/
│   ├── 03-Cpp基础/
│   └── 04-操作系统基础/
├── CMakeLists.txt
└── TODO.md
```

---

## 📖 References

- [muduo — Linux 多线程服务端编程](https://github.com/chenshuo/muduo)
- [ONNX Runtime C++ API](https://onnxruntime.ai/docs/api/c/)
- [Model Context Protocol](https://modelcontextprotocol.io/)
- [Aliyun Bailian API](https://help.aliyun.com/product/610100.html)
