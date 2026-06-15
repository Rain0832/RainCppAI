#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "mcp/AIToolRegistry.h"
#include "mcp/McpClientManager.h"
#include "server/ChatServer.h"

int main(int argc, char* argv[])
{
    LOG_INFO << "pid = " << getpid();
    std::string serverName = "ChatServer";
    int port = 80;
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
    muduo::Logger::setLogLevel(muduo::Logger::INFO);
    ChatServer server(port, serverName);
    server.setThreadNum(4);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 加载 MCP 工具配置（必须在 initChatMessage / 处理请求之前）
    auto& registry = AIToolRegistry::instance();
    registry.loadFromConfig("../mcp_config.json");

    // 初始化 McpClientManager（stdio/sse 远端工具）
    auto& mcpMgr = McpClientManager::instance();
    mcpMgr.loadFromConfig("../mcp_config.json");

    // 注入 McpClientManager 到 AIToolRegistry，打通降级路由
    registry.setMcpClientManager(&mcpMgr);

    server.initChatMessage();
    server.start();
}
