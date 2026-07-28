#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Middleware.h"

namespace http
{
namespace middleware
{

/// Authentication middleware: validates JWT from httpOnly cookie.
/// Throws HttpResponse with 401 if token is missing or invalid.
/// For public paths (/api/invite/verify, /api/verify/send, /api/verify/check)
/// the middleware passes through without authentication.
class AuthMiddleware : public Middleware
{
public:
    AuthMiddleware();

    void before(HttpRequest& request) override;
    void after(HttpResponse& response) override {}

private:
    static bool isPublicPath(const std::string& path);
    static std::string extractJwtFromCookie(const HttpRequest& request);

    std::string secret_;
};

}  // namespace middleware
}  // namespace http
