#pragma once
#include <string>
#include <vector>
#include <utility>
#include <iostream>
#include <sstream>
#include <memory>

#include "3rdparty/JsonUtil.h"
#include "common/Message.h"

class AIStrategy {
public:
    virtual ~AIStrategy() = default;

    virtual std::string getApiUrl() const = 0;
    virtual std::string getApiKey() const = 0;
    virtual void setApiKey(const std::string& key) = 0;

    // RAG 知识库 ID（仅 AliyunRAGStrategy 使用）
    virtual void setRagId(const std::string& id) { (void)id; }
    virtual std::string getRagId() const { return ""; }

    // 豆包端点 ID
    virtual void setEndpointId(const std::string& id) { (void)id; }

    virtual std::string getModel() const = 0;

    // buildRequest: Message 向量 + 可选的 tools JSON
    virtual json buildRequest(const std::vector<Message>& messages,
                              const json& tools = json::object()) const = 0;

    virtual std::string parseResponse(const json& response) const = 0;

    bool isMCPModel = false;
};

class AliyunStrategy : public AIStrategy {
public:
    AliyunStrategy() {
        const char* key = std::getenv("DASHSCOPE_API_KEY");
        if (key) api_key_ = key;
        isMCPModel = false;
    }

    std::string getApiUrl() const override;
    std::string getApiKey() const override;
    void setApiKey(const std::string& key) override { api_key_ = key; }
    std::string getModel() const override;

    json buildRequest(const std::vector<Message>& messages,
                      const json& tools = json::object()) const override;
    std::string parseResponse(const json& response) const override;

private:
    std::string api_key_;
};

class DouBaoStrategy : public AIStrategy {
public:
    DouBaoStrategy() {
        const char* key = std::getenv("DOUBAO_API_KEY");
        if (key) api_key_ = key;
        isMCPModel = false;
    }

    std::string getApiUrl() const override;
    std::string getApiKey() const override;
    void setApiKey(const std::string& key) override { api_key_ = key; }
    void setEndpointId(const std::string& id) override { endpoint_id_ = id; }
    std::string getModel() const override;

    json buildRequest(const std::vector<Message>& messages,
                      const json& tools = json::object()) const override;
    std::string parseResponse(const json& response) const override;

private:
    std::string api_key_;
    std::string endpoint_id_;
};

class AliyunRAGStrategy : public AIStrategy {
public:
    AliyunRAGStrategy() {
        const char* key = std::getenv("DASHSCOPE_API_KEY");
        if (key) api_key_ = key;
        const char* rid = std::getenv("Knowledge_Base_ID");
        if (rid) rag_id_ = rid;
        isMCPModel = false;
    }

    std::string getApiUrl() const override;
    std::string getApiKey() const override;
    void setApiKey(const std::string& key) override { api_key_ = key; }
    void setRagId(const std::string& id) override { rag_id_ = id; }
    std::string getRagId() const override { return rag_id_; }
    std::string getModel() const override;

    json buildRequest(const std::vector<Message>& messages,
                      const json& tools = json::object()) const override;
    std::string parseResponse(const json& response) const override;

private:
    std::string api_key_;
    std::string rag_id_;
};

class AliyunMcpStrategy : public AIStrategy {
public:
    AliyunMcpStrategy() {
        const char* key = std::getenv("DASHSCOPE_API_KEY");
        if (key) api_key_ = key;
        isMCPModel = true;
    }

    std::string getApiUrl() const override;
    std::string getApiKey() const override;
    void setApiKey(const std::string& key) override { api_key_ = key; }
    std::string getModel() const override;

    json buildRequest(const std::vector<Message>& messages,
                      const json& tools = json::object()) const override;
    std::string parseResponse(const json& response) const override;

private:
    std::string api_key_;
};







