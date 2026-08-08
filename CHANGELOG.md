# Changelog

本文档遵循**三级递进**格式：`# 大版本` → `### 中版本` → `##### 小版本`。
版本按时间正序排列，新版本追加在末尾。

**格式规则**：
- `# vX.0.0` — 大版本标题（破坏性变更、架构重构）
- `### vX.Y.0` — 中版本标题（功能新增、模块重构）
- `##### vX.Y.Z` — 小版本标题（Bug 修复、细节优化）
- 每个小版本条目以 `-` 开头，一行一条变更

---

# v1

> 初始版本

### v1.0

##### v1.0.0 — MVP
- 自研 HTTP 框架（muduo Reactor）
- 多模型 LLM（Qwen / Doubao / RAG / MCP）
- 图像识别 + 语音合成
- Session 管理 + MySQL 持久化

---

### v1.1
> 动态 API Key

##### v1.1.0 — 前端重构 + Runtime Key
- `AIStrategy::setApiKey()` 运行时注入
- 前端重构：明暗主题、打字机效果、API Key 面板
- 敏感信息不入仓库

---

### v1.2
> 读写锁 + LRU

##### v1.2.0 — shared_mutex & LRU
- `std::mutex` → `std::shared_mutex`（读写锁）
- LRU 淘汰策略（`list` + `unordered_map`，O(1)）
- 最大 500 会话内存上限

---

### v1.3
> 异步线程池

##### v1.3.0 — ThreadPool
- 通用 `ThreadPool`（std::thread + queue + condition_variable）
- `HttpResponse` deferred 异步模式
- AI 调用提交到 8 线程池，IO 线程零阻塞

---

### v1.4
> Phase 1 — 并发 Bug 修复（6 项）

##### v1.4.0 — 并发修复
- `msgMutex_` 保护 `messages_`（原无锁）
- `Message` 结构体 + 显式 `role` 字段（替代奇偶判断）
- `atomic<bool> processing_` 串行化并发请求
- 锁分离：chatInfo 锁 ↔ AIHelper 锁
- RabbitMQ `exclusive=false` 修复独占队列 bug
- DB 连接池 `wait_for(3s)` 超时防阻塞

---

### v1.5
> Phase 2 — 数据库表结构重设计

##### v1.5.0 — 三高表结构
- 新增 `sessions` 表（会话元数据、软删除、毫秒时间戳）
- 新增 `messages` 表（自增主键 + 显式 `role` ENUM，替代奇偶判断）
- 新增 `user_api_keys` 表（`(user_id, provider)` 唯一索引）
- 迁移 `chat_message` → `messages` + `sessions`，旧表保留
- readDataFromMySQL / pushMessageToMysql 适配新表

---

### v1.6
> SSE 流式输出 + 标准 MCP Server

##### v1.6.0 — SSE & MCP
- `AIHelper::chatStream()` — curl WRITEFUNCTION 逐 token 实时回调
- `ChatSseHandler` — `POST /chat/send-stream`，SSE 握手 + 流式回写
- `McpServer` — 标准 JSON-RPC 2.0，`tools/list` + `tools/call`
- `McpHandler` — `POST /mcp`，兼容 Claude Desktop / Cursor

##### v1.6.1
- chatStream 支持 MCP 模式
- 豆包 API 端点 ID 用户可配
- ChatHistoryHandler 内存 miss 时 MySQL fallback
- get_weather 超时 + 降级


---

# v2


### v2.0
> **架构重构** — 四模块拆分 + 代码规范标准化

##### v2.0.0 — 项目结构重组
- **架构**：四模块拆分 — HttpServer（网络框架）、AIServerCore（业务层）、AIEngine（AI 工具库）、web（前端资源）
- **规范**：新增 `DEVELOP_STANDARD.md`，统一命名风格（`snake_case_` 成员、`http::` / `ai::` / `chat::` 命名空间）
- **文档**：`README.md` 重写、`CHANGELOG.md` 重新设计为三级递进格式、`TODO.md` 四象限优先级
- **清理**：删除 `Internview/` 目录、各子模块 `CHANGELOG.md` 替换为 `TECHDOC.md`
- **流程**：新增 `AGENT.md` 规范 AI Agent 开发流程

---

##### v2.0.1 — 代码格式化
- `.clang-format`：新增 Clang-Format 配置文件，对齐 `DEVELOP_STANDARD.md` 规范
- 全项目 `.cpp` / `.h` 文件执行 `clang-format -i` 统一格式化

---

##### v2.0.3 — MCP 模块重构

> **MCP 工具调用重构** — 原生 Function Calling 替代文本解析

- `AIToolRegistry` 改为进程级单例，从 `mcp_config.json` 加载工具定义
- `McpServer` 接入 `AIToolRegistry` 单例，移除独立实例
- `AIStrategy` 增加 `parseToolCalls()` 方法，从 LLM 响应解析结构化 `tool_calls`
- `AIHelper::chat()` 重写为原生 Function Calling：`payload["tools"]` 传入 schema，解析 `tool_calls` 而非文本 JSON
- `AIHelper::chatStream()` 支持流式工具调用（循环模式，MAX 5 轮）
- `Message` 结构体增加 `tool_call_id` 字段，`messagesToJsonArray` 支持 `role:"tool"` 回传
- `AIConfig` 废弃 `buildPrompt`/`parseAIResponse`/`buildToolResultPrompt`
- `web/config.json` 清空旧 `prompt_template` 和 `tools` 字段
- 移除 `AIStrategy::isMCPModel` 标志和 `AliyunMcpStrategy` 类（合并入 `AliyunStrategy`）
- 新增 `mcp_config.json`：标准 OpenAI tools schema 格式的工具配置文件

---

##### v2.0.4 - MCP 流式上线

> **MCP 流式修复** — 流式 SSE 场景正式支持 Function Calling / MCP 工具调用

- `AIToolRegistry::loadFromConfig` 在 `main.cpp` 启动时加载，修复 tools schema 未传 LLM 的问题
- `StreamWriteCallback` 增量累积 `delta.tool_calls`（按 index 合并 id/name/arguments 片段）
- `executeCurlStream` 流结束后构造完整 OpenAI JSON 响应，供 `chatStream` 解析 tool_calls
- `StreamContext` 新增 `std::map<int, json> toolCalls` 累积字段
- MCP 工具调用增加 `LOG_INFO` 级别日志，调试信息 `LOG_DEBUG` 全部移除
- 日志级别默认 `INFO`（原 `DEBUG`）
- `HttpRequest.cpp` 补充 `#include <cassert>`

---

##### v2.0.5  — MCP 全量解耦 & 远端 SSE

> **MCP 架构解耦** — 配置驱动路由 + McpClientManager 远端 SSE 支持

