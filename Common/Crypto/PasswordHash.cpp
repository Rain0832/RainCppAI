#include "Common/Crypto/PasswordHash.h"
#include <sodium.h>
#include <stdexcept>

namespace common
{

std::string hashPassword(const std::string& plaintext)
{
    // argon2id 内存开销 256 MB，迭代 3 次（MODERATE 级别）
    // hash 输出格式：$argon2id$v=19$m=262144,t=3,p=1$<salt>$<hash>
    // 存入 accounts.password_hash VARCHAR(256)
    if (sodium_init() < 0)
        throw std::runtime_error("[PasswordHash] libsodium init failed");

    char hash[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(hash, plaintext.c_str(), plaintext.size(),
                          crypto_pwhash_OPSLIMIT_MODERATE,
                          crypto_pwhash_MEMLIMIT_MODERATE) != 0)
        throw std::runtime_error("[PasswordHash] argon2id hash failed — out of memory or thread limit");

    return std::string(hash);
}

bool verifyPassword(const std::string& plaintext, const std::string& hash)
{
    if (sodium_init() < 0)
        return false;
    return crypto_pwhash_str_verify(hash.c_str(), plaintext.c_str(), plaintext.size()) == 0;
}

}  // namespace common
