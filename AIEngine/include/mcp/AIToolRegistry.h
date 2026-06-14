#pragma once

#include <mutex>
#include <string>

#include "3rdparty/JsonUtil.h"

/**
 * @brief 纯血 MCP 工具注册表 — 薄层代理
 *
 * v2.0.6 起，所有工具调用均通过 McpClientManager 路由到外部 MCP Server。
 * AIToolRegistry 仅作为 McpClientManager 的薄层代理，不再持有任何 C++ 工具实现。
 */
class AIToolRegistry
{
public:
    /// 进程级单例
    static AIToolRegistry& instance();

    /// 从 mcp_config.json 加载 mcpServers，委托给 McpClientManager
    void loadFromConfig(const std::string& configPath);

    /// 调用工具 → 直接转发 McpClientManager
    json invoke(const std::string& name, const json& args) const;

    /**
     * @brief 返回 OpenAI 兼容的 tools[] 数组（用于 Function Calling）
     *
     * 直接委托 McpClientManager::discoverAllTools()，零本地缓存。
     */
    json getToolsSchema() const;

    /// 设置 McpClientManager 引用
    void setMcpClientManager(class McpClientManager* mgr);

private:
    AIToolRegistry() = default;
    AIToolRegistry(const AIToolRegistry&) = delete;
    AIToolRegistry& operator=(const AIToolRegistry&) = delete;

    mutable std::mutex mutex_;
    class McpClientManager* mcpManager_ = nullptr;
};