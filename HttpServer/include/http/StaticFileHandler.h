#pragma once
#include <string>

#include "router/RouterHandler.h"

namespace http
{

class StaticFileHandler : public http::router::RouterHandler
{
public:
    explicit StaticFileHandler(const std::string& resourceRoot);
    void handle(const HttpRequest& req, HttpResponse* resp) override;

private:
    bool isPathSafe(const std::string& path);
    std::string getMimeType(const std::string& path);
    std::string resourceRoot_;
};

}  // namespace http
