#include "mcp/AIToolRegistry.h"

#include <fstream>

#include "Common/Logging/Logger.h"
#include "mcp/McpClientManager.h"

// ─── Singleton ──────────────────────────────────────────────────
AIToolRegistry& AIToolRegistry::instance()
{
    static AIToolRegistry inst;
    return inst;
}

// ─── Config-driven loading ──────────────────────────────────────
void AIToolRegistry::loadFromConfig(const std::string& configPath)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!mcpManager_)
    {
        SPDLOG_WARN_TAG("MCP") << "[AIToolRegistry] McpClientManager not injected, skipping loadFromConfig";
        return;
    }

    std::ifstream file(configPath);
    if (!file.is_open())
    {
        SPDLOG_WARN_TAG("MCP") << "[AIToolRegistry] Cannot open config: " << configPath;
        return;
    }

    json config;
    file >> config;

    if (!config.contains("mcpServers"))
    {
        SPDLOG_WARN_TAG("MCP") << "[AIToolRegistry] No 'mcpServers' in config";
        return;
    }

    // 直接委托给 McpClientManager 注册所有 server
    for (auto& [name, serverDef] : config["mcpServers"].items())
    {
        mcpManager_->registerServer(name, serverDef);
    }

    SPDLOG_INFO_TAG("MCP") << "[AIToolRegistry] Registered servers from config";
}

// ─── invoke ─────────────────────────────────────────────────────
json AIToolRegistry::invoke(const std::string& name, const json& args) const
{
    if (!mcpManager_) throw std::runtime_error("McpClientManager not injected");

    SPDLOG_INFO_TAG("MCP") << "[AIToolRegistry] Tool '" << name << "' → McpClientManager";
    return mcpManager_->callTool(name, args);
}

// ─── getToolsSchema ─────────────────────────────────────────────
json AIToolRegistry::getToolsSchema() const
{
    if (!mcpManager_) return json::array();

    return mcpManager_->discoverAllTools();
}

// ─── setMcpClientManager ────────────────────────────────────────
void AIToolRegistry::setMcpClientManager(McpClientManager* mgr)
{
    std::lock_guard<std::mutex> lock(mutex_);
    mcpManager_ = mgr;
}
