#include "Common/Auth/JwtService.h"

#include <chrono>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <sstream>

#include "Common/Config/ConfigManager.h"

namespace common
{

JwtService::JwtService()
{
    auto& cfg = ConfigManager::instance();
    secret_ = cfg.get("jwt.secret", "dev-secret-change-in-production");
}

JwtService::JwtService(const std::string& secret) : secret_(secret) {}

std::string JwtService::base64UrlEncode(const std::string& data)
{
    BIO *bio, *b64;
    BUF_MEM* bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, data.data(), static_cast<int>(data.size()));
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);

    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);

    // Base64URL: replace + with -, / with _, remove padding =
    for (auto& c : result)
    {
        if (c == '+')
            c = '-';
        else if (c == '/')
            c = '_';
    }
    auto pos = result.find_last_not_of('=');
    if (pos != std::string::npos) result.resize(pos + 1);
    return result;
}

std::string JwtService::base64UrlDecode(const std::string& input)
{
    std::string tmp = input;
    for (auto& c : tmp)
    {
        if (c == '-')
            c = '+';
        else if (c == '_')
            c = '/';
    }
    // Add padding
    while (tmp.size() % 4) tmp += '=';

    BIO *bio, *b64;
    size_t decodeLen = tmp.size();
    std::string result(decodeLen, 0);
    bio = BIO_new_mem_buf(tmp.data(), static_cast<int>(tmp.size()));
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    int len = BIO_read(bio, result.data(), static_cast<int>(decodeLen));
    BIO_free_all(bio);
    if (len > 0) result.resize(static_cast<size_t>(len));
    return result;
}

std::string JwtService::hmacSha256(const std::string& data, const std::string& key)
{
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int mdLen = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()), reinterpret_cast<const unsigned char*>(data.data()),
         data.size(), md, &mdLen);
    return std::string(reinterpret_cast<char*>(md), mdLen);
}

std::string JwtService::sign(long long userId, const std::string& role, int ttlSec)
{
    auto now = std::chrono::system_clock::now();
    auto nowTs = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    auto expTs = nowTs + ttlSec;

    json header;
    header["alg"] = "HS256";
    header["typ"] = "JWT";

    json payload;
    payload["sub"] = userId;
    payload["role"] = role;
    payload["iat"] = nowTs;
    payload["exp"] = expTs;

    std::string headerB64 = base64UrlEncode(header.dump());
    std::string payloadB64 = base64UrlEncode(payload.dump());

    std::string signingInput = headerB64 + "." + payloadB64;
    std::string sig = hmacSha256(signingInput, secret_);
    std::string sigB64 = base64UrlEncode(sig);

    return signingInput + "." + sigB64;
}

json JwtService::verify(const std::string& token) const
{
    // Split into 3 parts
    auto dot1 = token.find('.');
    if (dot1 == std::string::npos) return {};
    auto dot2 = token.find('.', dot1 + 1);
    if (dot2 == std::string::npos) return {};

    std::string headerB64 = token.substr(0, dot1);
    std::string payloadB64 = token.substr(dot1 + 1, dot2 - dot1 - 1);
    std::string sigB64 = token.substr(dot2 + 1);

    // Verify signature
    std::string signingInput = headerB64 + "." + payloadB64;
    std::string expectedSig = hmacSha256(signingInput, secret_);
    std::string expectedSigB64 = base64UrlEncode(expectedSig);

    if (sigB64 != expectedSigB64) return {};

    // Decode payload
    std::string payloadJson = base64UrlDecode(payloadB64);
    json payload;
    try
    {
        payload = json::parse(payloadJson);
    }
    catch (...)
    {
        return {};
    }

    // Check expiration
    auto now = std::chrono::system_clock::now();
    auto nowTs = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    if (payload.contains("exp") && payload["exp"].get<long long>() < nowTs) return {};

    return payload;
}

}  // namespace common
