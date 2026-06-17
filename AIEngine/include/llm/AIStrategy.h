#pragma once
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <muduo/base/Logging.h>

#include "3rdparty/JsonUtil.h"
#include "common/Message.h"

/// OpenAI tool_calls 中的单个调用信息
struct ToolCallInfo
{
    std::string id;   ///< tool_call_id，用于回传 role:"tool" 消息
    std::string name; ///< function.name
    json arguments;   ///< function.arguments (已解析的 JSON 对象)
};

class AIStrategy
{
public:
    virtual ~AIStrategy() = default;

    virtual std::string getApiUrl() const = 0;
    virtual std::string getApiKey() const = 0;
    virtual void setApiKey(const std::string &key) = 0;

    // RAG 知识库 ID（仅 AliyunRAGStrategy 使用）
    virtual void setRagId(const std::string &id) { (void)id; }
    virtual std::string getRagId() const { return ""; }

    // 豆包端点 ID
    virtual void setEndpointId(const std::string &id) { (void)id; }

    virtual std::string getModel() const = 0;

    // buildRequest: Message 向量 + 可选的 tools JSON
    virtual json buildRequest(const std::vector<Message> &messages, const json &tools = json::object()) const = 0;

    /// 从 LLM 响应中提取文本内容（兼容现有代码）
    virtual std::string parseResponse(const json &response) const = 0;

    /// 从 LLM 响应中提取 tool_calls（OpenAI 原生 Function Calling）
    /// @return 工具调用列表，无调用时返回空 vector
    virtual std::vector<ToolCallInfo> parseToolCalls(const json &response) const = 0;
};

class AliyunStrategy : public AIStrategy
{
public:
    AliyunStrategy()
    {
        const char *key = std::getenv("DASHSCOPE_API_KEY");
        if (key)
            api_key_ = key;
    }

    std::string getApiUrl() const override;
    std::string getApiKey() const override;
    void setApiKey(const std::string &key) override { api_key_ = key; }
    std::string getModel() const override;

    json buildRequest(const std::vector<Message> &messages, const json &tools = json::object()) const override;
    std::string parseResponse(const json &response) const override;
    std::vector<ToolCallInfo> parseToolCalls(const json &response) const override;

private:
    std::string api_key_;
};

class DouBaoStrategy : public AIStrategy
{
public:
    DouBaoStrategy()
    {
        const char *key = std::getenv("DOUBAO_API_KEY");
        if (key)
            api_key_ = key;
    }

    std::string getApiUrl() const override;
    std::string getApiKey() const override;
    void setApiKey(const std::string &key) override { api_key_ = key; }
    void setEndpointId(const std::string &id) override
    {
        LOG_INFO << "[setEndpointId]: " << id;
        endpoint_id_ = id;
    }
    std::string getModel() const override;

    json buildRequest(const std::vector<Message> &messages, const json &tools = json::object()) const override;
    std::string parseResponse(const json &response) const override;
    std::vector<ToolCallInfo> parseToolCalls(const json &response) const override;

private:
    std::string api_key_;
    std::string endpoint_id_;
};

class AliyunRAGStrategy : public AIStrategy
{
public:
    AliyunRAGStrategy()
    {
        const char *key = std::getenv("DASHSCOPE_API_KEY");
        if (key)
            api_key_ = key;
        const char *rid = std::getenv("Knowledge_Base_ID");
        if (rid)
            rag_id_ = rid;
    }

    std::string getApiUrl() const override;
    std::string getApiKey() const override;
    void setApiKey(const std::string &key) override { api_key_ = key; }
    void setRagId(const std::string &id) override { rag_id_ = id; }
    std::string getRagId() const override { return rag_id_; }
    std::string getModel() const override;

    json buildRequest(const std::vector<Message> &messages, const json &tools = json::object()) const override;
    std::string parseResponse(const json &response) const override;
    std::vector<ToolCallInfo> parseToolCalls(const json &response) const override;

private:
    std::string api_key_;
    std::string rag_id_;
};
