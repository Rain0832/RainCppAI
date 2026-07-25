#include "Common/Config/ConfigManager.h"

#include <cstdlib>
#include <fstream>
#include <iostream>

namespace common
{

ConfigManager& ConfigManager::instance()
{
    static ConfigManager mgr;
    return mgr;
}

void ConfigManager::load(const std::string& path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "[ConfigManager] Cannot open config file: " << path
                  << ", using defaults" << std::endl;
        config_ = json::object();
        return;
    }
    file >> config_;
    // Config loaded successfully (no output needed at startup)
}

std::string ConfigManager::get(const std::string& path, const std::string& defaultVal) const
{
    // 1) 环境变量优先
    std::string envKey = toEnvKey(path);
    const char* envVal = std::getenv(envKey.c_str());
    if (envVal && envVal[0] != '\0')
        return std::string(envVal);

    // 2) JSON 兜底
    std::lock_guard<std::mutex> lock(mutex_);
    json node = resolvePath(path);
    if (node.is_string())
        return node.get<std::string>();
    if (node.is_number())
        return std::to_string(node.get<int>());
    return defaultVal;
}

int ConfigManager::getInt(const std::string& path, int defaultVal) const
{
    std::string val = get(path, "");
    if (val.empty())
        return defaultVal;
    try { return std::stoi(val); }
    catch (...) { return defaultVal; }
}

json ConfigManager::resolvePath(const std::string& path) const
{
    json node = config_;
    if (path.empty() || node.is_null())
        return node;

    size_t start = 0;
    size_t dot = path.find('.');
    while (dot != std::string::npos)
    {
        std::string key = path.substr(start, dot - start);
        if (!node.contains(key))
            return json();
        node = node[key];
        start = dot + 1;
        dot = path.find('.', start);
    }
    std::string lastKey = path.substr(start);
    if (!node.contains(lastKey))
        return json();
    return node[lastKey];
}

std::string ConfigManager::toEnvKey(const std::string& path)
{
    std::string result;
    for (char c : path)
    {
        if (c == '.')
            result += '_';
        else
            result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return result;
}

}  // namespace common
