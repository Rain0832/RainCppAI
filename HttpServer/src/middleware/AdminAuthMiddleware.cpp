#include "middleware/AdminAuthMiddleware.h"

#include <nlohmann/json.hpp>

#include "Logging/Logger.h"

namespace http
{
namespace middleware
{

void AdminAuthMiddleware::before(HttpRequest& request)
{
    std::string path = request.path();

    // Only intercept /admin/* paths
    if (path.size() < 6 || path.substr(0, 6) != "/admin") return;

    std::string role = request.getHeader("X-Auth-Role");
    if (role != "admin")
    {
        SPDLOG_WARN_TAG("AUTH") << "Admin access denied for role='" << role << "' path=" << path;

        HttpResponse resp(false);
        resp.setStatusCode(HttpResponse::k403Forbidden);
        resp.setStatusMessage("Forbidden");
        resp.setContentType("application/json");

        nlohmann::json body;
        body["success"] = false;
        body["error"]["code"] = 403;
        body["error"]["message"] = "Admin privileges required";

        std::string bodyStr = body.dump();
        resp.setContentLength(bodyStr.size());
        resp.setBody(bodyStr);
        throw resp;
    }

    SPDLOG_DEBUG_TAG("AUTH") << "Admin access granted for path=" << path;
}

}  // namespace middleware
}  // namespace http
