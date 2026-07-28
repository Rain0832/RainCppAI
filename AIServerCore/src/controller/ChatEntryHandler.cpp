#include "controller/ChatEntryHandler.h"

#include "Common/Logging/Logger.h"

void ChatEntryHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    std::string reqFile = server_->getResourceRoot() + "web/" + page_;
    FileUtil fileOperater(reqFile);
    if (!fileOperater.isValid())
    {
        SPDLOG_WARN_TAG("HTTP") << reqFile << " not exist";
        fileOperater.resetDefaultFile();
    }

    std::vector<char> buffer(fileOperater.size());
    fileOperater.readFile(buffer);
    std::string bufStr = std::string(buffer.data(), buffer.size());

    resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
    resp->setCloseConnection(false);
    resp->setContentType("text/html");
    resp->setContentLength(bufStr.size());
    resp->setBody(bufStr);
}
