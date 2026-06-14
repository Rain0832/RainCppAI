#include "mcp/McpClientManager.h"

#include <curl/curl.h>
#include <muduo/base/Logging.h>

#include <fstream>
#include <set>
#include <sstream>

#include "JsonUtil.h"

// ═══════════════════════════════════════════════════════════════
// McpClientManager 单例
// ═══════════════════════════════════════════════════════════════
McpClientManager& McpClientManager::instance()
{
    static McpClientManager inst;
    return inst;
}

// ═══════════════════════════════════════════════════════════════
// 配置加载入口
// ═══════════════════════════════════════════════════════════════
void McpClientManager::loadFromConfig(const std::string& configPath)
{
    configPath_ = configPath;

    std::ifstream file(configPath);
    if (!file.is_open())
    {
        LOG_WARN << "[McpClientManager] Cannot open config: " << configPath;
        return;
    }

    json config;
    file >> config;

    if (!config.contains("mcpServers"))
    {
        LOG_WARN << "[McpClientManager] No 'mcpServers' section in config";
        return;
    }

    for (auto& [name, serverDef] : config["mcpServers"].items())
    {
        registerServer(name, serverDef);
    }
}

// ═══════════════════════════════════════════════════════════════
// 热插拔：对比新旧配置，增量启停 server
// ═══════════════════════════════════════════════════════════════
void McpClientManager::reloadFromConfig(const std::string& configPath)
{
    std::ifstream file(configPath);
    if (!file.is_open())
    {
        LOG_WARN << "[McpClientManager] reload: Cannot open config: " << configPath;
        return;
    }

    json config;
    file >> config;

    if (!config.contains("mcpServers"))
    {
        LOG_WARN << "[McpClientManager] reload: No 'mcpServers' section";
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 收集新配置中的 server 名集合
    std::set<std::string> newNames;
    for (auto& [name, serverDef] : config["mcpServers"].items())
    {
        newNames.insert(name);
    }

    // 移除已不在配置中的 server
    std::vector<std::string> toRemove;
    for (auto& [name, idx] : serverToClient_)
    {
        if (!newNames.count(name))
            toRemove.push_back(name);
    }
    for (auto& name : toRemove)
        unregisterServer(name);

    // 新增或更新 server
    for (auto& [name, serverDef] : config["mcpServers"].items())
    {
        if (!serverToClient_.count(name))
        {
            // 解锁后调用 registerServer（registerServer 内部会加锁）
            mutex_.unlock();
            registerServer(name, serverDef);
            mutex_.lock();
        }
    }

    LOG_INFO << "[McpClientManager] reload complete, active servers: " << serverToClient_.size();
}

// ═══════════════════════════════════════════════════════════════
// 停止指定 server 并清理缓存
// ═══════════════════════════════════════════════════════════════
void McpClientManager::unregisterServer(const std::string& name)
{
    auto it = serverToClient_.find(name);
    if (it == serverToClient_.end())
        return;

    size_t idx = it->second;
    LOG_INFO << "[McpClientManager] Shutting down server '" << name << "' (client[" << idx << "])";

    clients_[idx]->stop();

    // 清除该 server 关联的工具缓存
    for (auto ti = toolToClient_.begin(); ti != toolToClient_.end();)
    {
        if (ti->second == idx)
            ti = toolToClient_.erase(ti);
        else
            ++ti;
    }

    serverToClient_.erase(it);
}

// ═══════════════════════════════════════════════════════════════
// 注册并启动单个 Server
// ═══════════════════════════════════════════════════════════════
void McpClientManager::registerServer(const std::string& name, const json& serverDef)
{
    std::string transport = serverDef.value("transport", "");
    if (transport.empty())
    {
        LOG_WARN << "[McpClientManager] Server '" << name << "' has no transport, skipped";
        return;
    }

    LOG_INFO << "[McpClientManager] Registering server '" << name << "' transport=" << transport;

    std::shared_ptr<McpClient> client;

    if (transport == "stdio")
    {
        // ── Stdio 模式：fork 子进程，pipe 通信 ──
        std::string command = serverDef.value("command", "");
        if (command.empty())
        {
            LOG_WARN << "[McpClientManager] Stdio server '" << name << "' missing 'command'";
            return;
        }

        json argsArr = serverDef.value("args", json::array());
        std::vector<std::string> args;
        for (auto& a : argsArr)
            args.push_back(a.get<std::string>());

        // 构建 popen 命令
        std::string cmdLine = command;
        for (auto& a : args)
            cmdLine += " " + a;

        FILE* pipe = popen(cmdLine.c_str(), "r+");
        if (!pipe)
        {
            LOG_WARN << "[McpClientManager] Stdio server '" << name << "' popen failed";
            return;
        }

        // 创建简易 lambda client（无独立类，直接封装）
        struct StdioClient : McpClient
        {
            FILE* p;
            int reqId = 0;
            std::string cachedEndpoint;  // sse 兼容字段

            explicit StdioClient(FILE* _p) : p(_p) {}

            bool start() override { return p != nullptr; }

            json sendRequest(const json& req) override
            {
                // 写入请求
                std::string reqStr = req.dump() + "\n";
                fwrite(reqStr.c_str(), 1, reqStr.size(), p);
                fflush(p);

                // 读取响应行
                char buf[65536];
                if (!fgets(buf, sizeof(buf), p))
                    throw std::runtime_error("Stdio read failed");
                return json::parse(buf);
            }

            json getTools() override
            {
                json req;
                req["jsonrpc"] = "2.0";
                req["id"] = ++reqId;
                req["method"] = "tools/list";
                req["params"] = json::object();

                json resp = sendRequest(req);
                if (resp.contains("result") && resp["result"].contains("tools"))
                    return resp["result"]["tools"];
                return json::array();
            }

            json callTool(const std::string& name, const json& args) override
            {
                json req;
                req["jsonrpc"] = "2.0";
                req["id"] = ++reqId;
                req["method"] = "tools/call";
                req["params"]["name"] = name;
                req["params"]["arguments"] = args;

                json resp = sendRequest(req);
                if (resp.contains("result") && resp["result"].contains("content"))
                {
                    auto& content = resp["result"]["content"];
                    if (content.is_array() && !content.empty())
                    {
                        std::string text = content[0].value("text", "{}");
                        try
                        {
                            return json::parse(text);
                        }
                        catch (...)
                        {
                            return json {{"text", text}};
                        }
                    }
                }
                return json {{"error", "Stdio: unexpected response"}};
            }

            void stop() override
            {
                if (p)
                {
                    pclose(p);
                    p = nullptr;
                }
            }

            ~StdioClient() override { stop(); }
        };

        client = std::make_shared<StdioClient>(pipe);
    }
    else if (transport == "sse")
    {
        // ── SSE 模式：HTTP GET 长连接 + POST 发送 ──
        std::string url = serverDef.value("url", "");
        if (url.empty())
        {
            LOG_WARN << "[McpClientManager] SSE server '" << name << "' missing 'url'";
            return;
        }

        struct SseClient : McpClient
        {
            std::string url_;
            std::string endpoint_;
            json headers_;
            int reqId = 0;

            SseClient(std::string url, json hdrs) : url_(std::move(url)), headers_(std::move(hdrs)) {}

            // libcurl write callback
            static size_t writeCb(void* contents, size_t size, size_t nmemb, std::string* output)
            {
                output->append(static_cast<char*>(contents), size * nmemb);
                return size * nmemb;
            }

            // SSE 解析：从 data: {...} 行提取 endpoint 事件
            static std::string parseSseEndpoint(const std::string& sseData)
            {
                std::istringstream ss(sseData);
                std::string line;
                while (std::getline(ss, line))
                {
                    if (line.empty() || line[0] == '\r')
                        continue;
                    if (line.rfind("data: ", 0) == 0)
                    {
                        std::string jsonStr = line.substr(6);
                        try
                        {
                            json j = json::parse(jsonStr);
                            if (j.contains("endpoint"))
                                return j["endpoint"].get<std::string>();
                        }
                        catch (...)
                        {
                        }
                    }
                }
                return "";
            }

            bool start() override
            {
                CURL* curl = curl_easy_init();
                if (!curl)
                    return false;

                std::string response;

                // GET SSE 长连接
                curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

                // 添加自定义 headers
                struct curl_slist* hlist = nullptr;
                for (auto& [k, v] : headers_.items())
                {
                    std::string h = k + ": " + v.get<std::string>();
                    hlist = curl_slist_append(hlist, h.c_str());
                }
                if (hlist)
                    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hlist);

                CURLcode res = curl_easy_perform(curl);
                curl_slist_free_all(hlist);
                curl_easy_cleanup(curl);

                if (res != CURLE_OK)
                {
                    LOG_WARN << "[McpSseClient] SSE GET to " << url_ << " failed: " << curl_easy_strerror(res);
                    return false;
                }

                // 解析 endpoint
                endpoint_ = parseSseEndpoint(response);
                if (endpoint_.empty())
                {
                    LOG_WARN << "[McpSseClient] No 'endpoint' in SSE response from " << url_;
                    return false;
                }

                LOG_INFO << "[McpSseClient] SSE connected, endpoint=" << endpoint_;
                return true;
            }

            json httpPost(const std::string& uri, const json& body)
            {
                CURL* curl = curl_easy_init();
                if (!curl)
                    throw std::runtime_error("curl init failed");

                std::string response;
                std::string bodyStr = body.dump();

                curl_easy_setopt(curl, CURLOPT_URL, uri.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyStr.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

                struct curl_slist* hlist = nullptr;
                hlist = curl_slist_append(hlist, "Content-Type: application/json");
                for (auto& [k, v] : headers_.items())
                {
                    std::string h = k + ": " + v.get<std::string>();
                    hlist = curl_slist_append(hlist, h.c_str());
                }
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hlist);

                CURLcode res = curl_easy_perform(curl);
                curl_slist_free_all(hlist);
                curl_easy_cleanup(curl);

                if (res != CURLE_OK)
                    throw std::runtime_error(std::string("POST failed: ") + curl_easy_strerror(res));

                return json::parse(response);
            }

            json sendRequest(const json& req) override { return httpPost(endpoint_, req); }

            json getTools() override
            {
                json req;
                req["jsonrpc"] = "2.0";
                req["id"] = ++reqId;
                req["method"] = "tools/list";
                req["params"] = json::object();

                json resp = sendRequest(req);
                if (resp.contains("result") && resp["result"].contains("tools"))
                    return resp["result"]["tools"];
                return json::array();
            }

            json callTool(const std::string& name, const json& args) override
            {
                json req;
                req["jsonrpc"] = "2.0";
                req["id"] = ++reqId;
                req["method"] = "tools/call";
                req["params"]["name"] = name;
                req["params"]["arguments"] = args;

                json resp = sendRequest(req);
                if (resp.contains("result") && resp["result"].contains("content"))
                {
                    auto& content = resp["result"]["content"];
                    if (content.is_array() && !content.empty())
                    {
                        std::string text = content[0].value("text", "{}");
                        try
                        {
                            return json::parse(text);
                        }
                        catch (...)
                        {
                            return json {{"text", text}};
                        }
                    }
                }
                return json {{"error", "SSE: unexpected response"}};
            }

            void stop() override {}
        };

        json hdrs = serverDef.value("headers", json::object());
        client = std::make_shared<SseClient>(url, hdrs);
        client->start();
    }
    else
    {
        LOG_WARN << "[McpClientManager] Unknown transport '" << transport << "' for server '" << name << "'";
        return;
    }

    if (client)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t idx = clients_.size();
        clients_.push_back(client);
        serverToClient_[name] = idx;
        LOG_INFO << "[McpClientManager] Server '" << name << "' registered at client[" << idx << "]";
    }
}

