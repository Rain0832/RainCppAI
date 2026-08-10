#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "llm/ReActLoop.h"
#include "llm/ReActStateMachine.h"
#include "llm/TokenRouter.h"

using namespace ai;

namespace
{

/// Fake LLM 策略：可编程返回响应序列，避免链接 ConfigManager / curl
class FakeStrategy : public AIStrategy
{
public:
    std::string getApiUrl() const override
    {
        return "http://fake";
    }
    std::string getApiKey() const override
    {
        return "fake-key";
    }
    void setApiKey(const std::string& key) override
    {
        (void)key;
    }
    std::string getModel() const override
    {
        return "fake-model";
    }

    json buildRequest(const std::vector<Message>& messages,
                      const json& tools = json::object(),
                      const std::string& modelName = "") const override
    {
        json payload;
        payload["model"] = modelName.empty() ? getModel() : modelName;
        payload["messages"] = json::array();
        for (const auto& m : messages)
        {
            json jm;
            jm["role"] = m.role;
            jm["content"] = m.content;
            payload["messages"].push_back(jm);
        }
        if (!tools.empty()) payload["tools"] = tools;
        return payload;
    }

    std::string parseResponse(const json& response) const override
    {
        if (response.contains("choices") && !response["choices"].empty())
        {
            const auto& msg = response["choices"][0]["message"];
            if (msg.contains("content") && !msg["content"].is_null()) return msg["content"].get<std::string>();
        }
        return {};
    }

    std::vector<ToolCallInfo> parseToolCalls(const json& response) const override
    {
        std::vector<ToolCallInfo> result;
        if (!response.contains("choices") || response["choices"].empty()) return result;
        const auto& msg = response["choices"][0]["message"];
        if (!msg.contains("tool_calls") || !msg["tool_calls"].is_array()) return result;
        for (const auto& tc : msg["tool_calls"])
        {
            ToolCallInfo info;
            info.id = tc.value("id", "");
            if (tc.contains("function"))
            {
                info.name = tc["function"].value("name", "");
                std::string args_str = tc["function"].value("arguments", "{}");
                try
                {
                    info.arguments = json::parse(args_str);
                }
                catch (...)
                {
                    info.arguments = json::object();
                }
            }
            if (!info.name.empty()) result.push_back(std::move(info));
        }
        return result;
    }
};

/// 构造 OpenAI 兼容的纯文本响应
std::string makeTextResponse(const std::string& content)
{
    json resp;
    resp["choices"] = json::array({json{{"message", json{{"role", "assistant"}, {"content", content}}}}});
    return resp.dump();
}

/// 构造 OpenAI 兼容的工具调用响应
std::string makeToolCallResponse(const std::string& id, const std::string& name, const json& args)
{
    json tc;
    tc["id"] = id;
    tc["type"] = "function";
    tc["function"]["name"] = name;
    tc["function"]["arguments"] = args.dump();
    json msg;
    msg["role"] = "assistant";
    msg["content"] = nullptr;
    msg["tool_calls"] = json::array({tc});
    json resp;
    resp["choices"] = json::array({json{{"message", msg}}});
    return resp.dump();
}

// ────────────────────────────────────────────────────────────────
// ReActStateMachine
// ────────────────────────────────────────────────────────────────

TEST(ReActStateMachineTest, HappyPathFlow)
{
    ReActStateMachine sm(5);
    EXPECT_EQ(sm.state(), ReActState::kIdle);

    EXPECT_TRUE(sm.transition(ReActEvent::kStart));
    EXPECT_EQ(sm.state(), ReActState::kThinking);

    EXPECT_TRUE(sm.transition(ReActEvent::kToolCallDetected));
    EXPECT_EQ(sm.state(), ReActState::kActing);
    EXPECT_TRUE(sm.transition(ReActEvent::kToolDispatched));
    EXPECT_EQ(sm.state(), ReActState::kObserving);
    EXPECT_TRUE(sm.transition(ReActEvent::kObservationReady));
    EXPECT_EQ(sm.state(), ReActState::kThinking);

    EXPECT_TRUE(sm.transition(ReActEvent::kStreamComplete));
    EXPECT_EQ(sm.state(), ReActState::kDone);
    EXPECT_TRUE(sm.isTerminal());
}

TEST(ReActStateMachineTest, InvalidTransitionRejected)
{
    ReActStateMachine sm(5);
    // Idle 直接 StreamComplete 非法
    EXPECT_FALSE(sm.transition(ReActEvent::kStreamComplete));
    EXPECT_EQ(sm.state(), ReActState::kIdle);

    EXPECT_TRUE(sm.transition(ReActEvent::kStart));
    // Thinking 直接 ToolDispatched 非法
    EXPECT_FALSE(sm.transition(ReActEvent::kToolDispatched));

    // 终结态拒绝一切事件
    EXPECT_TRUE(sm.transition(ReActEvent::kStreamComplete));
    EXPECT_FALSE(sm.transition(ReActEvent::kStart));
    EXPECT_FALSE(sm.transition(ReActEvent::kError));
}

TEST(ReActStateMachineTest, TimeoutTransitionsToError)
{
    ReActStateMachine sm(5);
    sm.transition(ReActEvent::kStart);
    EXPECT_TRUE(sm.transition(ReActEvent::kTimeout));
    EXPECT_EQ(sm.state(), ReActState::kError);
    EXPECT_TRUE(sm.isTerminal());
}

TEST(ReActStateMachineTest, MaxRoundsExceededTransitionsToDone)
{
    ReActStateMachine sm(2);
    EXPECT_TRUE(sm.transition(ReActEvent::kStart));
    sm.advanceRound();
    EXPECT_FALSE(sm.maxRoundsReached());
    sm.advanceRound();
    EXPECT_TRUE(sm.maxRoundsReached());
    EXPECT_TRUE(sm.transition(ReActEvent::kMaxRoundsExceeded));
    EXPECT_EQ(sm.state(), ReActState::kDone);
    EXPECT_TRUE(sm.isTerminal());
}

// ────────────────────────────────────────────────────────────────
// TokenRouter
// ────────────────────────────────────────────────────────────────

TEST(TokenRouterTest, ForwardsTokensWhenNotSuspended)
{
    std::vector<std::string> received;
    TokenRouter router(
        [&received](const std::string& chunk)
        {
            received.push_back(chunk);
            return true;
        });
    EXPECT_TRUE(router.onTextToken("你"));
    EXPECT_TRUE(router.onTextToken("好"));
    ASSERT_EQ(received.size(), 2u);
    EXPECT_EQ(received[0], "你");
    EXPECT_EQ(received[1], "好");
}

TEST(TokenRouterTest, SuspendBuffersThenResumeFlushes)
{
    std::vector<std::string> received;
    TokenRouter router(
        [&received](const std::string& chunk)
        {
            received.push_back(chunk);
            return true;
        });
    router.suspend();
    EXPECT_TRUE(router.suspended());
    EXPECT_TRUE(router.onTextToken("隐藏"));
    EXPECT_TRUE(received.empty());  // 挂起期间不转发

    router.resume();
    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0], "隐藏");
    EXPECT_FALSE(router.suspended());
}

