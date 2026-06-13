# AIEngine — 模块技术文档

## 模块职责

AIEngine 是项目的**AI 工具库**，封装所有 AI 相关能力：LLM 调用、MCP 协议、语音合成、图像识别、消息队列。设计目标是**零 HttpServer 依赖**，可独立编译为静态库在其他项目中使用。

## 核心文件流转逻辑

```
AIServerCore Handler
  └─► AIFactory::createStrategy(model_type)
        └─► AIStrategy::chat(messages, callback)
              ├─► AliyunStrategy        ← curl → 阿里云百炼
              ├─► DoubaoStrategy        ← curl → 火山引擎豆包
              ├─► AliyunRAGStrategy     ← curl → 百炼知识库
              └─► AliyunMcpStrategy     ← JSON-RPC → MCP 工具链

AIHelper::chat()
  ├─► strategy_->chat()
  ├─► MQManager::publish()  → RabbitMQ（异步持久化）
  └─► callback()  → runInLoop → 响应

McpServer
  └─► AIToolRegistry::registerTool()
        └─► tools/list  /  tools/call (JSON-RPC 2.0)
```

### 关键文件

| 文件 | 职责 |
|------|------|
| `include/llm/AIHelper.h` | AI 调用的外观类，管理 strategy + messages |
| `include/llm/AIStrategy.h` | 策略模式抽象基类 |
| `include/llm/AIFactory.h` | 工厂方法，根据 model_type 创建策略 |
| `include/mcp/McpServer.h` | MCP JSON-RPC 2.0 服务端 |
| `include/mcp/AIToolRegistry.h` | MCP 工具注册表 (get_weather, etc.) |
| `include/audio/AISpeechProcessor.h` | TTS 语音合成（百度 API + Token 缓存） |
| `include/vision/ImageRecognizer.h` | ONNX Runtime + OpenCV 推理 |
| `include/common/AIConfig.h` | AI 配置管理（模型列表、API 端点） |
| `include/common/base64.h` | Base64 编解码 |
| `include/common/MQManager.h` | RabbitMQ 消息队列管理 |
| `include/common/AISessionIdGenerator.h` | 会话 ID 生成器 |

### 核心类关系

```
AIHelper
  ├─► AIStrategy*（多态策略）
  │     ├─► AliyunStrategy
  │     ├─► DoubaoStrategy
  │     ├─► AliyunRAGStrategy
  │     └─► AliyunMcpStrategy
  ├─► MQManager（消息队列）
  └─► vector<Message>（会话消息缓存）

McpServer
  └─► AIToolRegistry
        └─► Tool（name + handler）
```

## 对外依赖与耦合边界

### 依赖

| 依赖 | 说明 |
|------|------|
| libcurl | HTTP 客户端（调用 LLM API） |
| OpenSSL | HTTPS 请求 |
| ONNX Runtime | 图像识别推理 |
| OpenCV | 图像预处理 |
| SimpleAmqpClient | RabbitMQ 客户端 |
| nlohmann/json | JSON（通过 `3rdparty/JsonUtil.h`） |

### 被依赖

- **AIServerCore**：引用 `llm/`、`mcp/`、`audio/`、`vision/`、`common/` 头文件
- **HttpServer**：**不依赖**（AIEngine 不引用任何 HttpServer 头文件）

### 命名空间

- `ai::` — 顶层命名空间