// ═══════════════════════════════════════════════════════════════
// 融合远端工具 schema
// ═══════════════════════════════════════════════════════════════
json McpClientManager::discoverAllTools()
{
    // 热插拔：每次获取工具列表前先 reload、检测配置变更
    {
        std::string cfg;
        {
            std::lock_guard<std::mutex> lk(mutex_);
            cfg = configPath_;
        }
        if (!cfg.empty())
            reloadFromConfig(cfg);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    json allTools = json::array();
    toolToClient_.clear();

    for (size_t i = 0; i < clients_.size(); ++i)
    {
        try
        {
            json tools = clients_[i]->getTools();
            for (auto& tool : tools)
            {
                std::string name = tool.value("name", "");
                if (!name.empty())
                    toolToClient_[name] = i;
            }
            for (auto& t : tools)
                allTools.push_back(std::move(t));
        }
        catch (const std::exception& e)
        {
            LOG_WARN << "[McpClientManager] tools/list failed for client " << i << ": " << e.what();
        }
    }

    LOG_INFO << "[McpClientManager] discoverAllTools: " << allTools.size() << " remote tools";
    return allTools;
}

// ═══════════════════════════════════════════════════════════════
// 按工具名路由执行
// ═══════════════════════════════════════════════════════════════
json McpClientManager::callTool(const std::string& name, const json& args)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = toolToClient_.find(name);
    if (it == toolToClient_.end())
    {
        throw std::runtime_error("Tool not found in any MCP client: " + name);
    }

    LOG_INFO << "[McpClientManager] callTool '" << name << "' → client[" << it->second << "]";
    return clients_[it->second]->callTool(name, args);
}