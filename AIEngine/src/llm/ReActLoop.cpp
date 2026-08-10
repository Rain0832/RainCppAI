#include "llm/ReActLoop.h"

#include <chrono>
#include <future>
#include <thread>

namespace ai
{

namespace
{

/// 常量提示文案（与 v3.2.0 行为保持一致）
const char* const kMaxRoundsNotice = "[提示] 工具调用次数过多，请简化您的请求";
const char* const kTotalTimeoutNotice = "[提示] 任务执行超时，请稍后重试";
const char* const kLlmFailedNotice = "[错误] LLM 请求失败";
const char* const kEmptyContextNotice = "[错误] 消息上下文为空";

}  // namespace

ReActLoop::ReActLoop(std::shared_ptr<AIStrategy> strategy, const json& tools_schema, Options options)
    : strategy_(std::move(strategy)), tools_schema_(tools_schema), options_(options)
{
}

ReActLoop::FinalAnswer ReActLoop::run(const std::vector<Message>& messages,
                                      LlmRoundFn llm_round,
                                      ToolInvokeFn tool_invoke,
                                      EventCallbacks cbs)
{
    FinalAnswer answer;
    auto start = std::chrono::steady_clock::now();
    auto elapsedMs = [&start]()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start)
            .count();
    };

    // TokenRouter 默认回退到空回调，避免空指针
    TokenRouter router(cbs.on_text ? cbs.on_text : [](const std::string&) { return true; });
    ReActStateMachine sm(options_.max_rounds);
    sm.transition(ReActEvent::kStart);

    // 本地工作副本：ReActLoop 内部持续追加 tool 消息，不污染调用方
    std::vector<Message> working = messages;

    while (!sm.isTerminal())
    {
        // ── 总超时熔断（每轮开头检查）──────────────────────────────
        if (elapsedMs() > options_.total_timeout_ms)
        {
            sm.transition(ReActEvent::kTimeout);
            answer.timed_out = true;
            answer.content = kTotalTimeoutNotice;
            router.onNotice(answer.content);
            if (cbs.on_notice) cbs.on_notice(answer.content);
            break;
        }

        // ── 空上下文保护 ───────────────────────────────────────────
        if (working.empty())
        {
            sm.transition(ReActEvent::kError);
            answer.llm_failed = true;
            answer.content = kEmptyContextNotice;
            router.onNotice(answer.content);
            if (cbs.on_notice) cbs.on_notice(answer.content);
            break;
        }

        // ── Thinking：轮次递增 + 构建 payload + 流式请求 ──────────
        sm.advanceRound();
        answer.total_rounds = sm.roundCount();

        json payload = strategy_->buildRequest(
            working, tools_schema_,
            options_.model_name.empty() ? strategy_->getModel() : options_.model_name);
        payload["stream"] = true;

        std::string raw_response;
        try
        {
            raw_response = llm_round(
                payload, [&router](const std::string& token) { return router.onTextToken(token); });
        }
        catch (const std::exception& e)
        {
            sm.transition(ReActEvent::kError);
            answer.llm_failed = true;
            answer.content = std::string(kLlmFailedNotice) + ": " + e.what();
            router.onNotice(answer.content);
            if (cbs.on_notice) cbs.on_notice(answer.content);
            break;
        }

        RoundResponse round = parseRoundResponse(raw_response);

        // ── 纯文本：流式完成 ───────────────────────────────────────
        if (!round.hasToolCalls())
        {
            sm.transition(ReActEvent::kStreamComplete);
            answer.content = round.content;
            answer.new_messages.push_back(
                {"assistant", round.content, strategy_->getModel(), "", nowMs()});
            break;
        }

        // ── 工具循环熔断：轮次已达上限仍请求工具调用 ────────────────
        if (sm.maxRoundsReached())
        {
            sm.transition(ReActEvent::kMaxRoundsExceeded);
            answer.max_rounds_exceeded = true;
            answer.content = kMaxRoundsNotice;
            // 保留本轮 tool_calls 消息，避免上下文断裂
            answer.new_messages.push_back(
                {"assistant", round.assistantToolCallsJson().dump(), strategy_->getModel(), "tool_calls", nowMs()});
            router.onNotice(answer.content);
            if (cbs.on_notice) cbs.on_notice(answer.content);
            break;
        }

        // ── Acting：记录工具调用，挂起输出流 ───────────────────────
        sm.transition(ReActEvent::kToolCallDetected);
        router.suspend();

        if (cbs.on_tool_call)
        {
            for (const auto& tc : round.tool_calls)
            {
                cbs.on_tool_call(tc.name, tc.arguments);
            }
        }

        json assistant_tc_json = round.assistantToolCallsJson();
        Message assistant_tc_msg{"assistant", assistant_tc_json.dump(), strategy_->getModel(), "tool_calls", nowMs()};
        working.push_back(assistant_tc_msg);
        answer.new_messages.push_back(std::move(assistant_tc_msg));

        // ── Observing：逐个执行工具（带单次超时）────────────────────
        sm.transition(ReActEvent::kToolDispatched);
        for (const auto& tc : round.tool_calls)
        {
            json error_out;
            json result = invokeToolWithTimeout(tc.name, tc.arguments, tool_invoke, error_out);
            if (!error_out.is_null())
            {
                result = std::move(error_out);  // {"error": "..."} 注入为 Observation
            }
            router.onToolResult(tc.name, result);
            if (cbs.on_tool_result) cbs.on_tool_result(tc.name, result);

            Message tool_msg{"tool", result.dump(), "", tc.id, 0};
            working.push_back(tool_msg);
            answer.new_messages.push_back(std::move(tool_msg));
        }

        // 成功或失败均视为 ObservationReady，回到 Thinking 开启下一轮
        sm.transition(ReActEvent::kObservationReady);
        router.resume();
    }

    answer.total_duration_ms = elapsedMs();
    return answer;
}

