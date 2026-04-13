#include"../include/AIUtil/AIStrategy.h"
#include"../include/AIUtil/AIFactory.h"

// ─── Helper: Message 向量转 OpenAI messages 格式 ────────────────
static json messagesToJsonArray(const std::vector<Message>& messages) {
    json arr = json::array();
    for (const auto& m : messages) {
        json msg;
        msg["role"] = m.role;
        msg["content"] = m.content;
        arr.push_back(msg);
    }
    return arr;
}

// ═══════════════════════════════════════════════════════════════
// AliyunStrategy
// ═══════════════════════════════════════════════════════════════

std::string AliyunStrategy::getApiUrl() const {
    return "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
}
std::string AliyunStrategy::getApiKey() const { return apiKey_; }
std::string AliyunStrategy::getModel() const { return "qwen-plus"; }

json AliyunStrategy::buildRequest(const std::vector<Message>& messages, const json& tools) const {
    json payload;
    payload["model"] = getModel();
    payload["messages"] = messagesToJsonArray(messages);
    if (!tools.empty()) {
        payload["tools"] = tools;
    }
    return payload;
}

std::string AliyunStrategy::parseResponse(const json& response) const {
    if (response.contains("choices") && !response["choices"].empty()) {
        const auto& msg = response["choices"][0]["message"];
        if (msg.contains("content") && !msg["content"].is_null())
            return msg["content"].get<std::string>();
    }
    if (response.contains("message"))
        return "[API 错误] " + response["message"].get<std::string>();
    return {};
}

// ═══════════════════════════════════════════════════════════════
// DouBaoStrategy
// ═══════════════════════════════════════════════════════════════

std::string DouBaoStrategy::getApiUrl() const {
    return "https://ark.cn-beijing.volces.com/api/v3/chat/completions";
}
std::string DouBaoStrategy::getApiKey() const { return apiKey_; }
std::string DouBaoStrategy::getModel() const {
    return endpointId_.empty() ? "doubao-lite-4k" : endpointId_;
}

json DouBaoStrategy::buildRequest(const std::vector<Message>& messages, const json& tools) const {
    json payload;
    payload["model"] = getModel();
    payload["messages"] = messagesToJsonArray(messages);
    if (!tools.empty()) {
        payload["tools"] = tools;
    }
    return payload;
}

std::string DouBaoStrategy::parseResponse(const json& response) const {
    if (response.contains("choices") && !response["choices"].empty()) {
        const auto& msg = response["choices"][0]["message"];
        if (msg.contains("content") && !msg["content"].is_null())
            return msg["content"].get<std::string>();
        if (msg.contains("reasoning_content") && msg["reasoning_content"].is_string())
            return msg["reasoning_content"].get<std::string>();
    }
    if (response.contains("message"))
        return "[API 错误] " + response["message"].get<std::string>();
    return {};
}

// ═══════════════════════════════════════════════════════════════
// AliyunRAGStrategy
// ═══════════════════════════════════════════════════════════════

std::string AliyunRAGStrategy::getApiUrl() const {
    if (ragId_.empty()) throw std::runtime_error("百炼 RAG 知识库 ID 未配置，请在个人中心填写");
    return "https://dashscope.aliyuncs.com/api/v1/apps/" + ragId_ + "/completion";
}
std::string AliyunRAGStrategy::getApiKey() const { return apiKey_; }
std::string AliyunRAGStrategy::getModel() const { return ""; }

json AliyunRAGStrategy::buildRequest(const std::vector<Message>& messages, const json&) const {
    json payload;
    payload["input"]["messages"] = messagesToJsonArray(messages);
    payload["parameters"] = json::object();
    return payload;
}

std::string AliyunRAGStrategy::parseResponse(const json& response) const {
    if (response.contains("output") && response["output"].contains("text"))
        return response["output"]["text"];
    if (response.contains("message"))
        return "[RAG 错误] " + response["message"].get<std::string>();
    if (response.contains("code"))
        return "[RAG 错误] code=" + response["code"].get<std::string>();
    return {};
}

// ═══════════════════════════════════════════════════════════════
// AliyunMcpStrategy（现在使用原生 Function Calling）
// ═══════════════════════════════════════════════════════════════

std::string AliyunMcpStrategy::getApiUrl() const {
    return "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
}
std::string AliyunMcpStrategy::getApiKey() const { return apiKey_; }
std::string AliyunMcpStrategy::getModel() const { return "qwen-plus"; }

json AliyunMcpStrategy::buildRequest(const std::vector<Message>& messages, const json& tools) const {
    json payload;
    payload["model"] = getModel();
    payload["messages"] = messagesToJsonArray(messages);
    if (!tools.empty()) {
        payload["tools"] = tools;
    }
    return payload;
}

std::string AliyunMcpStrategy::parseResponse(const json& response) const {
    if (response.contains("choices") && !response["choices"].empty()) {
        const auto& msg = response["choices"][0]["message"];
        if (msg.contains("content") && !msg["content"].is_null())
            return msg["content"].get<std::string>();
    }
    if (response.contains("message"))
        return "[MCP 错误] " + response["message"].get<std::string>();
    return {};
}

static StrategyRegister<AliyunStrategy> regAliyun("1");
static StrategyRegister<DouBaoStrategy> regDoubao("2");
static StrategyRegister<AliyunRAGStrategy> regAliyunRag("3");
static StrategyRegister<AliyunMcpStrategy> regAliyunMcp("4");
