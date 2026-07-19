#pragma once

#include <optional>
#include <string>

#include "3rdparty/JsonUtil.h"

namespace common
{

/// 统一 API 错误结构
struct ApiError
{
    int code;
    std::string message;
};

/// 统一 API 响应信封：{success, data?, error?{code, message}}
struct ApiResult
{
    bool success;
    json data;
    std::optional<ApiError> error;

    static ApiResult ok(json d = json::object()) { return {true, std::move(d), std::nullopt}; }
    static ApiResult fail(int code, std::string msg) { return {false, json::object(), ApiError{code, std::move(msg)}}; }

    json toJson() const
    {
        json j;
        j["success"] = success;
        if (error)
        {
            j["error"]["code"] = error->code;
            j["error"]["message"] = error->message;
        }
        else
        {
            j["data"] = data;
        }
        return j;
    }

    std::string dump() const { return toJson().dump(); }
};

}  // namespace common
