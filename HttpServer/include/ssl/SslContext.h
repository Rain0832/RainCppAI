#pragma once
#include <muduo/base/noncopyable.h>
#include <openssl/ssl.h>

#include <memory>

#include "SslConfig.h"

namespace ssl {

class SslContext : muduo::noncopyable
{
public:
    explicit SslContext(const SslConfig& config);
    ~SslContext();

    bool initialize();
    SSL_CTX* getNativeHandle() { return ctx_; }

private:
    bool loadCertificates();
    bool setupProtocol();
    void setupSessionCache();
    static void handleSslError(const char* msg);

private:
    SSL_CTX* ctx_;      // SSL上下文
    SslConfig config_;  // SSL配置
};

}  // namespace ssl