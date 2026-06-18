#include "llm/AIStrategy.h"

#include "common/Message.h"
#include "llm/AIFactory.h"

// ─── Helper: Message 向量转 OpenAI messages 格式 ────────────────
static json messagesToJsonArray(const std::vector<Message> &messages)
{
    json arr = json::array();
    for (const auto &m : messages)
    {
        json msg;
        msg["role"] = m.role;
        // role="tool" 的消息内容是 tool 执行结果
        if (m.role == "tool")
        {
            msg["content"] = m.content;
            msg["tool_call_id"] = m.tool_call_id;
        }
        // role="assistant" 且携带 tool_call_id 表示这是一条带 tool_calls 的助理回复
        else if (m.role == "assistant" && !m.tool_call_id.empty())
        {
            msg["content"] = nullptr;                   // OpenAI 要求 content=null
            msg["tool_calls"] = json::parse(m.content); // 存储为 JSON 数组
        }
        else
        {
            msg["content"] = m.content;
        }
        arr.push_back(msg);
    }
    return arr;
}

// ═══════════════════════════════════════════════════════════════
// AliyunStrategy
// ═══════════════════════════════════════════════════════════════

std::string AliyunStrategy::getApiUrl() const
{
    return "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
}
std::string AliyunStrategy::getApiKey() const
{
    return api_key_;
}
std::string AliyunStrategy::getModel() const
{
    return "qwen-plus";
}

json AliyunStrategy::buildRequest(const std::vector<Message> &messages, const json &tools,
                                  const std::string &modelName) const
{
    json payload;
    payload["model"] = getModel();
    payload["messages"] = messagesToJsonArray(messages);
    if (!tools.empty())
    {
        payload["tools"] = tools;
    }
    return payload;
}

std::string AliyunStrategy::parseResponse(const json &response) const
{
    if (response.contains("choices") && !response["choices"].empty())
    {
        const auto &msg = response["choices"][0]["message"];
        if (msg.contains("content") && !msg["content"].is_null())
            return msg["content"].get<std::string>();
    }
    if (response.contains("message"))
        return "[API 错误] " + response["message"].get<std::string>();
    return {};
}

std::vector<ToolCallInfo> AliyunStrategy::parseToolCalls(const json &response) const
{
    std::vector<ToolCallInfo> result;
    if (!response.contains("choices") || response["choices"].empty())
        return result;
    const auto &msg = response["choices"][0]["message"];
    if (!msg.contains("tool_calls") || !msg["tool_calls"].is_array())
        return result;
    for (const auto &tc : msg["tool_calls"])
    {
        ToolCallInfo info;
        info.id = tc.value("id", "");
        if (tc.contains("function"))
        {
            info.name = tc["function"].value("name", "");
            std::string argsStr = tc["function"].value("arguments", "{}");
            try
            {
                info.arguments = json::parse(argsStr);
            }
            catch (...)
            {
                info.arguments = json::object();
            }
        }
        if (!info.name.empty())
            result.push_back(std::move(info));
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════
// DouBaoStrategy
// ═══════════════════════════════════════════════════════════════

std::string DouBaoStrategy::getApiUrl() const
{
    return "https://ark.cn-beijing.volces.com/api/v3/chat/completions";
}
std::string DouBaoStrategy::getApiKey() const
{
    return api_key_;
}

std::string DouBaoStrategy::getModel() const
{
    return "doubao-lite-4k";
}

json DouBaoStrategy::buildRequest(const std::vector<Message> &messages, const json &tools,
                                  const std::string &modelName) const
{
    json payload;
    payload["model"] = modelName.empty() ? getModel() : modelName;
    payload["messages"] = messagesToJsonArray(messages);
    if (!tools.empty())
    {
        payload["tools"] = tools;
    }
    return payload;
}

std::string DouBaoStrategy::parseResponse(const json &response) const
{
    if (response.contains("choices") && !response["choices"].empty())
    {
        const auto &msg = response["choices"][0]["message"];
        if (msg.contains("content") && !msg["content"].is_null())
            return msg["content"].get<std::string>();
        if (msg.contains("reasoning_content") && msg["reasoning_content"].is_string())
            return msg["reasoning_content"].get<std::string>();
    }
    if (response.contains("message"))
        return "[API 错误] " + response["message"].get<std::string>();
    return {};
}

std::vector<ToolCallInfo> DouBaoStrategy::parseToolCalls(const json &response) const
{
    std::vector<ToolCallInfo> result;
    if (!response.contains("choices") || response["choices"].empty())
        return result;
    const auto &msg = response["choices"][0]["message"];
    if (!msg.contains("tool_calls") || !msg["tool_calls"].is_array())
        return result;
    for (const auto &tc : msg["tool_calls"])
    {
        ToolCallInfo info;
        info.id = tc.value("id", "");
        if (tc.contains("function"))
        {
            info.name = tc["function"].value("name", "");
            std::string argsStr = tc["function"].value("arguments", "{}");
            try
            {
                info.arguments = json::parse(argsStr);
            }
            catch (...)
            {
                info.arguments = json::object();
            }
        }
        if (!info.name.empty())
            result.push_back(std::move(info));
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════
// AliyunRAGStrategy
// ═══════════════════════════════════════════════════════════════

std::string AliyunRAGStrategy::getApiUrl() const
{
    if (rag_id_.empty())
        throw std::runtime_error("百炼 RAG 知识库 ID 未配置，请在个人中心填写");
    return "https://dashscope.aliyuncs.com/api/v1/apps/" + rag_id_ + "/completion";
}
std::string AliyunRAGStrategy::getApiKey() const
{
    return api_key_;
}
std::string AliyunRAGStrategy::getModel() const
{
    return "";
}

json AliyunRAGStrategy::buildRequest(const std::vector<Message> &messages, const json &tools,
                                     const std::string &modelName) const
{
    json payload;
    payload["input"]["messages"] = messagesToJsonArray(messages);
    payload["parameters"] = json::object();
    return payload;
}

std::string AliyunRAGStrategy::parseResponse(const json &response) const
{
    if (response.contains("output") && response["output"].contains("text"))
        return response["output"]["text"];
    if (response.contains("message"))
        return "[RAG 错误] " + response["message"].get<std::string>();
    if (response.contains("code"))
        return "[RAG 错误] code=" + response["code"].get<std::string>();
    return {};
}

std::vector<ToolCallInfo> AliyunRAGStrategy::parseToolCalls(const json &response) const
{
    (void)response;
    return {}; // RAG 策略不支持 Function Calling
}

static StrategyRegister<AliyunStrategy> regAliyun("aliyun");
static StrategyRegister<DouBaoStrategy> regDoubao("volcengine");
static StrategyRegister<AliyunRAGStrategy> regAliyunRag("aliyun-rag");
static StrategyRegister<AliyunStrategy> regAliyunMcp("aliyun-mcp"); // 合并：MCP 也走 AliyunStrategy
