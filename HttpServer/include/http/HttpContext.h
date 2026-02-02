#pragma once

#include <iostream>

#include <muduo/net/TcpServer.h>

#include "HttpRequest.h"

namespace http
{

class HttpContext 
{
public:
    enum HttpRequestParseState
    {
        kExpectRequestLine, // 解析请求行
        kExpectHeaders, // 解析请求头
        kExpectBody, // 解析请求体
        kGotAll, // 解析完成
    };
    
    HttpContext()
    : state_(kExpectRequestLine)
    {}

    // 从缓冲区解析 HTTP 请求数据，并更新内部请求状态。
    // 参数：
    // - buf：包含待解析字节流的缓冲区。
    // - receiveTime：接收数据的时间戳。
    // 返回值：解析成功或需要更多数据时返回 true；检测到语法错误时返回 false。
    bool parseRequest(muduo::net::Buffer* buf, muduo::Timestamp receiveTime);

    // 判断请求是否已完整解析。
    // 返回值：解析完成返回 true，否则返回 false。
    bool gotAll() const 
    { return state_ == kGotAll;  }

    // 重置解析状态并清空当前请求数据。
    void reset()
    {
        state_ = kExpectRequestLine;
        HttpRequest dummyData;
        request_.swap(dummyData);
    }

    // 获取已解析的请求（只读）。
    // 返回值：当前 HttpRequest 的常量引用。
    const HttpRequest& request() const
    { return request_;}

    // 获取已解析的请求（可修改）。
    // 返回值：当前 HttpRequest 的引用。
    HttpRequest& request()
    { return request_;}

private:
    // 解析请求行，填充方法、路径、查询参数与协议版本。
    // 参数：
    // - begin：请求行起始指针。
    // - end：请求行结束指针（不含 CRLF）。
    // 返回值：请求行有效返回 true，否则返回 false。
    bool processRequestLine(const char* begin, const char* end);
private:
    HttpRequestParseState state_;
    HttpRequest           request_;
};

} // namespace http