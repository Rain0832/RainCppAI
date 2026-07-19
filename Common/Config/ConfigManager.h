#pragma once

#include <mutex>
#include <string>

#include "3rdparty/JsonUtil.h"

namespace common
{

/**
 * @brief 统一配置管理器（单例）
 *
 * 加载 config.json，支持环境变量覆盖（DB_HOST → db.host）。
 * 路径用点号分隔："server.port" → 环境变量 SERVER_PORT。
 */
class ConfigManager
{
public:
    static ConfigManager& instance();

    /// 加载 JSON 配置文件
    void load(const std::string& path);

    /// 获取字符串值（环境变量优先）
    std::string get(const std::string& path, const std::string& defaultVal = "") const;

    /// 获取整数值（环境变量优先）
    int getInt(const std::string& path, int defaultVal = 0) const;

private:
    ConfigManager() = default;

    /// 遍历 JSON 路径 "a.b.c"
    json resolvePath(const std::string& path) const;

    /// "db.host" → "DB_HOST"
    static std::string toEnvKey(const std::string& path);

    json config_;
    mutable std::mutex mutex_;
};

}  // namespace common
