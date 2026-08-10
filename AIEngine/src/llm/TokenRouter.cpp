#include "llm/TokenRouter.h"

namespace ai
{

TokenRouter::TokenRouter(Callback frontend_cb) : frontend_cb_(std::move(frontend_cb)) {}

bool TokenRouter::onTextToken(const std::string& token)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (suspended_)
    {
        // 拦截模式：缓冲文本，待 resume() 后一次性 flush
        pending_text_ += token;
        return true;
    }
    return send(token);
}

bool TokenRouter::onToolCall(const std::string& name, const json& args)
{
    json event;
    event["type"] = "tool_call";
    event["name"] = name;
    event["args"] = args;
    return send(event.dump());
}

bool TokenRouter::onToolResult(const std::string& name, const json& result)
{
    json event;
    event["type"] = "tool_result";
    event["name"] = name;
    event["ok"] = !result.contains("error");
    event["result"] = result;
    return send(event.dump());
}

bool TokenRouter::onNotice(const std::string& message)
{
    json event;
    event["type"] = "notice";
    event["message"] = message;
    return send(event.dump());
}

void TokenRouter::suspend()
{
    std::lock_guard<std::mutex> lock(mutex_);
    suspended_ = true;
}

void TokenRouter::resume()
{
    std::lock_guard<std::mutex> lock(mutex_);
    suspended_ = false;
    flushPending();
}

void TokenRouter::cancel()
{
    std::lock_guard<std::mutex> lock(mutex_);
    suspended_ = false;
    pending_text_.clear();
}

bool TokenRouter::suspended() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return suspended_;
}

bool TokenRouter::send(const std::string& chunk)
{
    if (frontend_cb_)
    {
        return frontend_cb_(chunk);
    }
    return true;
}

void TokenRouter::flushPending()
{
    if (pending_text_.empty() || !frontend_cb_)
    {
        pending_text_.clear();
        return;
    }
    std::string buffered;
    buffered.swap(pending_text_);
    frontend_cb_(buffered);
}

}  // namespace ai
