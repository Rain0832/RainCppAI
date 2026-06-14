#include "mcp/AIToolRegistry.h"

#include <JsonUtil.h>

#include <fstream>
#include <sstream>

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

    if (!config.contains("tools") || !config["tools"].is_array())
    {
        std::cerr << "[AIToolRegistry] Missing 'tools' array in config" << std::endl;
        return;
    }

    for (auto& toolDef : config["tools"])
    {
        std::string name = toolDef.value("name", "");
        if (name.empty())
            continue;

        // 注册已知的内置实现
        if (name == "get_weather")
            tools_[name] = getWeather;
        else if (name == "get_time")
            tools_[name] = getTime;
        // else: 仅配置描述，实现需外部通过 registerTool() 注入

        // 保存 OpenAI Function Calling schema
        json schema;
        schema["type"] = "function";
        schema["function"]["name"] = name;
        schema["function"]["description"] = toolDef.value("description", "");
        schema["function"]["parameters"] = toolDef.value("parameters", json::object());
        toolSchemas_.push_back(std::move(schema));
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
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tools_.find(name);
    if (it == tools_.end())
    {
        throw std::runtime_error("Tool not found: " + name);
    }
    return it->second(args);
}

bool AIToolRegistry::hasTool(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return tools_.count(name) > 0;
}

json AIToolRegistry::getToolsSchema() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    json tools = json::array();
    for (auto& s : toolSchemas_)
        tools.push_back(s);
    return tools;
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