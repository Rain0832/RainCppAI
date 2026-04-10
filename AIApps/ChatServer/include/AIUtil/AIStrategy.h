#pragma once
#include <string>
#include <vector>
#include <utility>
#include <iostream>
#include <sstream>
#include <memory>

#include "../../../../HttpServer/include/utils/JsonUtil.h"



class AIStrategy {
public:
    virtual ~AIStrategy() = default;

    
    virtual std::string getApiUrl() const = 0;

    // API Key
    virtual std::string getApiKey() const = 0;

    // 运行时覆盖 API Key（前端传入）
    virtual void setApiKey(const std::string& key) = 0;

    // RAG 知识库 ID（仅 AliyunRAGStrategy 使用）
    virtual void setRagId(const std::string& id) { (void)id; }
    virtual std::string getRagId() const { return ""; }

    // 豆包 Endpoint ID（仅 DouBaoStrategy 使用）
    virtual void setEndpointId(const std::string& id) { (void)id; }

    virtual std::string getModel() const = 0;


    virtual json buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const = 0;


    virtual std::string parseResponse(const json& response) const = 0;

    bool isMCPModel = false;

};

class AliyunStrategy : public AIStrategy {

public:
    AliyunStrategy() {
        const char* key = std::getenv("DASHSCOPE_API_KEY");
        if (key) apiKey_ = key;
        isMCPModel = false;
    }

    std::string getApiUrl() const override;
    std::string getApiKey() const override;
    void setApiKey(const std::string& key) override { apiKey_ = key; }
    std::string getModel() const override;

    json buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const override;
    std::string parseResponse(const json& response) const override;

private:
    std::string apiKey_;
};

class DouBaoStrategy : public AIStrategy {

public:
    DouBaoStrategy() {
        const char* key = std::getenv("DOUBAO_API_KEY");
        if (key) apiKey_ = key;
        const char* ep = std::getenv("DOUBAO_ENDPOINT_ID");
        if (ep) endpointId_ = ep;
        isMCPModel = false;
    }
    std::string getApiUrl() const override;
    std::string getApiKey() const override;
    void setApiKey(const std::string& key) override { apiKey_ = key; }
    void setEndpointId(const std::string& id) { endpointId_ = id; }
    std::string getModel() const override;

    json buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const override;
    std::string parseResponse(const json& response) const override;

private:
    std::string apiKey_;
    std::string endpointId_;  ///< 火山方舟 Endpoint ID（ep-xxxxx）
};

class AliyunRAGStrategy : public AIStrategy {

public:
    AliyunRAGStrategy() {
        const char* key = std::getenv("DASHSCOPE_API_KEY");
        if (key) apiKey_ = key;
        const char* rid = std::getenv("Knowledge_Base_ID");
        if (rid) ragId_ = rid;
        isMCPModel = false;
    }

    std::string getApiUrl() const override;
    std::string getApiKey() const override;
    void setApiKey(const std::string& key) override { apiKey_ = key; }
    void setRagId(const std::string& id) override { ragId_ = id; }
    std::string getRagId() const override { return ragId_; }
    std::string getModel() const override;

    json buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const override;
    std::string parseResponse(const json& response) const override;

private:
    std::string apiKey_;
    std::string ragId_;
};

class AliyunMcpStrategy : public AIStrategy {

public:
    AliyunMcpStrategy() {
        const char* key = std::getenv("DASHSCOPE_API_KEY");
        if (key) apiKey_ = key;
        isMCPModel = true;
    }

    std::string getApiUrl() const override;
    std::string getApiKey() const override;
    void setApiKey(const std::string& key) override { apiKey_ = key; }
    std::string getModel() const override;

    json buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const override;
    std::string parseResponse(const json& response) const override;

private:
    std::string apiKey_;
};







