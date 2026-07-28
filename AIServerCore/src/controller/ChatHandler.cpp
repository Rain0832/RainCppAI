
#include "controller/ChatHandler.h"

#include "Common/Auth/JwtService.h"
#include "Common/Http/ApiResult.h"
#include "Common/Logging/Logger.h"

void ChatHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    try
    {
        long long userId = 0;
        std::string username;

        // Check old session first (backward compat)
        auto session = server_->getSessionManager()->getSession(req, resp);
        if (session->getValue("isLoggedIn") == "true")
        {
            userId = std::stoll(session->getValue("userId"));
            username = session->getValue("username");
        }
        else
        {
            // Fallback: check JWT cookie
            std::string cookie = req.getHeader("Cookie");
            std::string token;
            size_t pos = cookie.find("jwt=");
            if (pos != std::string::npos)
            {
                pos += 4;
                size_t end = cookie.find(';', pos);
                token = (end == std::string::npos) ? cookie.substr(pos) : cookie.substr(pos, end - pos);
            }
            if (!token.empty())
            {
                common::JwtService jwtService;
                json payload = jwtService.verify(token);
                if (!payload.empty())
                {
                    userId = payload["sub"].get<long long>();
                    username = "user" + std::to_string(userId);
                }
            }
            if (userId == 0)
            {
                json errorResp = common::ApiResult::fail(401, "Unauthorized").toJson();
                std::string errorBody = errorResp.dump();
                server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized, "Unauthorized", true,
                                     "application/json", errorBody.size(), errorBody, resp);
                return;
            }
        }

        std::string reqFile = server_->getResourceRoot() + "web/AI.html";
        FileUtil fileOperater(reqFile);
        if (!fileOperater.isValid())
        {
            SPDLOG_WARN_TAG("HTTP") << reqFile << "not exist.";
            fileOperater.resetDefaultFile();
        }

        std::vector<char> buffer(fileOperater.size());
        fileOperater.readFile(buffer);
        std::string htmlContent(buffer.data(), buffer.size());

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
