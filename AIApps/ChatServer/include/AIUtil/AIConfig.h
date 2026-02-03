#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <regex>
#include <fstream>
#include <sstream>
#include <iostream>
#include "../../../../HttpServer/include/utils/JsonUtil.h"

struct AITool
{
    std::string name;
    std::unordered_map<std::string, std::string> params;
    std::string desc;
};

struct AIToolCall
{
    std::string toolName;
    json args;
    bool isToolCall = false;
};

class AIConfig
{
public:
    /**
     * @brief 从配置文件加载提示模板与工具列表。
     * @param path 配置文件路径。
     * @return 是否加载成功。
     */
    bool loadFromFile(const std::string &path);

    /**
     * @brief 根据用户输入构建最终提示词。
     * @param userInput 用户输入内容。
     * @return 组合后的提示词文本。
     */
    std::string buildPrompt(const std::string &userInput) const;

    /**
     * @brief 解析模型响应并识别是否为工具调用。
     * @param response 模型返回的原始文本。
     * @return 解析后的工具调用信息。
     */
    AIToolCall parseAIResponse(const std::string &response) const;

    /**
     * @brief 构造工具调用结果的二次提示词。
     * @param userInput 用户输入内容。
     * @param toolName 工具名称。
     * @param toolArgs 工具调用参数。
     * @param toolResult 工具返回结果。
     * @return 组合后的提示词文本。
     */
    std::string buildToolResultPrompt(const std::string &userInput, const std::string &toolName, const json &toolArgs, const json &toolResult) const;

private:
    std::string promptTemplate_;
    std::vector<AITool> tools_;

    /**
     * @brief 构造工具列表的展示文本。
     * @return 工具列表字符串。
     */
    std::string buildToolList() const;
};
