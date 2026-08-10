#pragma once

namespace ai
{

/// ReAct Agent 状态
enum class ReActState
{
    kIdle,        ///< 初始态
    kThinking,    ///< 正在向 LLM 发起流式请求（可产出文本或工具调用）
    kActing,      ///< 已识别工具调用，准备执行
    kObserving,   ///< 工具执行中，等待 Observation
    kDone,        ///< 正常结束（最终回答完成）
    kError        ///< 不可恢复错误（总超时 / LLM 请求失败）
};

/// 状态转移事件
enum class ReActEvent
{
    kStart,              ///< 循环开始
    kToolCallDetected,   ///< 本轮响应包含工具调用
    kToolDispatched,     ///< 工具已派发执行
    kObservationReady,   ///< 工具结果（成功或失败）已就绪
    kStreamComplete,     ///< 本轮为纯文本，流式完成
    kMaxRoundsExceeded,  ///< 工具循环次数达到上限（熔断）
    kTimeout,            ///< 总超时（熔断）
    kError               ///< LLM 请求失败等不可恢复错误
};

/**
 * @brief ReAct Agent 状态机引擎
 *
 * 纯逻辑状态机，零 IO 依赖，可独立单元测试。
 * 状态流转规则：
 *   Idle --kStart--> Thinking
 *   Thinking --kToolCallDetected--> Acting
 *   Thinking --kStreamComplete--> Done
 *   Thinking --kMaxRoundsExceeded--> Done       (熔断)
 *   Acting --kToolDispatched--> Observing
 *   Observing --kObservationReady--> Thinking   (注入 Observation 后进入下一轮)
 *   Thinking/Acting/Observing --kTimeout--> Error
 *   Thinking/Acting/Observing --kError--> Error
 *
 * 轮次计数：由 ReActLoop 每轮 LLM 请求前调用 advanceRound() 递增；
 * maxRoundsReached() 供熔断判定（轮次已达上限且本轮仍需工具调用）。
 *
 * 线程安全：状态机实例仅被单个 ReActLoop 持有，单线程访问，无需加锁。
 */
class ReActStateMachine
{
public:
    explicit ReActStateMachine(int max_rounds = 5);

    /// 触发事件转移；非法转移（含终结态收事件）返回 false
    bool transition(ReActEvent event);

    /// 每轮 LLM 请求前调用，递增轮次计数
    void advanceRound();

    ReActState state() const
    {
        return state_;
    }
    bool isTerminal() const
    {
        return state_ == ReActState::kDone || state_ == ReActState::kError;
    }
    int roundCount() const
    {
        return round_count_;
    }

    /// 轮次是否已达上限（此时若再出现工具调用应触发熔断）
    bool maxRoundsReached() const
    {
        return round_count_ >= max_rounds_;
    }

    /// 重置到初始态（单次 run 内一般无需调用）
    void reset();

private:
    ReActState state_ = ReActState::kIdle;
    int max_rounds_ = 5;
    int round_count_ = 0;
};

}  // namespace ai
