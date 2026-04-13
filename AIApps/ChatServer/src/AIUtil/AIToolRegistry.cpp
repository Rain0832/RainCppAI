#include "../include/AIUtil/AIToolRegistry.h"
#include <sstream>


AIToolRegistry::AIToolRegistry() {
    registerTool("get_weather", getWeather);
    registerTool("get_time", getTime);
}


void AIToolRegistry::registerTool(const std::string& name, ToolFunc func) {
    tools_[name] = func;
}


json AIToolRegistry::invoke(const std::string& name, const json& args) const {
    auto it = tools_.find(name);
    if (it == tools_.end()) {
        throw std::runtime_error("Tool not found: " + name);
    }
    return it->second(args);
}


bool AIToolRegistry::hasTool(const std::string& name) const {
    return tools_.count(name) > 0;
}

json AIToolRegistry::getToolsSchema() const {
    json tools = json::array();
    // get_weather
    tools.push_back({
        {"type", "function"},
        {"function", {
            {"name", "get_weather"},
            {"description", "获取指定城市的实时天气信息"},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"city", {{"type", "string"}, {"description", "城市名称，如北京、上海、广州"}}}
                }},
                {"required", json::array({"city"})}
            }}
        }}
    });
    // get_time
    tools.push_back({
        {"type", "function"},
        {"function", {
            {"name", "get_time"},
            {"description", "获取当前服务器时间"},
            {"parameters", {
                {"type", "object"},
                {"properties", json::object()},
                {"required", json::array()}
            }}
        }}
    });
    return tools;
}


size_t AIToolRegistry::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}


json AIToolRegistry::getWeather(const json& args) {
    if (!args.contains("city")) {
        return json{ {"error", "Missing parameter: city"} };
    }

    std::string city = args["city"].get<std::string>();

    CURL* curl = curl_easy_init();
    if (!curl) {
        return json{ {"error", "Failed to init CURL"} };
    }

    // 使用 curl handle 进行 URL 编码（curl_easy_escape 第一个参数需要有效 handle）
    char* encoded = curl_easy_escape(curl, city.c_str(), static_cast<int>(city.length()));
    std::string encodedCity;
    if (encoded) {
        encodedCity = encoded;
        curl_free(encoded);
    } else {
        curl_easy_cleanup(curl);
        return json{ {"error", "URL encode failed"} };
    }

    // wttr.in 免费 API，国内可能超时（已设 8s 超时 + fallback）
    // 如需更可靠，可替换为和风天气等国内 API
    std::string url = "https://wttr.in/" + encodedCity + "?format=%l:+%C+%t+%w&lang=zh";

    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);        // 增加超时到8秒
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "curl/7.88.1");  // 设置 UA 避免被拒

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        // 网络失败时返回模拟数据（让 AI 至少能回答）
        return json{ {"city", city}, {"weather", "当前网络无法获取实时天气，建议用户查看手机天气App"}, {"source", "fallback"} };
    }

    return json{ {"city", city}, {"weather", response}, {"source", "wttr.in"} };
}


json AIToolRegistry::getTime(const json& args) {
    (void)args;
    std::time_t t = std::time(nullptr);
    std::tm* now = std::localtime(&t);
    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", now);
    return json{ {"time", buffer} };
}
