#include "mcp/AIToolRegistry.h"

#include <JsonUtil.h>
#include <muduo/base/Logging.h>

#include <fstream>
#include <sstream>

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

    std::ifstream file(configPath);
    if (!file.is_open())
    {
        std::cerr << "[AIToolRegistry] Cannot open config: " << configPath << std::endl;
        return;
    }

    json config;
    file >> config;

    // 旧格式兼容：顶层 "tools" 数组 → 自动包装为 builtin server
    json servers;
    if (config.contains("mcpServers"))
    {
        servers = config["mcpServers"];
    }
    else if (config.contains("tools") && config["tools"].is_array())
    {
        servers["auto_builtin"]["transport"] = "builtin";
        servers["auto_builtin"]["tools"] = config["tools"];
    }
    else
    {
        std::cerr << "[AIToolRegistry] No valid 'mcpServers' or 'tools' in config" << std::endl;
        return;
    }

    for (auto& [serverName, serverDef] : servers.items())
    {
        std::string transport = serverDef.value("transport", "");
        if (!serverDef.contains("tools") || !serverDef["tools"].is_array())
            continue;

        if (transport == "builtin")
        {
            // 内置工具：从 name→function 映射表注册
            static const std::unordered_map<std::string, ToolFunc> builtinMap = {
                    {"get_weather", getWeather},
                    {"get_time", getTime},
            };

            for (auto& toolDef : serverDef["tools"])
            {
                std::string name = toolDef.value("name", "");
                if (name.empty())
                    continue;

                auto it = builtinMap.find(name);
                if (it != builtinMap.end())
                    tools_[name] = it->second;

                json schema;
                schema["type"] = "function";
                schema["function"]["name"] = name;
                schema["function"]["description"] = toolDef.value("description", "");
                schema["function"]["parameters"] = toolDef.value("parameters", json::object());
                toolSchemas_.push_back(std::move(schema));
            }
        }
        else if (transport == "stdio" || transport == "sse")
        {
            // 委托给 McpClientManager（Phase 3 实现）
            // McpClientManager::instance().registerServer(serverName, serverDef);
            LOG_INFO << "[AIToolRegistry] Deferred server '" << serverName << "' (transport=" << transport
                     << "), McpClientManager not yet loaded";
        }
        else
        {
            std::cerr << "[AIToolRegistry] Unknown transport '" << transport << "' for server '" << serverName << "'"
                      << std::endl;
        }
    }

    std::cout << "[AIToolRegistry] Loaded " << toolSchemas_.size() << " tools from config" << std::endl;
}

// ─── Tool registration ──────────────────────────────────────────
void AIToolRegistry::registerTool(const std::string& name, ToolFunc func)
{
    std::lock_guard<std::mutex> lock(mutex_);
    tools_[name] = std::move(func);
}

json AIToolRegistry::invoke(const std::string& name, const json& args) const
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tools_.find(name);
        if (it != tools_.end())
        {
            LOG_INFO << "[AIToolRegistry] Tool '" << name << "' dispatched to [local]";
            return it->second(args);
        }
    }

    // 本地未命中 → 降级到 McpClientManager
    if (mcpManager_)
    {
        LOG_INFO << "[AIToolRegistry] Tool '" << name << "' dispatched to [McpClientManager]";
        return mcpManager_->callTool(name, args);
    }

    throw std::runtime_error("Tool not found: " + name);
}

bool AIToolRegistry::hasTool(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return tools_.count(name) > 0;
}

json AIToolRegistry::getToolsSchema() const
{
    json tools = json::array();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& s : toolSchemas_)
            tools.push_back(s);
    }

    // 融合 McpClientManager 远端工具 schema
    if (mcpManager_)
    {
        json remote = mcpManager_->discoverAllTools();
        for (auto& s : remote)
            tools.push_back(std::move(s));
    }

    return tools;
}

void AIToolRegistry::setMcpClientManager(McpClientManager* mgr)
{
    std::lock_guard<std::mutex> lock(mutex_);
    mcpManager_ = mgr;
}

// ─── curl helper ────────────────────────────────────────────────
size_t AIToolRegistry::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output)
{
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

// ─── Tool implementations ───────────────────────────────────────
json AIToolRegistry::getWeather(const json& args)
{
    if (!args.contains("city"))
    {
        return json {{"error", "Missing parameter: city"}};
    }

    std::string city = args["city"].get<std::string>();

    CURL* curl = curl_easy_init();
    if (!curl)
    {
        return json {{"error", "Failed to init CURL"}};
    }

    char* encoded = curl_easy_escape(curl, city.c_str(), static_cast<int>(city.length()));
    std::string encodedCity;
    if (encoded)
    {
        encodedCity = encoded;
        curl_free(encoded);
    }
    else
    {
        curl_easy_cleanup(curl);
        return json {{"error", "URL encode failed"}};
    }

    std::string url = "https://wttr.in/" + encodedCity + "?format=%l:+%C+%t+%w&lang=zh";

    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "curl/7.88.1");

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        return json {{"city", city},
                     {"weather", "当前网络无法获取实时天气，建议用户查看手机天气App"},
                     {"source", "fallback"}};
    }

    return json {{"city", city}, {"weather", response}, {"source", "wttr.in"}};
}

json AIToolRegistry::getTime(const json& args)
{
    (void)args;
    std::time_t t = std::time(nullptr);
    std::tm* now = std::localtime(&t);
    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", now);
    return json {{"time", buffer}};
}