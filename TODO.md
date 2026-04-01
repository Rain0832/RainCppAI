# RainCppAI TODO — 优化路线图

> 当前版本：v1.3.0

---

### ✅ 优化 3：chatInformation 的线程安全 & 内存管理（v1.2.0 已完成）

**改动**：
- 所有 `std::mutex` → `std::shared_mutex`（C++17 读写锁）
- 读操作（查找会话、获取历史）使用 `shared_lock`，写操作（创建会话）使用 `unique_lock`
- 新增 LRU 淘汰策略：`std::list` + `unordered_map` 实现 O(1) 访问/淘汰，最大会话数 500
- 覆盖所有共享数据：`chatInformation`、`onlineUsers_`、`sessionsIdsMap`、`ImageRecognizerMap`

---

### ✅ 优化 4：统一的配置管理（v1.1.0 部分完成）

**已完成**：
- API Key 支持前端用户级配置（localStorage → 请求传递 → 后端动态 setApiKey）
- Strategy 构造函数不再强制要求环境变量
- 敏感信息不入代码仓库

**未完成**：YAML/TOML 配置文件、运行时热更新

---

### ✅ 优化 1：引入异步线程池处理 AI 调用（v1.3.0 已完成）

**改动**：
- 新增通用 `ThreadPool` 类（`std::thread` + `std::queue` + `condition_variable`）
- `HttpResponse` 新增 `deferred` 异步模式 + 持有 `TcpConnectionPtr`
- `HttpServer::onRequest` 支持异步响应（deferred 模式跳过自动发送）
- `ChatSendHandler` / `ChatCreateAndSendHandler` AI 调用提交到 8 线程的线程池
- 线程池任务完成后通过 `runInLoop` 回到 IO 线程发送响应
- IO 线程完全不阻塞，并发能力从 4 提升到 8+

---

### 🟠 优化 2：SSE（Server-Sent Events）流式输出

**现状**：HttpResponse 不支持 chunked transfer encoding，前端用打字机效果模拟流式。

**方案（待实施）**：
- HttpResponse 新增流式发送能力（`Transfer-Encoding: chunked`）
- `AIHelper::chat()` 使用 curl `WRITEFUNCTION` 回调逐块转发
- 前端使用 `EventSource` API 接收 SSE
- **依赖优化 1**（需要异步响应框架）

---

### 🔵 优化 5：完善 MCP 实现——支持标准 MCP Server

**现状**：Prompt 注入 + JSON 解析的"伪 MCP"。

**方案**：
- 实现标准 MCP Server（JSON-RPC 2.0 / stdio 传输）
- 支持 `tools/list`、`tools/call` 等标准方法
- 实现 Function Calling 标准接口

---

### ⚪ 优化 6：可观测性（Observability）

**现状**：仅 muduo `LOG_INFO`，无结构化日志、无 metrics。

**方案**：
- 结构化日志（JSON + 请求 ID）
- Prometheus metrics（QPS、延迟、连接池使用率）
- OpenTelemetry tracing

---

## 优先级评估（更新后）

| 优先级 | 优化项 | 状态 | 难度 |
|--------|--------|------|------|
| ✅ 已完成 | 异步线程池 — IO 线程不阻塞 | v1.3.0 | 大 |
| ✅ 已完成 | 线程安全 — shared_mutex + LRU | v1.2.0 | 中 |
| ✅ 部分完成 | 配置管理 — 前端 API Key 传递 | v1.1.0 | 小 |
| 🟠 P0 | SSE 流式输出（依赖异步框架 ✅） | 待实施 | 大 |
| 🔵 P1 | 标准 MCP Server | 待实施 | 大 |
| ⚪ P2 | 可观测性 | 待实施 | 中 |
