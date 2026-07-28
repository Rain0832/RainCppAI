#include "middleware/AuthMiddleware.h"

#include <sstream>

#include "Auth/JwtService.h"

namespace http
{
namespace middleware
{

AuthMiddleware::AuthMiddleware()
{
    common::JwtService jwtService;
    secret_ = jwtService.secret();
}

bool AuthMiddleware::isPublicPath(const std::string& path)
{
    return path == "/api/invite/verify" || path == "/api/verify/send" || path == "/api/verify/check" ||
           path == "/login" || path == "/register" || path == "/" || path == "/entry";
}

std::string AuthMiddleware::extractJwtFromCookie(const HttpRequest& request)
{
    std::string cookie = request.getHeader("Cookie");
    if (cookie.empty()) return {};

    // Parse: "jwt=<token>; other=val"
    size_t pos = cookie.find("jwt=");
    if (pos == std::string::npos) return {};

    pos += 4;  // skip "jwt="
    size_t end = cookie.find(';', pos);
    if (end == std::string::npos) return cookie.substr(pos);
    return cookie.substr(pos, end - pos);
}

void AuthMiddleware::before(HttpRequest& request)
{
    // Only protect /api/* paths
    std::string path = request.path();
    if (path.size() < 4 || path.substr(0, 4) != "/api") return;
    if (isPublicPath(path)) return;

    // Extract JWT from cookie
    std::string token = extractJwtFromCookie(request);
    if (token.empty())
    {
        HttpResponse resp(false);
        resp.setStatusCode(HttpResponse::k401Unauthorized);
        resp.setStatusMessage("Unauthorized");
        resp.setContentType("application/json");
        json body;
        body["success"] = false;
        body["error"]["code"] = 401;
        body["error"]["message"] = "Authentication required";
        std::string bodyStr = body.dump();
        resp.setContentLength(bodyStr.size());
        resp.setBody(bodyStr);
        throw resp;
    }

    // Verify JWT
    common::JwtService jwtService(secret_);
    json payload = jwtService.verify(token);
    if (payload.empty())
    {
        HttpResponse resp(false);
        resp.setStatusCode(HttpResponse::k401Unauthorized);
        resp.setStatusMessage("Unauthorized");
        resp.setContentType("application/json");
        json body;
        body["success"] = false;
        body["error"]["code"] = 401;
        body["error"]["message"] = "Invalid or expired token";
        std::string bodyStr = body.dump();
        resp.setContentLength(bodyStr.size());
        resp.setBody(bodyStr);
        throw resp;
    }

    // Attach user info as custom headers for downstream handlers
    request.addHeader("X-Auth-UserId", std::to_string(payload["sub"].get<long long>()));
    request.addHeader("X-Auth-Role", payload["role"].get<std::string>());
}

}  // namespace middleware
}  // namespace http
