#pragma once

#include "../Middleware.h"
#include "../../http/HttpRequest.h"
#include "../../http/HttpResponse.h"
#include "CorsConfig.h"

namespace http
{
    namespace middleware
    {

        class CorsMiddleware : public Middleware
        {
        public:
            /**
             * @brief 构造 CORS 中间件。
             * @param config CORS 配置对象。
             */
            explicit CorsMiddleware(const CorsConfig &config = CorsConfig::defaultConfig());

            /**
             * @brief 在请求进入时执行 CORS 相关处理。
             * @param request HTTP 请求对象。
             */
            void before(HttpRequest &request) override;

            /**
             * @brief 在响应返回前追加 CORS 响应头。
             * @param response HTTP 响应对象。
             */
            void after(HttpResponse &response) override;

            /**
             * @brief 将字符串数组按指定分隔符拼接。
             * @param strings 待拼接的字符串数组。
             * @param delimiter 分隔符。
             * @return 拼接后的字符串。
             */
            std::string join(const std::vector<std::string> &strings, const std::string &delimiter);

        private:
            /**
             * @brief 判断来源是否允许访问。
             * @param origin 请求来源。
             * @return 允许访问返回 true，否则返回 false。
             */
            bool isOriginAllowed(const std::string &origin) const;

            /**
             * @brief 处理预检请求并写入必要响应。
             * @param request HTTP 请求对象。
             * @param response HTTP 响应对象。
             */
            void handlePreflightRequest(const HttpRequest &request, HttpResponse &response);

            /**
             * @brief 为响应添加 CORS 头信息。
             * @param response HTTP 响应对象。
             * @param origin 请求来源。
             */
            void addCorsHeaders(HttpResponse &response, const std::string &origin);

        private:
            CorsConfig config_;
        };

    } // namespace middleware
} // namespace http