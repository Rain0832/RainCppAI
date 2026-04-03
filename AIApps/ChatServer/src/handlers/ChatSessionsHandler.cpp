#include "../include/handlers/ChatSessionsHandler.h"

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

            server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized,
                                 "Unauthorized", true, "application/json", errorBody.size(),
                                 errorBody, resp);
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
        try {
            http::MysqlUtil mu;
            std::string sql = "SELECT id, title FROM sessions WHERE user_id = " + std::to_string(userId)
                            + " AND deleted_at IS NULL ORDER BY updated_at DESC";
            auto res = mu.executeQuery(sql);
            while (res && res->next()) {
                std::string sid = res->getString("id");
                std::string title = res->isNull("title") ? "" : res->getString("title");
                titleMap[sid] = title;
            }
        } catch (...) {
            // DB 查询失败时降级为纯内存列表，不影响主流程
        }

        json successResp;
        successResp["success"] = true;
        json sessionArray = json::array();
        for (auto& sid : memSessions) {
            json s;
            s["sessionId"] = sid;
            auto it = titleMap.find(sid);
            s["name"] = (it != titleMap.end() && !it->second.empty())
                        ? it->second
                        : "Session " + sid.substr(0, 8);
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
