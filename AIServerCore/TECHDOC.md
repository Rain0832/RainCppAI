# AIServerCore — 模块技术文档

## 模块职责

AIServerCore 是项目的**业务编排层**，位于 HttpServer（网络层）和 AIEngine（AI 工具层）之间。负责注册 HTTP 路由、组装 Handler、管理 ChatServer 生命周期。当前注册 30+ 条路由、25+ 个 Handler。

## 核心文件流转逻辑

```
main.cpp
  └─► ChatServer::start()
        ├─► HttpServer 初始化 + 路由注册（14 个 Handler）
        ├─► muduo EventLoop 启动
        └─► 等待 shutdown 信号

请求到达
  └─► Middleware 链: RequestId → Auth → AdminAuth → RateLimit → SecurityHeaders → Cors
        └─► Router::dispatch()
              └─► Handler::handle()
                    ├─► ChatSseHandler           ← SSE 流式（唯一对话入口 + Vision-Agent ONNX 推理）
                    ├─► StaticFileHandler        ← 静态资源（CSS / JS / 图片 / 字体）
              ├─► ChatHistoryHandler       ← 历史消息
              ├─► ChatSessionsHandler      ← 会话列表
              ├─► ChatSpeechHandler        ← TTS
              ├─► ChatRegisterHandler      ← 注册
              ├─► ChatLoginHandler         ← 登录
              ├─► ChatLogoutHandler        ← 登出
              ├─► ChatEntryHandler         ← 入口页
              ├─► AIMenuHandler            ← 菜单页
              ├─► ChatHandler              ← 聊天页
              ├─► AIUploadHandler          ← 上传页
              ├─► AIUploadSendHandler      ← 图片识别
              └─► McpHandler               ← MCP JSON-RPC
```

### 关键文件

| 文件 | 职责 |
|------|------|
| `src/main.cpp` | 入口：解析命令行参数 → ChatServer::start() |
| `include/server/ChatServer.h` | 服务启动器：路由注册、muduo 配置 |
| `include/controller/ChatSseHandler.h` | SSE 流式（唯一对话入口，Vision-Agent ONNX 推理 + 图片落盘） |
| `include/controller/ChatHistoryHandler.h` | 会话历史（内存 + MySQL fallback，含 payload.image） |
| `include/controller/ChatSessionsHandler.h` | 用户会话列表 |
| `include/controller/ChatDeleteSessionHandler.h` | 会话软删除 |
| `include/controller/ChatUpdateTitleHandler.h` | 会话标题更新 |
| `include/controller/ChatSpeechHandler.h` | TTS 语音合成代理 |
| `include/controller/ChatLoginHandler.h` | 用户登录 + JWT 签发 |
| `include/controller/ChatRegisterHandler.h` | 邀请码验证 + 邮箱验证 + 注册 |
| `include/controller/ChatLogoutHandler.h` | 用户登出 |
| `include/controller/ChatEntryHandler.h` | 登录/注册页入口 |
| `include/controller/ChatInviteVerifyHandler.h` | 邀请码可用性校验 |
| `include/controller/ChatVerifySendHandler.h` | 邮箱验证码发送 |
| `include/controller/ChatVerifyCheckHandler.h` | 邮箱验证码核验 |
| `include/controller/ChatFeedbackHandler.h` | 用户意见反馈 |
| `include/controller/AIUploadSendHandler.h` | 独立图像识别页（旧） |
| `include/controller/ApiKeyHandler.h` | API Key 增删查 |
| `include/controller/ModelListHandler.h` | 模型注册表（models.json） |
| `include/controller/McpHandler.h` | MCP JSON-RPC 入口 |
| `include/controller/HealthHandler.h` | 健康检查（GET /health） |
| `include/controller/MetricsHandler.h` | Prometheus 指标（GET /metrics） |
| `include/controller/AdminDashboardHandler.h` | Admin 看板 SSR |
| `include/controller/AdminSseHandler.h` | Admin 实时数据 SSE |
| `include/controller/AdminUsersHandler.h` | 用户列表 JSON |
| `include/controller/AdminToggleUserHandler.h` | 用户启用/禁用 |
| `include/controller/AdminFeedbackHandler.h` | 反馈列表 JSON |
| `include/controller/AdminLogsHandler.h` | 日志查看器 |
| `include/controller/AdminInviteCodesHandler.h` | 邀请码管理（列表/创建/启禁） |
| `include/controller/ChangePasswordHandler.h` | 修改密码（POST /api/user/password） |
| `include/controller/TaskStatusHandler.h` | 异步任务状态轮询（GET /task/{id}/status） |
| `include/server/SessionStore.h` | 会话池 + LRU 驱逐封装 |
| `include/Repository/` | 数据访问层（Account/Session/Message/ApiKey/Admin/InviteCode/VerificationCode） |
| `include/Service/` | 业务逻辑层（Auth/Session/ApiKey/Chat） |
| `include/server/ChatServer.h` | 服务启动器：路由注册、DB 初始化、Root 播种、ONNX 检查、Redis/MQ 初始化 |

