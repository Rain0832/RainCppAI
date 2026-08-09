#include <chrono>
#include <iostream>
#include <muduo/base/Logging.h>
#include <muduo/base/TimeZone.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <string>
#include <thread>

#include "Common/Config/ConfigManager.h"
#include "Common/Logging/Logger.h"
#include "mcp/AIToolRegistry.h"
#include "mcp/McpClientManager.h"
#include "server/ChatServer.h"

// Redirect muduo logs to spdlog
void muduoLogRedirect(const char* msg, int len)
{
    auto logger = spdlog::get("rain");
    if (!logger) return;
    std::string m(msg, len > 0 ? len - 1 : 0);
    logger->info("[MUDUO] {}", m);
}
int main(int argc, char* argv[])
{
    SPDLOG_INFO_TAG("MAIN") << "pid = " << getpid();
    std::string serverName = "ChatServer";
    auto& cfg = common::ConfigManager::instance();
    cfg.load("../config.json");
    // 安全检查：配置文件负责读取结构性配置，数据库密码等敏感值仍可通过环境变量注入。
    // 这里的逻辑保证了 config.json 与运行时密钥分离。
    std::string dbPass = cfg.get("db.password", "");
    if (dbPass.empty())
    {
        SPDLOG_WARN_TAG("MAIN")
            << "DB_PASSWORD env var is not set; if config.json has no password, DB connection will fail";
    }
    int port = cfg.getInt("server.port", 80);
    //
    int opt;
    const char* str = "p:";
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
            case 'p':
            {
                port = atoi(optarg);
                break;
            }
            default:
                break;
        }
    }
    muduo::Logger::setLogLevel(muduo::Logger::DEBUG);
    // 日志时区设为东八区 (CST)，方便国内排查
    muduo::Logger::setTimeZone(muduo::TimeZone(8 * 3600, "CST"));
    muduo::Logger::setOutput(muduoLogRedirect);
    ChatServer server(port, serverName);
    server.setThreadNum(cfg.getInt("server.threads", 4));

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 加载 MCP 工具配置（必须在 initChatMessage / 处理请求之前）
    auto& registry = AIToolRegistry::instance();
    registry.loadFromConfig(cfg.get("paths.mcp_config", "../mcp_config.json"));

    // 初始化 McpClientManager（stdio/sse 远端工具）
    auto& mcpMgr = McpClientManager::instance();
    mcpMgr.loadFromConfig(cfg.get("paths.mcp_config", "../mcp_config.json"));

    // 注入 McpClientManager 到 AIToolRegistry，打通降级路由
    registry.setMcpClientManager(&mcpMgr);

    server.initChatMessage();
    server.start();
}
