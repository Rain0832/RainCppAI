#include "mcp/McpClientManager.h"

#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <muduo/base/Logging.h>
#include <sys/wait.h>
#include <unistd.h>

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

        // 构建 argv for execvp
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(command.c_str()));
        for (auto& a : args)
            argv.push_back(const_cast<char*>(a.data()));
        argv.push_back(nullptr);

        // pipe + fork + dup2 全双工通信
        int pipe_in[2], pipe_out[2];
        if (pipe(pipe_in) != 0 || pipe(pipe_out) != 0)
        {
            LOG_WARN << "[McpClientManager] Stdio server '" << name << "' pipe() failed";
            return;
        }

        pid_t pid = fork();
        if (pid == 0)
        {
            // ── 子进程 ──
            close(pipe_in[1]);                 // 子进程不写 stdin 管道
            close(pipe_out[0]);                // 子进程不读 stdout 管道
            dup2(pipe_in[0], STDIN_FILENO);    // pipe_in[0] → stdin
            dup2(pipe_out[1], STDOUT_FILENO);  // stdout → pipe_out[1]
            close(pipe_in[0]);
            close(pipe_out[1]);

            execvp(command.c_str(), argv.data());
            _exit(1);  // exec 失败
        }
        else if (pid < 0)
        {
            LOG_WARN << "[McpClientManager] Stdio server '" << name << "' fork() failed";
            close(pipe_in[0]);
            close(pipe_in[1]);
            close(pipe_out[0]);
            close(pipe_out[1]);
            return;
        }

        // ── 父进程 ──
        close(pipe_in[0]);
        close(pipe_out[1]);
        int write_fd = pipe_in[1];
        int read_fd = pipe_out[0];

        // 设置非阻塞
        fcntl(write_fd, F_SETFL, fcntl(write_fd, F_GETFL) | O_NONBLOCK);
        fcntl(read_fd, F_SETFL, fcntl(read_fd, F_GETFL) | O_NONBLOCK);

        LOG_INFO << "[McpClientManager] Forked process PID=" << pid << " command=" << command;

        // ═══════════════════════════════════════════════════════
        // MCP 初始化握手 (initialize → initialized notification)
        // ═══════════════════════════════════════════════════════
        {
            // Step 1: 发送 initialize 请求
            json initReq;
            initReq["jsonrpc"] = "2.0";
            initReq["id"] = "init-1";
            initReq["method"] = "initialize";
            initReq["params"]["protocolVersion"] = "2024-11-05";
            initReq["params"]["capabilities"] = json::object();
            initReq["params"]["clientInfo"]["name"] = "RainCppAI";
            initReq["params"]["clientInfo"]["version"] = "2.1.0";

            std::string initReqStr = initReq.dump() + "\n";
            ssize_t w1 = write(write_fd, initReqStr.c_str(), initReqStr.size());
            (void)w1;

            // Step 2: 等待 initialize 响应（含 "id":"init-1"）
            std::string buf;
            char chunk[4096];
            bool handshakeOk = false;
            for (int retries = 0; retries < 500; ++retries)
            {
                ssize_t n = read(read_fd, chunk, sizeof(chunk) - 1);
                if (n > 0)
                {
                    chunk[n] = '\0';
                    buf += chunk;
                    // 找到完整行 (以 \n 结尾)
                    size_t nl = buf.find('\n');
                    if (nl != std::string::npos)
                    {
                        std::string line = buf.substr(0, nl);
                        try
                        {
                            json resp = json::parse(line);
                            if (resp.value("id", "") == "init-1" && !resp.contains("error"))
                            {
                                handshakeOk = true;
                                LOG_INFO << "[McpClientManager] initialize handshake OK";
                            }
                            else
                            {
                                LOG_WARN << "[McpClientManager] initialize response error: " << line;
                            }
                        }
                        catch (...)
                        {
                            LOG_WARN << "[McpClientManager] initialize response parse failed: " << line;
                        }
                        break;
                    }
                }
                else if (n == 0)
                {
                    LOG_WARN << "[McpClientManager] initialize EOF (child exited)";
                    break;
                }
                else
                {
                    if (errno != EAGAIN && errno != EWOULDBLOCK)
                    {
                        LOG_WARN << "[McpClientManager] initialize read error: " << strerror(errno);
                        break;
                    }
                    usleep(10000);  // 10ms
                }
            }

            if (!handshakeOk)
            {
                LOG_ERROR << "[McpClientManager] MCP handshake failed for '" << name
                          << "', terminating child PID=" << pid;
                close(read_fd);
                close(write_fd);
                kill(pid, SIGTERM);
                waitpid(pid, nullptr, 0);
                return;
            }

            // Step 3: 发送 initialized 通知（无 id 字段，不需要响应）
            json initDone;
            initDone["jsonrpc"] = "2.0";
            initDone["method"] = "notifications/initialized";

            std::string initDoneStr = initDone.dump() + "\n";
            ssize_t w2 = write(write_fd, initDoneStr.c_str(), initDoneStr.size());
            (void)w2;
            LOG_INFO << "[McpClientManager] Sent notifications/initialized";
        }

        // 全双工 StdioClient（pipe + fork）
        struct StdioClient : McpClient
        {
            int read_fd = -1;
            int write_fd = -1;
            pid_t child_pid = -1;
            int reqId = 0;

            StdioClient(int rfd, int wfd, pid_t p) : read_fd(rfd), write_fd(wfd), child_pid(p) {}

            bool start() override { return read_fd >= 0 && write_fd >= 0; }

            json sendRequest(const json& req) override
            {
                // 写入请求
                std::string reqStr = req.dump() + "\n";
                ssize_t written = write(write_fd, reqStr.c_str(), reqStr.size());
                (void)written;

                // 非阻塞循环读取，缓冲区拼接到完整行
                std::string buf;
                char chunk[4096];
                for (int retries = 0; retries < 1000; ++retries)
                {
                    ssize_t n = read(read_fd, chunk, sizeof(chunk) - 1);
                    if (n > 0)
                    {
                        chunk[n] = '\0';
                        buf += chunk;
                        size_t nl = buf.find('\n');
                        if (nl != std::string::npos)
                            return json::parse(buf.substr(0, nl));
                    }
                    else if (n == 0)
                    {
                        throw std::runtime_error("Stdio EOF (child process exited)");
                    }
                    else
                    {
                        if (errno != EAGAIN && errno != EWOULDBLOCK)
                            throw std::runtime_error("Stdio read error: " + std::string(strerror(errno)));
                        usleep(10000);  // 10ms 等待
                    }
                }
                throw std::runtime_error("Stdio read timeout");
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
                if (read_fd >= 0)
                {
                    close(read_fd);
                    read_fd = -1;
                }
                if (write_fd >= 0)
                {
                    close(write_fd);
                    write_fd = -1;
                }
                if (child_pid > 0)
                {
                    int status;
                    waitpid(child_pid, &status, WNOHANG);
                    child_pid = -1;
                }
            }

            ~StdioClient() override { stop(); }
        };

        client = std::make_shared<StdioClient>(read_fd, write_fd, pid);
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