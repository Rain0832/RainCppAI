#pragma once

#include "../http/HttpResponse.h"
#include "Middleware.h"

namespace http
{
namespace middleware
{

/**
 * @brief 安全响应头中间件 — 为每个响应追加 CSP / HSTS / X-Frame-Options / X-Content-Type-Options / X-XSS-Protection
 *
 * 仅在 after() 阶段注入安全头，不拦截请求。
 */
class SecurityHeadersMiddleware : public Middleware
{
public:
    void before(HttpRequest&) override { /* no-op: 不拦截请求 */ }

    /**
     * @brief 在响应返回前追加安全 HTTP 头
     */
    void after(HttpResponse& response) override;
};

}  // namespace middleware
}  // namespace http
