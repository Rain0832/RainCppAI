#include "middleware/RequestIdMiddleware.h"
#include "Common/Logging/LogContext.h"
#include <fstream>
#include <sstream>
#include <chrono>

namespace http { namespace middleware {

std::string RequestIdMiddleware::generateUuid()
{
    std::ifstream f("/proc/sys/kernel/random/uuid");
    if (f) {
        std::string uuid;
        std::getline(f, uuid);
        if (!uuid.empty()) return uuid;
    }
    auto now = std::chrono::system_clock::now();
    auto ts = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
    return "req-" + std::to_string(ts);
}

void RequestIdMiddleware::before(HttpRequest& request)
{
    std::string req_id = request.getHeader("X-Request-Id");
    if (req_id.empty()) {
        req_id = generateUuid();
        request.addHeader("X-Request-Id", req_id);
    }
    std::string uid = request.getHeader("X-Auth-UserId");
    common::setLogContext(req_id, uid);
}

void RequestIdMiddleware::after(HttpResponse& response)
{
    if (!common::tls_log_ctx.req_id.empty())
        response.addHeader("X-Request-Id", common::tls_log_ctx.req_id);
    common::clearLogContext();
}

}} // namespace http::middleware