- `mcp_config.json` 重构为 `mcpServers` 结构，废弃旧 `tools` 数组（保留旧格式兼容）
- `AIToolRegistry::loadFromConfig` 移除硬编码 `if (name == "get_weather")` 分支，改为 `builtinMap` 查表 + `transport` 分发
- `AIToolRegistry::invoke` 降级路由：本地未命中 → 转发 `McpClientManager::callTool()`
- `AIToolRegistry::getToolsSchema` 融合本地 + 远端 schema
- 新增 `McpClientManager` 单例：工厂模式按 `transport` 创建 Client
- 新增 `McpStdioClient`（内联，popen pipe 通信）
- 新增 `McpSseClient`（libcurl GET SSE + POST JSON-RPC 2.0）
- `main.cpp` 启动时初始化 McpClientManager 并注入 AIToolRegistry
- `chatStream` / `StreamContext` 零修改保护

---

##### v2.0.6 — 纯血 MCP & 热插拔

> **纯血 MCP 重构** — C++ 引擎大清洗，Python 微服务接管工具实现

- 新建 `mcp_servers/weather_server.py`：Python FastMCP 天气服务，替代 C++ getWeather
- `mcp_config.json` 纯血化：移除 builtin，全部指向外部 stdio/sse MCP Server
- `AIToolRegistry` 极简化：删除 getWeather/getTime/builtinMap/registerTool/hasTool，仅作为 McpClientManager 薄层代理
- `McpServer` 移除 registerTool（ToolFunc 已不存在）
- `McpClientManager` 新增 `reloadFromConfig`：增量对比启停 server，修改 config 无需重启 C++
- `McpClientManager::discoverAllTools` 每次自动触发 reload + 工具发现
- `McpClientManager::registerServer` 记录 server→client 映射，支持 unregisterServer 精确清理
- `main.cpp` 通过 `AIToolRegistry::loadFromConfig` → 委托 `McpClientManager::registerServer` 启动子进程

---

##### v2.0.7 — 双向管道底层重构

> **StdioClient 底层重写** — popen → pipe/fork/dup2/execvp 全双工通信

- `StdioClient` 从 `popen("r+")` 重写为原生 `pipe()` + `fork()` + `dup2()` + `execvp()`
- 两对管道实现双向 JSON-RPC（父写 stdin，父读 stdout），解决 Linux popen 不支持全双工的 POSIX 限制
- `sendRequest` 中 `fwrite/fgets` 替换为 `write()/read()` + 非阻塞（`O_NONBLOCK`）+ 缓冲区行拼接
- 子进程回收：`stop()` 中 `close(fd)` + `waitpid(WNOHANG)`
- 新增头文件：`errno.h` / `fcntl.h` / `sys/wait.h` / `unistd.h`

---

##### v2.0.8 — MCP 标准握手协议

> **FastMCP 初始化握手** — initialize → notifications/initialized

- `registerServer` 在 `fork` 成功后插入 MCP 标准握手：`initialize` 请求 → 等待响应 → `notifications/initialized` 通知
- 握手轮询 read 缓冲区行拼接 + JSON 解析，5 秒超时（500×10ms）
- 握手失败路径：`LOG_ERROR` + `close(fd)` + `kill(SIGTERM)` + `waitpid` 回收子进程
- 消除 2 处 `write()` 返回值未检查警告

---

### v2.1
> **前端解耦 & 流式统一**

