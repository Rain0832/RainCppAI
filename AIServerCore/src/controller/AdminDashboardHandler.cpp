#include "controller/AdminDashboardHandler.h"

#include <fstream>
#include <sstream>

#include "Common/Logging/Logger.h"

void AdminDashboardHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    std::string htmlPath = server_->getResourceRoot() + "web/admin/dashboard.html";
    std::ifstream file(htmlPath);
    if (!file.is_open())
    {
        SPDLOG_ERROR_TAG("ADMIN") << "Cannot open dashboard template: " << htmlPath;
        resp->setStatusCode(http::HttpResponse::k404NotFound);
        resp->setStatusMessage("Not Found");
        resp->setContentType("text/plain");
        std::string body = "Dashboard template not found";
        resp->setContentLength(body.size());
        resp->setBody(body);
        return;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string body = ss.str();

    resp->setStatusCode(http::HttpResponse::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType("text/html; charset=utf-8");
    resp->setContentLength(body.size());
    resp->setBody(body);
}
