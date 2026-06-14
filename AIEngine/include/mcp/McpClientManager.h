#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "3rdparty/JsonUtil.h"

/**
 * @brief MCP Client 抽象基类
 *
 * McpStdioClient（stdio fork+pipe）和 McpSseClient（HTTP SSE）均派生自此类。
 */
class McpClient
{
public:
    virtual ~McpClient() = default;

    /// 启动连接（stdio: fork；sse: GET SSE）
    virtual bool start() = 0;

    /// 发送 JSON-RPC 2.0 请求，返回响应 JSON
    virtual json sendRequest(const json& request) = 0;

    /// 获取此 client 提供的工具 schema 列表（OpenAI Function Calling 格式）
    virtual json getTools() = 0;

    /// 调用指定工具并返回结果
    virtual json callTool(const std::string& name, const json& args) = 0;

    /// 停止连接
    virtual void stop() = 0;
};

/**
 * @brief McpClientManager — 管理多个 MCP Server 连接
 *
 * 单例，支持 stdio / sse 两种 transport，通过工厂创建对应 Client。
 * 提供 discoverAllTools() 和 callTool() 接口供 AIToolRegistry 路由。
 */
class McpClientManager
{
public:
    static McpClientManager& instance();

    /// 从 mcpServers 配置加载并启动所有 client
    void loadFromConfig(const std::string& configPath);

    /// 注册并启动单个 server
    void registerServer(const std::string& name, const json& serverDef);

    /// 获取所有远端工具的 OpenaAI tools schema（融合）
    json discoverAllTools();

    /// 按工具名路由到对应 client 执行
    json callTool(const std::string& name, const json& args);

private:
    McpClientManager() = default;
    McpClientManager(const McpClientManager&) = delete;
    McpClientManager& operator=(const McpClientManager&) = delete;

    std::mutex mutex_;
    std::vector<std::shared_ptr<McpClient>> clients_;

    /// tool name → client index 缓存（加速 callTool 查找）
    std::unordered_map<std::string, size_t> toolToClient_;
};