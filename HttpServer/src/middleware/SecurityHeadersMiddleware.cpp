#include "middleware/SecurityHeadersMiddleware.h"

namespace http
{
namespace middleware
{

void SecurityHeadersMiddleware::after(HttpResponse& response)
{
    // Content-Security-Policy：只允许同源资源，禁止 inline script（防 XSS）
    response.addHeader("Content-Security-Policy",
                       "default-src 'self'; "
                       "script-src 'self' 'unsafe-inline' https://cdn.jsdelivr.net; "
                       "style-src 'self' 'unsafe-inline'; "
                       "img-src 'self' data:; "
                       "font-src 'self'; "
                       "connect-src 'self'");

    // HSTS：强制 HTTPS（max-age=1年，含子域，允许预加载）
    response.addHeader("Strict-Transport-Security", "max-age=31536000; includeSubDomains; preload");

    // 禁止被其他网站嵌套为 iframe（防 clickjacking）
    response.addHeader("X-Frame-Options", "DENY");

    // 禁止浏览器 MIME-type 嗅探
    response.addHeader("X-Content-Type-Options", "nosniff");

    // 启用浏览器 XSS 过滤器
    response.addHeader("X-XSS-Protection", "1; mode=block");
}

}  // namespace middleware
}  // namespace http
