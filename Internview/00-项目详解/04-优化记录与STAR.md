# 📈 项目优化记录 & STAR 法则

---

## 一、版本优化记录

| 版本 | 优化项 | 状态 | 变动大小 | 影响 |
|------|--------|------|---------|------|
| v1.6.0 | SSE 流式输出 + 标准 MCP Server | ✅ 已完成 | 大 | 实时响应 & 生态对接 |
| v1.5.0 | Phase 2 — 表结构重设计 | ✅ 已完成 | 中 | 可扩展性 & 正确性 |
| v1.4.0 | Phase 1 — 并发 Bug 修复（6项） | ✅ 已完成 | 中 | 消除 UB、角色判断、DB 不死锁 |
| v1.3.0 | 异步线程池 — IO 线程不阻塞 | ✅ 已完成 | 大 | 并发 4→8+，IO 零阻塞 |
| v1.2.0 | shared_mutex + LRU 会话淘汰 | ✅ 已完成 | 中 | 并发性能 & 内存管理 |
| v1.1.0 | 前端 API Key 传递 | ✅ 已完成 | 小 | 配置灵活性 |
| v1.0.0 | 前端全面重构 | ✅ 已完成 | 中 | 用户体验 |

### 版本号规则

| 变动大小 | 版本号 | 参考标准 |
|---------|--------|---------|
| 大改动 | +1.0.0 | 架构级变更、新增核心功能模块 |
| 中改动 | +0.1.0 | 多文件联动、重要 Bug 修复 |
| 小改动 | +0.0.1 | 单文件修改、配置调整 |

---

## 二、STAR 法则

| 维度 | 内容 |
|------|------|
| **S**ituation | 需要一个 C++ AI 应用平台，支持多模型对话、RAG、工具调用，可私有化部署 |
| **T**ask | 自研 HTTP 框架 + 实现 AI Agent 核心能力 + 保证并发安全与高可用 |
| **A**ction | muduo 五层架构；策略+工厂管理多模型；异步线程池不阻塞 IO；shared_mutex LRU；三高表设计；MCP 两段式推理；RabbitMQ 异步入库 |
| **R**esult | 4 种模型热切换、RAG 知识增强、工具调用、图像识别、语音处理；IO 线程不阻塞；消除并发 UB；框架可复用 |

---

## 三、待实施优化

### 🟠 SSE 流式输出（v1.6.0）
- `HttpResponse` 新增 `setSseMode()` — 握手头 `text/event-stream`
- `AIHelper::chatStream()` — curl WRITEFUNCTION 回调逐块 `conn->send`
- 前端 `EventSource` 替代 fetch

### 🟠 标准 MCP Server（v1.6.0）
- `McpServer` 类 — JSON-RPC 2.0，`tools/list` + `tools/call`
- `/mcp` 路由 — 接收 JSON-RPC 请求
- 复用现有 `AIToolRegistry`

### ⚪ 可观测性（待定）
- 结构化日志（JSON + 请求 ID）
- Prometheus metrics（QPS、延迟、连接池）

---

## 四、推荐阅读

1. 陈硕《Linux 多线程服务端编程》
2. Stevens《UNIX 网络编程》
3. Anthony Williams《C++ Concurrency in Action》
4. MCP 协议：https://modelcontextprotocol.io/
5. ONNX Runtime：https://onnxruntime.ai/docs/
