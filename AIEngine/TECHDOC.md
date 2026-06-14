# AIEngine — Module Technical Doc

## Responsibility

AIEngine is the project's AI utility library, encapsulating all AI capabilities: LLM calls, MCP protocol, speech synthesis, image recognition, message queue. Designed with zero HttpServer dependency, compilable as a static library for use in other projects.

## Core Flow

```
AIServerCore Handler
  → AIFactory::createStrategy(modelType)
    → AIHelper::chatStream(messages, onChunk)
      ├─ buildRequest(snapshot, toolsSchema) ← AIToolRegistry::getToolsSchema()
      ├─ executeCurlStream → LLM stream tokens + tool_calls to frontend
      ├─ parseToolCalls(accumulated response) → vector<ToolCallInfo>
      ├─ AIToolRegistry::instance().invoke(name, args)
      │     └─ McpClientManager::instance().callTool(name, args)
      │           └─ StdioClient → Python weather_server.py (pipe JSON-RPC)
      ├─ messages_.push_back({"tool", result, tool_call_id})
      └─ second LLM request → final reply

McpServer
  → AIToolRegistry::instance() (shared singleton)
    ├─ tools/list  → getToolsSchema() → McpClientManager::discoverAllTools()
    └─ tools/call  → invoke(name, args) → McpClientManager::callTool()

McpClientManager startup (main.cpp):
  1. loadFromConfig("mcp_config.json") → registerServer(name, serverDef)
  2. registerServer → fork() + pipe() + dup2() + execvp()
  3. MCP handshake: initialize → response → notifications/initialized
  4. StdioClient ready → discoverAllTools() → tools/list JSON-RPC
```

## Key Files

| File | Responsibility |
|------|----------------|
| `include/llm/AIHelper.h` | AI call facade, manages strategy + messages |
| `include/llm/AIStrategy.h` | Strategy pattern base + ToolCallInfo struct |
| `include/llm/AIFactory.h` | Factory method + static registration macro |
| `include/mcp/McpServer.h` | MCP JSON-RPC 2.0 server (local endpoint) |
| `include/mcp/AIToolRegistry.h` | Thin proxy, delegates all tool calls to McpClientManager |
| `include/mcp/McpClientManager.h` | Manages stdio/sse client connections, hot-plug support |
| `include/audio/AISpeechProcessor.h` | TTS speech synthesis |
| `include/vision/ImageRecognizer.h` | ONNX Runtime + OpenCV inference |
| `include/common/Message.h` | Message entry (role + content + tool_call_id + ts) |
| `include/common/MQManager.h` | RabbitMQ message queue manager |

## MCP Architecture (v2.0.8)

### Transport Layers

| Transport | Implementation | Communication |
|-----------|---------------|---------------|
| `stdio` | `StdioClient` (in McpClientManager.cpp) | `pipe()` + `fork()` + `dup2()` + `execvp()`, non-blocking `read()`/`write()` |
| `sse` | `SseClient` (in McpClientManager.cpp) | libcurl GET SSE → discover endpoint → HTTP POST JSON-RPC |

### Initialization Handshake

```
registerServer (stdio):
  fork() → execvp()
  → write initialize {"jsonrpc":"2.0","method":"initialize","params":{"protocolVersion":"2024-11-05",...}}
  ← read response (poll up to 5s, non-blocking + usleep 10ms)
  → write notifications/initialized
  → StdioClient ready
```

### Tool Invocation Chain

```
LLM → tool_calls
  → AIHelper::chatStream
    → AIToolRegistry::invoke(name, args)
      → McpClientManager::callTool(name, args)
        → StdioClient::callTool → JSON-RPC tools/call over pipe
          → Python FastMCP process → wttr.in HTTP API
        ← {"result":{"content":[{"text":"{...}"}]}}
```

### Hot-Plug Support

- `McpClientManager::discoverAllTools()` triggers `reloadFromConfig()` before each call
- `reloadFromConfig()` diffs old/new server names, adds/removes client processes
- `unregisterServer()` stops client, clears `toolToClient_` cache, erases `serverToClient_` entry

## Dependencies & Coupling Boundaries

### Depends On

| Dependency | Notes |
|-----------|-------|
| libcurl | HTTP client (LLM API + SSE transport) |
| OpenSSL | HTTPS |
| ONNX Runtime | Image recognition inference |
| OpenCV | Image preprocessing |
| SimpleAmqpClient | RabbitMQ client |
| nlohmann/json | JSON (via JsonUtil.h) |
| POSIX | pipe, fork, dup2, execvp, waitpid (stdio transport) |

### Depended By

- AIServerCore: references llm/, mcp/, audio/, vision/, common/ headers
- HttpServer: no dependency

### External Dependencies (Runtime)

- Python 3 + `mcp` + `requests` (for `mcp_servers/weather_server.py`)