## v3.0.0 变更摘要
- Vision-Agent: ChatSseHandler 支持 image_base64 → ONNX 推理 → Prompt 注入
- Root 播种: 首次启动自动创建 root/admin + 随机密码
- Admin 邀请码: 列表/创建/启禁，accounts.invite_code_id 全链路追溯
- 修改密码: ChangePasswordHandler（POST /api/user/password）
- 中间件链: RequestId → Auth → AdminAuth → RateLimit → SecurityHeaders
- ChatServer::checkOnnxModel() 启动自检
- 全量 Handler 清单已从 15 更新至 30+

## v3.2.0 变更摘要

### v3.2.0 关键实现点映射
- `AIServerCore/src/server/ChatServer.cpp`: 启动时初始化 RedisClient、SessionCache 与 TaskProducer
- `Infralib/Cache/RedisClient.{h,cpp}`: hiredis 封装，提供 GET/SETEX/LPUSH/LRANGE/HGET/HSET 等基础 Redis 操作
- `Infralib/Cache/SessionCache.{h,cpp}`: 会话三级缓存门面，Memory → Redis → MySQL；包括会话列表、会话元数据、chat context 缓存
- `Infralib/Mq/TaskProducer.{h,cpp}`: RabbitMQ 生产者；`publish()` 投递 vision_tasks/tts_tasks，不阻塞 HTTP Reactor
- `AIServerCore/src/controller/ChatSessionsHandler.cpp`: 使用 SessionCache 读取 Redis 会话列表，未命中时回退 MySQL，并在返回后回写 Redis
- `AIServerCore/src/controller/ChatSseHandler.cpp`: image_base64 请求直接投递到 RabbitMQ；同时对新会话进行 Redis 会话列表同步
- `AIEngine/src/llm/AIHelper.cpp`: chatStream 启动时恢复 Redis chat context；助手回复完成后保存最新上下文到 Redis
- `AIServerCore/src/controller/ChatSpeechHandler.cpp`: 统一从 config.json 的 `api_keys.baidu.*` 读取百度 TTS Key，避免 getenv()
- `AIServerCore/src/main.cpp`: 统一加载 config.json，初始化主流程；DB 密码等敏感项仍建议环境变量注入，保持 config.json 与运行时密钥分离

- Redis 三级缓存: ChatSessionsHandler 接入 SessionCache（内存→Redis→MySQL），hit 率>90% 时零 DB 查询
- RabbitMQ 异步削峰: ChatSseHandler 图片请求分流至 TaskProducer，HTTP 202 + taskId 立即返回，不阻塞 Reactor
- 新增 TaskStatusHandler: GET /task/{taskId}/status → Redis GET → 状态 (processing/completed/failed)
- ChatServer 新增 initializeRedis() / initializeMQ()，启动时连接 Infralib 中间件
- HttpResponse 新增 k202Accepted

## 对外依赖与耦合边界

### 依赖

| 依赖 | 说明 |
|------|------|
| HttpServer | `http/`、`router/`、`session/`、`utils/`（网络 + 路由） |
| Storage | `storage/`（数据库持久化） |
| AIEngine | `llm/`、`mcp/`、`audio/`、`vision/`、`common/`（AI 能力） |
| 3rdparty | `JsonUtil.h`（JSON 工具） |
| muduo | `EventLoop`、`InetAddress`（间接依赖） |

### 被依赖

- **无**（AIServerCore 是顶层模块，不被其他模块引用）

### 命名空间

- `chat::` — 顶层命名空间
