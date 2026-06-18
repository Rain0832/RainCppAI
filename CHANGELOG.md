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
