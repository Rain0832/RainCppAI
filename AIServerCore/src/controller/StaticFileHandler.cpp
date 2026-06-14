#include "controller/StaticFileHandler.h"

#include "HttpServer/include/utils/FileUtil.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "server/ChatServer.h"

StaticFileHandler::StaticFileHandler(ChatServer* server) : server_(server) {}

void StaticFileHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    std::string urlPath = req.path();

    // 安全检查：拒绝路径穿越
    if (!isPathSafe(urlPath))
    {
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("text/plain");
        resp->setBody("400 Bad Request");
        return;
    }

    // 构建文件系统路径
    std::string filePath = server_->getResourceRoot() + "web" + urlPath;

    FileUtil fileOperater(filePath);
    if (!fileOperater.isValid())
    {
        // 文件不存在 → 404
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k404NotFound, "Not Found");
        resp->setCloseConnection(true);
        resp->setContentType("text/plain");
        resp->setBody("404 Not Found");
        return;
    }

    // 读取文件（二进制模式，兼容所有文件类型）
    std::vector<char> buffer(fileOperater.size());
    fileOperater.readFile(buffer);
    std::string content(buffer.data(), buffer.size());

    // 根据后缀设置 Content-Type
    std::string mime = getMimeType(filePath);

    resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
    resp->setCloseConnection(false);
    resp->setContentType(mime);
    resp->setContentLength(content.size());
    resp->setBody(content);
}

std::string StaticFileHandler::getMimeType(const std::string& path)
{
    // 提取文件后缀（不含点）
    auto dotPos = path.rfind('.');
    if (dotPos == std::string::npos)
        return "application/octet-stream";

    std::string ext = path.substr(dotPos);  // 含点，如 ".css"

    if (ext == ".html" || ext == ".htm")
        return "text/html; charset=utf-8";
    if (ext == ".css")
        return "text/css; charset=utf-8";
    if (ext == ".js")
        return "application/javascript; charset=utf-8";
    if (ext == ".json")
        return "application/json; charset=utf-8";
    if (ext == ".png")
        return "image/png";
    if (ext == ".jpg" || ext == ".jpeg")
        return "image/jpeg";
    if (ext == ".gif")
        return "image/gif";
    if (ext == ".svg")
        return "image/svg+xml";
    if (ext == ".ico")
        return "image/x-icon";
    if (ext == ".woff")
        return "font/woff";
    if (ext == ".woff2")
        return "font/woff2";
    if (ext == ".ttf")
        return "font/ttf";
    if (ext == ".eot")
        return "application/vnd.ms-fontobject";

    return "application/octet-stream";
}

bool StaticFileHandler::isPathSafe(const std::string& path)
{
    // 拒绝包含 "../" 的路径穿越
    if (path.find("..") != std::string::npos)
        return false;
    // 拒绝空路径或以非 "/" 开头
    if (path.empty() || path[0] != '/')
        return false;
    return true;
}