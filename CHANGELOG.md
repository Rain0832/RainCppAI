# Changelog

本文档遵循**三级递进**格式：`# 大版本` → `### 中版本` → `##### 小版本`。
版本按时间正序排列，新版本追加在末尾。

**格式规则**：
- `# vX.0.0` — 大版本标题（破坏性变更、架构重构）
- `### vX.Y.0` — 中版本标题（功能新增、模块重构）
- `##### vX.Y.Z` — 小版本标题（Bug 修复、细节优化）
- 每个小版本条目以 `-` 开头，一行一条变更

---

# v1.0.0

> 初始版本

### v1.0.0 — MVP

##### v1.0.0 (2026-04)
- 自研 HTTP 框架（muduo Reactor）
- 多模型 LLM（Qwen / Doubao / RAG / MCP）
- 图像识别 + 语音合成
- Session 管理 + MySQL 持久化

---

# v1.1.0

> 动态 API Key

### v1.1.0 — 前端重构 + Runtime Key

##### v1.1.0 (2026-04)
- `AIStrategy::setApiKey()` 运行时注入
- 前端重构：明暗主题、打字机效果、API Key 面板
- 敏感信息不入仓库

---

# v1.2.0

> 读写锁 + LRU

### v1.2.0 — shared_mutex & LRU

##### v1.2.0 (2026-04)
- `std::mutex` → `std::shared_mutex`（读写锁）
- LRU 淘汰策略（`list` + `unordered_map`，O(1)）
- 最大 500 会话内存上限

---

# v1.3.0

> 异步线程池

### v1.3.0 — ThreadPool

##### v1.3.0 (2026-04)
- 通用 `ThreadPool`（std::thread + queue + condition_variable）
- `HttpResponse` deferred 异步模式
- AI 调用提交到 8 线程池，IO 线程零阻塞

---

# v1.4.0

> Phase 1 — 并发 Bug 修复（6 项）

### v1.4.0 — 并发修复

##### v1.4.0 (2026-04)
- `msgMutex_` 保护 `messages_`（原无锁）
- `Message` 结构体 + 显式 `role` 字段（替代奇偶判断）
- `atomic<bool> processing_` 串行化并发请求
- 锁分离：chatInfo 锁 ↔ AIHelper 锁
- RabbitMQ `exclusive=false` 修复独占队列 bug
- DB 连接池 `wait_for(3s)` 超时防阻塞

---

# v1.5.0

> Phase 2 — 数据库表结构重设计

### v1.5.0 — 三高表结构

##### v1.5.0 (2026-04)
- 新增 `sessions` 表（会话元数据、软删除、毫秒时间戳）
- 新增 `messages` 表（自增主键 + 显式 `role` ENUM，替代奇偶判断）
- 新增 `user_api_keys` 表（`(user_id, provider)` 唯一索引）
- 迁移 `chat_message` → `messages` + `sessions`，旧表保留
- readDataFromMySQL / pushMessageToMysql 适配新表

---

# v1.6.0

> SSE 流式输出 + 标准 MCP Server

### v1.6.0 — SSE & MCP

##### v1.6.0 (2026-04)
- `AIHelper::chatStream()` — curl WRITEFUNCTION 逐 token 实时回调
- `ChatSseHandler` — `POST /chat/send-stream`，SSE 握手 + 流式回写
- `McpServer` — 标准 JSON-RPC 2.0，`tools/list` + `tools/call`
- `McpHandler` — `POST /mcp`，兼容 Claude Desktop / Cursor

##### v1.6.1 (2026-04)
- chatStream 支持 MCP 模式
- 豆包 API 端点 ID 用户可配
- ChatHistoryHandler 内存 miss 时 MySQL fallback
- get_weather 超时 + 降级

---

# v2.0.1

> **代码格式化标准化** — 引入 Clang-Format + 全量格式化

### v2.0.1 — 代码格式化

##### v2.0.1 (2026-06)
- `.clang-format`：新增 Clang-Format 配置文件，对齐 `DEVELOP_STANDARD.md` 规范
- 全项目 `.cpp` / `.h` 文件执行 `clang-format -i` 统一格式化

---

# v2.0.0

> **架构重构** — 四模块拆分 + 代码规范标准化

### v2.0.0 — 项目结构重组

##### v2.0.0 (2026-06)
- **架构**：四模块拆分 — HttpServer（网络框架）、AIServerCore（业务层）、AIEngine（AI 工具库）、web（前端资源）
- **规范**：新增 `DEVELOP_STANDARD.md`，统一命名风格（`snake_case_` 成员、`http::` / `ai::` / `chat::` 命名空间）
- **文档**：`README.md` 重写、`CHANGELOG.md` 重新设计为三级递进格式、`TODO.md` 四象限优先级
- **清理**：删除 `Internview/` 目录、各子模块 `CHANGELOG.md` 替换为 `TECHDOC.md`
- **流程**：新增 `AGENT.md` 规范 AI Agent 开发流程

---

# v2.1.0

> **MCP 工具调用重构** — 原生 Function Calling 替代文本解析

### v2.1.0 — MCP 模块重构

##### v2.0.3 (2026-06)
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

# v2.1.2

> **MCP 流式修复** — 流式 SSE 场景正式支持 Function Calling / MCP 工具调用

### v2.1.2 — MCP 流式上线

##### v2.0.4 (2026-06)
- `AIToolRegistry::loadFromConfig` 在 `main.cpp` 启动时加载，修复 tools schema 未传 LLM 的问题
- `StreamWriteCallback` 增量累积 `delta.tool_calls`（按 index 合并 id/name/arguments 片段）
- `executeCurlStream` 流结束后构造完整 OpenAI JSON 响应，供 `chatStream` 解析 tool_calls
- `StreamContext` 新增 `std::map<int, json> toolCalls` 累积字段
- MCP 工具调用增加 `LOG_INFO` 级别日志，调试信息 `LOG_DEBUG` 全部移除
- 日志级别默认 `INFO`（原 `DEBUG`）
- `HttpRequest.cpp` 补充 `#include <cassert>`
