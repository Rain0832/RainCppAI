# RainCppAI TODO — 优化路线图

> 当前版本：v1.2.0

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

### 🔴 优化 1：引入异步/协程处理 AI 调用

**现状**：`AIHelper::chat()` 中 `curl_easy_perform()` 同步阻塞 muduo IO 线程（~500ms-数秒）。当前 4 个 IO 线程并行处理，最多支持 4 个并发 AI 请求。

**方案（待实施）**：
- **方案 A**：改造 `HttpServer::onRequest` 支持异步响应——Handler 捕获 `TcpConnectionPtr`，将 AI 调用提交到独立线程池，完成后通过 `conn->send()` 异步回写响应
- **方案 B**：C++20 协程 + `co_await`
- **方案 C**：`curl_multi` 异步接口 + EventLoop fd 注册

**评估**：方案 A 最务实，但需改造 HttpServer 的同步响应模型。属于**大改动**。

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
| ✅ 已完成 | 线程安全 — shared_mutex + LRU | v1.2.0 | 中 |
| ✅ 部分完成 | 配置管理 — 前端 API Key 传递 | v1.1.0 | 小 |
| 🔴 P0 | 异步 AI 调用（线程池 + 异步回写） | 待实施 | 大 |
| 🟠 P1 | SSE 流式输出（依赖 P0） | 待实施 | 大 |
| 🔵 P2 | 标准 MCP Server | 待实施 | 大 |
| ⚪ P3 | 可观测性 | 待实施 | 中 |
