#pragma once
#include <iostream>
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include <regex>
#include <vector>

#include "RouterHandler.h"
#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"

namespace http
{
    namespace router
    {

        // 选择注册对象式的路由处理器还是注册回调函数式的处理器取决于处理器执行的复杂程度
        // 如果是简单的处理可以注册回调函数，否则注册对象式路由处理器(对象中可封装多个相关函数)
        // 二者注册其一即可
        class Router
        {
        public:
            using HandlerPtr = std::shared_ptr<RouterHandler>;
            using HandlerCallback = std::function<void(const HttpRequest &, HttpResponse *)>;

            // 路由键（请求方法 + URI）
            struct RouteKey
            {
                HttpRequest::Method method;
                std::string path;

                // 比较两个路由键是否相等。
                //
                // Args:
                //   other: 需要比较的另一个路由键。
                //
                // Returns:
                //   如果方法和路径都相同则返回 true，否则返回 false。
                bool operator==(const RouteKey &other) const
                {
                    return method == other.method && path == other.path;
                }
            };

            // 为 RouteKey 定义哈希函数
            struct RouteKeyHash
            {
                // size_t operator()(const RouteKey& key) const
                // {
                //     return std::hash<int>{}(static_cast<int>(key.method)) ^
                //            std::hash<std::string>{}(key.path);
                // }
                // 计算路由键的哈希值，用于 unordered_map 索引。
                //
                // Args:
                //   key: 需要计算哈希的路由键。
                //
                // Returns:
                //   结合方法和路径后的哈希值。
                size_t operator()(const RouteKey &key) const
                {
                    +size_t methodHash = std::hash<int>{}(static_cast<int>(key.method));
                    size_t pathHash = std::hash<std::string>{}(key.path);
                    return methodHash * 31 + pathHash;
                }
            };

            // 注册路由处理器
            //
            // Args:
            //   method: HTTP 请求方法。
            //   path: 需要精确匹配的路径。
            //   handler: 处理该路由的处理器对象。
            void registerHandler(HttpRequest::Method method, const std::string &path, HandlerPtr handler);

            // 注册回调函数形式的处理器
            //
            // Args:
            //   method: HTTP 请求方法。
            //   path: 需要精确匹配的路径。
            //   callback: 处理该路由的回调函数。
            void registerCallback(HttpRequest::Method method, const std::string &path, const HandlerCallback &callback);

            // 注册动态路由处理器
            //
            // Args:
            //   method: HTTP 请求方法。
            //   path: 含路径参数的模式字符串，例如 "/users/:id"。
            //   handler: 处理该路由的处理器对象。
            void addRegexHandler(HttpRequest::Method method, const std::string &path, HandlerPtr handler)
            {
                std::regex pathRegex = convertToRegex(path);
                regexHandlers_.emplace_back(method, pathRegex, handler);
            }

            // 注册动态路由处理函数
            //
            // Args:
            //   method: HTTP 请求方法。
            //   path: 含路径参数的模式字符串，例如 "/users/:id"。
            //   callback: 处理该路由的回调函数。
            void addRegexCallback(HttpRequest::Method method, const std::string &path, const HandlerCallback &callback)
            {
                std::regex pathRegex = convertToRegex(path);
                regexCallbacks_.emplace_back(method, pathRegex, callback);
            }

            // 处理请求
            //
            // Args:
            //   req: 输入的 HTTP 请求。
            //   resp: 输出的 HTTP 响应指针。
            //
            // Returns:
            //   如果找到匹配路由并完成处理则返回 true，否则返回 false。
            bool route(const HttpRequest &req, HttpResponse *resp);

        private:
            // 将路径模式转换为正则表达式，支持匹配路径参数。
            //
            // Args:
            //   pathPattern: 含路径参数的模式字符串。
            //
            // Returns:
            //   用于匹配实际路径的正则表达式。
            std::regex convertToRegex(const std::string &pathPattern)
            { // 将路径模式转换为正则表达式，支持匹配任意路径参数
                std::string regexPattern = "^" + std::regex_replace(pathPattern, std::regex(R"(/:([^/]+))"), R"(/([^/]+))") + "$";
                return std::regex(regexPattern);
            }

            // 提取路径参数并写入请求对象。
            //
            // Args:
            //   match: 正则匹配结果。
            //   request: 需要写入参数的请求对象。
            void extractPathParameters(const std::smatch &match, HttpRequest &request)
            {
                // Assuming the first match is the full path, parameters start from index 1
                for (size_t i = 1; i < match.size(); ++i)
                {
                    request.setPathParameters("param" + std::to_string(i), match[i].str());
                }
            }

        private:
            struct RouteCallbackObj
            {
                HttpRequest::Method method_;
                std::regex pathRegex_;
                HandlerCallback callback_;
                RouteCallbackObj(HttpRequest::Method method, std::regex pathRegex, const HandlerCallback &callback)
                    : method_(method), pathRegex_(pathRegex), callback_(callback) {}
            };

            struct RouteHandlerObj
            {
                HttpRequest::Method method_;
                std::regex pathRegex_;
                HandlerPtr handler_;
                RouteHandlerObj(HttpRequest::Method method, std::regex pathRegex, HandlerPtr handler)
                    : method_(method), pathRegex_(pathRegex), handler_(handler) {}
            };

            std::unordered_map<RouteKey, HandlerPtr, RouteKeyHash> handlers_;       // 精准匹配
            std::unordered_map<RouteKey, HandlerCallback, RouteKeyHash> callbacks_; // 精准匹配
            std::vector<RouteHandlerObj> regexHandlers_;                            // 正则匹配
            std::vector<RouteCallbackObj> regexCallbacks_;                          // 正则匹配
        };

    } // namespace router
} // namespace http