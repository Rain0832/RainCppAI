# RainCppAI

**[English](README_EN.md)** | 中文

> 基于自研 C++17 HTTP 框架的 AI 应用服务平台，支持多模型对话、RAG 知识库、MCP 工具调用、SSE 流式输出、图像识别与语音合成。

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License](https://img.shields.io/badge/License-Apache2.0-green.svg)](LICENSE)
[![Version](https://img.shields.io/badge/Version-1.6.0-orange.svg)](TODO.md)

---

## ✨ 功能特性

- **多模型 LLM 对话** — 策略模式 + 工厂模式，热切换通义千问、豆包、百炼 RAG、百炼 MCP
- **SSE 流式输出** — curl 回调逐 token 推送，前端实时渲染，告别等待
- **标准 MCP Server** — JSON-RPC 2.0 实现 `tools/list` + `tools/call`，可被 Claude Desktop / Cursor 直接接入
- **RAG 知识库** — 对接阿里百炼知识库，检索增强生成
- **图像识别** — ONNX Runtime + MobileNetV2 端侧推理
- **语音合成 / 识别** — 百度 TTS（token 缓存 + 快速轮询）
- **异步架构** — AI 调用卸载到独立线程池（8 线程），IO 线程零阻塞
- **并发安全** — `shared_mutex` 读写锁 + LRU 淘汰（最大 500 会话）+ `atomic` CAS 串行化
- **异步入库** — RabbitMQ 解耦聊天记录持久化
- **自研 HTTP 框架** — Reactor 模型（muduo）、状态机解析、中间件洋葱模型、双匹配路由、会话管理、SSL/TLS

---

## 🏗 架构

```
┌─────────────────────────────────────────────────────────────┐
│                    客户端 (浏览器 / curl)                     │
└────────────────────────────┬────────────────────────────────┘
                             │ HTTP / HTTPS
┌────────────────────────────▼────────────────────────────────┐
│  ① 网络层     muduo TcpServer — Reactor, epoll, 4 线程       │
├─────────────────────────────────────────────────────────────┤
│  ② 协议层     HttpContext 状态机解析                          │
├─────────────────────────────────────────────────────────────┤
│  ③ 中间件层   MiddlewareChain (CORS 等)                      │
├─────────────────────────────────────────────────────────────┤
│  ④ 路由层     精确匹配 O(1) + 正则匹配 O(n)                  │
├─────────────────────────────────────────────────────────────┤
│  ⑤ 业务层     ChatServer (15 个 Handler)                     │
│    ├── AIHelper ──► 线程池(8) ──► LLM API (curl)             │
│    │   ├── AIStrategy (通义千问 / 豆包 / RAG / MCP)           │
│    │   ├── McpServer (JSON-RPC 2.0 标准 MCP)                 │
│    │   └── MQManager ──► RabbitMQ ──► MySQL                  │
│    ├── ImageRecognizer (ONNX Runtime + OpenCV)                │
│    └── AISpeechProcessor (百度 TTS/ASR)                      │
└─────────────────────────────────────────────────────────────┘
        │ 异步写入           │ 同步读取
   ┌────▼─────┐          ┌─────▼────┐
   │ RabbitMQ │          │  MySQL   │
   └──────────┘          └──────────┘
```

---

## 🚀 快速开始

### 依赖项

| 依赖 | 版本 | 说明 |
|-----|------|------|
| GCC | ≥ 12 | 需要 C++17 |
| CMake | ≥ 3.16 | |
| muduo | 最新 | 需源码编译 |
| OpenSSL | ≥ 1.1 | |
| libcurl | ≥ 7.x | |
| OpenCV | ≥ 4.x | |
| ONNX Runtime | 1.17.1 | 预编译包 |
| MySQL C++ Connector | 8.0 | JDBC 头文件路径 `/usr/local/include/jdbc` |
| SimpleAmqpClient | 最新 | 依赖 rabbitmq-c |
| nlohmann/json | 3.11+ | Header-only |

### Linux (TencentOS / Ubuntu / CentOS)

```bash
# 1. 克隆
git clone git@github.com:Rain0832/RainCppAI.git && cd RainCppAI

# 2. 安装依赖
sudo yum install -y cmake make openssl-devel libcurl-devel mysql-devel opencv-devel boost-devel git
# muduo、ONNX Runtime、MySQL C++ Connector、nlohmann/json 的安装详见 README_EN.md

# 3. 配置 MySQL
mysqladmin -u root --port=3307 create ChatHttpServer
mysql -u root --port=3307 ChatHttpServer < schema.sql  # 或手动执行建表 SQL

# 4. 启动 RabbitMQ
sudo rabbitmq-server -detached

# 5. 配置 API Key（可在启动后通过浏览器 UI 配置）
export DASHSCOPE_API_KEY=sk-xxxx      # 阿里百炼
export DOUBAO_API_KEY=xxxx            # 豆包

# 6. 编译
mkdir -p build && cd build
cmake .. && make -j$(nproc)

# 7. 运行
export LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64:$LD_LIBRARY_PATH
./http_server -p 8088
```

浏览器打开 `http://localhost:8088` 即可使用。

### API Key 配置

登录后进入 **个人中心 → API Key** 标签页，填入对应模型的密钥即可。密钥存储在浏览器本地，随请求发送，无需重启服务。

---

## 📚 API 接口

| 方法 | 路径 | 鉴权 | 说明 |
|------|------|------|------|
| GET | `/` 或 `/entry` | ✗ | 登录 / 注册页 |
| POST | `/login` | ✗ | 登录，返回 `userId` |
| POST | `/register` | ✗ | 注册 |
| POST | `/user/logout` | ✓ | 退出登录 |
| GET | `/menu` | ✓ | 主菜单 |
| GET | `/chat` | ✓ | 聊天页面 |
| POST | `/chat/send` | ✓ | 发送消息（异步 AI） |
| POST | `/chat/send-stream` | ✓ | SSE 流式发送 |
| POST | `/chat/send-new-session` | ✓ | 新建会话并发送 |
| GET | `/chat/sessions` | ✓ | 会话列表 |
| POST | `/chat/history` | ✓ | 会话历史 |
| POST | `/chat/tts` | ✓ | 语音合成 |
| GET | `/upload` | ✓ | 图像识别页 |
| POST | `/upload/send` | ✓ | 提交图像识别 |
| POST | `/mcp` | ✗ | MCP Server (JSON-RPC 2.0) |

---

## 🗃 数据库表结构

```sql
-- 用户表
CREATE TABLE users (
  id INT AUTO_INCREMENT PRIMARY KEY,
  username VARCHAR(255) NOT NULL UNIQUE,
  password VARCHAR(255) NOT NULL
) CHARSET=utf8mb4;

-- 会话表 (v1.5.0+)
CREATE TABLE sessions (
  id VARCHAR(64) NOT NULL PRIMARY KEY,
  user_id BIGINT UNSIGNED NOT NULL,
  title VARCHAR(128) DEFAULT NULL,
  model_type VARCHAR(32) DEFAULT NULL,
  created_at DATETIME(3) DEFAULT CURRENT_TIMESTAMP(3),
  updated_at DATETIME(3) DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  deleted_at DATETIME DEFAULT NULL,
  INDEX idx_user_id (user_id)
) CHARSET=utf8mb4;

-- 消息表 (v1.5.0+)
CREATE TABLE messages (
  id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  session_id VARCHAR(64) NOT NULL,
  user_id BIGINT UNSIGNED NOT NULL,
  role ENUM('user','assistant','system') NOT NULL,
  content MEDIUMTEXT NOT NULL,
  model VARCHAR(64) DEFAULT NULL,
  created_at DATETIME(3) DEFAULT CURRENT_TIMESTAMP(3),
  INDEX idx_session_created (session_id, created_at)
) ENGINE=InnoDB CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC;

-- API Key 表 (v1.5.0+)
CREATE TABLE user_api_keys (
  id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  user_id BIGINT UNSIGNED NOT NULL,
  provider VARCHAR(32) NOT NULL,
  api_key VARCHAR(512) NOT NULL,
  UNIQUE KEY uk_user_provider (user_id, provider)
) CHARSET=utf8mb4;
```

---

## 📋 更新日志

### v1.6.0
**SSE 流式输出 + 标准 MCP Server**
- `chatStream()` + curl `WRITEFUNCTION` 逐 token 回调推送
- `ChatSseHandler` — SSE 握手 + `runInLoop` 流式回写
- `McpServer` — JSON-RPC 2.0 标准实现，可被 Claude Desktop / Cursor 直接接入
- 前端改用 `fetch().body.getReader()` 实时渲染

### v1.5.0
**数据库表结构重设计** — sessions / messages / user_api_keys 三高表

### v1.4.0
**并发 Bug 修复（6 项）** — msgMutex_ / Message.role / CAS 串行化 / exclusive=false / wait_for

### v1.3.0
**异步线程池** — deferred 响应 + ThreadPool + runInLoop，IO 零阻塞

### v1.2.0
**读写锁 + LRU** — shared_mutex + list+map O(1) 淘汰

### v1.1.0
**动态 API Key + 前端重构**

### v1.0.0
初始部署

---

## 🗺 路线图

### v2.0.0
- 可观测性（结构化日志 + Prometheus Metrics）
- Nginx 反向代理 + Redis Session 共享
- C++20 协程探索

---

## 📁 项目结构

```
RainCppAI/
├── HttpServer/          # 自研 HTTP 框架
│   ├── include/
│   │   ├── http/        # HttpServer, HttpRequest, HttpResponse
│   │   ├── router/      # Router (精确 + 正则)
│   │   ├── middleware/   # MiddlewareChain, CorsMiddleware
│   │   ├── session/     # SessionManager, SessionStorage
│   │   ├── ssl/         # SslContext, SslConnection
│   │   └── utils/       # MysqlUtil, ThreadPool, DbConnectionPool
│   └── src/
├── AIApps/ChatServer/   # AI 聊天应用
│   ├── include/
│   │   ├── ChatServer.h
│   │   ├── AIUtil/      # AIHelper, AIStrategy, McpServer, MQManager...
│   │   └── handlers/    # 15 个 HTTP Handler
│   ├── resource/        # 前端 HTML (entry, menu, AI, upload)
│   └── src/
├── Internview/          # 面试知识库
├── CMakeLists.txt
├── TODO.md
└── README_EN.md         # English version
```

---

## 📖 参考资料

- [muduo — Linux 多线程服务端编程](https://github.com/chenshuo/muduo)
- [ONNX Runtime C++ API](https://onnxruntime.ai/docs/api/c/)
- [Model Context Protocol](https://modelcontextprotocol.io/)
- [阿里云百炼 API](https://help.aliyun.com/product/610100.html)
