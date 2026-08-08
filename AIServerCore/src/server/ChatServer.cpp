// ChatServer.cpp - AI聊天服务器实现文件

#include "server/ChatServer.h"

#include <filesystem>
#include <random>

#include "Common/Config/ConfigManager.h"
#include "Common/Crypto/PasswordHash.h"
#include "Common/Logging/Logger.h"
#include "controller/AIUploadHandler.h"
#include "controller/AIUploadSendHandler.h"
#include "controller/AdminDashboardHandler.h"
#include "controller/AdminFeedbackHandler.h"
#include "controller/AdminInviteCodesHandler.h"
#include "controller/AdminLogsHandler.h"
#include "controller/AdminSseHandler.h"
#include "controller/AdminToggleUserHandler.h"
#include "controller/AdminUsersHandler.h"
#include "controller/ApiKeyHandler.h"
#include "controller/ChangePasswordHandler.h"
#include "controller/ChatDeleteSessionHandler.h"
#include "controller/ChatEntryHandler.h"
#include "controller/ChatFeedbackHandler.h"
#include "controller/ChatHandler.h"
#include "controller/ChatHistoryHandler.h"
#include "controller/ChatInviteVerifyHandler.h"
#include "controller/ChatLoginHandler.h"
#include "controller/ChatLogoutHandler.h"
#include "controller/ChatRegisterHandler.h"
#include "controller/ChatSessionsHandler.h"
#include "controller/ChatSpeechHandler.h"
#include "controller/ChatSseHandler.h"
#include "controller/ChatUpdateTitleHandler.h"
#include "controller/ChatVerifyCheckHandler.h"
#include "controller/ChatVerifySendHandler.h"
#include "controller/HealthHandler.h"
#include "controller/McpHandler.h"
#include "controller/MetricsHandler.h"
#include "controller/ModelListHandler.h"
#include "controller/TaskStatusHandler.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "http/HttpServer.h"
#include "http/StaticFileHandler.h"
#include "middleware/AdminAuthMiddleware.h"
#include "middleware/AuthMiddleware.h"
#include "middleware/RateLimitMiddleware.h"
#include "middleware/RequestIdMiddleware.h"

#include "Infralib/Cache/RedisClient.h"
#include "Infralib/Cache/SessionCache.h"
#ifdef HAS_AMQPCPP
#include "Infralib/Mq/TaskProducer.h"
#endif

using namespace http;

ChatServer::ChatServer(int port, const std::string& name, muduo::net::TcpServer::Option option)
    : httpServer_(port, name, option)
{
    initialize();
}

void ChatServer::initialize()
{
    SPDLOG_INFO_TAG("HTTP") << "ChatServer initializing ...";

    // 初始化MySQL数据库连接池
    auto& cfg = common::ConfigManager::instance();
    cfg.load("../config.json");
    common::Logger::init(cfg.get("log.level", "info"), cfg.get("log.path", "logs/app.log"));

    // Ensure logs/ directory exists (spdlog daily_file_sink does not auto-create)
    std::filesystem::path logDir = std::filesystem::path(cfg.get("log.path", "logs/app.log")).parent_path();
    if (!logDir.empty() && !std::filesystem::exists(logDir))
    {
        std::filesystem::create_directories(logDir);
    }

    std::string dbConn = cfg.get("db.host", "127.0.0.1") + ":" + std::to_string(cfg.getInt("db.port", 3307));
    storage::MysqlUtil::init(dbConn, cfg.get("db.user", "chat"), cfg.get("db.password", ""),
                             cfg.get("db.name", "ChatHttpServer"), cfg.getInt("db.pool_size", 5));

    initDatabase();
    checkOnnxModel();
    seedRootAccount();
    initializeSession();
    initializeMiddleware();
    initializeRedis();
#ifdef HAS_AMQPCPP
    initializeMQ();
#endif
    initializeRouter();
}

