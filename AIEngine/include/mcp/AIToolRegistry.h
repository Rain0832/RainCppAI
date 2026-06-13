#pragma once
#include <curl/curl.h>

#include <ctime>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "3rdparty/JsonUtil.h"

class AIToolRegistry
{
public:
    using ToolFunc = std::function<json(const json&)>;

    AIToolRegistry();

    void registerTool(const std::string& name, ToolFunc func);
    json invoke(const std::string& name, const json& args) const;
    bool hasTool(const std::string& name) const;

    /**
     * @brief 返回 OpenAI 兼容的 tools[] 数组（用于 Function Calling）
     */
    json getToolsSchema() const;

private:
    std::unordered_map<std::string, ToolFunc> tools_;

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output);
    static json getWeather(const json& args);
    static json getTime(const json& args);
};
