#pragma once

#include <string>

#include "JsonUtil.h"

namespace common
{

/// JWT token service using HS256 (HMAC-SHA256) via OpenSSL.
/// No external JWT library required.
class JwtService
{
public:
    /// Load secret from ConfigManager ("jwt.secret")
    JwtService();

    /// Use explicit secret
    explicit JwtService(const std::string& secret);

    /// Sign a JWT token for the given user.
    /// @param userId  User ID
    /// @param role    User role (user/admin/org)
    /// @param ttlSec  Token time-to-live in seconds (default 3600 = 1h)
    /// @return Signed JWT string (header.payload.signature)
    std::string sign(long long userId, const std::string& role, int ttlSec = 3600);

    /// Verify and decode a JWT token.
    /// @param token   The JWT string
    /// @return JSON with {sub, role, iat, exp} on success, empty JSON on failure
    json verify(const std::string& token) const;

    /// Get the secret (for use by AuthMiddleware)
    const std::string& secret() const
    {
        return secret_;
    }

private:
    static std::string base64UrlEncode(const std::string& data);
    static std::string base64UrlDecode(const std::string& input);
    static std::string hmacSha256(const std::string& data, const std::string& key);

    std::string secret_;
};

}  // namespace common
