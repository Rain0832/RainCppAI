#pragma once
#include <functional>
#include <string>

#include "3rdparty/JsonUtil.h"
#include "mcp/AIToolRegistry.h"

/**
 * @brief 标准 MCP Server（JSON-RPC 2.0）
 *
 * 支持方法：
 *   - tools/list   — 返回所有已注册工具的描述
 *   - tools/call   — 调用指定工具并返回结果
 *
 * 复用 AIToolRegistry 单例，与 AIHelper 共享同一工具注册表。
 *
 * JSON-RPC 2.0 请求格式：
 *   {"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}
 *   {"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"get_weather","arguments":{"city":"广州"}}}
 *
 * JSON-RPC 2.0 响应格式：
 *   {"jsonrpc":"2.0","id":1,"result":{...}}
 *   {"jsonrpc":"2.0","id":1,"error":{"code":-32601,"message":"Method not found"}}
 */
class McpServer
{
public:
    McpServer();

    /**
     * @brief 处理一条 JSON-RPC 2.0 请求
     * @param requestBody 请求 JSON 字符串
     * @return 响应 JSON 字符串
     */
    std::string handleRequest(const std::string& requestBody);

private:
    json handleToolsList(const json& params);
    json handleToolsCall(const json& params);
    json buildError(int code, const std::string& message);
    json buildResult(const json& id, const json& result);

    /// 从 AIToolRegistry 单例同步 toolMetas_ 缓存
    void syncMetasFromRegistry();

private:
    /// 工具元信息（name → {description, inputSchema}）
    struct ToolMeta
    {
        std::string name;
        std::string description;
        json inputSchema;
    };
    std::vector<ToolMeta> toolMetas_;
};