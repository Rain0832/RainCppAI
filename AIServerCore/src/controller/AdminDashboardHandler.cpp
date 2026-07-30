#include "controller/AdminDashboardHandler.h"

#include "Common/Logging/Logger.h"
#include "HttpServer/include/utils/FileUtil.h"

void AdminDashboardHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    std::string htmlPath = server_->getResourceRoot() + "web/admin/dashboard.html";

    FileUtil fileOperater(htmlPath);
    if (!fileOperater.isValid())
    {
        SPDLOG_ERROR_TAG("ADMIN") << "Cannot open dashboard template: " << htmlPath;
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k404NotFound, "Not Found");
        resp->setCloseConnection(true);
        resp->setContentType("text/plain");
        std::string body = "Dashboard template not found";
        resp->setContentLength(body.size());
        resp->setBody(body);
        return;
    }

    std::vector<char> buffer(fileOperater.size());
    fileOperater.readFile(buffer);
    std::string body = std::string(buffer.data(), buffer.size());

    resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
    resp->setCloseConnection(false);
    resp->setContentType("text/html; charset=utf-8");
    resp->setContentLength(body.size());
    resp->setBody(body);

    SPDLOG_INFO_TAG("ADMIN") << "Dashboard served: " << body.size() << " bytes";
}