void ChatServer::initDatabase()
{
    const char* createAccounts = R"SQL(
        CREATE TABLE IF NOT EXISTS accounts (
            id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            username VARCHAR(64) NOT NULL,
            password_hash VARCHAR(256) NOT NULL,
            email VARCHAR(255) DEFAULT NULL,
            role ENUM('user','admin','org') NOT NULL DEFAULT 'user',
            is_disabled TINYINT(1) NOT NULL DEFAULT 0,
            failed_attempts TINYINT UNSIGNED NOT NULL DEFAULT 0,
            locked_until DATETIME NULL DEFAULT NULL,
            invite_code_id BIGINT UNSIGNED DEFAULT NULL,
            created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
            updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
            last_login_at DATETIME(3) DEFAULT NULL,
            UNIQUE KEY uk_username (username),
            UNIQUE KEY uk_email (email),
            INDEX idx_invite_code (invite_code_id)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )SQL";

    const char* createSessions = R"SQL(
        CREATE TABLE IF NOT EXISTS sessions (
            id VARCHAR(64) NOT NULL PRIMARY KEY,
            account_id BIGINT UNSIGNED NOT NULL,
            title VARCHAR(128) DEFAULT NULL,
            is_deleted TINYINT(1) NOT NULL DEFAULT 0,
            created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
            updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
            INDEX idx_account_id (account_id),
            INDEX idx_account_deleted (account_id, is_deleted),
            CONSTRAINT fk_session_account FOREIGN KEY (account_id) REFERENCES accounts(id)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )SQL";

    const char* createMessages = R"SQL(
        CREATE TABLE IF NOT EXISTS messages (
            id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            session_id VARCHAR(64) NOT NULL,
            role ENUM('user','assistant','system','tool') NOT NULL,
            content MEDIUMTEXT NOT NULL,
            model VARCHAR(64) DEFAULT NULL,
            tool_call_id VARCHAR(128) DEFAULT NULL,
            payload JSON DEFAULT NULL,
            created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
            INDEX idx_session_created (session_id, created_at),
            CONSTRAINT fk_message_session FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=DYNAMIC
    )SQL";

    const char* createApiKeys = R"SQL(
        CREATE TABLE IF NOT EXISTS api_keys (
            id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            account_id BIGINT UNSIGNED NOT NULL,
            provider VARCHAR(32) NOT NULL,
            api_key VARCHAR(512) NOT NULL,
            created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
            updated_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
            UNIQUE KEY uk_account_provider (account_id, provider),
            CONSTRAINT fk_apikey_account FOREIGN KEY (account_id) REFERENCES accounts(id)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )SQL";

    const char* createInviteCodes = R"SQL(
        CREATE TABLE IF NOT EXISTS invite_codes (
            id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            code VARCHAR(32) NOT NULL,
            created_by BIGINT UNSIGNED NOT NULL,
            max_uses INT UNSIGNED NOT NULL DEFAULT 1,
            used_count INT UNSIGNED NOT NULL DEFAULT 0,
            expires_at DATETIME(3) DEFAULT NULL,
            is_disabled TINYINT(1) NOT NULL DEFAULT 0,
            is_admin TINYINT(1) NOT NULL DEFAULT 0,
            created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
            UNIQUE KEY uk_code (code),
            INDEX idx_created_by (created_by),
            CONSTRAINT fk_invite_creator FOREIGN KEY (created_by) REFERENCES accounts(id)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )SQL";

    const char* createVerificationCodes = R"SQL(
        CREATE TABLE IF NOT EXISTS verification_codes (
            id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            email VARCHAR(255) NOT NULL,
            code VARCHAR(8) NOT NULL,
            purpose ENUM('register','reset_password') NOT NULL DEFAULT 'register',
            is_used TINYINT(1) NOT NULL DEFAULT 0,
            expires_at DATETIME(3) NOT NULL,
            created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
            INDEX idx_email_purpose (email, purpose)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )SQL";

    const char* createFeedback = R"SQL(
        CREATE TABLE IF NOT EXISTS feedback (
            id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            account_id BIGINT UNSIGNED NOT NULL,
            content TEXT NOT NULL,
            created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
            INDEX idx_account_id (account_id),
            CONSTRAINT fk_feedback_account FOREIGN KEY (account_id) REFERENCES accounts(id)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )SQL";

    const char* createCallLogs = R"SQL(
        CREATE TABLE IF NOT EXISTS call_logs (
            id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            session_id VARCHAR(64) DEFAULT NULL,
            account_id BIGINT UNSIGNED NOT NULL,
            model VARCHAR(64) NOT NULL,
            provider VARCHAR(32) NOT NULL,
            duration_ms INT UNSIGNED NOT NULL,
            status ENUM('success','error','timeout') NOT NULL DEFAULT 'success',
            error_message VARCHAR(512) DEFAULT NULL,
            created_at DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
            INDEX idx_account_created (account_id, created_at),
            INDEX idx_created (created_at)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
    )SQL";

    auto initAllTables = [&]()
    {
        mysqlUtil_.executeRawSql(createAccounts);
        mysqlUtil_.executeRawSql(createSessions);
        mysqlUtil_.executeRawSql(createMessages);
        mysqlUtil_.executeRawSql(createApiKeys);
        mysqlUtil_.executeRawSql(createInviteCodes);
        mysqlUtil_.executeRawSql(createVerificationCodes);
        mysqlUtil_.executeRawSql(createFeedback);
        mysqlUtil_.executeRawSql(createCallLogs);
    };
    try
    {
        initAllTables();
        SPDLOG_INFO_TAG("HTTP") << "Database tables initialized (8 tables with FK)";
    }
    catch (const std::exception& e)
    {
        std::cerr << "First attempt to init database tables failed: " << e.what() << " -- retrying after 2s ..."
                  << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        try
        {
            initAllTables();
            std::cout << "Database tables initialized successfully (retry)." << std::endl;
        }
        catch (const std::exception& e2)
        {
            std::cerr << "Failed to init database tables after retry: " << e2.what() << std::endl;
        }
    }
}