TEST(TokenRouterTest, CancelDiscardsBuffer)
{
    std::vector<std::string> received;
    TokenRouter router(
        [&received](const std::string& chunk)
        {
            received.push_back(chunk);
            return true;
        });
    router.suspend();
    router.onTextToken("丢弃");
    router.cancel();
    EXPECT_TRUE(received.empty());

    router.onTextToken("恢复");
    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0], "恢复");
}

TEST(TokenRouterTest, ToolCallEventFormat)
{
    std::vector<std::string> received;
    TokenRouter router(
        [&received](const std::string& chunk)
        {
            received.push_back(chunk);
            return true;
        });
    json args = {{"city", "北京"}};
    EXPECT_TRUE(router.onToolCall("get_weather", args));
    ASSERT_EQ(received.size(), 1u);
    json event = json::parse(received[0]);
    EXPECT_EQ(event["type"], "tool_call");
    EXPECT_EQ(event["name"], "get_weather");
    EXPECT_EQ(event["args"]["city"], "北京");
}

// ────────────────────────────────────────────────────────────────
// ReActLoop
// ────────────────────────────────────────────────────────────────

class ReActLoopTest : public ::testing::Test
{
protected:
    std::shared_ptr<FakeStrategy> strategy_{std::make_shared<FakeStrategy>()};
    json tools_schema_{json::array()};

    std::vector<std::string> responses_;
    int llm_call_count_ = 0;

    /// 可编程 LLM：按调用次序返回 responses_，并模拟流式逐字符回调文本 token
    ReActLoop::LlmRoundFn makeLlm()
    {
        return [this](const json& payload, const ReActLoop::StreamCallback& on_chunk) -> std::string
        {
            (void)payload;
            llm_call_count_++;
            if (llm_call_count_ > static_cast<int>(responses_.size())) return makeTextResponse("fallback");

            std::string raw = responses_[llm_call_count_ - 1];
            try
            {
                json resp = json::parse(raw);
                if (resp.contains("choices") && !resp["choices"].empty())
                {
                    const auto& msg = resp["choices"][0]["message"];
                    if (msg.contains("content") && msg["content"].is_string())
                    {
                        std::string text = msg["content"].get<std::string>();
                        for (char c : text)
                        {
                            if (!on_chunk(std::string(1, c))) break;
                        }
                    }
                }
            }
            catch (...)
            {
            }
            return raw;
        };
    }

