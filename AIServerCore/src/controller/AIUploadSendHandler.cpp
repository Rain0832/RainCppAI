#include "controller/AIUploadSendHandler.h"
#include "Common/Http/ApiResult.h"

void AIUploadSendHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    try
    {
        auto session = server_->getSessionManager()->getSession(req, resp);
        LOG_INFO << "session->getValue(\"isLoggedIn\") = " << session->getValue("isLoggedIn");
        if (session->getValue("isLoggedIn") != "true")
        {
            json errorResp = common::ApiResult::fail(400, "Unauthorized").toJson();
            std::string errorBody = errorResp.dump(4);

            server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized, "Unauthorized", true,
                                 "application/json", errorBody.size(), errorBody, resp);
            return;
        }

        int userId = std::stoi(session->getValue("userId"));
        std::shared_ptr<ImageRecognizer> ImageRecognizerPtr;
        {
            // 先读锁查找
            std::shared_lock<std::shared_mutex> rlock(server_->getImageRecognizerMutex());
            auto it = server_->getImageRecognizers().find(userId);
            if (it != server_->getImageRecognizers().end())
            {
                ImageRecognizerPtr = it->second;
            }
            rlock.unlock();

            if (!ImageRecognizerPtr)
            {
                std::unique_lock<std::shared_mutex> wlock(server_->getImageRecognizerMutex());
                if (server_->getImageRecognizers().find(userId) == server_->getImageRecognizers().end())
                {
                    server_->getImageRecognizers().emplace(
                            userId, std::make_shared<ImageRecognizer>("/root/models/mobilenetv2/mobilenetv2-7.onnx"));
                }
                ImageRecognizerPtr = server_->getImageRecognizers()[userId];
            }
        }

        auto body = req.getBody();
        std::string filename;
        std::string imageBase64;
        if (!body.empty())
        {
            auto j = json::parse(body);
            if (j.contains("filename"))
                filename = j["filename"];
            if (j.contains("image"))
                imageBase64 = j["image"];
        }
        if (imageBase64.empty())
        {
            throw std::runtime_error("No image data provided");
        }

        std::string decodedData = base64_decode(imageBase64);
        // 图片安全校验：base64 解码后数据不超过 10 MB
        static constexpr size_t kMaxImageBytes = 10 * 1024 * 1024;  // 10 MB
        if (decodedData.size() > kMaxImageBytes)
        {
            throw std::runtime_error("Image size exceeds 10 MB limit");
        }
        if (decodedData.size() < 12)
        {
            throw std::runtime_error("Image data too small to be valid");
        }
        std::vector<uchar> imgData(decodedData.begin(), decodedData.end());

        std::string className = ImageRecognizerPtr->PredictFromBuffer(imgData);

        json successResp;
        successResp["success"] = "ok";
        successResp["filename"] = filename;
        successResp["class_name"] = className;

        successResp["confidence"] = 0.95;  // todo:Calculating true confidence

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
        LOG_ERROR << "AIUploadSendHandler exception: " << e.what();
        json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = "Image processing failed";
        std::string failureBody = failureResp.dump(4);
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(failureBody.size());
        resp->setBody(failureBody);
    }
}
