# RainCppAI TODO — 优化路线图

> 当前版本：v2.0.0（Phase B 已完成）

---

## ✅ v2.0.0 — Agent 架构重构（已完成）

### Phase A：修复紧急 Bug（v1.6.1 ✅ 已完成）
1. ✅ **chatStream 支持 MCP** — MCP 模式走非流式两段式推理后逐字回调
2. ✅ **豆包 API 修复** — 支持用户配置 Endpoint ID（`ep-xxxxx`），个人中心新增输入框
3. ✅ **历史消息 DB fallback** — `ChatHistoryHandler` 内存 miss 时从 MySQL `messages` 表恢复
4. ✅ **天气工具优化** — 增加超时 8s、User-Agent、网络失败时返回降级回答而非崩溃

### Phase B：Agent Loop 重构（✅ 已完成）
- ✅ **`AIHelper::agentLoop()`** — N 步 Function Calling 循环，最多 5 步防死循环
- ✅ **原生 Function Calling** — `buildRequest()` 声明 `tools[]`，解析 `tool_calls` 结构化字段
- ✅ **`buildRequest()` 传递 role** — 从 `Message.role` 读取
- ✅ **流式 Agent Loop** — `chatStream()` 中 MCP 模式走 Agent Loop 后逐字 UTF-8 安全回调

---

## 🔴 v2.1.0 — 安全 & 性能优化（待实施）

### 已发现的架构缺陷

| 缺陷 | 影响 | 严重度 |
|------|------|--------|
| `pushMessageToMysql` SQL 拼接 | 注入风险，手工 `escapeString` 不可靠 | 🔴 高 |
| Agent Loop tool 角色消息不持久化 | 服务重启后工具调用上下文丢失 | 🔴 高 |
| `AliyunMcpStrategy` 与 `AliyunStrategy` 代码重复 | 违反 DRY，维护成本翻倍 | 🟡 中 |
| `chatInformation` 全局读写锁 | 不同用户间互斥，高并发下成为瓶颈 | 🟡 中 |
| `get_weather` 仍调 `wttr.in`（境外） | 国内超时，虽有 fallback 但根本问题未解 | 🟡 中 |
| 连接池 `detach()` 线程 | 无法优雅关闭，程序退出可能中断 ping | 🟡 中 |

---

## 🟠 v3.0.0 — 生态 & 可靠性升级（规划中）

> 以下优化项无优先级顺序，可并行推进

### 1. 压测系统 + Redis 评估
- 先建压测系统（wrk/wrk2 + 自定义 Lua 脚本），评估当前负载能力
- 根据压测结果决定是否引入 Redis：Session 共享 / 历史消息缓存 / 热点用户 chatInfo 缓存
- 当前用户量不大，过早引入 Redis 可能增加复杂度而无收益

### 2. SQL 注入修复
- `pushMessageToMysql` 改用 MySQL Prepared Statement（参数化查询）
- 消除 `escapeString` 手工转义，彻底防注入

### 3. Phase C：工具生态
- **外部 MCP Server 对接** — 连接外部 MCP Server（通过 HTTP/stdio），不仅用本地 `AIToolRegistry`
- **工具声明配置化** — `config.json` → 标准 MCP `tools/list` 格式，支持动态加载
- **用户自定义工具** — 通过 Web UI 注册自定义 HTTP API 作为工具

### 4. Phase D：可靠性 & 可观测性
- **结构化日志** — JSON 格式 + 请求 ID + Agent Loop 步骤追踪
- **Prometheus Metrics** — QPS、AI 延迟、工具调用成功率
- **Redis 缓存** — 历史消息 DB fallback + 热点会话缓存

### 5. 分片锁优化
- `chatInformation` 全局锁 → 分片锁（`shared_mutex[userId % N]`）
- 消除不同用户间的互斥，提升并发读性能

### 6. C++20 协程探索
- `co_await` 替代线程池 + `runInLoop`
- 用 `asio::awaitable` 实现天然异步的 AI 调用链，无需手动线程管理

### 7. Nginx + Redis 部署
- Nginx 反向代理 + 负载均衡
- Session 迁移到 Redis 共享，支持多实例水平扩展

### 8. HTTP/2 支持
- 引入 nghttp2 库
- 二进制帧 + 多路复用 + HPACK 头部压缩 + 无队头阻塞

### 9. 自建 AI 能力（替代外部 API）
- **TTS/ASR**：sherpa-onnx (C++ TTS) + whisper.cpp (ASR)，去掉百度 API 依赖
- **RAG 链路**：ONNX BGE-small Embedding + FAISS 向量索引，去掉阿里百炼 API 依赖
- **天气工具**：换国内天气 API（和风天气）
- **路由优化**：正则 O(n) → Radix Tree O(k)

---

## ✅ 历史版本归档

### v1.6.0 — SSE 流式 + MCP Server
- `AIHelper::chatStream()` + `StreamWriteCallback` 逐 token 回调
- `McpServer` JSON-RPC 2.0 `tools/list` + `tools/call`

### v1.5.0 — 三高表结构
- sessions / messages / user_api_keys 三表设计

### v1.4.0 — 并发 Bug 修复（6项）
- msgMutex_ / Message.role / CAS / 锁分离 / exclusive=false / wait_for(3s)

### v1.3.0 — 异步线程池
- ThreadPool + deferred + runInLoop，IO 不阻塞

### v1.2.0 — 读写锁 + LRU
- shared_mutex + list+map O(1) 淘汰

### v1.1.0 — 动态 API Key
- 前端 localStorage → 请求传递 → setApiKey()

### v1.0.0 — 初始部署
- 前端重构 + 基础功能
