#include "controller/AIMenuHandler.h"

#include "Common/Http/ApiResult.h"
#include "Common/Logging/Logger.h"

void AIMenuHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    try
    {
        // 通过会话管理器验证登录状态
        auto session = server_->getSessionManager()->getSession(req, resp);
        SPDLOG_INFO_TAG("AI") << "session->getValue(\"isLoggedIn\") = " << session->getValue("isLoggedIn");
        if (session->getValue("isLoggedIn") != "true")
        {
            json errorResp = common::ApiResult::fail(400, "Unauthorized").toJson();
            std::string errorBody = errorResp.dump(4);

            server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized, "Unauthorized", true,
                                 "application/json", errorBody.size(), errorBody, resp);
            return;
        }

        int userId = std::stoi(session->getValue("userId"));
        std::string username = session->getValue("username");

        // 读取菜单页面模板
        std::string reqFile = server_->getResourceRoot() + "web/menu.html";
        FileUtil fileOperater(reqFile);
        if (!fileOperater.isValid())
        {
            SPDLOG_WARN_TAG("AI") << reqFile << "not exist.";
            fileOperater.resetDefaultFile();
        }

        std::vector<char> buffer(fileOperater.size());
        fileOperater.readFile(buffer);  // ļ
        std::string htmlContent(buffer.data(), buffer.size());

        // 向页面注入用户ID，便于前端脚本获取
        size_t headEnd = htmlContent.find("</head>");
        if (headEnd != std::string::npos)
        {
            std::string script = "<script>const userId = '" + std::to_string(userId) + "';</script>";
            htmlContent.insert(headEnd, script);
        }

        // server_->packageResp(req.getVersion(), HttpResponse::k200Ok, "OK"
        //             , false, "text/html", htmlContent.size(), htmlContent, resp);
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("text/html");
        resp->setContentLength(htmlContent.size());
        resp->setBody(htmlContent);
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR_TAG("AI") << "AIMenuHandler exception: " << e.what();
        json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = "Internal server error";
        std::string failureBody = failureResp.dump(4);
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(failureBody.size());
        resp->setBody(failureBody);
    }
}
