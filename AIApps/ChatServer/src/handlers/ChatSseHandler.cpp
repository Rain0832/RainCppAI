#include "../include/handlers/ChatSseHandler.h"
#include "../include/AIUtil/AIHelper.h"

static void sendSseChunk(const muduo::net::TcpConnectionPtr &conn, const std::string &data)
{
    // SSE 帧格式：data: <payload>\n\n
    std::string frame = "data: " + data + "\n\n";
    conn->getLoop()->runInLoop([conn, frame]() {
        if (conn->connected()) conn->send(frame);
    });
}

static void sendSseDone(const muduo::net::TcpConnectionPtr &conn)
{
    conn->getLoop()->runInLoop([conn]() {
        if (conn->connected()) conn->send("data: [DONE]\n\n");
    });
}

void ChatSseHandler::handle(const http::HttpRequest &req, http::HttpResponse *resp)
{
    try
    {
        auto session = server_->getSessionManager()->getSession(req, resp);
        if (session->getValue("isLoggedIn") != "true")
        {
            json e; e["status"] = "error"; e["message"] = "Unauthorized";
            std::string b = e.dump();
            server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized,
                                 "Unauthorized", true, "application/json", b.size(), b, resp);
            return;
        }

        int userId = std::stoi(session->getValue("userId"));
        std::string username = session->getValue("username");

        std::string userQuestion, modelType, sessionId, apiKey;
        auto body = req.getBody();
        if (!body.empty()) {
            auto j = json::parse(body);
            if (j.contains("question"))  userQuestion = j["question"];
            if (j.contains("sessionId")) sessionId    = j["sessionId"];
            if (j.contains("apiKey"))    apiKey       = j["apiKey"];
            modelType = j.contains("modelType") ? j["modelType"].get<std::string>() : "1";
        }

        // 获取/创建 AIHelperPtr（读写锁）
        std::shared_ptr<AIHelper> AIHelperPtr;
        {
            std::shared_lock<std::shared_mutex> rlock(server_->rwMutexForChatInfo);
            auto uit = server_->chatInformation.find(userId);
            if (uit != server_->chatInformation.end()) {
                auto sit = uit->second.find(sessionId);
                if (sit != uit->second.end()) AIHelperPtr = sit->second;
            }
        }
        if (!AIHelperPtr) {
            std::unique_lock<std::shared_mutex> wlock(server_->rwMutexForChatInfo);
            auto &us = server_->chatInformation[userId];
            if (!us.count(sessionId)) us.emplace(sessionId, std::make_shared<AIHelper>());
            AIHelperPtr = us[sessionId];
            server_->touchSession(userId, sessionId);
            server_->evictIfNeeded();
        }

        // ★ 标记 deferred，发送 SSE 握手头
        resp->setDeferred(true);
        auto conn = resp->getConnection();

        // SSE 握手：在 IO 线程中立即发送响应头
        conn->getLoop()->runInLoop([conn, req]() {
            if (!conn->connected()) return;
            std::string sseHeader =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/event-stream\r\n"
                "Cache-Control: no-cache\r\n"
                "Connection: keep-alive\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "\r\n";
            conn->send(sseHeader);
        });

        // 提交流式 AI 调用到线程池
        server_->aiThreadPool_.submit([conn, AIHelperPtr, userId, username, sessionId,
                                       userQuestion, modelType, apiKey]() {
            try {
                AIHelperPtr->chatStream(
                    userId, username, sessionId, userQuestion, modelType, apiKey,
                    [&conn](const std::string& token) -> bool {
                        if (!conn->connected()) return false;
                        // 将 token 包装为 JSON 发送
                        json data; data["token"] = token;
                        sendSseChunk(conn, data.dump());
                        return true;
                    }
                );
                sendSseDone(conn);
            } catch (const std::exception &e) {
                json err; err["error"] = e.what();
                sendSseChunk(conn, err.dump());
                sendSseDone(conn);
            }
        });
    }
    catch (const std::exception &e)
    {
        json f; f["status"] = "error"; f["message"] = e.what();
        std::string b = f.dump();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(b.size());
        resp->setBody(b);
    }
}
