#include "http/StaticFileHandler.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include <muduo/base/Logging.h>
#include <algorithm>
#include <fstream>
#include <sstream>

namespace http
{

StaticFileHandler::StaticFileHandler(const std::string& resourceRoot) : resourceRoot_(resourceRoot) {}

void StaticFileHandler::handle(const HttpRequest& req, HttpResponse* resp)
{
    std::string urlPath = req.path();
    LOG_INFO << "[StaticFile] Request: " << urlPath;
    if (!isPathSafe(urlPath))
    {
        LOG_WARN << "[StaticFile] Unsafe path rejected: " << urlPath;
        resp->setStatusCode(HttpResponse::k404NotFound);
        resp->setBody("404 Not Found");
        return;
    }
    std::string filePath = resourceRoot_ + "web" + urlPath;
    LOG_INFO << "[StaticFile] Resolved to: " << filePath;
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
    {
        LOG_WARN << "[StaticFile] File not found: " << filePath;
        resp->setStatusCode(HttpResponse::k404NotFound);
        resp->setBody("404 Not Found");
        return;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();
    LOG_INFO << "[StaticFile] Served " << content.size() << " bytes, MIME: " << getMimeType(urlPath);
    resp->setVersion(req.getVersion());
    resp->setStatusMessage("OK");
    resp->setContentType(getMimeType(urlPath));
    resp->setContentLength(content.size());
    resp->setBody(content);
    resp->setStatusCode(HttpResponse::k200Ok);
}

bool StaticFileHandler::isPathSafe(const std::string& path)
{
    return path.find("..") == std::string::npos;
}

std::string StaticFileHandler::getMimeType(const std::string& path)
{
    auto dot = path.rfind('.');
    if (dot == std::string::npos) return "application/octet-stream";
    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".css")  return "text/css";
    if (ext == ".js")   return "application/javascript";
    if (ext == ".html") return "text/html";
    if (ext == ".json") return "application/json";
    if (ext == ".png")  return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".ico")  return "image/x-icon";
    if (ext == ".woff") return "font/woff";
    if (ext == ".woff2") return "font/woff2";
    if (ext == ".ttf")  return "font/ttf";
    return "application/octet-stream";
}

}  // namespace http
