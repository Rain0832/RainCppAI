#pragma once

#include "Middleware.h"

namespace http
{
namespace middleware
{

/**
 * @brief Admin role authorization middleware.
 *
 * Must be registered AFTER AuthMiddleware in the middleware chain.
 * Reads X-Auth-Role header (injected by AuthMiddleware) and rejects
 * non-admin requests to /admin/* paths with HTTP 403.
 */
class AdminAuthMiddleware : public Middleware
{
public:
    AdminAuthMiddleware() = default;

    void before(HttpRequest& request) override;
    void after(HttpResponse& response) override {}
};

}  // namespace middleware
}  // namespace http
