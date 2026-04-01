# MCP 协议是什么？和 RAG 的关系？项目中如何实现？

---

## 一、MCP 协议是什么？

### 1.1 一句话定义

**MCP（Model Context Protocol，模型上下文协议）** 是由 Anthropic 于 2024 年底提出的一种**开放标准协议**，目的是标准化 LLM 与外部工具/数据源之间的交互方式。你可以把它理解为 **AI 世界的 USB 接口**——不管是什么设备（工具），只要遵循这个标准，就能即插即用地和 LLM 协作。

### 1.2 为什么需要 MCP？

在 MCP 出现之前，每个 AI 应用都要**自己定义**如何让 LLM 调用外部工具：

```
没有 MCP 的世界：
  App A → 自定义 JSON 格式 → LLM → 解析自定义格式 → 调工具
  App B → 另一种 JSON 格式 → LLM → 解析另一种格式 → 调工具
  App C → 又一种格式...
  
  问题：每个 App 都要重复造轮子，工具不能复用
```

```
有 MCP 的世界：
  App A ─┐
  App B ─┼─ 统一的 MCP 协议 → 任意 MCP Server（工具）
  App C ─┘
  
  好处：工具标准化，一次实现到处复用
```

### 1.3 MCP 的核心架构

```
┌─────────────┐     JSON-RPC 2.0      ┌─────────────┐
│  MCP Client │ ←──────────────────→ │  MCP Server │
│  (LLM 应用) │     stdio / HTTP+SSE  │  (工具提供方) │
└─────────────┘                        └─────────────┘
```

**三个核心概念**：

| 概念 | 说明 | 类比 |
|------|------|------|
| **Tools**（工具） | Server 暴露的可调用函数 | 类似 REST API 的 endpoint |
| **Resources**（资源） | Server 提供的只读数据 | 类似 GET 请求获取的数据 |
| **Prompts**（提示模板） | Server 提供的预定义 Prompt | 类似 API 文档中的示例 |

**标准交互流程**：

```
1. Client 发起 initialize → Server 返回自己的能力声明
2. Client 调 tools/list → Server 返回可用工具列表
3. LLM 决定调用某个工具
4. Client 调 tools/call(name, args) → Server 执行并返回结果
5. Client 把结果回注给 LLM，LLM 生成最终回答
```

### 1.4 MCP vs Function Calling

| 维度 | MCP | Function Calling (OpenAI) |
|------|-----|--------------------------|
| **提出者** | Anthropic | OpenAI |
| **本质** | 通信协议（Client-Server） | API 参数（在请求中声明 tools） |
| **传输** | JSON-RPC 2.0，支持 stdio/HTTP | HTTP（内嵌在 Chat API 中） |
| **工具声明** | Server 通过 `tools/list` 动态暴露 | Client 在请求的 `tools` 参数中传入 |
| **工具执行** | Server 执行（远程/本地都行） | Client 自己执行 |
| **生态** | 独立于模型，任何 LLM 都能用 | 绑定 OpenAI API 格式 |
| **复用性** | 一个 MCP Server 可被多个 Client 复用 | 每个应用自己实现工具逻辑 |

**简单说**：Function Calling 是"告诉 LLM 有哪些工具可用"的**接口格式**，MCP 是"LLM 应用和工具提供方之间如何通信"的**协议标准**。二者不冲突，MCP Client 内部可以用 Function Calling 来让 LLM 决定调哪个工具。

---

## 二、MCP 和 RAG 的关系

### 2.1 RAG 是什么（一句话）

**RAG（Retrieval-Augmented Generation，检索增强生成）** = 给 LLM 提供外部记忆。LLM 回答问题前，先从知识库检索相关文档，注入到 Prompt 中，让 LLM "现学现答"。

### 2.2 MCP 和 RAG 是什么关系？

**它们不是同一层级的概念，而是可以协作的关系：**

```
MCP = 通信协议（解决"怎么让 LLM 和外部系统交互"）
RAG = 应用模式（解决"怎么让 LLM 获取外部知识"）

RAG 可以作为 MCP 的一个具体应用场景：
  MCP Server A → 提供"知识库检索"工具（本质就是 RAG）
  MCP Server B → 提供"天气查询"工具
  MCP Server C → 提供"数据库查询"工具
```

**具体来说**：

| 维度 | MCP | RAG |
|------|-----|-----|
| **解决的问题** | LLM 如何调用外部工具/获取外部数据 | LLM 如何获取特定领域的知识 |
| **抽象层级** | 传输层/协议层 | 应用层/业务模式 |
| **包含关系** | MCP 可以承载 RAG（RAG 作为一种 Tool） | RAG 可以通过 MCP 协议暴露检索能力 |
| **独立性** | MCP 不一定用于 RAG（也可用于天气、代码执行等） | RAG 不一定用 MCP（可以直接嵌入应用） |

