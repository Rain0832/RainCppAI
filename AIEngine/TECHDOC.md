# AIEngine — Module Technical Doc

## Responsibility

AIEngine is the project's AI utility library, encapsulating all AI capabilities: LLM calls, MCP protocol, speech synthesis, image recognition, message queue. Designed with zero HttpServer dependency, compilable as a static library for use in other projects.

## Core Flow

```
AIServerCore Handler
  → AIFactory::createStrategy(modelType)
    → AIHelper::chat(messages)
      ├─ buildRequest(snapshot, toolsSchema) ← passes OpenAI tools param
      ├─ executeCurl → LLM returns structured tool_calls
      ├─ parseToolCalls(response) → vector<ToolCallInfo>
      ├─ AIToolRegistry::instance().invoke(name, args)
      ├─ messages_.push_back({"tool", result, tool_call_id})
      └─ second LLM request → final reply

AIHelper::chatStream()
  ├─ Same flow, stream=true
  ├─ Parses accumulated response for tool_calls after each stream completes
  └─ Loops MAX 5 tool-call rounds

McpServer
  → AIToolRegistry::instance() (shared singleton)
    ├─ tools/list  → getToolsSchema()
    └— tools/call  → invoke(name, args)

AIToolRegistry (process-level singleton)
  → loadFromConfig("mcp_config.json")
    ├─ Auto-registers built-in tools: get_weather / get_time
    └─ Supports external registerTool() injection
```

## Key Files

| File | Responsibility |
|------|----------------|
| `include/llm/AIHelper.h` | AI call facade, manages strategy + messages |
| `include/llm/AIStrategy.h` | Strategy pattern base + ToolCallInfo struct |
| `include/llm/AIFactory.h` | Factory method + static registration macro |
| `include/mcp/McpServer.h` | MCP JSON-RPC 2.0 server |
| `include/mcp/AIToolRegistry.h` | MCP tool registry singleton (config-driven) |
| `include/audio/AISpeechProcessor.h` | TTS speech synthesis |
| `include/vision/ImageRecognizer.h` | ONNX Runtime + OpenCV inference |
| `include/common/AIConfig.h` | Generic JSON config loader |
| `include/common/Message.h` | Message entry (role + content + tool_call_id + ts) |
| `include/common/base64.h` | Base64 encode/decode |
| `include/common/MQManager.h` | RabbitMQ message queue manager |
| `include/common/AISessionIdGenerator.h` | Session ID generator |

## Dependencies & Coupling Boundaries

### Depends On

| Dependency | Notes |
|-----------|-------|
| libcurl | HTTP client (LLM API) |
| OpenSSL | HTTPS |
| ONNX Runtime | Image recognition inference |
| OpenCV | Image preprocessing |
| SimpleAmqpClient | RabbitMQ client |
| nlohmann/json | JSON (via 3rdparty/JsonUtil.h) |

### Depended By

- AIServerCore: references llm/, mcp/, audio/, vision/, common/ headers
- HttpServer: no dependency

### Namespace

- `ai::` — top-level namespace