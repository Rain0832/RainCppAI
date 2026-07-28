#pragma once

#include <string>

namespace common
{

/// Thread-local log context for request-level metadata.
/// Set by RequestIdMiddleware (SP 4.2), consumed by Logger for automatic field injection.
struct LogContext
{
    std::string req_id;
    std::string user_id;
};

extern thread_local LogContext tls_log_ctx;

inline void setLogContext(const std::string& req_id, const std::string& user_id = "")
{
    tls_log_ctx.req_id = req_id;
    tls_log_ctx.user_id = user_id;
}

inline void clearLogContext()
{
    tls_log_ctx.req_id.clear();
    tls_log_ctx.user_id.clear();
}

}  // namespace common
