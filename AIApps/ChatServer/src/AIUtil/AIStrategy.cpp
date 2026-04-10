#include"../include/AIUtil/AIStrategy.h"
#include"../include/AIUtil/AIFactory.h"

std::string AliyunStrategy::getApiUrl() const {
    return "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
}

std::string AliyunStrategy::getApiKey()const {
    return apiKey_;
}


std::string AliyunStrategy::getModel() const {
    return "qwen-plus";
}


json AliyunStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    json payload;
    payload["model"] = getModel();
    json msgArray = json::array();

    for (size_t i = 0; i < messages.size(); ++i) {
        json msg;
        if (i % 2 == 0) {
            msg["role"] = "user";
        }
        else {
            msg["role"] = "assistant";
        }
        msg["content"] = messages[i].first;
        msgArray.push_back(msg);
    }
    payload["messages"] = msgArray;
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


std::string DouBaoStrategy::getApiUrl()const {
    return "https://ark.cn-beijing.volces.com/api/v3/chat/completions";
}

std::string DouBaoStrategy::getApiKey()const {
    return apiKey_;
}


std::string DouBaoStrategy::getModel() const {
    if (endpointId_.empty()) {
        // 未配置 Endpoint ID 时使用默认模型名（可能不支持）
        return "doubao-lite-4k";
    }
    return endpointId_;  // 火山方舟需要 ep-xxxxx 格式
}


json DouBaoStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    json payload;
    payload["model"] = getModel();
    json msgArray = json::array();

    for (size_t i = 0; i < messages.size(); ++i) {
        json msg;
        if (i % 2 == 0) {
            msg["role"] = "user";
        }
        else {
            msg["role"] = "assistant";
        }
        msg["content"] = messages[i].first;
        msgArray.push_back(msg);
    }
    payload["messages"] = msgArray;
    return payload;
}


std::string DouBaoStrategy::parseResponse(const json& response) const {
    if (response.contains("choices") && !response["choices"].empty()) {
        const auto& msg = response["choices"][0]["message"];
        // thinking 模型（doubao-seed-*-thinking-*）的 content 可能为 null
        // 真正的回答在 reasoning_content 或 content（两者都可能出现）
        if (msg.contains("content") && !msg["content"].is_null()) {
            return msg["content"].get<std::string>();
        }
        // 兜底：尝试 reasoning_content
        if (msg.contains("reasoning_content") && msg["reasoning_content"].is_string()) {
            return msg["reasoning_content"].get<std::string>();
        }
    }
    return {};
}


std::string AliyunRAGStrategy::getApiUrl() const {
    if (ragId_.empty()) throw std::runtime_error("百炼 RAG 知识库 ID 未配置，请在个人中心填写知识库 ID");
    return "https://dashscope.aliyuncs.com/api/v1/apps/" + ragId_ + "/completion";
}

std::string AliyunRAGStrategy::getApiKey()const {
    return apiKey_;
}


std::string AliyunRAGStrategy::getModel() const {
    return ""; //Ҫģ
}


json AliyunRAGStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    json payload;
    json msgArray = json::array();
    for (size_t i = 0; i < messages.size(); ++i) {
        json msg;
        // messages 里 pair.second 存的是时间戳，role 信息已通过奇偶约定传入
        // 但更规范地，偶数下标 = user，奇数下标 = assistant
        msg["role"] = (i % 2 == 0 ? "user" : "assistant");
        msg["content"] = messages[i].first;
        msgArray.push_back(msg);
    }
    payload["input"]["messages"] = msgArray;
    payload["parameters"] = json::object(); 
    return payload;
}


std::string AliyunRAGStrategy::parseResponse(const json& response) const {
    // 正常响应
    if (response.contains("output") && response["output"].contains("text")) {
        return response["output"]["text"];
    }
    // API 错误（如 InvalidApiKey、App not found 等）
    if (response.contains("message")) {
        return "[RAG 错误] " + response["message"].get<std::string>();
    }
    if (response.contains("code")) {
        return "[RAG 错误] code=" + response["code"].get<std::string>();
    }
    return {};
}



std::string AliyunMcpStrategy::getApiUrl() const {
    return "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
}

std::string AliyunMcpStrategy::getApiKey()const {
    return apiKey_;
}


std::string AliyunMcpStrategy::getModel() const {
    return "qwen-plus";
}


json AliyunMcpStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    json payload;
    payload["model"] = getModel();
    json msgArray = json::array();

    for (size_t i = 0; i < messages.size(); ++i) {
        json msg;
        if (i % 2 == 0) {
            msg["role"] = "user";
        }
        else {
            msg["role"] = "assistant";
        }
        msg["content"] = messages[i].first;
        msgArray.push_back(msg);
    }
    payload["messages"] = msgArray;
    return payload;
}


std::string AliyunMcpStrategy::parseResponse(const json& response) const {
    if (response.contains("choices") && !response["choices"].empty()) {
        return response["choices"][0]["message"]["content"];
    }
    return {};
}


static StrategyRegister<AliyunStrategy> regAliyun("1");
static StrategyRegister<DouBaoStrategy> regDoubao("2");
static StrategyRegister<AliyunRAGStrategy> regAliyunRag("3");
static StrategyRegister<AliyunMcpStrategy> regAliyunMcp("4");