**一个典型的 MCP + RAG 架构**：

```
用户问题 → MCP Client(LLM 应用)
              │
              ├─→ MCP Server: RAG 知识库
              │     tools/call("search_docs", {"query": "..."})
              │     → Embedding → FAISS 检索 → 返回相关文档
              │
              ├─→ MCP Server: 天气服务
              │     tools/call("get_weather", {"city": "北京"})
              │
              └─→ LLM 综合所有工具结果 → 生成回答
```

---

## 三、项目代码中是如何实现的？

### 3.1 项目的 MCP 实现概述

项目实现了 MCP 的**核心思想**——让 LLM 自主判断是否需要调用工具，但采用的是**基于 Prompt 注入的轻量实现**，而非标准的 JSON-RPC 2.0 协议。

### 3.2 完整代码执行流程

```
                    ┌──────────────────────────────────────┐
                    │        用户发送消息                    │
                    └────────────────┬─────────────────────┘
                                     ▼
                    ┌──────────────────────────────────────┐
                    │  AIHelper::chat() 判断 isMCPModel    │
                    │  modelType="4" → AliyunMcpStrategy   │
                    └────────────────┬─────────────────────┘
                                     ▼
              ┌─────────────────────────────────────────────────┐
              │  第一段：意图识别                                  │
              │                                                   │
              │  1. AIConfig::loadFromFile("config.json")        │
              │     → 加载工具列表和 Prompt 模板                   │
              │                                                   │
              │  2. AIConfig::buildPrompt(userQuestion)           │
              │     → 把工具描述注入到 Prompt 中                   │
              │     → "你有以下工具: get_weather(city), get_time()" │
              │     → "如果需要调用，请输出JSON: {tool, args}"       │
              │                                                   │
              │  3. messages.push_back({tempPrompt, 0})           │
              │     → 临时推入带工具说明的 Prompt                   │
              │                                                   │
              │  4. strategy->buildRequest(messages)              │
              │     executeCurl(payload)                          │
              │     strategy->parseResponse(response)             │
              │     → 第一次调用 LLM，让它判断要不要用工具           │
              │                                                   │
              │  5. messages.pop_back()                           │
              │     → 用完立即移除提示词，不污染上下文               │
              └────────────────────┬──────────────────────────────┘
                                   ▼
                    ┌──────────────────────────────────────┐
                    │  AIConfig::parseAIResponse(aiResult) │
                    │  尝试 JSON 解析，判断是否为工具调用    │
                    └────────────┬─────────┬───────────────┘
                                 ▼         ▼
                    ┌────────────┐  ┌──────────────────┐
                    │ 不需要工具  │  │ 需要调用工具      │
                    │ 直接返回    │  │ call.isToolCall   │
                    │ aiResult   │  │ = true            │
                    └────────────┘  └────────┬─────────┘
                                             ▼
              ┌─────────────────────────────────────────────────┐
              │  工具执行                                         │
              │                                                   │
              │  AIToolRegistry registry;                        │
              │  toolResult = registry.invoke(call.toolName,     │
              │                               call.args);        │
              │  → 在 unordered_map<string, ToolFunc> 中查找      │
              │  → 调用 getWeather() 或 getTime()                │
              └────────────────────┬──────────────────────────────┘
                                   ▼
              ┌─────────────────────────────────────────────────┐
              │  第二段：结果综合                                  │
              │                                                   │
              │  1. AIConfig::buildToolResultPrompt(              │
              │       userQuestion, toolName, args, toolResult)   │
              │     → "用户问了XX，我调了get_weather，结果是..."     │
              │     → "请根据以上信息，用自然语言回答用户"            │
              │                                                   │
              │  2. messages.push_back({secondPrompt, 0})         │
              │     strategy->buildRequest(messages)              │
              │     executeCurl(payload)                          │
              │     → 第二次调用 LLM，生成最终自然语言回答           │
              │                                                   │
              │  3. messages.pop_back()                           │
              │     → 清理临时提示词                                │
              │                                                   │
              │  4. addMessage(userId, ..., finalAnswer, ...)     │
              │     → 只将原始问题和最终回答存入上下文和数据库       │
              └─────────────────────────────────────────────────┘
```

### 3.3 关键代码详解

**config.json — 工具声明（类似 MCP 的 tools/list）**：

