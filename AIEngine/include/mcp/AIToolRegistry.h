#pragma once
#include <curl/curl.h>

#include <ctime>
#include <functional>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "3rdparty/JsonUtil.h"

class AIToolRegistry
{
public:
    using ToolFunc = std::function<json(const json&)>;

    /// 进程级单例
    static AIToolRegistry& instance();

    /// 从 mcp_config.json 加载工具定义并自动注册内置实现
    void loadFromConfig(const std::string& configPath);

    /// 手动注册工具（供 McpServer / 动态加载使用）
    void registerTool(const std::string& name, ToolFunc func);

    /// 调用工具：name → func(args)
    json invoke(const std::string& name, const json& args) const;

    bool hasTool(const std::string& name) const;

    /**
     * @brief 返回 OpenAI 兼容的 tools[] 数组（用于 Function Calling）
     */
    json getToolsSchema() const;

private:
    AIToolRegistry() = default;
    AIToolRegistry(const AIToolRegistry&) = delete;
    AIToolRegistry& operator=(const AIToolRegistry&) = delete;

    std::unordered_map<std::string, ToolFunc> tools_;
    mutable std::mutex mutex_;

    /// 工具参数 schema（从 config 读取，供 getToolsSchema 用）
    std::vector<json> toolSchemas_;

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output);
    static json getWeather(const json& args);
    static json getTime(const json& args);
};