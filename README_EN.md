# RainCppAI

English | **[中文](README.md)**

> A production-grade C++ AI application platform built on a self-developed HTTP framework, supporting multi-model LLM chat, RAG knowledge base, MCP tool calling, SSE streaming, image recognition, and speech synthesis.

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License](https://img.shields.io/badge/License-Apache2.0-green.svg)](LICENSE)
[![Version](https://img.shields.io/badge/Version-1.6.0-orange.svg)](TODO.md)

---

## ✨ Features

- **Multi-Model LLM** — Strategy + Factory pattern, hot-swap between Aliyun Qwen, Doubao, RAG, and MCP models
- **SSE Streaming** — curl `WRITEFUNCTION` callback pushes tokens in real-time; frontend renders instantly
- **Standard MCP Server** — JSON-RPC 2.0 implementing `tools/list` + `tools/call`; compatible with Claude Desktop / Cursor
- **RAG Knowledge Base** — Aliyun Bailian knowledge base integration with citation-based answers
- **Image Recognition** — ONNX Runtime + MobileNetV2 on-device inference via OpenCV
- **Speech Synthesis / ASR** — Baidu TTS with token caching and fast polling
- **Async Architecture** — AI API calls offloaded to a dedicated thread pool (8 threads), IO threads never blocked
- **Thread-Safe Session Store** — `shared_mutex` read-write lock + LRU eviction (MAX 500 sessions) + `atomic` CAS serialization
- **Async DB Writes** — RabbitMQ decouples chat persistence from the request path
- **Self-Developed HTTP Framework** — Reactor model (muduo), state-machine HTTP parser, middleware chain, regex router, session manager, SSL/TLS

---

## 🏗 Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                   Client (Browser / curl)                    │
└────────────────────────────┬────────────────────────────────┘
                             │ HTTP / HTTPS
┌────────────────────────────▼────────────────────────────────┐
│  ① Network     muduo TcpServer — Reactor, epoll, 4 threads  │
├─────────────────────────────────────────────────────────────┤
│  ② Protocol    HttpContext state machine                     │
├─────────────────────────────────────────────────────────────┤
│  ③ Middleware  MiddlewareChain (CORS, ...)                   │
├─────────────────────────────────────────────────────────────┤
│  ④ Router      unordered_map O(1) + regex O(n)              │
├─────────────────────────────────────────────────────────────┤
│  ⑤ Business    ChatServer (15 Handlers)                     │
│    ├── AIHelper ──► ThreadPool(8) ──► LLM API (curl)        │
│    │   ├── AIStrategy (Qwen / Doubao / RAG / MCP)           │
│    │   ├── McpServer (JSON-RPC 2.0 standard MCP)            │
│    │   └── MQManager ──► RabbitMQ ──► MySQL                 │
│    ├── ImageRecognizer (ONNX Runtime + OpenCV)               │
│    └── AISpeechProcessor (Baidu TTS/ASR)                     │
└─────────────────────────────────────────────────────────────┘
        │ async write          │ sync read
   ┌────▼─────┐          ┌─────▼────┐
   │ RabbitMQ │          │  MySQL   │
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

### Linux (TencentOS / Ubuntu / CentOS)

```bash
# 1. Clone
git clone git@github.com:Rain0832/RainCppAI.git && cd RainCppAI

# 2. Install system dependencies
sudo yum install -y cmake make openssl-devel libcurl-devel mysql-devel opencv-devel boost-devel git

# Install muduo
git clone https://github.com/chenshuo/muduo.git /tmp/muduo
cd /tmp/muduo && mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DMUDUO_BUILD_EXAMPLES=OFF
make -j$(nproc) && sudo make install

# Install ONNX Runtime
curl -L https://github.com/microsoft/onnxruntime/releases/download/v1.17.1/onnxruntime-linux-x64-1.17.1.tgz | tar xz -C /tmp
sudo cp -r /tmp/onnxruntime-linux-x64-1.17.1/include/* /usr/local/include/
sudo cp -r /tmp/onnxruntime-linux-x64-1.17.1/lib/* /usr/local/lib/

# Install MySQL C++ Connector
curl -L https://dev.mysql.com/get/Downloads/Connector-C++/mysql-connector-c++-8.0.33-linux-glibc2.28-x86-64bit.tar.gz | tar xz -C /tmp
sudo cp -r /tmp/mysql-connector-c++-8.0.33-*/include/* /usr/local/include/
sudo cp -r /tmp/mysql-connector-c++-8.0.33-*/lib64/* /usr/local/lib64/

# Install nlohmann/json
sudo mkdir -p /usr/local/include/nlohmann
sudo curl -sL https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp -o /usr/local/include/nlohmann/json.hpp

sudo ldconfig

# 3. Set up MySQL
mysqladmin -u root --port=3307 create ChatHttpServer
# Execute schema SQL (see Database Schema section in Chinese README)

# 4. Start RabbitMQ
sudo rabbitmq-server -detached

# 5. Configure API Keys (via browser UI after startup, or env vars)
export DASHSCOPE_API_KEY=sk-xxxx
export DOUBAO_API_KEY=xxxx

# 6. Build
mkdir -p build && cd build
cmake .. && make -j$(nproc)

# 7. Run
export LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64:$LD_LIBRARY_PATH
./http_server -p 8088
```

Open `http://localhost:8088` in your browser.

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
