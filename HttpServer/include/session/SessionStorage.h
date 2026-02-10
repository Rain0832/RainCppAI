#pragma once
#include "Session.h"
#include <memory>

namespace http
{
    namespace session
    {

        class SessionStorage
        {
        public:
            // 析构函数。
            virtual ~SessionStorage() = default;

            // 保存会话数据。
            //
            // Args:
            //   session: 需要保存的会话对象。
            virtual void save(std::shared_ptr<Session> session) = 0;

            // 通过会话 ID 加载会话。
            //
            // Args:
            //   sessionId: 目标会话 ID。
            //
            // Returns:
            //   找到的会话对象；若不存在则返回空指针。
            virtual std::shared_ptr<Session> load(const std::string &sessionId) = 0;

            // 删除指定会话。
            //
            // Args:
            //   sessionId: 需要删除的会话 ID。
            virtual void remove(const std::string &sessionId) = 0;
        };

        // 基于内存的会话存储实现
        class MemorySessionStorage : public SessionStorage
        {
        public:
            // 保存会话数据到内存。
            //
            // Args:
            //   session: 需要保存的会话对象。
            void save(std::shared_ptr<Session> session) override;

            // 从内存中加载会话。
            //
            // Args:
            //   sessionId: 目标会话 ID。
            //
            // Returns:
            //   找到的会话对象；若不存在则返回空指针。
            std::shared_ptr<Session> load(const std::string &sessionId) override;

            // 从内存中移除会话。
            //
            // Args:
            //   sessionId: 需要删除的会话 ID。
            void remove(const std::string &sessionId) override;

        private:
            std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;
        };

    } // namespace session
} // namespace http