```json
{
  "prompt_template": "...你可以使用以下工具:\n{tool_list}\n如果需要调用，请输出JSON格式...",
  "tools": [
    { "name": "get_weather", "params": {"city": "北京"}, "desc": "获取天气" },
    { "name": "get_time", "params": {}, "desc": "获取当前时间" }
  ]
}
```

**AIHelper::chat() — 核心调度（两段式推理）**：

```cpp
// 第一段：带工具描述的 Prompt 发给 LLM
messages.push_back({tempUserQuestion, 0});  // 临时推入
json firstResp = executeCurl(firstReq);
std::string aiResult = strategy->parseResponse(firstResp);
messages.pop_back();  // 立即清除

// 解析：LLM 返回的是 JSON（工具调用）还是文本（直接回答）？
AIToolCall call = config.parseAIResponse(aiResult);

if (call.isToolCall) {
    // 执行本地工具
    json toolResult = registry.invoke(call.toolName, call.args);
    
    // 第二段：工具结果回注，让 LLM 综合回答
    std::string secondPrompt = config.buildToolResultPrompt(...);
    messages.push_back({secondPrompt, 0});
    json secondResp = executeCurl(secondReq);
    std::string finalAnswer = strategy->parseResponse(secondResp);
    messages.pop_back();  // 清除
}
```

**AIToolRegistry — 工具注册中心（类似 MCP Server 的工具集）**：

```cpp
AIToolRegistry::AIToolRegistry() {
    registerTool("get_weather", getWeather);  // 注册天气工具
    registerTool("get_time", getTime);        // 注册时间工具
}

json AIToolRegistry::invoke(const std::string& name, const json& args) const {
    auto it = tools_.find(name);              // O(1) 查找
    return it->second(args);                  // 执行工具函数
}
```

### 3.4 项目实现 vs 标准 MCP 的差异

| 维度 | 项目实现 | 标准 MCP |
|------|---------|---------|
| **工具声明** | Prompt 文本注入 | `tools/list` JSON-RPC 方法 |
| **工具调用** | 解析 LLM 自由文本中的 JSON | `tools/call` 结构化请求 |
| **传输协议** | 无（内嵌在应用中） | JSON-RPC 2.0 over stdio/HTTP |
| **可靠性** | 依赖 LLM 按格式输出（可能出错） | 结构化协议，稳定可靠 |
| **工具复用** | 工具逻辑和应用耦合 | MCP Server 独立部署，多 Client 复用 |
| **优势** | 实现简单，兼容任何 LLM | 标准化、可扩展、生态丰富 |

### 3.5 面试话术

> "我在项目中实现了 MCP 协议的核心理念——让 LLM 自主判断是否需要调用外部工具。具体采用了**两段式推理**架构：第一段将工具列表通过 Prompt 注入让 LLM 做意图识别，第二段将工具执行结果回注让 LLM 综合生成自然语言回答。工具注册通过 `AIToolRegistry`（`unordered_map<string, function>`）实现，支持运行时动态添加。这种方式兼容任何 LLM（不要求模型原生支持 Function Calling），但可靠性不如标准 Function Calling 接口。后续计划升级为标准 MCP Server（基于 JSON-RPC 2.0），支持 `tools/list` 和 `tools/call` 标准方法。"

---

## 📝 复习要点速记

### MCP 协议
- **定义**：Anthropic 提出的开放标准协议，标准化 LLM 与外部工具/数据源的交互
- **架构**：MCP Client（LLM 应用）↔ JSON-RPC 2.0 ↔ MCP Server（工具提供方）
- **三大概念**：Tools（工具）、Resources（资源）、Prompts（提示模板）
- **vs Function Calling**：MCP 是通信协议（Client-Server），FC 是 API 参数格式
- **类比**：AI 世界的 USB 接口——即插即用

### MCP 与 RAG 的关系
- **不同层级**：MCP = 传输协议层，RAG = 应用业务层
- **协作关系**：RAG 可以作为 MCP Server 的一个工具暴露出来
- **不互斥**：MCP 不只服务于 RAG，RAG 也不必通过 MCP

### 项目实现
- **方式**：基于 Prompt 注入的"轻量 MCP"，非标准 JSON-RPC
- **两段式推理**：① LLM 判断是否调工具 → ② 工具结果回注 LLM 综合回答
- **关键类**：`AIConfig`（Prompt 模板+工具列表）、`AIToolRegistry`（工具注册执行）、`AIHelper::chat()`（调度核心）
- **临时 Prompt 管理**：push → LLM 调用 → pop，不污染上下文
- **vs 标准做法**：Prompt 注入 vs Function Calling API，兼容性好但可靠性差
