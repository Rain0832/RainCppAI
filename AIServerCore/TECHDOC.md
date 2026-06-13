# AIServerCore — 模块技术文档

## 模块职责

AIServerCore 是项目的**业务编排层**，位于 HttpServer（网络层）和 AIEngine（AI 工具层）之间。负责注册 HTTP 路由、组装 Handler、管理 ChatServer 生命周期。

## 核心文件流转逻辑

```
main.cpp
  └─► ChatServer::start()
        ├─► HttpServer 初始化 + 路由注册（15 个 Handler）
        ├─► muduo EventLoop 启动
        └─► 等待 shutdown 信号

请求到达
  └─► Router::dispatch()
        └─► Handler::handle()
              ├─► ChatSendHandler          ← 普通 AI 对话
              ├─► ChatCreateAndSendHandler ← 新建会话 + 对话
              ├─► ChatSseHandler           ← SSE 流式
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
| `include/controller/ChatSendHandler.h` | AI 对话（提交到线程池） |
| `include/controller/ChatSseHandler.h` | SSE 流式输出 |
| `include/controller/ChatHistoryHandler.h` | 会话历史（内存 + MySQL fallback） |
| `include/controller/ChatSessionsHandler.h` | 用户会话列表 |
| `include/controller/ChatSpeechHandler.h` | TTS 语音合成代理 |
| `include/controller/McpHandler.h` | MCP JSON-RPC 入口 |
| `include/controller/ChatLoginHandler.h` | 用户登录 |
| `include/controller/ChatRegisterHandler.h` | 用户注册 |
| `include/controller/AIUploadSendHandler.h` | 图像识别代理 |

### Handler 依赖关系

```
ChatServer
  ├─► chatInformation (map<session_id, AIHelper>) — 会话池
  ├─► onlineUsers_ (map<session_cookie, user_id>)  — 登录态
  └─► 路由表 → 15 Handler
        └─► 每个 Handler 引用 AIEngine 组件（AIHelper / ImageRecognizer / AISpeechProcessor）
```

## 对外依赖与耦合边界

### 依赖

| 依赖 | 说明 |
|------|------|
| HttpServer | `http/`、`router/`、`session/`、`utils/`（网络 + DB + 路由） |
| AIEngine | `llm/`、`mcp/`、`audio/`、`vision/`、`common/`（AI 能力） |
| 3rdparty | `JsonUtil.h`（JSON 工具） |
| muduo | `EventLoop`、`InetAddress`（间接依赖） |

### 被依赖

- **无**（AIServerCore 是顶层模块，不被其他模块引用）

### 命名空间

- `chat::` — 顶层命名空间
