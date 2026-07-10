#include "controller/ChatSessionsHandler.h"

void ChatSessionsHandler::handle(const http::HttpRequest &req, http::HttpResponse *resp)
{
    try
    {
        auto session = server_->getSessionManager()->getSession(req, resp);
        LOG_INFO << "session->getValue(\"isLoggedIn\") = " << session->getValue("isLoggedIn");
        if (session->getValue("isLoggedIn") != "true")
        {
            json errorResp;
            errorResp["status"] = "error";
            errorResp["message"] = "Unauthorized";
            std::string errorBody = errorResp.dump(4);

            server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized, "Unauthorized", true,
                                 "application/json", errorBody.size(), errorBody, resp);
            return;
        }

        int userId = std::stoi(session->getValue("userId"));

        // Phase 2: 从 sessions 表读取含 title 的会话列表
        // 同时兼容内存中的会话（新建但 MQ 尚未持久化的）
        std::vector<std::string> memSessions;
        {
            std::shared_lock<std::shared_mutex> lock(server_->rwMutexForSessionsId);
            memSessions = server_->sessionsIdsMap[userId];
        }

        // 查 DB 获取 title
        std::unordered_map<std::string, std::string> titleMap;
        try
        {
            storage::MysqlUtil mu;
            std::string sql = "SELECT id, title FROM sessions WHERE user_id = ? "
                              "AND is_deleted = 0 ORDER BY updated_at DESC";
            auto res = mu.executeQuery(sql, userId);
            while (res && res->next())
            {
                std::string sid = res->getString("id");
                std::string title = res->isNull("title") ? "" : res->getString("title");
                titleMap[sid] = title;
            }
        }
        catch (...)
        {
            // DB 查询失败时降级为纯内存列表，不影响主流程
        }

        // 对 title 为空的 session，用 messages 首条用户消息作为 fallback
        for (auto &sid : memSessions)
        {
            auto it = titleMap.find(sid);
            if (it == titleMap.end() || it->second.empty())
            {
                try
                {
                    storage::MysqlUtil mu;
                    auto msgRes = mu.executeQuery(
                        "SELECT content FROM messages WHERE session_id = ? AND role = 'user' "
                        "ORDER BY created_at ASC LIMIT 1",
                        sid);
                    if (msgRes && msgRes->next())
                    {
                        std::string firstMsg = msgRes->getString("content");
                        // 取前 18 个字符，去除首尾空白
                        size_t len = std::min(firstMsg.size(), size_t(18));
                        std::string fallback = firstMsg.substr(0, len);
                        // 找第一个换行或句末标点截断
                        auto pos = fallback.find('\n');
                        if (pos != std::string::npos)
                            fallback = fallback.substr(0, pos);
                        if (!fallback.empty())
                            titleMap[sid] = fallback;
                    }
                }
                catch (...)
                {
                    // message 查询失败，保持空 title，后续用 fallback
                }
            }
        }

        json successResp;
        successResp["success"] = true;
        json sessionArray = json::array();
        for (auto &sid : memSessions)
        {
            json s;
            s["sessionId"] = sid;
            auto it = titleMap.find(sid);
            s["name"] = (it != titleMap.end() && !it->second.empty()) ? it->second : ("会话 " + sid.substr(0, 8));
            sessionArray.push_back(s);
        }
        successResp["sessions"] = sessionArray;

        std::string successBody = successResp.dump(4);

        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(successBody.size());
        resp->setBody(successBody);
        return;
    }
    catch (const std::exception &e)
    {
        json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = e.what();
        std::string failureBody = failureResp.dump(4);
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(failureBody.size());
        resp->setBody(failureBody);
    }
}
