# RainCppAI

> 自研 C++ HTTP 框架驱动的 AI 应用平台 — 多模型 LLM 对话 · RAG 知识库 · MCP 工具调用 · 图像识别 · 语音合成

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License](https://img.shields.io/badge/License-Apache2.0-green.svg)](LICENSE)
[![Version](https://img.shields.io/badge/Version-2.2.0-orange.svg)](CHANGELOG.md)

---

## ✨ 核心能力

- **多模型 LLM** — Strategy + Factory 模式，Aliyun Qwen / Doubao / RAG / MCP 热切换
- **MCP 工具调用** — 标准 JSON-RPC 2.0，`tools/list` + `tools/call`，兼容 Claude Desktop / Cursor
- **RAG 知识库** — 阿里云百炼知识库集成，带引用溯源
- **图像识别** — ONNX Runtime + MobileNetV2 端侧推理
- **语音合成** — 百度 TTS，Token 缓存 + 快速轮询（亚秒级响应）
- **SSE 流式输出** — curl WRITEFUNCTION 逐 token 实时推送
- **并发架构** — 8 线程 AI 线程池，IO 线程零阻塞
- **线程安全会话** — `shared_mutex` 读写锁 + LRU 淘汰（内存中最大 500 会话）
- **同步持久化** — Prepared Statement 直写 MySQL，零 SQL 注入风险

---

## 🏗 架构

```
Browser / MCP Client
        │
   HTTP / SSE / JSON-RPC
        │
┌───────▼──────────┐
│  HttpServer       │  muduo Reactor, epoll, 4 IO 线程
│  ├─ Middleware    │  CORS 中间件链
│  ├─ Router        │  精确匹配 O(1) + 正则匹配
│  └─ Session       │  内存存储 + LRU
├──────────────────┤
│  AIServerCore     │  业务编排层
│  └─ 15 Handlers   │  Chat / SSE / MCP / TTS / Upload
├──────────────────┤
│  AIEngine         │  AI 工具库 (零 HTTP 依赖)
│  ├─ llm/          │  AIHelper · AIStrategy · AIFactory
│  ├─ mcp/          │  McpServer · AIToolRegistry
│  ├─ audio/        │  AISpeechProcessor (TTS)
│  ├─ vision/       │  ImageRecognizer (ONNX)
│  └─ common/       │  AIConfig · base64 · Snowflake
├──────────────────┤
│  Storage          │  持久化基础设施
│  └─ storage/      │  DbConnectionPool · MysqlUtil
└──────────────────┘
        │
   ┌────▼────┐
   │  MySQL  │
   └─────────┘
```

---

## 🚀 快速开始

### 环境要求

| 依赖 | 版本 | 用途 |
|------|------|------|
| GCC | ≥ 12 | C++17 |
| CMake | ≥ 3.16 | 构建系统 |
| muduo | latest | 网络库 |
| OpenSSL | ≥ 1.1 | HTTPS |
| libcurl | ≥ 7.x | HTTP 客户端 |
| OpenCV | ≥ 4.x | 图像处理 |
| ONNX Runtime | 1.17.1 | 推理引擎 |
| MySQL C++ Connector | 8.0 | 数据库 |
| nlohmann/json | 3.11+ | JSON (header-only) |

> 依赖管理规则（新增 / 移除 / 版本登记）详见 `CONTRIBUTING.md` §四。

### 构建 & 运行

```bash
git clone git@github.com:Rain0832/RainCppAI.git && cd RainCppAI

# 安装依赖 (TencentOS / CentOS)
sudo yum install -y cmake make openssl-devel libcurl-devel mysql-devel opencv-devel boost-devel git

# muduo
git clone https://github.com/chenshuo/muduo.git /tmp/muduo
cd /tmp/muduo && mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DMUDUO_BUILD_EXAMPLES=OFF
make -j$(nproc) && sudo make install

# ONNX Runtime
curl -L https://github.com/microsoft/onnxruntime/releases/download/v1.17.1/onnxruntime-linux-x64-1.17.1.tgz | tar xz -C /tmp
sudo cp -r /tmp/onnxruntime-linux-x64-1.17.1/include/* /usr/local/include/
sudo cp -r /tmp/onnxruntime-linux-x64-1.17.1/lib/* /usr/local/lib/

# nlohmann/json
sudo mkdir -p /usr/local/include/nlohmann
sudo curl -sL https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp -o /usr/local/include/nlohmann/json.hpp

sudo ldconfig

# 构建
mkdir -p build && cd build && cmake .. && make -j$(nproc)

# 运行 (默认端口 80, -p 指定)
export LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64:$LD_LIBRARY_PATH
./http_server -p 8088
```

浏览器打开 `http://localhost:8088`。

---

## 📚 API

| Method | Path | Auth | 说明 |
|--------|------|------|------|
| GET | `/` `/entry` | ✗ | 登录 / 注册 |
| POST | `/login` | ✗ | 登录，返回 userId |
| POST | `/register` | ✗ | 注册 |
| POST | `/user/logout` | ✓ | 登出 |
| GET | `/menu` | ✓ | 功能菜单 |
| GET | `/chat` | ✓ | 聊天页 |
| POST | `/chat/send-stream` | ✓ | SSE 流式发送（唯一对话入口，支持自动创建会话） |
| GET | `/chat/sessions` | ✓ | 会话列表 |
| POST | `/chat/history` | ✓ | 会话历史 |
| POST | `/chat/tts` | ✓ | 语音合成 |
| POST | `/chat/update-title` | ✓ | 更新会话标题 |
| POST | `/chat/delete-session` | ✓ | 软删除会话 |
| GET | `/api/user/apikey` | ✓ | 获取已保存的 API Key（掩码） |
| POST | `/api/user/apikey` | ✓ | 保存 API Key |
| GET | `/css/*` | ✗ | 静态 CSS 资源 |
| GET | `/js/*` | ✗ | 静态 JS 资源 |
| GET | `/assets/*` | ✗ | 静态图片 / 字体资源 |
| GET | `/upload` | ✓ | 图像识别页 |
| POST | `/upload/send` | ✓ | 提交识别 |
| POST | `/mcp` | ✗ | MCP JSON-RPC 2.0 |

---

## 📁 项目结构

```
RainCppAI/
├── HttpServer/           # 自研 HTTP 框架（纯网络库）
│   ├── include/
│   │   ├── http/         # HttpServer, HttpRequest, HttpResponse, HttpContext
│   │   ├── router/       # Router (精确 + 正则), RouterHandler
│   │   ├── middleware/   # MiddlewareChain, CorsMiddleware
│   │   ├── session/      # Session, SessionManager, SessionStorage
│   │   ├── ssl/          # SslConfig, SslConnection, SslContext
│   │   └── utils/        # ThreadPool, FileUtil
│   ├── src/
│   └── TECHDOC.md
├── Storage/              # 持久化基础设施
│   ├── include/
│   │   └── storage/      # DbConnection, DbConnectionPool, MysqlUtil, DbException
│   ├── src/
│   └── TECHDOC.md
├── AIServerCore/         # 业务逻辑层
│   ├── include/
│   │   ├── server/       # ChatServer
│   │   └── controller/   # 15 HTTP Handlers
│   ├── src/
│   └── TECHDOC.md
├── AIEngine/             # AI 工具库（零 HTTP 依赖）
│   ├── include/
│   │   ├── llm/          # AIHelper, AIStrategy, AIFactory
│   │   ├── mcp/          # McpServer, AIToolRegistry
│   │   ├── audio/        # AISpeechProcessor
│   │   ├── vision/       # ImageRecognizer
│   │   └── common/       # AIConfig, base64, AISessionIdGenerator (Snowflake)
│   ├── src/
│   └── TECHDOC.md
├── web/                  # 前端资源
│   ├── *.html            # 服务端模板
│   ├── config.json       # 前端配置
│   ├── css/              # 样式表
│   ├── js/               # 脚本
│   ├── assets/           # 图片 / 字体
│   └── TECHDOC.md
├── CMakeLists.txt
├── DEVELOP_STANDARD.md   # 开发规范
├── CHANGELOG.md          # 变更日志
├── TODO.md               # 待办路线图
└── README.md
```

---

## 📖 参考

- [muduo — Linux 多线程服务端编程](https://github.com/chenshuo/muduo)
- [ONNX Runtime C++ API](https://onnxruntime.ai/docs/api/c/)
- [Model Context Protocol](https://modelcontextprotocol.io/)
- [阿里云百炼 API](https://help.aliyun.com/product/610100.html)