void ChatServer::checkOnnxModel()
{
    const char* kModelPath = "/root/models/mobilenetv2/mobilenetv2-7.onnx";
    if (!std::filesystem::exists(kModelPath))
    {
        SPDLOG_WARN("============================================================");
        SPDLOG_WARN("  ⚠️  ONNX 模型文件未找到！");
        SPDLOG_WARN("  路径: {}", kModelPath);
        SPDLOG_WARN("  请手动执行: bash scripts/download_models.sh");
        SPDLOG_WARN("  否则视觉识别功能将无法使用（纯文本对话不受影响）");
        SPDLOG_WARN("============================================================");
    }
    else
    {
        SPDLOG_INFO_TAG("AI") << "ONNX model found: " << kModelPath;
    }
}

void ChatServer::seedRootAccount()
{
    try
    {
        storage::MysqlUtil mu;
        auto res = mu.executeQuery("SELECT COUNT(*) AS cnt FROM accounts");
        if (res && res->next() && res->getInt64("cnt") > 0)
        {
            SPDLOG_INFO_TAG("HTTP") << "Accounts table already populated, skip seeding";
            return;
        }

        // 动态生成 12 位随机强密码
        static const char kChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        static constexpr size_t kCharCount = sizeof(kChars) - 1;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> dis(0, kCharCount - 1);

        std::string plainPassword;
        plainPassword.reserve(12);
        for (int i = 0; i < 12; ++i) plainPassword += kChars[dis(gen)];

        std::string hashed = common::hashPassword(plainPassword);
        mu.executeUpdate("INSERT INTO accounts (username, password_hash, email, role) VALUES (?, ?, ?, ?)", "root",
                         hashed, "root@localhost", "admin");

        SPDLOG_CRITICAL("╔══════════════════════════════════════════════════════════╗");
        SPDLOG_CRITICAL("║  [系统初始化] 已创建超级管理员！                           ║");
        SPDLOG_CRITICAL("║  账号: root                                              ║");
        SPDLOG_CRITICAL("║  初始登录密码: {:<41}                                     ║", plainPassword);
        SPDLOG_CRITICAL("║  ⚠️  请立即登录并修改密码！                               ║");
        SPDLOG_CRITICAL("╚══════════════════════════════════════════════════════════╝");
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR_TAG("HTTP") << "Root account seeding failed: " << e.what();
    }
}

void ChatServer::initChatMessage()
{
    SPDLOG_INFO_TAG("HTTP") << "Loading chat history from database ...";
    readDataFromMySQL();
    SPDLOG_INFO_TAG("HTTP") << "Chat history loaded successfully";
}

