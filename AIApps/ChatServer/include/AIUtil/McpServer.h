#pragma once
#include <string>
#include <functional>
#include "../../../HttpServer/include/utils/JsonUtil.h"
#include "AIToolRegistry.h"

/**
 * @brief 标准 MCP Server（JSON-RPC 2.0）
 *
 * 支持方法：
 *   - tools/list   — 返回所有已注册工具的描述
 *   - tools/call   — 调用指定工具并返回结果
 *
 * 复用现有 AIToolRegistry，无需重复实现工具逻辑。
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
    std::string handleRequest(const std::string &requestBody);

    /**
     * @brief 注册额外工具（工具名 + 描述 + 参数 schema + 执行函数）
     */
    void registerTool(const std::string &name,
                      const std::string &description,
                      const json &inputSchema,
                      AIToolRegistry::ToolFunc func);

private:
    json handleToolsList(const json &params);
    json handleToolsCall(const json &params);
    json buildError(int code, const std::string &message);
    json buildResult(const json &id, const json &result);

private:
    AIToolRegistry registry_;

    /// 工具元信息（name → {description, inputSchema}）
    struct ToolMeta {
        std::string name;
        std::string description;
        json        inputSchema;
    };
    std::vector<ToolMeta> toolMetas_;
};
