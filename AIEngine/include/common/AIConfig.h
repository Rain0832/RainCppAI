#pragma once
#include <fstream>
#include <iostream>
#include <string>

#include "3rdparty/JsonUtil.h"

/**
 * @brief 通用 JSON 配置文件加载器
 *
 * 废弃了旧的 prompt 模板 + 工具列表解析逻辑（Commit 4/5 已迁移到原生 Function Calling）。
 * 保留 loadFromFile() 供未来配置加载使用。
 */
class AIConfig
{
public:
    bool loadFromFile(const std::string& path);

    /// 直接访问解析后的 JSON
    const json& getJson() const { return json_; }

private:
    json json_;
};