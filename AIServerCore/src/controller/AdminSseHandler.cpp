#include "controller/AdminSseHandler.h"

#include <chrono>
#include <thread>

#include "Common/Logging/Logger.h"
#include "Repository/AdminRepository.h"

static void sendAdminSse(const muduo::net::TcpConnectionPtr& conn, const std::string& data)
{
    std::string frame = "data: " + data + "\n\n";
    conn->getLoop()->runInLoop(
        [conn, frame]()
        {
            if (conn->connected()) conn->send(frame);
        });
}

void AdminSseHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    resp->setDeferred(true);
    auto conn = resp->getConnection();

    // SSE handshake on IO thread
    conn->getLoop()->runInLoop(
        [conn]()
        {
            if (!conn->connected()) return;
            std::string header =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/event-stream\r\n"
                "Cache-Control: no-cache\r\n"
                "Connection: keep-alive\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "\r\n";
            conn->send(header);
        });

    // Push stats every 10 seconds on a dedicated thread
    std::thread(
        [conn]()
        {
            AdminRepository repo;
            while (conn->connected())
            {
                try
                {
                    json stats = repo.getDashboardStats();
                    sendAdminSse(conn, stats.dump());
                }
                catch (const std::exception& e)
                {
                    SPDLOG_ERROR_TAG("ADMIN") << "Admin SSE error: " << e.what();
                    json err;
                    err["error"] = "Internal error fetching stats";
                    sendAdminSse(conn, err.dump());
                }
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }
            SPDLOG_INFO_TAG("ADMIN") << "Admin SSE client disconnected";
        })
        .detach();
}
