#include "controller/ChatSseHandler.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

#include "Common/Auth/JwtService.h"
#include "Common/Config/ConfigManager.h"
#include "Common/Http/ApiResult.h"
#include "Common/Logging/Logger.h"
#include "common/AISessionIdGenerator.h"
#include "common/base64.h"
#include "llm/AIHelper.h"

/// 生成 UUID v4 风格的随机文件名
static std::string generateUUID()
{
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    uint64_t a = dis(gen);
    uint64_t b = dis(gen);
    oss << std::setw(8) << ((a >> 32) & 0xFFFFFFFF) << '-' << std::setw(4) << ((a >> 16) & 0xFFFF) << '-'
        << std::setw(4) << ((a & 0xFFFF) | 0x4000) << '-'        // version 4
        << std::setw(4) << ((b >> 48) & 0x3FFF | 0x8000) << '-'  // variant 1
        << std::setw(12) << (b & 0xFFFFFFFFFFFF);
    return oss.str();
}

static void sendSseChunk(const muduo::net::TcpConnectionPtr& conn, const std::string& data)
{
    std::string frame = "data: " + data + "\n\n";
    conn->getLoop()->runInLoop(
        [conn, frame]()
        {
            if (conn->connected()) conn->send(frame);
        });
}

static void sendSseDone(const muduo::net::TcpConnectionPtr& conn)
{
    conn->getLoop()->runInLoop(
        [conn]()
        {
            if (conn->connected()) conn->send("data: [DONE]\n\n");
        });
}

void ChatSseHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    try
    {
        long long userId = 0;
        std::string username;

        auto session = server_->getSessionManager()->getSession(req, resp);
        if (session->getValue("isLoggedIn") == "true")
        {
            userId = std::stoll(session->getValue("userId"));
            username = session->getValue("username");
        }
        else
        {
            std::string ck = req.getHeader("Cookie");
            std::string tok;
            size_t pos = ck.find("jwt=");
            if (pos != std::string::npos)
            {
                pos += 4;
                size_t end = ck.find(';', pos);
                tok = (end == std::string::npos) ? ck.substr(pos) : ck.substr(pos, end - pos);
            }
            if (!tok.empty())
            {
                common::JwtService js;
                json pld = js.verify(tok);
                if (!pld.empty())
                {
                    userId = pld["sub"].get<long long>();
                    username = "user" + std::to_string(userId);
                }
            }
            if (userId == 0)
            {
                json e = common::ApiResult::fail(401, "Unauthorized").toJson();
                std::string b = e.dump();
                server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized, "Unauthorized", true,
                                     "application/json", (int)b.size(), b, resp);
                return;
            }
        }

        std::string userQuestion, modelType, sessionId, ragId, provider, imageBase64;
        auto body = req.getBody();
        if (!body.empty())
        {
            auto j = json::parse(body);
            if (j.contains("question"))
            {
                userQuestion = j["question"];
                if (userQuestion.length() > 5000)
                {
                    resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
                    resp->setCloseConnection(false);
                    resp->setContentType("application/json");
                    json err = common::ApiResult::fail(400, "Content too long, max 5000 characters").toJson();
                    std::string body = err.dump();
                    resp->setContentLength(body.size());
                    resp->setBody(body);
                    return;
                }
            }
            if (j.contains("sessionId")) sessionId = j["sessionId"];
            if (j.contains("ragId")) ragId = j["ragId"];
            modelType = j.contains("modelType") ? j["modelType"].get<std::string>() : "qwen-plus";
            provider = j.contains("provider") ? j["provider"].get<std::string>() : "aliyun";
            if (j.contains("image_base64")) imageBase64 = j["image_base64"].get<std::string>();
        }

        SPDLOG_INFO_TAG("AI") << "Received chat request: provider=" << provider << ", model=" << modelType;

        // provider → DB api_key provider 映射, 无记录时 fallback 到 config.json 默认 Key
        const std::string dbProvider = (provider == "volcengine") ? "doubao" : "dashscope";
        std::string apiKey;
        try
        {
            storage::MysqlUtil mu;
            auto res = mu.executeQuery("SELECT api_key FROM api_keys WHERE account_id = ? AND provider = ?", userId,
                                       dbProvider);
            if (res && res->next()) apiKey = res->getString("api_key");
        }
        catch (...)
        {
        }
        if (apiKey.empty())
        {
            auto& cfg = common::ConfigManager::instance();
            std::string cfgKey = (provider == "volcengine") ? "doubao" : "dashscope";
            apiKey = cfg.get("default_api_keys." + cfgKey, "");
            if (!apiKey.empty()) SPDLOG_INFO_TAG("AI") << "Using default " << cfgKey << " API key from config.json";
        }

        // 新会话：前端不传 sessionId，后端自动生成
        bool isNewSession = sessionId.empty();
        if (isNewSession)
        {
            AISessionIdGenerator generator;
            sessionId = generator.generate();
        }

        // 获取/创建 AIHelperPtr（读写锁）
        std::shared_ptr<AIHelper> AIHelperPtr;
        {
            std::shared_lock<std::shared_mutex> rlock(server_->getChatInfoMutex(userId));
            auto uit = server_->getChatInformation().find(userId);
            if (uit != server_->getChatInformation().end())
            {
                auto sit = uit->second.find(sessionId);
                if (sit != uit->second.end()) AIHelperPtr = sit->second;
            }
        }
        if (!AIHelperPtr)
        {
            std::unique_lock<std::shared_mutex> wlock(server_->getChatInfoMutex(userId));
            auto& us = server_->getChatInformation()[userId];
            if (!us.count(sessionId))
            {
                us.emplace(sessionId,
                           std::make_shared<AIHelper>(&server_->getMysqlUtil(), &server_->getAiThreadPool()));
                // 同步记录 sessionId 到列表中
                {
                    std::unique_lock<std::shared_mutex> slock(server_->getSessionIdsMutex());
                    server_->getSessionIdsMap()[userId].push_back(sessionId);
                }
            }
            AIHelperPtr = us[sessionId];
            // SessionStore handles LRU touch/evict internally via getOrCreate
        }

        // 标记 deferred，发送 SSE 握手头
        resp->setDeferred(true);
        auto conn = resp->getConnection();

        // SSE 握手：在 IO 线程中立即发送响应头
        conn->getLoop()->runInLoop(
            [conn, req]()
            {
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
        server_->getAiThreadPool().submit(
            [this, conn, AIHelperPtr, userId, username, sessionId, userQuestion, modelType, apiKey, ragId, provider,
             isNewSession, imageBase64]()
            {
                try
                {
                    // 若包含图片：异步执行 ONNX 推理并将结果注入到 AIHelper 上下文中
                    if (!imageBase64.empty())
                    {
                        try
                        {
                            static const char* kModelPath = "/root/models/mobilenetv2/mobilenetv2-7.onnx";
                            if (!std::filesystem::exists(kModelPath))
                            {
                                SPDLOG_ERROR_TAG("AI") << "ONNX model not found: " << kModelPath
                                                       << " — skipping vision pipeline (text-only fallback)";
                                // 优雅降级：模型缺失时不阻断，按纯文本请求继续
                                goto skip_vision;
                            }

                            std::shared_ptr<ImageRecognizer> ImageRecognizerPtr;
                            {
                                std::shared_lock<std::shared_mutex> rlock(this->server_->getImageRecognizerMutex());
                                auto it = this->server_->getImageRecognizers().find(userId);
                                if (it != this->server_->getImageRecognizers().end())
                                {
                                    ImageRecognizerPtr = it->second;
                                }
                            }

                            if (!ImageRecognizerPtr)
                            {
                                std::unique_lock<std::shared_mutex> wlock(this->server_->getImageRecognizerMutex());
                                if (this->server_->getImageRecognizers().find(userId) ==
                                    this->server_->getImageRecognizers().end())
                                {
                                    this->server_->getImageRecognizers().emplace(
                                        userId, std::make_shared<ImageRecognizer>(kModelPath));
                                }
                                ImageRecognizerPtr = this->server_->getImageRecognizers()[userId];
                            }

                            // 剥离 Data URL 前缀 (前端 readAsDataURL 产生的 header)
                            std::string rawBase64 = imageBase64;
                            static const char kDataUrlPrefix[] = "data:";
                            if (rawBase64.compare(0, 5, kDataUrlPrefix) == 0)
                            {
                                size_t commaPos = rawBase64.find(',');
                                if (commaPos != std::string::npos && commaPos + 1 < rawBase64.size())
                                    rawBase64 = rawBase64.substr(commaPos + 1);
                            }

                            std::string decoded = base64_decode(rawBase64);
                            SPDLOG_DEBUG_TAG("AI") << "Base64 decoded, size=" << decoded.size()
                                                   << " bytes, original=" << rawBase64.size() << " chars";
                            static constexpr size_t kMaxImageBytes = 10 * 1024 * 1024;
                            if (decoded.size() <= kMaxImageBytes && decoded.size() >= 12)
                            {
                                SPDLOG_DEBUG_TAG("AI") << "Image decode OK, passing to ONNX inference...";
                                std::vector<unsigned char> imgData(decoded.begin(), decoded.end());
                                std::string className = ImageRecognizerPtr->PredictFromBuffer(imgData);
                                SPDLOG_INFO_TAG("AI") << "ONNX inference result: " << className;
                                std::string visionPrompt = std::string("识别结果为 \"") + className + "\"";
                                AIHelperPtr->injectVisionContext(visionPrompt);

                                // SP 10.3: Base64 不入库，落地为文件
                                std::string uploadDir = "web/assets/images/uploads";
                                std::error_code ec;
                                std::filesystem::create_directories(uploadDir, ec);
                                if (!ec)
                                {
                                    std::string uuid = generateUUID();
                                    std::string filename = uuid + ".jpg";
                                    std::string filePath = uploadDir + "/" + filename;
                                    std::ofstream ofs(filePath, std::ios::binary);
                                    if (ofs.is_open())
                                    {
                                        ofs.write(decoded.c_str(), decoded.size());
                                        ofs.close();
                                        std::string thumbPath = "/assets/images/uploads/" + filename;
                                        json imagePayload;
                                        imagePayload["text"] = userQuestion;
                                        imagePayload["image"]["thumbnail"] = thumbPath;
                                        imagePayload["image"]["recognition"] = className;
                                        AIHelperPtr->setUserMessagePayload(imagePayload.dump());
                                        SPDLOG_INFO_TAG("AI") << "Image saved: " << thumbPath;
                                    }
                                    else
                                    {
                                        SPDLOG_ERROR_TAG("AI") << "Failed to write image file: " << filePath;
                                    }
                                }
                                else
                                {
                                    SPDLOG_ERROR_TAG("AI") << "Failed to create upload dir: " << ec.message();
                                }
                            }
                            else
                            {
                                AIHelperPtr->injectVisionContext("无法识别图片（无效或超限）");
                            }
                        }
                        catch (const std::exception& e)
                        {
                            SPDLOG_ERROR_TAG("AI") << "Vision inference failed: " << e.what();
                            // 优雅降级：推理失败不阻断，跳过视觉注入继续纯文本对话
                            goto skip_vision;
                        }
                    }

                skip_vision:
                    // 新会话：先发送 sessionId 事件让前端知道
                    if (isNewSession)
                    {
                        json sidEvent;
                        sidEvent["sessionId"] = sessionId;
                        sendSseChunk(conn, sidEvent.dump());
                    }

                    AIHelperPtr->chatStream(
                        userId, username, sessionId, userQuestion, provider, apiKey, ragId, modelType,
                        [&conn](const std::string& token) -> bool
                        {
                            if (!conn->connected()) return false;
                            json data;
                            data["token"] = token;
                            sendSseChunk(conn, data.dump());
                            return true;
                        },
                        "", isNewSession);
                    sendSseDone(conn);
                }
                catch (const std::exception& e)
                {
                    json err;
                    err["error"] = e.what();
                    sendSseChunk(conn, err.dump());
                    sendSseDone(conn);
                }
            });
    }
    catch (const std::exception& e)
    {
        json f;
        f["status"] = "error";
        f["message"] = e.what();
        std::string b = f.dump();
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(b.size());
        resp->setBody(b);
    }
}