void ChatServer::readDataFromMySQL()
{
    // Phase 2: 从新的 messages 表读取，JOIN sessions 获取用户 ID
    // 通过 sessions.user_id 关联，按 created_at 排序保证消息顺序
    std::string sql =
        "SELECT m.session_id, s.account_id AS user_id, m.role, m.content, "
        "UNIX_TIMESTAMP(m.created_at) * 1000 AS ts_ms "
        "FROM messages m "
        "INNER JOIN sessions s ON m.session_id = s.id "
        "ORDER BY m.created_at ASC, m.id ASC";

    sql::ResultSet* res;
    try
    {
        res = mysqlUtil_.executeQuery(sql);
    }
    catch (const std::exception& e)
    {
        std::cerr << "MySQL query failed: " << e.what() << std::endl;
        return;
    }

    while (res->next())
    {
        long long user_id = 0;
        std::string session_id;
        std::string role, content;
        long long ts_ms = 0;

        try
        {
            user_id = res->getInt64("user_id");
            session_id = res->getString("session_id");
            role = res->getString("role");
            content = res->getString("content");
            ts_ms = res->getInt64("ts_ms");
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to read row: " << e.what() << std::endl;
            continue;
        }

        auto& userSessions = chatInformation[user_id];

        std::shared_ptr<AIHelper> helper;
        auto itSession = userSessions.find(session_id);
        if (itSession == userSessions.end())
        {
            helper = std::make_shared<AIHelper>(&mysqlUtil_, &aiThreadPool_);
            userSessions[session_id] = helper;
            sessionsIdsMap[user_id].push_back(session_id);
        }
        else
        {
            helper = itSession->second;
        }

        helper->restoreMessage(content, ts_ms, role);
    }

    SPDLOG_INFO_TAG("HTTP") << "Chat history restore complete";
}

void ChatServer::setThreadNum(int numThreads)
{
    httpServer_.setThreadNum(numThreads);
}

void ChatServer::start()
{
    httpServer_.start();
}