##### v2.1.0 — 前端工程化解耦与非流式 API 彻底净化
- `web/AI.html` 大单体拆分：882 行 → 56 行 DOM 骨架 + `css/style.css` (CSS 变量+暗色主题) + `js/api.js` (网络层) + `js/ui.js` (UI 渲染层)
- 新建 `StaticFileHandler`：基于 Router 正则匹配的通用静态文件服务，支持 MIME 映射（text/css, application/javascript, application/json, image/*, font/*）
- ChatSseHandler 增强：不传 sessionId 时后端自动创建并通过 SSE 回传 `{"sessionId":"..."}`
- **【破坏性】删除** `AIHelper::chat()` 非流式方法，删除 `ChatSendHandler` 和 `ChatCreateAndSendHandler`（移除路由 `/chat/send` 和 `/chat/send-new-session`）
- `/chat/send-stream` 成为唯一 AI 对话入口，前端 regerate() 和新会话创建统一走 SSE
- 路径安全校验：拒绝 `..` 目录穿越
- 更新 README API 表、各模块 TECHDOC 文档同步至当前态

##### v2.1.1 — 全栈沉浸式交互重构 & 全页面 CSS/JS 解耦
- `web/entry.html` / `menu.html` / `upload.html` / `NotFound.html` 内联 CSS/JS 暴力拆分为独立文件
- 登录成功路由短路：`/menu` → 直接进入 `/chat`（沉浸式"登录即聊天"体验）
- AI.html ChatGPT 式布局重构：Avatar 右上角下拉菜单（API Key 设置 + 退出登录）、模型选择移至输入框上方
- 删除「同步历史」按钮，页面加载自动获取会话列表并激活最近会话
- 前端模型列表去重：移除"百炼 MCP"冗余选项
- 后端新增 `ApiKeyHandler`：`POST /api/user/apikey`，API Key 持久化到 MySQL `user_api_keys` 表
- `.gitignore` 加固：追加 `*.sqlite`、`.env`

---

### v2.2
> **深水区架构重构** — 存储层剥离 · 防注入 · 同步写 · 雪花算法 · 异步标题 · 软删除

##### v2.2.0 — 底层存储架构彻底重构
- **【Storage 模块】新建 `Storage/` 顶层模块**（`storage::` 命名空间），DB 连接池从 HttpServer 剥离
- **【Schema 重写】** `sessions` 删除 `model_type`、新增 `is_deleted TINYINT(1)`；`messages` 删除 `user_id`、新增 `payload JSON`
- **【防注入】** 全局废弃 SQL 字符串拼接 + `escapeString()`，改用 Prepared Statement + `?` 占位符 + 多类型 `bindParams`
- **【同步写】** `pushMessageToMysql` 废弃 RabbitMQ 异步 → 当前线程同步 `executeUpdate`
- **【移除 RabbitMQ】** 删除 `main.cpp` 消费者启动代码、CMakeLists SimpleAmqpClient/rabbitmq 链接
- **【雪花算法】** `AISessionIdGenerator` 重写为标准 Snowflake（41-bit 时间戳 + 10-bit 机器 ID + 12-bit 序列号），趋势递增
- **【异步标题】** `AIHelper::startTitleSummarization()` → LLM 10 字内总结 → `UPDATE sessions SET title`
- **【软删除】** 新增 `POST /chat/delete-session` → `UPDATE sessions SET is_deleted = 1`
- **【API Key 掩码】** `GET /api/user/apikey` 返回掩码格式 `sk-****1234`
- **【文档同步】** 全量更新 TECHDOC、README、CHANGELOG、TODO

##### v2.2.1 — Bug 修复与通道分离
- **【DDL/DML 通道分离】** `DbConnection` 新增 `executeRawSql()` 文本协议专用于建表，解决 Prepared Statement 执行 DDL 导致的 Malformed packet 崩溃
- **【Schema 冗余清理】** `sessions` 表删除 `deleted_at` 字段（已被 `is_deleted` 替代）
- **【model 字段全链路】** `Message` 结构体增 `model` 字段，`pushMessageToMysql`/`restoreMessage`/`ChatHistoryHandler` SQL 及 JSON 响应全链路读写
- **【标题生成修复】** `ChatSseHandler` 补传 `isNewSession` 参数，异步 LLM 标题生成链路打通
- **【SQL 注入扫尾】** `ChatRegisterHandler::insertUser`/`isUserExist` 改用 Prepared Statement
- **【Include 修正】** `SessionManager.cpp` include 路径修正为 `session/SessionManager.h`

##### v2.2.2 — DouBao 策略重构
- **【模型映射字典】** `DouBaoStrategy` 新增 `DOUBAO_ENDPOINT_MAP` 静态映射表，前端模型名 → 火山引擎 Endpoint ID（`ep-xxx`）
- **【buildRequest 扩展】** 基类 `AIStrategy::buildRequest()` 新增 `modelName` 参数（默认空字符串），保留向后兼容
- **【清理冗余】** `DouBaoStrategy` 删除 `endpoint_id_` 成员和 `setEndpointId()` 方法；`ChatSseHandler` 移除前端 `endpointId` 参数解析链路

##### v2.2.3 — 废除豆包映射字典，回归标准 OpenAI 协议
- **【废字典】** 删除 `DOUBAO_ENDPOINT_MAP` 静态映射表，不再做模型名→端点的中间转换
- **【模型透传】** `DouBaoStrategy::buildRequest()` 改为极简逻辑：直接将前端传入的 `modelName` 透传至 JSON payload（空时兜底 `doubao-lite-4k`），完全对齐字节跳动预置推理服务标准
- **【代码清理】** 移除不再需要的 `#include <stdexcept>` 和 `#include <unordered_map>`

##### v2.2.4 — MCP 工具格式适配 & Curl 错误日志拦截
- **【MCP→OpenAI 格式转换】** `chatStream` 增加工具 schema 转译层：`inputSchema` → `parameters`，外层包装 `type: "function"` + `function: {...}`，解决 MCP 原生格式直接传给千问/豆包导致的 HTTP 400
- **【Curl 静默失败修复】** `StreamWriteCallback` 增加非 SSE JSON 拦截：当 LLM API 返回 `{"error": ...}` 时打印 `LOG_ERROR << "[API Raw Error]"`

##### v2.2.5 — 历史记录 API Schema 稳定性
- **【无条件输出 model】** `ChatHistoryHandler` 移除 `if (!msg.model.empty())` 条件判断，改为无条件赋值 `"model": ""`，确保前端始终能安全读取 `model` 字段，避免 `undefined` 引发的渲染缺失

##### v2.2.6 — 前端体验打磨与异步标题解耦
- **【model 标签修复】** `sendWithSSE` 在内存消息保存时补传 `modelName`
- **【新会话入列】** `tempSession` 收到 SSE `sessionId` 后兜底插入 `sessions[id]`
- **【异步标题解硬编码】** `startTitleSummarization` 新增 `modelType` + `modelName` 参数，移除 `"1"` / `"qwen-turbo"` 硬编码
- **【极简首页】** 删除 `AI.html` 静态 `#welcomeHint` 占位节点

##### v2.2.7 — 模型注册表与体验闭环
- **【模型注册表】** 新增 `GET /api/chat/models`，`ModelListHandler` 返回厂商-模型双层 JSON 配置（阿里云百炼 / 字节火山引擎）
- **【前端动态渲染】** `<select id="modelType">` 改为 `<optgroup>` 分组动态生成，`modelId` 字符串（如 `"qwen-plus"`）替代数字下标 `"1"/"2"/"3"`
- **【标题异步刷新】** 新会话首轮结束后 `setTimeout(fetchSessions, 1500ms)` 拉取后端 LLM 生成的异步标题
- **【Commit 合规】** 历史 commit `324872a` 修正为 `【web】` 前缀（`CONTRIBUTING.md` 规范）
- **【架构蓝图】** `TODO.md` 追加 P3 级 RBAC 权限系统、Admin 动态看板、角色扩展规划

##### v2.2.8 — 基于 Provider 的无状态策略路由
- **【策略工厂重构】** `StrategyFactory` 注册键从数字 `"1"/"2"/"3"/"4"` 改为字符串 `"aliyun"/"volcengine"/"aliyun-rag"/"aliyun-mcp"`，`create()` 增加 provider 日志埋点与 fallback 兜底
- **【ChatSseHandler 无状态路由】** 从 JSON body 直接解析 `provider` + `modelType` 字符串，删除 `providerMap[]` 数组 + `std::stoi` 硬编码映射，provider 直传 `StrategyFactory::create()`
- **【AIHelper 参数拆分】** `chatStream` 签名拆分为 `provider` + `modelId` 两个独立参数，provider 用于策略选择，modelId 透传至 `buildRequest` 的 `modelName`
- **【前端 Payload 透传】** `<option>` 增加 `data-provider` 属性，form submit 时组装 `{provider, modelType, ...}` 发往后端；`regenarate()` 同步适配
- **【前端 getApiKey 迁移】** 从数字 key (`'1'→'rain-key-dashscope'`) 改为 provider 字符串匹配 (`'aliyun'→'rain-key-dashscope'`)
- **【DB API Key 查询修复】** `ChatSseHandler` 中 SQL provider 参数从 `provider` 修正为 `dbProvider`（`"doubao"/"dashscope"`）
- **【模型注册表文件外部化】** 新建 `models.json`，`ModelListHandler` 引入 `stat` mtime 缓存机制实现零停机热加载

##### v2.2.9 — 魔术数字扫尾与 payload model 注入修复
- **【构造函数硬编码】** `AIHelper` 构造函数 `create("1")` → `create("aliyun")`，消除启动阶段 Unknown strategy 异常
- **【异步标题参数重命名】** `startTitleSummarization` 参数 `modelType`/`modelName` → `provider`/`modelId`，lambda 捕获同步更正
- **【AliyunStrategy model 注入】** `buildRequest` 中 `payload["model"]` 从硬编码 `getModel()` 改为 `modelName.empty() ? getModel() : modelName`，前端透传的 `qwen-max` 等精确模型名不再被忽略

##### v2.2.10 — 模型配置热加载路径修复 & 侧边栏会话删除
- **【models.json 热加载路径修复】** `ModelListHandler` 构造函数注入 `projectRoot_`，`stat` 从 `ChatServer::resource_root_` 拼接绝对路径，解决 CWD 不匹配导致永远加载内置硬编码兜底 JSON 的问题
- **【侧边栏会话删除】** `renderSessions()` 新增 🗑 按钮 + `stopPropagation`，`handleDeleteSession()` 调 `POST /chat/delete-session` → 软删除 `sessions.is_deleted=1`，删除后自动从 sidebar 移除

 ##### v2.2.11 — LICENSE 更换 + 文档勘误
- **【Docs】LICENSE 更换为 Apache License 2.0**（原 GPL v3；README badge 已标注 Apache 2.0 但文件内容不符，现统一）
- **【Docs】CONTRIBUTING.md 移除 SimpleAmqpClient 依赖行**（v2.2.0 已移除 RabbitMQ 异步写，依赖表长期未同步）

##### v2.2.12 — 目录命名规范 + 文档归档 + skill 目录
- **【Docs】DEVELOP_STANDARD.md 新增 §1.7 目录命名规范**（新增目录 PascalCase，存量 lowercase 逐步对齐）
- **【Docs】归档过时文档**：`docs/Session_Architecture_Guide.md`、`retrospective-v2.0.3-MCP重构.md`、`retrospective-v2.0.8.md` → `Docs/Archive/`；删除旧 `docs/` 目录
- **【Infra】新建 `skill/` 目录**，用于项目级可复用技能沉淀

##### v2.2.13 — 数据库全量重设计（8 表 + FK）
- **【Storage】`ChatServer::initDatabase()` 全量重写**：`users` → `accounts`（含 role ENUM + email + password_hash）+ `user_api_keys` → `api_keys` + 新增 `invite_codes` / `verification_codes` / `feedback` / `call_logs` 四表
- **【Storage】全部表满足 1NF/2NF/3NF**，InnoDB + utf8mb4 + FOREIGN KEY 约束
- **【Storage】`messages.role` ENUM 扩展 `tool`**，新增 `tool_call_id` 字段，支撑 Agent Loop 上下文持久化

##### v2.2.14 — ThreadPool 提取 + AIEngine 解耦
- **【Common】新建 `Common/Threading/ThreadPool.h`**（命名空间 `common::`），HttpServer 旧文件改为兼容别名
- **【AIEngine】`AIHelper.h/.cpp` 头文件引用脱钩**：`HttpServer/include/utils/ThreadPool.h` → `Common/Threading/ThreadPool.h`，终结 AIEngine → HttpServer 非法依赖
- **【AIServerCore】`ChatServer::aiThreadPool_` 类型更新**：`http::ThreadPool` → `common::ThreadPool`

##### v2.2.15 — 统一配置层 ConfigManager + config.json
- **【Common】新建 `Common/Config/ConfigManager`**：单例，JSON 加载 + 环境变量覆盖（`DB_HOST` → `db.host`）
- **【AIServerCore】`ChatServer::initialize()` DB 初始化配置化**：写死的凭据改为从 ConfigManager 读取
- **【AIServerCore】`main.cpp` 端口 / 线程数 / MCP 路径配置化**：`config.json` + 命令行 `-p` 覆盖
- **【Infra】新增 `config.json`**：统一管理 server/db/paths/mcp/ai 全部可配项

##### v2.2.16 — 死代码清理 + MQManager 迁移
- **【HttpServer】删除 `utils/db/` 旧 DB 文件**（DbConnection/DbConnectionPool/DbException，v2.2.0 已迁至 Storage 模块，CMake 早已排除）
- **【Infralib】`MQManager.h/.cpp` 迁至 `Infralib/Mq/`**：RabbitMQ 异步写已废弃，保留代码备查，新增 README 说明退役原因

##### v2.2.17 — 统一错误响应 ApiResult
- **【Common】新建 `Common/Http/ApiResult.h`**：统一 `{success, data, error{code, message}}` 信封，提供 `ok()` / `fail()` 工厂
- **【AIServerCore】12 个 Handler 错误响应全部迁移**：`json e; e["status"]="error"` → `ApiResult::fail(code, msg).toJson()`

##### v2.2.18 — Repository 层（收拢全部 SQL）
- **【AIServerCore】新建 `Repository/` 目录 + 4 个 Repository**：`AccountRepository` / `SessionRepository` / `MessageRepository` / `ApiKeyRepository`
- **【AIServerCore】全部 SQL 从 Handler 迁入 Repository**：统一使用 Prepared Statement，清除控制器层裸 SQL

##### v2.2.19 — Service 层（抽业务逻辑）
- **【AIServerCore】新建 `Service/` 目录 + 4 个 Service**：`AuthService` / `SessionService` / `ApiKeyService` / `ChatService`
- **【AIServerCore】Service 层封装 Repository 调用与业务逻辑**：Handler 不再直接操作 Repository，通过 Service 间接调用

##### v2.2.20 — Handler 瘦身 + ChatServer 去 friend
- **【AIServerCore】新建 `Server/SessionStore`**：封装 chatInformation + LRU + sessionsIdsMap，线程安全
- **【AIServerCore】ChatServer 移除 15 个 `friend class` 声明**，新增公开 getter（`getMysqlUtil` / `getAiThreadPool` / `getOnlineUsers` / `getImageRecognizers`）
 Handler 通过 getter 访问 ChatServer，不再直接穿透私有成员

##### v2.2.21 — StaticFileHandler 下沉至 HttpServer
- **【HttpServer】`StaticFileHandler` 从 AIServerCore 迁入 HttpServer**（命名空间 `http::`）
- **【AIServerCore】路由注册改用 `http::StaticFileHandler`，不依赖 `ChatServer*`

##### v2.2.22 — CMake 模块化 + libaiengine.a + GoogleTest
- **【CMake】全量重构**：`aiengine` / `httpserver` / `storage` 独立静态库，`http_server` 仅链接
- **【Tests】引入 GoogleTest（FetchContent）**：`Tests/test_router` / `test_config` / `test_db_pool` 冒烟测试
- **【Infra】`.gitignore` 追加测试产物忽略项**

##### v2.2.13 — 路径配置化 + TECHDOC 同步（Plan 1 收尾）
- **【Docs】全量更新 4 个模块 TECHDOC**：移除过时描述，同步 Plan 1 全部架构变更
- **【Docs】CONTRIBUTING.md 登记 GoogleTest 新依赖**
- **【Docs】`mcp_config.json` 添加 MCP_PYTHON 环境变量注释**
- **【Plan 1 总结】架构重构完成**：ChatServer 去 friend → Repository/Service 分层 → ThreadPool/ApiResult/ConfigManager 公用化 → 数据库 8 表全量重设计 →前端功能回归通过



## Plan 2 — 安全加固
### v2.3
##### v2.3.1 — 密码 argon2id 哈希
- **【Security】libsodium 集成**：`Common/Crypto/PasswordHash.h` `hashPassword()` / `verifyPassword()` argon2id 哈希
- **【AuthService】注册/登录迁移至 argon2id**：`registerAccount()` 哈希后存储，`login()` 验证哈希
- **【Handler】ChatLoginHandler / ChatRegisterHandler 迁移至 AuthService**，移除裸 SQL 查询
- **【CMake】target_link_libraries(aiengine PUBLIC sodium)**

##### v2.3.2 — 工具调用上下文持久化
- **【AIHelper】pushMessageToMysql / addMessage 签名 bool is_user → const string &role**
- **【AIHelper】insert messages 支持 payload / tool_call_id 写入**
- **【AIHelper】tool_calls / tool 结果补写 MySQL**：解决重启后上下文断裂
- **【ChatSseHandler】chatStream 线程安全回调更新**

##### v2.3.3 — 登录失败锁定
- **【accounts】新增 failed_attempts / locked_until 字段**：5 次失败 / 15 分钟锁定
- **【AuthService::login()】** 锁定检查 → 失败自增 → 达阈值锁定 → 成功重置
- **【ChatLoginHandler】** 区分锁定（429 "账号已锁定，请X分钟后重试"）vs 密码错误（401）
- **【HttpResponse.h】** 新增 `k429TooManyRequests`

##### v2.3.4 — CORS allowlist + 安全响应头
- **【新增】SecurityHeadersMiddleware**：CSP / HSTS / X-Frame-Options / X-Content-Type-Options / X-XSS-Protection
- **【CorsConfig.h】** allowedOrigins `{"*"}` → `{}`（强制显式 allowlist）
- **【ChatServer::initializeMiddleware()】** CORS allowlist + SecurityHeadersMiddleware 注册
- **【config.json】** 新增 `cors` / `security` 段

##### v2.3.5 — chatInformation 分片锁
- **【ChatServer.h】** `rwMutexForChatInfo` → `std::array<shared_mutex, 16>`（userId % 16）
- **【ChatSseHandler / ChatHistoryHandler】** `getChatInfoMutex()` → `getChatInfoMutex(userId)`

##### v2.3.6 — 错误脱敏 + 图片上传校验
- **【AIMenuHandler / ChatUpdateTitleHandler / AIUploadSendHandler】** `e.what()` 不暴露给客户端 → `"Internal server error"` + `LOG_ERROR`
- **【AIUploadSendHandler】** 图片 base64 大小限制 ≤ 10 MB + 最小 12 字节

##### v2.3.7 — DB 凭据环境变量化 + CVE 审计
- **【新增】SECURITY.md**：依赖清单（OpenSSL 3.0.13 / libcurl 8.5.0 / libsodium 1.0.18 / OpenCV 4.6.0）+ 环境变量文档
- **【main.cpp】** 启动时检查 `DB_PASSWORD` 环境变量，未设置则警告

##### 编译修复 + Bugfix (post-v2.3.7)
- **【AIHelper】pushMessageToMysql 重复调用修复 + payload/tool_call_id 空值INSERT 动态SQL**
- **【SecurityHeadersMiddleware】CSP script-src 白名单加 cdn.jsdelivr.net**
- **【AIHelper::executeCurlStream】** curl_easy_perform 返回值检查 + `LOG_ERROR`
- **【ChatSseHandler】SSE 诊断日志回退，恢复原始线程池模型**

- **【Plan 2 总结】安全加固完成**：argon2id → 工具调用持久化 → 登录锁定 → CORS/HSTS/CSP → 分片锁 → 错误脱敏 → 依赖审计

##### 编译修复 (post-v2.2.13)
- **【AIServerCore】ChatServer.h 单 public:/private: 结构重构**：移除死 friend forward declarations，整合为干净的访问控制
- **【AIServerCore】6 个 Handler 直接成员访问替换为公共获取器调用**（onlineUsers_ / ImageRecognizerMap / chatInformation 等 → getter 代理）
- **【Docs】DEVELOP_STANDARD.md 新增 clang-format 格式化规则**


## Plan 3 — User System Upgrade
> Plan 3 has 6 SPs (v2.4.1 ~ v2.4.6), final v2.5.0
### v2.4

##### v2.4.1 - SP 3.1 InviteCodeRepository + invite code verify API
- [AIServerCore] InviteCodeRepository: findByCode / incrementUsedCount
- [AIServerCore] ChatInviteVerifyHandler: POST /api/invite/verify
- [AIServerCore] ChatServer: register route
- Add [INVITE] TAG to log messages

##### v2.4.2 - SP 3.2 QQ SMTP mail + VerificationCodeRepository + verify API
- [Common] MailSender: libcurl SMTPS based QQ mail sender
- [AIServerCore] VerificationCodeRepository: create/findByEmailAndCode/markUsed/countRecent
- [AIServerCore] ChatVerifySendHandler: POST /api/verify/send
- [AIServerCore] ChatVerifyCheckHandler: POST /api/verify/check
- [AIServerCore] ChatServer: register routes
- [Infra] CMakeLists: add MailSender.cpp
- [Infra] config.json: add mail section

##### v2.4.3 - SP 3.3 JWT Service + AuthMiddleware + Login JWT
- [Common] JwtService: HS256 JWT sign/verify via OpenSSL, no new dependency
- [HttpServer] AuthMiddleware: intercept /api/*, validate JWT from httpOnly cookie
- [HttpServer] HttpRequest: add addHeader() for middleware context passing
- [AIServerCore] ChatLoginHandler: sign JWT + set httpOnly cookie on login
- [Infra] config.json.example: add jwt.secret config
- [Infra] CMakeLists.txt: add Common/Auth/JwtService.cpp

##### v2.4.4 - SP 3.4 Register flow integration
- [AIServerCore] AuthService: add registerWithInviteCode() with email validation
- [AIServerCore] AccountRepository: add findByEmail()
- [AIServerCore] ChatRegisterHandler: rewrite with 4-step verify-then-register flow
- [AIServerCore] Logging: add [REGISTER] TAG


##### v2.4.5 - SP 3.5 Feedback API
- [AIServerCore] ChatFeedbackHandler: POST /api/feedback
- Protected by AuthMiddleware via X-Auth-UserId header
- Content truncated at 5000 chars
- Add [FEEDBACK] TAG to log messages
##### v2.4.6 - SP 3.6 Frontend multi-step register page
- [web] register.html: 5-step registration page with stepper UI
- [web] register.js: multi-step form with invite code/email/verify/register flow
- [AIServerCore] ChatEntryHandler: add page parameter for serving register.html
- [AIServerCore] ChatServer: add /register route
- [Docs] CHANGELOG v2.4.6



## Plan 4 — 可观测性与防护
> Plan 3 v2.4.6 最终 → Plan 4 v2.5.0 基线

### v2.5
> **可观测性** — spdlog JSON 日志 / 全链路追踪 / LLM 埋点 / 限流 / 脱敏 / Health & Metrics

##### v2.5.1 — SP 4.1 spdlog 双通道日志 + LogContext
- [Common] Logger: spdlog 双 sink（终端可读 + 文件 JSON），TAG 自动提取为 module 字段
- [Common] LogContext: thread_local 上下文（request_id + user_id），setContext/clearContext
- [Common] SPDLOG_INFO_TAG / SPDLOG_ERROR_TAG 宏，渐进替代 muduo LOG_*
- [AIServerCore] 全项目 Handler 代码 LOG_* → SPDLOG_*_TAG 全量迁移

##### v2.5.2 — SP 4.2 RequestId 中间件
- [HttpServer] RequestIdMiddleware: 入站检查/生成 X-Request-Id UUID v4，出站注入响应头
- [Common] 中间件 before()/after() 绑定/清除 LogContext，防止线程池污染

##### v2.5.3 — SP 4.3 LLM call_logs 调用埋点
- [AIServerCore] CallLogRepository: insert(session_id, account_id, model, provider, duration_ms, status, error_msg)
- [AIEngine] AIHelper::chatStream(): std::chrono::steady_clock 计时 + try/catch 兜底落库

##### v2.5.4 — SP 4.4 Token Bucket 限流
- [Common] TokenBucket: std::atomic 无锁实现，单用户 10 req/min
- [HttpServer] RateLimitMiddleware: /api/chat/* 路由拦截，触发返回 HTTP 429 + JSON 提示

##### v2.5.5 — SP 4.5 输入限制与数据脱敏
- [Common] Redactor: maskEmail / maskPhone 正则脱敏
- [AIServerCore] ChatSseHandler: content > 5000 字符返回 400
- [AIServerCore] ChatLoginHandler: 密码脱敏不入日志

##### v2.5.6 — SP 4.6 Health & Metrics
- [AIServerCore] HealthHandler: GET /health → MySQL SELECT 1 + uptime JSON
- [AIServerCore] MetricsHandler: GET /metrics → Prometheus 文本格式
- [Docs] HTTPS_DEPLOY.md: Nginx 反代 + SSL 证书指南

##### v2.5.7 — SPDLOG 迁移扫尾与修复
- [AIEngine] AIEngine 模块 SPDLOG 全量迁移
- [AIServerCore] SSE 401 JWT fallback 修复 + muduo log redirect



## Plan 5 — Admin 后台 & Dr.Rain 品牌化
### v2.6
> v2.6.0 = Plan 4 最终发布版基线（v2.5.6 → v2.6.0 回归验证），以下为 Plan 5 增量

##### v2.6.1 — SP 5.1 AdminAuthMiddleware
- [HttpServer] AdminAuthMiddleware: 拦截 /admin/* 请求，读取 X-Auth-Role 校验 admin 权限
- [AIServerCore] ChatServer::initializeMiddleware() 注册 AdminAuthMiddleware（在 AuthMiddleware 之后）

##### v2.6.2 — SP 5.2 Admin 实时看板
- [AIServerCore] AdminRepository: getDashboardStats() call_logs 聚合查询（今日调用量/provider分布/状态分布/活跃用户/Top5模型）
- [AIServerCore] AdminDashboardHandler: GET /admin/dashboard SSR 返回 dashboard.html
- [AIServerCore] AdminSseHandler: GET /admin/sse SSE 每 10 秒推送实时统计 JSON
- [web] dashboard.html: 侧边栏 + EventSource 实时看板（卡片/柱状图/模型排行）

##### v2.6.3 — SP 5.3 用户管理 & Dr.Rain 品牌化
- [AIServerCore] AdminRepository: getUsers() 全量用户查询 + toggleUserDisable() 启禁用切换
- [AIServerCore] AdminUsersHandler: GET /admin/api/users JSON 用户列表
- [AIServerCore] AdminToggleUserHandler: POST /admin/api/users/toggle 禁用/启用接口
- [web] dashboard.html: Tab 切换架构，用户管理面板（表格/搜索/角色徽章/禁用按钮 + Toast 反馈）
- [web] Dr.Rain 品牌化: 渐变色 Logo、蓝紫渐变 Sidebar、版本号 v2.2.0、中文字体栈

##### v2.6.4 — SP 5.4 Admin 日志查看器
- [AIServerCore] AdminLogsHandler: GET /admin/logs SSR 返回日志查看页面
- [AIServerCore] 日志读取引擎: 逆序分块读取文件最后 200 行，按日志级别着色（INFO=绿/WARN=橙/ERROR=红/DEBUG=灰）
- [web] logs.html: Dr.Rain 风格日志查看器模板，含级别图例 + 30s 自动刷新 + 倒计时
- [web] dashboard.html: 侧边栏新增「日志查看」导航入口

##### v2.6.5 — SP 5.5 Dr.Rain 品牌化 System Prompt 注入
- [AIEngine] AIHelper::chatStream(): Dr.Rain 医疗人设 System Prompt 注入（消息头部插入/替换）
- [web] 全站品牌化: title 改为 Dr.Rain — Healthcare AI，Rain AI → Dr.Rain，智能助手平台 → 智能医疗助手平台
- [web] favicon: 内联 SVG 医疗风格图标（蓝底十字 + 粉色心形）
- [web] AI.html / entry.html / menu.html / register.html / upload.html / NotFound.html: 品牌文案替换
- [web] admin/dashboard.html / admin/logs.html: 页脚 RainCppAI → Dr.Rain，版本号 v2.5

##### v2.6.6 — SP 5.6 医疗免责声明
- [web] AI.html: 聊天区与输入框之间新增医疗免责固定栏（⚠️ 免责声明文案）
- [web] style.css: disclaimer-bar 样式（亮色 amber 警告色 / 暗色 dark amber），含 .dark 适配

##### v2.6.7 — SP 5.7 前端导航与 Admin 入口
- [AIServerCore] ChatLoginHandler: 登录响应新增 role 字段，前端 sessionStorage 持久化角色
- [web] entry.js: 登录成功后存储 role 至 sessionStorage
- [web] menu.html / menu.js: admin 角色用户可见「🛡️ 管理后台」入口卡片
- [web] menu.css: 新增 .card-icon.admin 样式（翠绿色背景）
- [web] admin/dashboard.html / admin/logs.html: 页脚版本号 v2.5 → v2.6

##### v2.6.8 — 邀请码 is_admin + 前端 API Key 移除
- [AIServerCore] invite_codes 表新增 is_admin TINYINT(1) 字段，邀请码可控制注册角色
- [AIServerCore] InviteCodeRepository: findByCode 返回 is_admin
- [AIServerCore] AccountRepository::create / AuthService::registerWithInviteCode: 新增 role 参数
- [AIServerCore] ChatRegisterHandler: 根据邀请码 is_admin 注册对应角色，JWT 签名修复为动态 role
- [web] register.js: 注册成功存储 role 至 sessionStorage
- [web] AI.html / menu.html / ui.js / menu.js: 移除前端 API Key 设置面板，模型服务统一由服务端管理



## Plan 6 — 工程治理与 Debug
### v2.7
> v2.7.0 = Plan 5 最终发布版基线，以下为 Plan 6 增量

##### v2.7.1 — SP 6.1 冗余代码清理
- [Docs] AIEngine/TECHDOC.md: 移除已废弃的 SimpleAmqpClient 依赖行（v2.2.0 已移除 RabbitMQ）

##### v2.7.2 — SP 6.2 修复 Admin 日志看板
- [AIServerCore] AdminLogsHandler: 修正日志文件路径 `../logs/app-` → `logs/app_`（匹配 spdlog daily_file_sink 命名规则）
- [AIServerCore] ChatServer::initialize(): 新增 `std::filesystem::create_directories()` 确保 logs/ 目录存在
- [Infra] config.json / config.json.example: 新增 `log` 配置节（level + path）
- [Infra] config.json: 补充缺失的 `jwt` 配置节

##### v2.7.3 — SP 6.3 部署资产与示例配置
- [Infra] deploy/nginx.conf: Nginx 反向代理配置（SSE proxy_buffering off + gzip + 安全头 + SSL 模板）
- [Infra] deploy/raincppai.service: Systemd 服务脚本（自动重启 + 环境变量 + 安全加固）
- [Infra] config.json.example: 补充 `log` 配置节，结构对齐 config.json

##### v2.7.4 — SP 6.4 CI 工作流
- [Infra] .github/workflows/ci.yml: GitHub Actions CMake 编译检查（push/PR 触发，依赖安装 + muduo + ONNX Runtime + 编译 + 格式检查）

##### v2.7.5 — Hotfix: double free 崩溃 + 安全加固
- [Storage] DbConnection::reconnect(): 加 `std::lock_guard<mutex>` 防止 checkConnections 线程与 getConnection 并发修改 `conn_` 导致 double free
- [Storage] DbConnection::reconnect(): close 后立即 `conn_.reset()` 避免持有无效句柄
- [Storage] DbConnectionPool::getConnection(): 故障连接不再放回池中，由 shared_ptr 析构释放
- [Security] config.json: 清空 mail.password（原含真实 QQ SMTP 授权码，已泄露至 git 历史，需轮换）

##### v2.7.6 — spdlog 控制台增强 + muduo 日志残留清理
- [Common] Logger::init(): 控制台 sink pattern 新增 `.%e`（毫秒）和 `[%s:%#]`（文件名:行号），便于排查
- [HttpServer] CorsMiddleware / Router / SslConnection / SslContext: 移除死 `#include <muduo/base/Logging.h>`（Plan 4 已全量迁移至 SPDLOG，残留 include 无实际调用）

### v2.8
> v2.8.0 = Plan 6 最终发布版，工程治理与 Debug 完结

##### v2.8.0 — Plan 6 回归验证与发版
- SP 6.1: 移除 AIEngine/TECHDOC 中废弃 SimpleAmqpClient 依赖行
- SP 6.2: 修复 Admin 日志看板（路径 + 目录创建 + 日志文件名 `.log` 后缀 + dashboard JS 路由修复）
- SP 6.3: 部署资产（deploy/nginx.conf SSE 配置 + deploy/raincppai.service Systemd 脚本）
- SP 6.4: CI 工作流（GitHub Actions CMake build check）
- Hotfix: double free 竞态修复（DbConnection::reconnect 加锁 + 故障连接不放回池）
- Hotfix: spdlog 控制台增强（毫秒 + 文件:行号尾注格式）+ muduo/Logging.h 死 include 清理
- Hotfix: config.json 安全加固（清空邮件授权码）



## Plan 7 — 开源包装与体验闭环
### v2.8
> Plan 6 v2.8.0 基线，以下为 Plan 7 增量

##### v2.8.1 — SP 7.1 系统反馈闭环
- [AIServerCore] AdminFeedbackHandler: GET /admin/api/feedback JOIN accounts 返回最近 50 条反馈 JSON
- [web] menu.html / JS: 新增「💬 意见反馈」卡片 + 模态框表单 → POST /api/feedback
- [web] dashboard.html: Admin 看板新增「📝 用户反馈」Tab（用户名 / 内容 / 时间）
- [AIServerCore] ChatServer: 注册 GET /admin/api/feedback 路由

##### v2.8.2 — SP 7.2 README 旗舰重构
- [Docs] README.md 全面重写：标题 → Dr.Rain 智能医疗 AI 助手，新增 CMake Badge
- [Docs] README.md: 功能表格化、架构图更新、部署分 Ubuntu 24.04 节
- [Docs] README.md: API 表精简 + 补齐 Admin 路由、项目结构含 deploy/Tests/Docs
- [Docs] README_EN.md: 同步中文版结构，移除 RabbitMQ/SimpleAmqpClient 过时引用



## Plan 8 — 发版前代码与体验扫除
### v2.9
> v2.9.0 = Plan 7 最终，以下为 Plan 8 增量

##### v2.9.1 — SP 8.1 修复建表 SQL + SP 8.2 图像识别入口
- [AIServerCore] ChatServer::initDatabase(): createInviteCodes 删除重复的 `failed_attempts` 字段（修复新库建表崩溃）
- [web] AI.html: topbar 新增 🖼️ 图像识别按钮 → window.open('/upload', '_blank')
- [web] css/style.css: 新增 .upload-btn 样式
- [web] js/ui.js: 绑定 uploadBtn 点击事件

##### v2.9.2 — 日志查看器现代化
- [AIServerCore] AdminLogsHandler: 支持 ?date=YYYY-MM-DD 查询参数，新增 JSON API 模式 (?format=json)
- [web] admin/logs.html → dark theme 暗色主题（GitHub Dark 风格），日期选择器 + 级别 Tab 过滤（全部/错误/警告/信息/调试 带计数徽章）

##### v2.9.3 — Chat 头像 → 个人中心
- [AIServerCore] ChatLoginHandler / ChatRegisterHandler: 登录/注册响应新增 `username` + `email` 字段
- [web] AI.html: 旧 dropdown 菜单替换为个人中心模态框（用户名 / 邮箱 / 角色）+ 反馈入口
- [web] js/ui.js: 头像点击 → openProfile()，新增 feedback 提交逻辑
- [web] css/style.css: 新增 .profile-info / .profile-actions / .btn-primary 样式
- [web] js/entry.js / register.js: 登录/注册成功后存储 username + email 到 sessionStorage

##### v2.9.4 — 统一 Admin UI
- [AIServerCore] AdminLogsHandler: 拆分 readLastLines() (返回 vector) / readLastLinesHtml() (SSR) 双版本
- [web] admin/dashboard.html: 日志并入统一 Tab 架构（sidebar 4 tab + 日期选择器 + 级别过滤 + 暗色 log-viewer）
- [web] admin/dashboard.html: sidebar 版本号 v2.6 → v3.0.0

##### v2.9.5 — 品牌宣导 + 版本号统一
- [web] entry.html: 登录页新增 Dr.Rain 品牌宣导区（⚕️ 医疗顾问 + 功能标签：症状分析/用药参考/报告解读/隐私保护）
- [web] css/entry.css: 新增 .hero / .hero-features 样式（亮色/暗色双主题适配）
- [Docs] README.md / README_EN.md Badge: v2.8.0 → v3.0.0
- [web] admin/dashboard.html / logs.html: sidebar 版本号统一 v3.0.0

##### v2.9.6 — SP 8.4 移除废弃 menu 路由
- [AIServerCore] ChatServer: 移除 GET /menu → AIMenuHandler 路由注册及 include
- [web] 删除 menu.html / js/menu.js / css/menu.css（已被 /chat 沉浸式体验取代）

##### v2.9.7 — Plan 8 最终发布版 (内测里程碑)
- Plan 6: 工程治理（冗余清理 / 日志修复 / 部署资产 / CI）
- Plan 7: 反馈闭环 + README 旗舰重构
- Plan 8: 建表修复 / 图像识别入口 / 个人中心 / Admin UI 统一 / 品牌宣导 / 废弃路由清理



## Plan 9 — 开源社区基础设施
### v2.10

##### v2.10.0 — SP 9.1 Issue/PR 模板 + SP 9.2 CI
- [Infra] .github/ISSUE_TEMPLATE/bug_report.md: Bug 报告模板（复现步骤 / 环境信息 / 日志）
- [Infra] .github/ISSUE_TEMPLATE/feature_request.md: 功能请求模板（需求背景 / 期望方案）
- [Infra] .github/pull_request_template.md: PR 检查单（编译 / 格式化 / 规范 / TECHDOC）
- [Infra] .github/workflows/ci.yml: GitHub Actions 编译检查（ubuntu-24.04, push/PR 触发）



## Plan 10 — 聊天流融合端侧视觉能力 (Vision-Agent Workflow)
### v2.11
> v2.11.0 = Plan 10 功能发布

##### v2.11.0 — SP 10.1~10.4 Vision-Agent + UI 深度打磨
- **【Vision】SP 10.1** 前端聊天框支持图片上传（📎 附件按钮 + Base64 编码 + 缩略预览）
- **【Vision】SP 10.2** 后端 ONNX 推理与 Prompt 注入（ChatSseHandler 异步推理 → AIHelper::injectVisionContext）
- **【Vision】SP 10.3** 图片文件持久化（Base64→JPG 落盘 UUID 文件名 + Message.payload JSON 仅存缩略路径/识别结果）
- **【Vision】SP 10.4** UI 气泡图片渲染（payload.image.thumbnail 缩略图 + 🖼️ 识别结果标签）
- **【Vision】ONNX 优雅降级**：模型缺失时跳过视觉管线（text-only fallback），不阻断正常聊天
- **【Infra】scripts/download_models.sh**：MobileNetV2 ONNX 模型 + ImageNet 标签下载脚本
- **【UI】登录/注册页打磨**：标题与 Logo 间距收紧、注册页「已有账号？前往登录」链接、底部 Beta 免责声明
- **【UI】聊天页净化**：移除旧图像识别入口（🖼️）、Markdown 气泡增加左侧内边距
- **【UI】高级感主题背景图**：聊天页/登录页双主题动态背景



## v3.0.0 — 正式发版 (2026-08-03)

> 🎉 **Dr.Rain v3.0.0 正式发布**

### 发布范围
覆盖 Plan 1 ~ Plan 10 + SP 12/13 全部功能、安全加固、UI 打磨与 Bug 修复。

### 核心能力
- **AI 对话**：多模型 LLM（百炼/豆包）、SSE 流式输出、MCP Function Calling、Agent Loop
- **视觉识别**：聊天框图片上传 → ONNX MobileNetV2 推理 → Prompt 注入 → LLM 自然回答
- **用户系统**：邀请码注册、邮箱验证（QQ SMTP）、JWT 鉴权、RBAC 角色（admin/user/org）
- **安全加固**：argon2id 密码哈希、登录失败锁定、CORS/CSP/HSTS 安全头、错误脱敏
- **管理后台**：实时看板/用户管理/邀请码管理/反馈管理/日志查看器、Root 自动播种
- **工程基建**：Repository/Service 分层、ConfigManager 配置化、GoogleTest、CI/CD

### 技术栈
C++17 · muduo · MySQL · ONNX Runtime · OpenCV · libcurl · OpenSSL · libsodium · spdlog · nlohmann/json

### 变更统计 (v2.9.7 → v3.0.0)
Plan 9 (社区模板) + Plan 10 (Vision-Agent) + SP 12 (Root播种/Admin邀请码) + SP 13 (SQL修复/修改密码) + UI 补丁（背景图/对齐/Data URL修复/Vision上下文修复）



## v3.1.0 — 开源社区基础设施建设 (2026-08)

> 🏗️ **Git 工作流规范化 + CI/CD 基础设施搭建**

### 分支策略标准化

##### v3.1.0 — 分支模型 + CI/CD + Wiki + PR 模板 + CodeQL
- **【Docs】分支策略**：确立 `main`（生产🔒）/ `dev`（默认）双主干模型，采用 `deve/<username>/<type>/<desc>` 个人分支命名规范（`DEVELOP_STANDARD.md` §10）
- **【Docs】PR 模板**：`.github/PULL_REQUEST_TEMPLATE.md` 标准化代码合入检查单（变更类型 / Issue 关联 / C++ 底层逻辑说明 / 测试清单）
- **【Docs】Wiki 迁移**：新建 `wiki/` 目录（Architecture / API-Reference / Deployment / MCP-Plugin-Development / Project-Structure），README 从 240+ 行瘦身至 129 行
- **【CI/CD】ci.yml 升级**：push/PR 触发增加 `dev` 分支，引入 `ccache` + `actions/cache@v4` 编译缓存（热缓存减少 80%+ 编译时间），clang-format 格式检查从不阻断 warning 升级为硬性 `exit 1`
- **【CI/CD】Release 自动化**：`.github/workflows/release.yml`，`v*.*.*` tag 触发云端编译 → 打包 `http_server` + `web/` → 自动发布 GitHub Releases
- **【CI/CD】CodeQL 安全扫描**：`.github/workflows/codeql.yml`，合入 `main`/`dev` 后自动 C/C++ 静态分析（内存安全 / 未定义行为 / 注入漏洞），每周日兜底全量扫描
- **【Infra】CodeQL 优化**：移除 `pull_request` 触发器仅保留合入后扫描（省 CI 算力）



## v3.2.0 — 中间件与高可用重构 (2026-08)

> 🚀 **Redis 二级缓存 + RabbitMQ 异步削峰**

### Redis 二级缓存 (#10)

##### v3.2.0 — Redis 三级存储 + AMQP 任务队列
- **【Infralib】新增 `Infralib/Cache/RedisClient`**：hiredis 封装（连接管理、GET/SETEX/DEL/EXPIRE、Pipeline 批量）
- **【Infralib】新增 `Infralib/Cache/SessionCache`**：内存 → Redis → MySQL 三级流转门面
- **【Cache】缓存雪崩防护**：所有 SETEX TTL 附加 ±20% 随机抖动（`jitteredTTL`）
- **【Cache】缓存击穿防护**：互斥锁 + double-check，仅一个线程查 DB 重建缓存
- **【Cache】缓存穿透防护**：不存在的 session ID 缓存 `__NULL__` 标记（60s TTL）
- **【AIServerCore】ChatSessionsHandler 接入 SessionCache**：优先 Redis → 降级 MySQL，命中后异步刷新 TTL
- **【Infra】CMakeLists 新增 hiredis 依赖**：`find_library(HIREDIS_LIBRARY)` + 链接
- **【Infra】config.json 新增 `redis` 配置节**：host/port/password/db

### RabbitMQ 异步削峰 (#11)

##### v3.2.0 — RabbitMQ 任务队列
- **【Infralib】新增 `Infralib/Mq/TaskMessage`**：JSON 消息协议（version/taskId/type/payload/replyTo/ttl）
- **【Infralib】新增 `Infralib/Mq/TaskProducer`**：AMQP-CPP 生产者，publish() 投递任务不阻塞 Reactor
- **【Infralib】新增 `Infralib/Mq/TaskConsumer`**：独立消费者进程，type 路由分发（vision/tts/summarize）
- **【AIServerCore】新增 `TaskStatusHandler`**：`GET /task/{taskId}/status` SSE 轮询端点
- **【AIServerCore】ChatSseHandler 异步分流**：图片请求 → MQ 投递 + 立即返回 202 taskId
- **【HttpServer】HttpResponse 新增 `k202Accepted`**：异步任务受理状态码
- **【Infra】config.json 新增 `rabbitmq` 配置节**：host/port/vhost/user/password

### 工程

- **【Docs】CHANGELOG 追加 v3.2.0**
- **【Docs】wiki/Project-Structure.md 更新依赖清单**

