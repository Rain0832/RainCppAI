#pragma once
#include <string>

#include "Middleware.h"
namespace http
{
namespace middleware
{
class RequestIdMiddleware : public Middleware
{
public:
    RequestIdMiddleware() = default;
    void before(HttpRequest& request) override;
    void after(HttpResponse& response) override;

private:
    static std::string generateUuid();
};
};  // namespace middleware
}  // namespace http