### 🔴 优化 1：引入异步/协程处理 AI 调用

**现状**：`AIHelper::chat()` 中使用 `curl_easy_perform()` 同步阻塞，在 muduo 的 IO 线程中调用外部 API（~500ms），会阻塞该线程上所有连接的事件处理。

**方案**：
- 引入 `curl_multi` 异步接口，配合 muduo EventLoop 注册 fd 监听
- 或引入 C++20 协程（`co_await`），将阻塞调用转为协程挂起
- 或使用独立线程池处理 AI 请求，通过 `runInLoop` 回调到 IO 线程发送响应

**影响**：这是**最严重的性能瓶颈**，解决后可显著提升并发处理能力。

---

### 🟠 优化 2：SSE（Server-Sent Events）流式输出

**现状**：AI 回答需要等待 LLM 完整生成后一次性返回，用户等待时间长。

**方案**：
- 支持 SSE 协议，LLM 生成的每个 token 实时推送到前端
- 修改 `AIHelper::chat()` 使用 curl 的 `WRITEFUNCTION` 回调逐块转发
- HttpResponse 支持 `Transfer-Encoding: chunked`

**影响**：大幅提升用户体验，这是现代 AI 应用的标配。

---

### 🟡 优化 3：chatInformation 的线程安全 & 内存管理

**现状**：`unordered_map<userId, map<sessionId, AIHelper>>` 全部在内存中，无上限控制，无过期清理。

**方案**：
- 加读写锁（`shared_mutex`）保护并发访问
- 实现 LRU 淘汰策略，限制内存中的会话数
- 空闲会话序列化到 Redis/磁盘，按需加载（lazy loading）

**影响**：避免内存泄漏和 OOM，支持更多并发用户。

---

### 🟢 优化 4：统一的配置管理

**现状**：API Key、数据库连接信息等硬编码在代码中（策略类中直接写死 API Key 和 URL）。

**方案**：
- 统一的配置中心（YAML/TOML 配置文件 + 环境变量覆盖）
- 敏感信息通过环境变量注入，不入代码仓库
- 支持运行时热更新配置

**影响**：安全性、可维护性、部署灵活性大幅提升。

---

### 🔵 优化 5：完善 MCP 实现——支持标准 MCP Server

**现状**：当前 MCP 是通过 Prompt 注入 + JSON 解析实现的"伪 MCP"，工具调用依赖 LLM 按照指定格式输出 JSON。

**方案**：
- 实现标准 MCP Server（基于 JSON-RPC 2.0 / stdio 传输）
- 支持 `tools/list`、`tools/call` 等标准方法
- 支持多 MCP Server 连接，每个 Server 提供不同能力
- 实现 Tool Use 的 Function Calling 标准接口（而非 Prompt 注入）

**影响**：让项目与 MCP 生态对接，支持 Claude/GPT 等模型的原生 Function Calling。

---

### ⚪ 优化 6：可观测性（Observability）

**现状**：日志使用 muduo 的 `LOG_INFO`，无结构化日志、无 metrics、无 tracing。

**方案**：
- 结构化日志（JSON 格式 + 日志级别 + 请求 ID）
- Prometheus metrics（QPS、延迟、连接池使用率、MQ 堆积）
- OpenTelemetry tracing（请求从入口到 AI API 到数据库的全链路追踪）

**影响**：排障效率和系统稳定性的基础设施。