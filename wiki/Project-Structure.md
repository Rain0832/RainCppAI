# 项目结构与依赖

## 目录树

```
RainCppAI/
├── HttpServer/          # 自研 HTTP 框架 (muduo Reactor)
│   ├── include/http/    # HttpServer, HttpRequest, HttpResponse
│   ├── include/router/  # Router + RouterHandler
│   ├── include/middleware/ # Cors, Auth, AdminAuth, RateLimit, RequestId, SecurityHeaders
│   └── include/session/ # Session + SessionManager
├── AIServerCore/        # 业务编排层 (30+ Handler)
│   ├── include/controller/  # 所有 Handler 头文件
│   ├── include/Repository/  # 数据访问层 (Account/Session/Message/ApiKey...)
│   ├── include/Service/     # 业务逻辑 (Auth/Session/ApiKey/Chat)
│   ├── include/server/      # ChatServer + SessionStore
│   └── src/                 # 实现
├── AIEngine/            # AI 工具库（零 HttpServer 依赖）
│   ├── include/llm/     # AIHelper, AIStrategy, AIFactory
│   ├── include/mcp/     # McpServer, McpClientManager, AIToolRegistry
│   ├── include/vision/  # ImageRecognizer (ONNX Runtime)
│   ├── include/audio/   # AISpeechProcessor (TTS)
│   └── include/common/  # Message, base64, AISessionIdGenerator
├── Storage/             # MySQL 连接池 + PreparedStatement
├── Common/              # 共享组件
│   ├── Auth/            # JWT 服务
│   ├── Config/          # ConfigManager
│   ├── Crypto/          # argon2id 哈希
│   ├── Logging/         # spdlog 封装
│   ├── Mail/            # SMTP 邮件
│   ├── Metrics/         # Prometheus 指标
│   ├── RateLimit/       # Token Bucket 限流
│   └── Threading/       # 线程池
├── Infralib/            # 基础设施库
│   ├── Cache/           # Redis 客户端 + SessionCache (v3.2.0)
│   └── Mq/              # RabbitMQ 生产者/消费者 + TaskMessage (v3.2.0)
├── web/                 # 前端 (HTML + Vanilla JS ES modules)
├── mcp_servers/         # Python MCP 微服务
├── Tests/               # GoogleTest 单元测试
├── deploy/              # nginx.conf + systemd service
├── scripts/             # download_models.sh
├── Docs/                # 设计文档
└── wiki/                # Wiki 源文件
```

## 依赖清单

| Library | Version | Usage | Required |
|---------|---------|-------|----------|
| muduo | 2.0+ | Network I/O (Reactor) | ✅ 核心 |
| MySQL Connector C++ | 8.0 | Database | ✅ 核心 |
| ONNX Runtime | 1.17.1 | Image recognition | ⚠️ 可选（Vision） |
| OpenCV | 4.x | Image preprocessing | ⚠️ 可选（Vision） |
| libcurl | 7.x | LLM API + SMTP | ✅ 核心 |
| OpenSSL | 3.x | HTTPS + JWT | ✅ 核心 |
| libsodium | 1.0.18 | argon2id | ✅ 核心 |
| spdlog | 1.x | Logging | ✅ 核心 |
| nlohmann/json | 3.x | JSON | ✅ 核心 |
| hiredis | 1.x | Redis client | 🆕 v3.2.0 |
| AMQP-CPP | 4.x | RabbitMQ producer/consumer | 🆕 v3.2.0 |
| libev | 4.x | Event loop for AMQP-CPP | 🆕 v3.2.0 |
| GoogleTest | 1.15.2 | Unit tests | ⚠️ 仅测试 |

## 编译系统

```bash
# CMake 最低版本: 3.16
# C++ 标准: C++17

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 产物
# ├── build/http_server      — 主程序
# ├── build/lib/libaiengine.a    — AI 引擎静态库
# ├── build/lib/libhttpserver.a  — HTTP 框架静态库
# └── build/lib/libstorage.a     — 存储层静态库
```

### CMake Target 结构

```
http_server (executable)
├── aiengine (static lib)
│   └── storage (static lib)
├── httpserver (static lib)
└── Common/* (object files)
```

## 新增依赖规范

参见 [`CONTRIBUTING.md`](https://github.com/Rain0832/RainCppAI/blob/main/CONTRIBUTING.md)：
1. 在 README 依赖表登记版本
2. 在 CONTRIBUTING.md 登记类别
3. CMake 设为可选（`find_package` + `REQUIRED` / optional）
4. CHANGELOG 说明原因