    static json okTool(const std::string&, const json&)
    {
        return json{{"ok", true}};
    }
};

TEST_F(ReActLoopTest, PlainTextCompletes)
{
    responses_ = {makeTextResponse("今天天气很好")};
    ReActLoop loop(strategy_, tools_schema_, ReActLoop::Options{});
    auto result = loop.run({{"user", "你好", "", "", 0}}, makeLlm(), okTool, {});

    EXPECT_FALSE(result.max_rounds_exceeded);
    EXPECT_FALSE(result.timed_out);
    EXPECT_FALSE(result.llm_failed);
    EXPECT_EQ(result.content, "今天天气很好");
    EXPECT_EQ(llm_call_count_, 1);
    ASSERT_EQ(result.new_messages.size(), 1u);
    EXPECT_EQ(result.new_messages[0].role, "assistant");
    EXPECT_EQ(result.new_messages[0].content, "今天天气很好");
}

TEST_F(ReActLoopTest, ToolCallRoundTrip)
{
    json args = {{"city", "北京"}};
    responses_ = {makeToolCallResponse("call_1", "get_weather", args), makeTextResponse("北京今天晴")};
    std::vector<std::string> tool_names;
    ReActLoop loop(strategy_, tools_schema_, ReActLoop::Options{});
    auto result = loop.run({{"user", "北京天气？", "", "", 0}}, makeLlm(),
                           [&tool_names](const std::string& name, const json&)
                           {
                               tool_names.push_back(name);
                               return json{{"weather", "晴"}};
                           },
                           {});

    EXPECT_EQ(llm_call_count_, 2);
    ASSERT_EQ(tool_names.size(), 1u);
    EXPECT_EQ(tool_names[0], "get_weather");
    EXPECT_EQ(result.content, "北京今天晴");

    // new_messages: assistant tool_calls + tool + assistant 文本
    ASSERT_EQ(result.new_messages.size(), 3u);
    EXPECT_EQ(result.new_messages[0].role, "assistant");
    EXPECT_EQ(result.new_messages[0].tool_call_id, "tool_calls");
    EXPECT_EQ(result.new_messages[1].role, "tool");
    EXPECT_EQ(result.new_messages[1].tool_call_id, "call_1");
    EXPECT_EQ(result.new_messages[2].role, "assistant");
    EXPECT_EQ(result.new_messages[2].content, "北京今天晴");
}

TEST_F(ReActLoopTest, MultipleToolRounds)
{
    responses_ = {makeToolCallResponse("c1", "tool_a", json{{"n", 1}}),
                  makeToolCallResponse("c2", "tool_b", json{{"n", 2}}), makeTextResponse("完成")};
    std::vector<std::string> tool_names;
    ReActLoop loop(strategy_, tools_schema_, ReActLoop::Options{});
    auto result = loop.run({{"user", "hi", "", "", 0}}, makeLlm(),
                           [&tool_names](const std::string& name, const json&)
                           {
                               tool_names.push_back(name);
                               return json{{"ok", true}};
                           },
                           {});

    EXPECT_EQ(llm_call_count_, 3);
    ASSERT_EQ(tool_names.size(), 2u);
    EXPECT_EQ(tool_names[0], "tool_a");
    EXPECT_EQ(tool_names[1], "tool_b");
    // messages: assistant tc + tool + assistant tc + tool + assistant 文本
    ASSERT_EQ(result.new_messages.size(), 5u);
    EXPECT_EQ(result.content, "完成");
}

TEST_F(ReActLoopTest, ToolFailureInjectsErrorObservation)
{
    responses_ = {makeToolCallResponse("call_1", "get_weather", json::object()), makeTextResponse("工具不可用，抱歉")};
    ReActLoop loop(strategy_, tools_schema_, ReActLoop::Options{});
    auto result = loop.run({{"user", "hi", "", "", 0}}, makeLlm(),
                           [](const std::string&, const json&) -> json { throw std::runtime_error("boom"); }, {});

    EXPECT_EQ(llm_call_count_, 2);
    ASSERT_EQ(result.new_messages.size(), 3u);
    EXPECT_EQ(result.new_messages[1].role, "tool");
    json obs = json::parse(result.new_messages[1].content);
    EXPECT_TRUE(obs.contains("error"));
    EXPECT_EQ(result.content, "工具不可用，抱歉");
}

