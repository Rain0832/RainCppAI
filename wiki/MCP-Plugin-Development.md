# MCP 插件开发指南

## 概述

Dr.Rain 使用 [Model Context Protocol (MCP)](https://modelcontextprotocol.io/) 实现 LLM 的 Function Calling 能力。当前支持 **stdio** 和 **SSE** 两种传输方式。

## 架构

```
User Message
  ↓
AIEngine::AIHelper::chat()
  ↓
AIEngine::McpClientManager
  ├─► stdio transport → Python MCP server process (subprocess)
  └─► sse transport   → Remote MCP server (HTTP SSE)
  ↓
AIEngine::AIToolRegistry → tool list → LLM decides to call
  ↓
LLM response + tool result → final answer to user
```

## 快速开始：创建一个天气插件

### 1. Python 环境

```bash
cd RainCppAI/mcp_servers
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

### 2. 编写 MCP Server

参考 [`mcp_servers/weather_server.py`](https://github.com/Rain0832/RainCppAI/blob/main/mcp_servers/weather_server.py)：

```python
from mcp.server.fastmcp import FastMCP

mcp = FastMCP("my_tool")

@mcp.tool()
def my_function(param: str) -> dict:
    """工具描述（LLM 会读取此 docstring 来决定是否调用）"""
    return {"result": f"processed {param}"}

if __name__ == "__main__":
    mcp.run(transport="stdio")
```

### 3. 注册到 MCP 配置

编辑 `mcp_config.json`：

```json
{
  "mcpServers": {
    "my_tool": {
      "command": "python3",
      "args": ["mcp_servers/my_tool.py"],
      "env": {
        "API_KEY": "xxx"
      }
    }
  }
}
```

### 4. 验证

```bash
# 启动 C++ 后端，检查日志
./http_server -p 8088

# 应看到：
# [MCP] registering server 'my_tool'
# [MCP] discoverAllTools: N remote tools
```

## 配置规范

`mcp_config.json` 完整 schema：

```json
{
  "mcpServers": {
    "<server_name>": {
      "command": "<executable>",
      "args": ["<arg1>", "<arg2>"],
      "env": {
        "<KEY>": "<value>"
      }
    }
  }
}
```

| 字段 | 必填 | 说明 |
|------|------|------|
| `command` | ✅ | 可执行文件（`python3`、`node` 等） |
| `args` | ✅ | 命令行参数（脚本路径 + 参数） |
| `env` | ❌ | 环境变量（API Key 等敏感信息） |

## C++ 端新增 MCP Server 类型

如需在 C++ 端新增传输协议或 Server 类型，涉及以下文件：

| 文件 | 作用 |
|------|------|
| `AIEngine/include/mcp/McpServer.h` | Server 抽象基类 |
| `AIEngine/include/mcp/McpClientManager.h` | Client 管理器（进线程管理） |
| `AIEngine/include/mcp/AIToolRegistry.h` | 工具注册表（discover → register） |
| `AIEngine/src/mcp/` | 具体实现 |

## 依赖版本兼容性

| 依赖 | 版本 | 注意事项 |
|------|------|---------|
| `mcp` (Python) | `< 2.0.0` | v2.0.0+ API 有 Breaking Change，当前锁定 `<2.0` |
| `fastmcp` | ≥ 0.4.0 | 需 `httpx` 和 `sse-starlette` |
| Python | ≥ 3.10 | 生产环境建议 3.11+ |
