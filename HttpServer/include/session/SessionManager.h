#pragma once

#include "SessionStorage.h"
#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"
#include <memory>
#include <random>

namespace http
{
    namespace session
    {

        class SessionManager
        {
        public:
            // 创建会话管理器并注入会话存储实现。
            //
            // Args:
            //   storage: 负责会话持久化的存储实现。
            explicit SessionManager(std::unique_ptr<SessionStorage> storage);

            // 从请求中获取或创建会话。
            //
            // Args:
            //   req: HTTP 请求对象。
            //   resp: HTTP 响应对象，用于回写 Set-Cookie。
            //
            // Returns:
            //   获取到的会话对象；若不存在则创建新的会话并返回。
            std::shared_ptr<Session> getSession(const HttpRequest &req, HttpResponse *resp);

            // 销毁指定会话。
            //
            // Args:
            //   sessionId: 需要销毁的会话 ID。
            void destroySession(const std::string &sessionId);

            // 清理过期会话。
            void cleanExpiredSessions();

            // 更新会话数据。
            //
            // Args:
            //   session: 需要保存的会话对象。
            void updateSession(std::shared_ptr<Session> session)
            {
                storage_->save(session);
            }

        private:
            // 生成新的会话 ID。
            //
            // Returns:
            //   生成的会话 ID 字符串。
            std::string generateSessionId();

            // 从请求头中提取会话 ID。
            //
            // Args:
            //   req: HTTP 请求对象。
            //
            // Returns:
            //   提取到的会话 ID；若不存在则返回空字符串。
            std::string getSessionIdFromCookie(const HttpRequest &req);

            // 将会话 ID 写入响应 Cookie。
            //
            // Args:
            //   sessionId: 需要写入的会话 ID。
            //   resp: HTTP 响应对象。
            void setSessionCookie(const std::string &sessionId, HttpResponse *resp);

        private:
            std::unique_ptr<SessionStorage> storage_;
            std::mt19937 rng_; // 用于生成随机会话id
        };

    } // namespace session
} // namespace http