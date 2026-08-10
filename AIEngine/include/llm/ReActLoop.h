#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "3rdparty/JsonUtil.h"
#include "common/Message.h"
#include "llm/AIStrategy.h"
#include "llm/ReActStateMachine.h"
#include "llm/TokenRouter.h"

namespace ai
{

/**
 * @brief ReAct Agent 循环执行器 — think → act → observe 状态机闭环
 *
 * 替代 AIHelper::chatStream() 中的硬编码 for 循环，统一编排：
 *   1. Thinking    — 调用 LlmRoundFn 发起流式 LLM 请求（token 经 TokenRouter 分流）；
 *   2. Acting      — 解析 tool_calls，逐个调用 ToolInvokeFn（带单次工具超时）；
 *   3. Observing   — 收集 Observation（成功结果或 {"error":...}），注入消息上下文，
 *                    回到 Thinking 开启下一轮，直到 LLM 返回纯文本或触发熔断。
 *
 * 熔断机制（三重）：
 *   - 工具循环熔断：max_rounds 轮后仍请求工具调用 → 终止并给出提示；
 *   - 单次工具超时：tool_timeout_ms 内未返回 → 注入 {"error":"tool timeout"} 让 LLM 降级；
 *   - 总超时熔断：total_timeout_ms 内未完成 → 强制终止（保证主 Reactor 线程绝对安全）。
 *
 * 线程安全：run() 必须在单个调用线程内执行（AIHelper 在线程池中调用）；
 * 内部状态机 / TokenRouter 均为该线程独占，无跨线程共享。
 *
 * 解耦设计：通过 LlmRoundFn / ToolInvokeFn 函数对象注入 IO 依赖，
 * 单元测试无需链接 curl / MCP，直接注入 fake 实现即可验证完整闭环。
 */
class ReActLoop
{
public:
    using StreamCallback = std::function<bool(const std::string& chunk)>;

    /// 单轮 LLM 流式调用：payload → 累积的完整响应字符串（含 tool_calls）
    using LlmRoundFn = std::function<std::string(const json& payload, const StreamCallback& on_chunk)>;

    /// 工具调用：name + args → 结果 JSON（异常应抛出，由 ReActLoop 捕获注入 error）
    using ToolInvokeFn = std::function<json(const std::string& name, const json& args)>;

    struct Options
    {
        int max_rounds = 5;             ///< 最大工具调用轮次（熔断阈值）
        int tool_timeout_ms = 30000;    ///< 单次工具执行超时
        int total_timeout_ms = 120000;  ///< 整个 ReAct 循环总超时
        std::string model_name;         ///< 前端传入的模型名（空时使用策略默认模型）
    };

    /// 事件回调（前端 SSE 协议 / 业务日志）
    struct EventCallbacks
    {
        StreamCallback on_text = nullptr;  ///< 文本 token → 前端
        std::function<void(const std::string& name, const json& args)> on_tool_call = nullptr;
        std::function<void(const std::string& name, const json& result)> on_tool_result = nullptr;
        std::function<void(const std::string& message)> on_notice = nullptr;
    };

    /// 一次 run() 的最终结果
    struct FinalAnswer
    {
        std::string content;                ///< 最终纯文本回复（含熔断提示）
        std::vector<Message> new_messages;  ///< 本 run 新增消息（assistant tool_calls / tool / assistant 文本）
        int total_rounds = 0;               ///< 实际 LLM 请求轮数
        long long total_duration_ms = 0;    ///< 总耗时
        bool max_rounds_exceeded = false;   ///< 工具循环熔断
        bool timed_out = false;             ///< 总超时熔断
        bool llm_failed = false;            ///< LLM 请求失败（不可恢复）
    };

    /**
     * @param options 熔断/超时配置（无默认值：嵌套类型在类内作默认参数受限，调用方显式传入）
     */
    ReActLoop(std::shared_ptr<AIStrategy> strategy, const json& tools_schema, Options options);

    /**
     * @brief 执行完整 ReAct 循环
     *
     * @param messages 消息上下文快照（AIHelper 已注入 system prompt 与工具结果历史）
     * @param llm_round 单轮 LLM 流式调用函数
     * @param tool_invoke 工具调用函数（MCP 路由）
     * @param cbs 事件回调
     */
    FinalAnswer run(const std::vector<Message>& messages,
                    LlmRoundFn llm_round,
                    ToolInvokeFn tool_invoke,
                    EventCallbacks cbs);

private:
    /// 单轮 LLM 响应解析结果
    struct RoundResponse
    {
        std::string content;
        std::vector<ToolCallInfo> tool_calls;
        bool hasToolCalls() const
        {
            return !tool_calls.empty();
        }
        /// 构造 assistant 消息的 tool_calls JSON（OpenAI Function Calling 数组格式）
        json assistantToolCallsJson() const;
    };

    RoundResponse parseRoundResponse(const std::string& raw_response) const;

    /// 带超时的工具调用；超时/异常时向 error_out 写入 {"error":...} 并返回空对象
    json invokeToolWithTimeout(const std::string& name, const json& args, ToolInvokeFn invoke, json& error_out);

    static long long nowMs();

    std::shared_ptr<AIStrategy> strategy_;
    json tools_schema_;
    Options options_;
};

}  // namespace ai
