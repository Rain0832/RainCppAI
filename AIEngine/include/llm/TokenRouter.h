#pragma once

#include <functional>
#include <mutex>
#include <string>

#include "3rdparty/JsonUtil.h"

namespace ai
{

/**
 * @brief SSE Token 智能分流器 — ReAct Agent 输出门面
 *
 * 职责：
 * 1. 文本 token 转发：正常模式下每个 LLM 文本 token 直接回调给前端；
 * 2. 输出流挂起：工具调用执行期间 suspend() 进入拦截模式，文本 token 缓冲
 *    而非转发，工具结果就绪后 resume() 一次性 flush，保证前端时序一致；
 * 3. 结构化事件：工具调用/结果/提示以统一 JSON 事件格式下发
 *    （{"type":"tool_call"|"tool_result"|"notice", ...}），
 *    前端对未知字段静默忽略，向后兼容。
 *
 * 线程安全：内部使用 mutex，可在多线程场景复用；
 * ReActLoop 内为单线程顺序调用，无竞争开销。
 */
class TokenRouter
{
public:
    /// 前端回调：返回 false 表示连接已断开，上游应终止输出
    using Callback = std::function<bool(const std::string& chunk)>;

    explicit TokenRouter(Callback frontend_cb);

    /**
     * @brief 处理 LLM 文本 token
     * @return false 表示前端连接断开，调用方应中止流
     */
    bool onTextToken(const std::string& token);

    /// 工具调用事件（name + args），格式化为 JSON 事件下发
    bool onToolCall(const std::string& name, const json& args);

    /// 工具结果事件（name + result），格式化为 JSON 事件下发
    bool onToolResult(const std::string& name, const json& result);

    /// 提示/错误信息（熔断、超时等），格式化为 JSON 事件下发
    bool onNotice(const std::string& message);

    /// 挂起输出流：后续文本 token 进入缓冲，不再转发前端
    void suspend();

    /// 恢复输出流：退出拦截模式并 flush 缓冲文本到前端
    void resume();

    /// 取消挂起并丢弃缓冲文本（错误路径）
    void cancel();

    bool suspended() const;

private:
    bool send(const std::string& chunk);
    void flushPending();

    Callback frontend_cb_;
    mutable std::mutex mutex_;
    bool suspended_ = false;
    std::string pending_text_;
};

}  // namespace ai
