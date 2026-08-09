#include "controller/ChatSpeechHandler.h"

#include "Common/Config/ConfigManager.h"
#include "Common/Http/ApiResult.h"
#include "Common/Logging/Logger.h"

void ChatSpeechHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    try
    {
        auto session = server_->getSessionManager()->getSession(req, resp);
        SPDLOG_INFO_TAG("TTS") << "session->getValue(\"isLoggedIn\") = " << session->getValue("isLoggedIn");
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

        std::string text;

        auto body = req.getBody();
        if (!body.empty())
        {
            auto j = json::parse(body);
            if (j.contains("text")) text = j["text"];
        }

        // 统一从 config.json 中读取私密 API Key，避免直接依赖 getenv().
        auto& cfg = common::ConfigManager::instance();
        std::string clientId = cfg.get("api_keys.baidu.client_id", "");
        std::string clientSecret = cfg.get("api_keys.baidu.client_secret", "");

        if (clientId.empty() || clientSecret.empty())
            throw std::runtime_error("Baidu API keys not configured in config.json!");

        AISpeechProcessor speechProcessor(clientId, clientSecret);

        std::string speechUrl = speechProcessor.synthesize(text, "mp3-16k", "zh", 5, 5, 5);

        json successResp;
        successResp["success"] = true;
        successResp["url"] = speechUrl;
        std::string successBody = successResp.dump(4);
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(successBody.size());
        resp->setBody(successBody);
        return;
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
