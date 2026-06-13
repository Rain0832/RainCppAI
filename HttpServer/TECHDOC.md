# HttpServer — 模块技术文档

## 模块职责

HttpServer 是本项目的**自研 HTTP 网络框架**，负责所有网络 I/O、协议解析、路由分发、中间件和会话管理。定位为**纯网络库**，不包含任何 AI 业务逻辑。

## 核心文件流转逻辑

```
TcpServer (muduo)
  └─► HttpServer::onMessage()
        └─► HttpContext::parseRequest()       ← 状态机解析 HTTP
              └─► Router::dispatch()           ← URL 匹配 (精确/正则)
                    └─► MiddlewareChain::process()  ← 中间件前置处理
                          └─► RouterHandler::handle()  ← 业务 Handler
                                └─► MiddlewareChain::postProcess()  ← 中间件后置
                                      └─► conn->send(response)
```

### 关键文件

| 文件 | 职责 |
|------|------|
| `include/http/HttpServer.h` | 服务器入口，绑定 muduo TcpServer |
| `include/http/HttpContext.h` | HTTP 状态机解析器 |
| `include/http/HttpRequest.h` | 请求体结构 |
| `include/http/HttpResponse.h` | 响应封装 + deferred 异步模式 |
| `include/router/Router.h` | 路由注册（精确 O(1) + 正则匹配） |
| `include/router/RouterHandler.h` | 路由处理器基类 |
| `include/middleware/MiddlewareChain.h` | 中间件管道 |
| `include/middleware/cors/CorsMiddleware.h` | CORS 跨域中间件 |
| `include/session/Session.h` | 会话对象 |
| `include/session/SessionManager.h` | LRU 会话管理器 |
| `include/session/SessionStorage.h` | 存储接口 |
| `include/ssl/SslConfig.h` | SSL/TLS 配置 |
| `include/ssl/SslContext.h` | SSL 上下文 |
| `include/ssl/SslConnection.h` | SSL 连接封装 |
| `include/utils/ThreadPool.h` | 通用线程池 |
| `include/utils/FileUtil.h` | 文件读写工具 |
| `include/utils/db/DbConnectionPool.h` | MySQL 连接池 |
| `include/utils/db/DbConnection.h` | 单个连接封装 |

### 核心类关系

```
HttpServer
  ├─► Router（路由表）
  ├─► MiddlewareChain（中间件链）
  ├─► SessionManager（会话管理）
  │     └─► Session（LRU 链表节点）
  └─► EventLoop（muduo 事件循环）
```

## 对外依赖与耦合边界

### 依赖

| 依赖 | 说明 |
|------|------|
| muduo | 网络库（Reactor + TcpServer） |
| OpenSSL | SSL/TLS 支持 |
| MySQL C++ Connector | 数据库连接 |
| nlohmann/json | JSON 工具（通过 `3rdparty/JsonUtil.h`） |

### 被依赖

- **AIServerCore**：引用 `http/`、`router/`、`session/`、`utils/` 头文件
- **AIEngine**：**零依赖**（AIEngine 使用独立的 `3rdparty/JsonUtil.h`）

### 命名空间

- `http::` — HTTP 核心
- `http::router` — 路由
- `http::middleware` — 中间件
- `http::session` — 会话
- `http::ssl` — SSL/TLS