TEST_F(ReActLoopTest, ToolTimeoutInjectsErrorObservation)
{
    responses_ = {makeToolCallResponse("c1", "slow_tool", json::object()), makeTextResponse("超时降级")};
    ReActLoop::Options opts;
    opts.tool_timeout_ms = 50;
    ReActLoop loop(strategy_, tools_schema_, opts);
    auto result = loop.run({{"user", "hi", "", "", 0}}, makeLlm(),
                           [](const std::string&, const json&)
                           {
                               std::this_thread::sleep_for(std::chrono::milliseconds(500));
                               return json{{"ok", true}};
                           },
                           {});

    // 超时注入 {"error":...}，但循环继续到下一轮 LLM
    ASSERT_EQ(result.new_messages.size(), 3u);
    EXPECT_EQ(result.new_messages[1].role, "tool");
    json obs = json::parse(result.new_messages[1].content);
    EXPECT_TRUE(obs.contains("error"));
    EXPECT_EQ(llm_call_count_, 2);
    EXPECT_EQ(result.content, "超时降级");
}

TEST_F(ReActLoopTest, MaxRoundsCircuitBreaker)
{
    json args = {{"q", "x"}};
    responses_ = {makeToolCallResponse("c1", "tool_a", args)};
    ReActLoop::Options opts;
    opts.max_rounds = 1;
    ReActLoop loop(strategy_, tools_schema_, opts);
    auto result = loop.run({{"user", "hi", "", "", 0}}, makeLlm(), okTool, {});

    EXPECT_TRUE(result.max_rounds_exceeded);
    EXPECT_EQ(llm_call_count_, 1);
    // 熔断时仍保留本轮 assistant tool_calls 消息，避免上下文断裂
    ASSERT_EQ(result.new_messages.size(), 1u);
    EXPECT_EQ(result.new_messages[0].tool_call_id, "tool_calls");
    EXPECT_FALSE(result.content.empty());
}

TEST_F(ReActLoopTest, TotalTimeoutCircuitBreaker)
{
    responses_ = {makeToolCallResponse("c1", "tool_a", json::object()), makeTextResponse("done")};
    ReActLoop::Options opts;
    opts.total_timeout_ms = 50;
    ReActLoop loop(strategy_, tools_schema_, opts);
    auto result = loop.run({{"user", "hi", "", "", 0}},
                           [this](const json& payload, const ReActLoop::StreamCallback& on_chunk) -> std::string
                           {
                               (void)on_chunk;
                               std::this_thread::sleep_for(std::chrono::milliseconds(100));
                               llm_call_count_++;
                               return responses_.empty() ? makeTextResponse("x") : responses_[0];
                           },
                           okTool, {});

    EXPECT_TRUE(result.timed_out);
    EXPECT_EQ(llm_call_count_, 1);
    EXPECT_FALSE(result.content.empty());
}

TEST_F(ReActLoopTest, NoticeCallbackOnCircuitBreaker)
{
    json args = {{"q", "x"}};
    responses_ = {makeToolCallResponse("c1", "tool_a", args)};
    ReActLoop::Options opts;
    opts.max_rounds = 1;
    ReActLoop loop(strategy_, tools_schema_, opts);
    std::vector<std::string> notices;
    ReActLoop::EventCallbacks cbs;
    cbs.on_notice = [&notices](const std::string& msg) { notices.push_back(msg); };
    auto result = loop.run({{"user", "hi", "", "", 0}}, makeLlm(), okTool, cbs);

    EXPECT_TRUE(result.max_rounds_exceeded);
    ASSERT_EQ(notices.size(), 1u);
    EXPECT_FALSE(notices[0].empty());
}

TEST_F(ReActLoopTest, ToolCallbacksInvoked)
{
    json args = {{"city", "上海"}};
    responses_ = {makeToolCallResponse("c1", "get_weather", args), makeTextResponse("上海晴")};
    ReActLoop loop(strategy_, tools_schema_, ReActLoop::Options{});
    std::vector<std::string> calls;
    std::vector<std::string> results;
    ReActLoop::EventCallbacks cbs;
    cbs.on_tool_call = [&calls](const std::string& name, const json&) { calls.push_back(name); };
    cbs.on_tool_result = [&results](const std::string& name, const json&) { results.push_back(name); };
    auto result = loop.run({{"user", "hi", "", "", 0}}, makeLlm(), okTool, cbs);

    ASSERT_EQ(calls.size(), 1u);
    EXPECT_EQ(calls[0], "get_weather");
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], "get_weather");
}

}  // namespace
