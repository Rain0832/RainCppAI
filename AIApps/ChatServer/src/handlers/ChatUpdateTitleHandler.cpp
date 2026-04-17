#include "../include/handlers/ChatUpdateTitleHandler.h"

void ChatUpdateTitleHandler::handle(const http::HttpRequest &req, http::HttpResponse *resp)
{
    try {
        auto session = server_->getSessionManager()->getSession(req, resp);
        if (session->getValue("isLoggedIn") != "true") {
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k401Unauthorized, "Unauthorized");
            resp->setBody("{}"); resp->setContentType("application/json"); resp->setContentLength(2);
            return;
        }

        auto body = req.getBody();
        if (body.empty()) {
            resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
            resp->setBody("{}"); resp->setContentType("application/json"); resp->setContentLength(2);
            return;
        }

        auto j = json::parse(body);
        std::string sessionId = j.value("sessionId", "");
        std::string title = j.value("title", "");

        if (!sessionId.empty() && !title.empty()) {
            // 转义 title 防止 SQL 注入
            std::string safeTitle;
            for (char c : title) {
                if (c == '\'') safeTitle += "\\'";
                else if (c == '\\') safeTitle += "\\\\";
                else safeTitle += c;
            }
            std::string sql = "UPDATE sessions SET title = '" + safeTitle + "' WHERE id = '" + sessionId + "'";
            try {
                http::MysqlUtil mu;
                mu.executeUpdate(sql);
            } catch (...) {
                // DB 更新失败不影响主流程
            }
        }

        json res; res["success"] = true;
        std::string b = res.dump();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(b.size());
        resp->setBody(b);
    } catch (const std::exception &e) {
        json f; f["status"] = "error"; f["message"] = e.what();
        std::string b = f.dump();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad");
        resp->setContentType("application/json"); resp->setContentLength(b.size()); resp->setBody(b);
    }
}
