#include "llm/ReActStateMachine.h"

namespace ai
{

ReActStateMachine::ReActStateMachine(int max_rounds) : max_rounds_(max_rounds > 0 ? max_rounds : 1) {}

bool ReActStateMachine::transition(ReActEvent event)
{
    // 终结态拒绝一切后续事件
    if (isTerminal())
    {
        return false;
    }

    switch (state_)
    {
    case ReActState::kIdle:
        if (event == ReActEvent::kStart)
        {
            state_ = ReActState::kThinking;
            return true;
        }
        break;

    case ReActState::kThinking:
        switch (event)
        {
        case ReActEvent::kToolCallDetected:
            state_ = ReActState::kActing;
            return true;
        case ReActEvent::kStreamComplete:
            state_ = ReActState::kDone;
            return true;
        case ReActEvent::kMaxRoundsExceeded:
            state_ = ReActState::kDone;  // 熔断：视为可接受的终止
            return true;
        case ReActEvent::kTimeout:
        case ReActEvent::kError:
            state_ = ReActState::kError;
            return true;
        default:
            break;
        }
        break;

    case ReActState::kActing:
        switch (event)
        {
        case ReActEvent::kToolDispatched:
            state_ = ReActState::kObserving;
            return true;
        case ReActEvent::kTimeout:
        case ReActEvent::kError:
            state_ = ReActState::kError;
            return true;
        default:
            break;
        }
        break;

    case ReActState::kObserving:
        switch (event)
        {
        case ReActEvent::kObservationReady:
            // 工具结果（成功或失败）注入后回到 Thinking，开启下一轮
            state_ = ReActState::kThinking;
            return true;
        case ReActEvent::kTimeout:
        case ReActEvent::kError:
            state_ = ReActState::kError;
            return true;
        default:
            break;
        }
        break;

    default:
        break;
    }

    return false;
}

void ReActStateMachine::advanceRound()
{
    ++round_count_;
}

void ReActStateMachine::reset()
{
    state_ = ReActState::kIdle;
    round_count_ = 0;
}

}  // namespace ai