void ChatServer::initializeRouter()
{
    // 入口页面路由
    httpServer_.Get("/", std::make_shared<ChatEntryHandler>(this));
    httpServer_.Get("/entry", std::make_shared<ChatEntryHandler>(this));
    httpServer_.Get("/register", std::make_shared<ChatEntryHandler>(this, "register.html"));
    httpServer_.Get("/health", std::make_shared<HealthHandler>(this));
    httpServer_.Get("/metrics", std::make_shared<MetricsHandler>(this));

    // 用户认证路由
    httpServer_.Post("/login", std::make_shared<ChatLoginHandler>(this));
    httpServer_.Post("/register", std::make_shared<ChatRegisterHandler>(this));

    // Invite code verification
    httpServer_.Post("/api/invite/verify", std::make_shared<ChatInviteVerifyHandler>(this));

    // Verification code
    httpServer_.Post("/api/verify/send", std::make_shared<ChatVerifySendHandler>(this));
    httpServer_.Post("/api/verify/check", std::make_shared<ChatVerifyCheckHandler>(this));

    // Feedback
    httpServer_.Post("/api/feedback", std::make_shared<ChatFeedbackHandler>(this));
    httpServer_.Post("/user/logout", std::make_shared<ChatLogoutHandler>(this));

    // 聊天功能路由
    httpServer_.Get("/chat", std::make_shared<ChatHandler>(this));
    httpServer_.Post("/chat/send-stream", std::make_shared<ChatSseHandler>(this));  // SSE 流式（唯一对话入口）
    httpServer_.Get("/chat/sessions", std::make_shared<ChatSessionsHandler>(this));
    httpServer_.Post("/chat/history", std::make_shared<ChatHistoryHandler>(this));
    httpServer_.Post("/chat/tts", std::make_shared<ChatSpeechHandler>(this));
    httpServer_.Post("/chat/update-title", std::make_shared<ChatUpdateTitleHandler>(this));
    httpServer_.Post("/chat/delete-session", std::make_shared<ChatDeleteSessionHandler>(this));

    // AI功能路由
    httpServer_.Get("/upload", std::make_shared<AIUploadHandler>(this));
    httpServer_.Post("/upload/send", std::make_shared<AIUploadSendHandler>(this));

    // 静态文件路由（CSS / JS / 图片 / 字体 — 动态正则匹配）
    auto staticFileHandler = std::make_shared<http::StaticFileHandler>(resource_root_);
    httpServer_.addRoute(http::HttpRequest::kGet, "/css/:file", staticFileHandler);
    httpServer_.addRoute(http::HttpRequest::kGet, "/js/:file", staticFileHandler);
    httpServer_.addRoute(http::HttpRequest::kGet, "/assets/:path", staticFileHandler);
    httpServer_.addRoute(http::HttpRequest::kGet, "/assets/images/:file", staticFileHandler);

    // 模型列表路由（厂商-模型双层注册表）
    httpServer_.Get("/api/chat/models", std::make_shared<ModelListHandler>(this, resource_root_));

    // API Key 管理路由（GET 返回掩码列表，POST 保存新 Key）
    httpServer_.Get("/api/user/apikey", std::make_shared<ApiKeyHandler>(this));
    httpServer_.Post("/api/user/apikey", std::make_shared<ApiKeyHandler>(this));

    // 修改密码路由
    httpServer_.Post("/api/user/password", std::make_shared<ChangePasswordHandler>(this));

    // MCP Server 路由（标准 JSON-RPC 2.0）
    httpServer_.Post("/mcp", std::make_shared<McpHandler>(this));

    // v3.2.0: 异步任务状态查询（RabbitMQ 削峰）
    if (redisClient_)
        httpServer_.addRoute(http::HttpRequest::kGet, "/task/:taskId/status",
                             std::make_shared<TaskStatusHandler>(redisClient_));

    // Admin 后台路由
    httpServer_.Get("/admin/dashboard", std::make_shared<AdminDashboardHandler>(this));
    httpServer_.Get("/admin/logs", std::make_shared<AdminLogsHandler>(this));
    httpServer_.Get("/admin/sse", std::make_shared<AdminSseHandler>(this));
    httpServer_.Get("/admin/api/users", std::make_shared<AdminUsersHandler>(this));
    httpServer_.Post("/admin/api/users/toggle", std::make_shared<AdminToggleUserHandler>(this));
    httpServer_.Get("/admin/api/feedback", std::make_shared<AdminFeedbackHandler>(this));
    httpServer_.Get("/admin/api/invite-codes", std::make_shared<AdminInviteCodesListHandler>(this));
    httpServer_.Post("/admin/api/invite-codes/create", std::make_shared<AdminInviteCodeCreateHandler>(this));
    httpServer_.Post("/admin/api/invite-codes/toggle", std::make_shared<AdminInviteCodeToggleHandler>(this));
}

void ChatServer::initializeSession()
{
    auto sessionStorage = std::make_unique<http::session::MemorySessionStorage>();
    auto sessionManager = std::make_unique<http::session::SessionManager>(std::move(sessionStorage));
    setSessionManager(std::move(sessionManager));
}

void ChatServer::initializeMiddleware()
{
    // CORS 中间件：内测阶段仅允许本地访问
    http::middleware::CorsConfig corsCfg = http::middleware::CorsConfig::defaultConfig();
    corsCfg.allowedOrigins = {"http://localhost:8080", "http://127.0.0.1:8080", "http://localhost:8088",
                              "http://127.0.0.1:8088"};
    corsCfg.allowCredentials = true;

    auto corsMiddleware = std::make_shared<http::middleware::CorsMiddleware>(corsCfg);
    httpServer_.addMiddleware(corsMiddleware);

    // 安全响应头中间件：CSP / HSTS / X-Frame-Options / X-Content-Type-Options / X-XSS-Protection
    auto secHeaders = std::make_shared<http::middleware::SecurityHeadersMiddleware>();
    httpServer_.addMiddleware(secHeaders);

    auto authMiddleware = std::make_shared<http::middleware::AuthMiddleware>();
    httpServer_.addMiddleware(authMiddleware);

    auto adminAuthMiddleware = std::make_shared<http::middleware::AdminAuthMiddleware>();
    httpServer_.addMiddleware(adminAuthMiddleware);

    // RequestId middleware
    auto reqIdMiddleware = std::make_shared<http::middleware::RequestIdMiddleware>();
    httpServer_.addMiddleware(reqIdMiddleware);

    // RateLimit middleware: 10 req/min per user for /api/chat/*
    auto rateLimitMiddleware = std::make_shared<http::middleware::RateLimitMiddleware>();
    httpServer_.addMiddleware(rateLimitMiddleware);
}

