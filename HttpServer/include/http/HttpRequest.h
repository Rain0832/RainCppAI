#pragma once

#include <map>
#include <string>
#include <unordered_map>

#include <muduo/base/Timestamp.h>

namespace http
{

class HttpRequest
{
public:
    enum Method
    {
        kInvalid, kGet, kPost, kHead, kPut, kDelete, kOptions
    };
    
    HttpRequest()
        : method_(kInvalid)
        , version_("Unknown")
    {
    }
    
    // 设置请求的接收时间。
    // 参数：
    // - t：请求数据接收时间。
    void setReceiveTime(muduo::Timestamp t);

    // 获取请求的接收时间。
    // 返回值：请求数据接收时间。
    muduo::Timestamp receiveTime() const { return receiveTime_; }
    
    // 设置请求方法。
    // 参数：
    // - start：方法字符串起始指针。
    // - end：方法字符串结束指针。
    // 返回值：方法合法返回 true，否则返回 false。
    bool setMethod(const char* start, const char* end);

    // 获取请求方法枚举值。
    // 返回值：当前请求方法。
    Method method() const { return method_; }

    // 设置请求路径。
    // 参数：
    // - start：路径字符串起始指针。
    // - end：路径字符串结束指针。
    void setPath(const char* start, const char* end);

    // 获取请求路径。
    // 返回值：请求路径字符串。
    std::string path() const { return path_; }

    // 设置路径参数键值对。
    // 参数：
    // - key：参数名。
    // - value：参数值。
    void setPathParameters(const std::string &key, const std::string &value);

    // 获取路径参数值。
    // 参数：
    // - key：参数名。
    // 返回值：参数值，不存在时返回空字符串。
    std::string getPathParameters(const std::string &key) const;

    // 解析并设置查询参数。
    // 参数：
    // - start：查询字符串起始指针。
    // - end：查询字符串结束指针。
    void setQueryParameters(const char* start, const char* end);

    // 获取查询参数值。
    // 参数：
    // - key：参数名。
    // 返回值：参数值，不存在时返回空字符串。
    std::string getQueryParameters(const std::string &key) const;
    
    // 设置请求协议版本。
    // 参数：
    // - v：协议版本字符串。
    void setVersion(std::string v)
    {
        version_ = v;
    }

    // 获取请求协议版本。
    // 返回值：协议版本字符串。
    std::string getVersion() const
    {
        return version_;
    }
    
    // 添加请求头字段。
    // 参数：
    // - start：头字段名起始指针。
    // - colon：分隔冒号位置指针。
    // - end：头字段值结束指针。
    void addHeader(const char* start, const char* colon, const char* end);

    // 获取指定头字段值。
    // 参数：
    // - field：头字段名。
    // 返回值：字段值，不存在时返回空字符串。
    std::string getHeader(const std::string& field) const;

    // 获取所有请求头字段。
    // 返回值：请求头映射表的常量引用。
    const std::map<std::string, std::string>& headers() const
    { return headers_; }

    // 设置请求体内容。
    // 参数：
    // - body：请求体字符串。
    void setBody(const std::string& body) { content_ = body; }

    // 设置请求体内容。
    // 参数：
    // - start：请求体起始指针。
    // - end：请求体结束指针。
    void setBody(const char* start, const char* end) 
    { 
        if (end >= start) 
        {
            content_.assign(start, end - start); 
        }
    }
    
    // 获取请求体内容。
    // 返回值：请求体字符串。
    std::string getBody() const
    { return content_; }

    // 设置请求体长度。
    // 参数：
    // - length：请求体长度。
    void setContentLength(uint64_t length)
    { contentLength_ = length; }
    
    // 获取请求体长度。
    // 返回值：请求体长度。
    uint64_t contentLength() const
    { return contentLength_; }

    // 交换当前对象与目标对象的内容。
    // 参数：
    // - that：待交换的 HttpRequest 对象。
    void swap(HttpRequest& that);

private:
    Method                                       method_; // 请求方法
    std::string                                  version_; // http版本
    std::string                                  path_; // 请求路径
    std::unordered_map<std::string, std::string> pathParameters_; // 路径参数
    std::unordered_map<std::string, std::string> queryParameters_; // 查询参数
    muduo::Timestamp                             receiveTime_; // 接收时间
    std::map<std::string, std::string>           headers_; // 请求头
    std::string                                  content_; // 请求体
    uint64_t                                     contentLength_ { 0 }; // 请求体长度
};  

} // namespace http