ReActLoop::RoundResponse ReActLoop::parseRoundResponse(const std::string& raw_response) const
{
    RoundResponse round;
    try
    {
        json full = json::parse(raw_response);
        round.tool_calls = strategy_->parseToolCalls(full);
        if (full.contains("choices") && !full["choices"].empty())
        {
            const auto& msg = full["choices"][0]["message"];
            if (msg.contains("content") && !msg["content"].is_null())
            {
                round.content = msg["content"].get<std::string>();
            }
        }
    }
    catch (...)
    {
        // 解析失败：视为纯文本（与 v3.2.0 行为一致）
        round.content = raw_response;
    }
    return round;
}

json ReActLoop::RoundResponse::assistantToolCallsJson() const
{
    json tc_arr = json::array();
    for (const auto& tc : tool_calls)
    {
        json obj;
        obj["id"] = tc.id;
        obj["type"] = "function";
        obj["function"]["name"] = tc.name;
        obj["function"]["arguments"] = tc.arguments.dump();
        tc_arr.push_back(std::move(obj));
    }
    return tc_arr;
}

json ReActLoop::invokeToolWithTimeout(const std::string& name,
                                      const json& args,
                                      ToolInvokeFn invoke,
                                      json& error_out)
{
    error_out = json();  // null：表示成功

    // packaged_task 包裹阻塞工具调用；detach 的 worker 线程保证超时后不阻塞调用方。
    // 工具调用本身有界（MCP client 内部带 poll 超时），后台线程完成任务后自然退出。
    std::packaged_task<json()> task([invoke, name, args]() { return invoke(name, args); });
    auto future = task.get_future();
    std::thread worker(std::move(task));
    worker.detach();

    if (future.wait_for(std::chrono::milliseconds(options_.tool_timeout_ms)) == std::future_status::timeout)
    {
        error_out = json{{"error", "tool execution timeout (> " + std::to_string(options_.tool_timeout_ms) +
                                       "ms): " + name}};
        return json();
    }

    try
    {
        return future.get();
    }
    catch (const std::exception& e)
    {
        error_out = json{{"error", std::string("tool execution failed: ") + e.what()}};
        return json();
    }
}

long long ReActLoop::nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace ai