void ChatServer::initializeRedis()
{
    auto& cfg = common::ConfigManager::instance();
    std::string host = cfg.get("redis.host", "127.0.0.1");
    int port = cfg.getInt("redis.port", 6379);
    std::string password = cfg.get("redis.password", "");
    int db = cfg.getInt("redis.db", 0);

    redisClient_ = std::make_shared<infra::cache::RedisClient>();
    if (redisClient_->connect(host, port, password, db))
    {
        sessionCache_ = std::make_shared<infra::cache::SessionCache>(redisClient_);
        SPDLOG_INFO_TAG("REDIS") << "Redis initialized: " << host << ":" << port;
    }
    else
    {
        SPDLOG_WARN_TAG("REDIS") << "Redis unavailable — falling back to MySQL-only mode";
        redisClient_.reset();
    }
}

#ifdef HAS_AMQPCPP
void ChatServer::initializeMQ()
{
    auto& cfg = common::ConfigManager::instance();
    std::string mqUri = cfg.get("rabbitmq.uri", "");

    if (mqUri.empty())
    {
        SPDLOG_INFO_TAG("MQ") << "RabbitMQ URI not configured — MQ disabled";
        return;
    }

    taskProducer_ = std::make_shared<infra::mq::TaskProducer>();
    if (taskProducer_->connect(mqUri))
    {
        taskProducer_->declareQueue("vision_tasks");
        taskProducer_->declareQueue("tts_tasks");
        SPDLOG_INFO_TAG("MQ") << "RabbitMQ producer initialized";
    }
    else
    {
        SPDLOG_WARN_TAG("MQ") << "RabbitMQ unavailable — async tasks disabled";
        taskProducer_.reset();
    }
}
#endif

void ChatServer::packageResp(const std::string& version,
                             http::HttpResponse::HttpStatusCode statusCode,
                             const std::string& statusMsg,
                             bool close,
                             const std::string& contentType,
                             int contentLen,
                             const std::string& body,
                             http::HttpResponse* resp)
{
    if (resp == nullptr)
    {
        SPDLOG_ERROR_TAG("HTTP") << "Response pointer is null";
        return;
    }

    try
    {
        resp->setVersion(version);
        resp->setStatusCode(statusCode);
        resp->setStatusMessage(statusMsg);
        resp->setCloseConnection(close);
        resp->setContentType(contentType);
        resp->setContentLength(contentLen);
        resp->setBody(body);

        SPDLOG_INFO_TAG("HTTP") << "Response packaged successfully";
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR_TAG("HTTP") << "Error in packageResp: " << e.what();
        resp->setStatusCode(http::HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Internal Server Error");
        resp->setCloseConnection(true);
    }
}

void ChatServer::touchSession(int userId, const std::string& sessionId)
{
    std::string key = std::to_string(userId) + ":" + sessionId;
    auto it = lruMap_.find(key);
    if (it != lruMap_.end())
    {
        lruList_.erase(it->second);
    }
    lruList_.push_front(key);
    lruMap_[key] = lruList_.begin();
}

void ChatServer::evictIfNeeded()
{
    while (lruList_.size() > MAX_SESSIONS)
    {
        std::string oldest = lruList_.back();
        lruList_.pop_back();
        lruMap_.erase(oldest);

        // 解析 "userId:sessionId"
        auto pos = oldest.find(':');
        if (pos != std::string::npos)
        {
            int uid = std::stoi(oldest.substr(0, pos));
            std::string sid = oldest.substr(pos + 1);
            auto uit = chatInformation.find(uid);
            if (uit != chatInformation.end())
            {
                uit->second.erase(sid);
                if (uit->second.empty())
                {
                    chatInformation.erase(uit);
                }
            }
            SPDLOG_INFO_TAG("HTTP") << "LRU evicted session: userId=" << uid << " sessionId=" << sid;
        }
    }
}
