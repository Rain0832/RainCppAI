#pragma once

#include <vector>
#include <memory>
#include "Middleware.h"

namespace http
{
    namespace middleware
    {

        class MiddlewareChain
        {
        public:
            /**
             * @brief 添加中间件到执行链。
             * @param middleware 中间件实例。
             */
            void addMiddleware(std::shared_ptr<Middleware> middleware);

            /**
             * @brief 顺序执行所有中间件的前置处理。
             * @param request HTTP 请求对象。
             */
            void processBefore(HttpRequest &request);

            /**
             * @brief 逆序执行所有中间件的后置处理。
             * @param response HTTP 响应对象。
             */
            void processAfter(HttpResponse &response);

        private:
            std::vector<std::shared_ptr<Middleware>> middlewares_;
        };

    } // namespace middleware
} // namespace http