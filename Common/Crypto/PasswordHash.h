#pragma once
#include <string>

namespace common
{

/// argon2id password hashing via libsodium crypto_pwhash_str
/// @param plaintext 明文密码
/// @return argon2id hash string ($argon2id$v=19$m=...,t=3,p=1$...)
std::string hashPassword(const std::string& plaintext);

/// 验证明文密码与 argon2id hash 是否匹配
/// @param plaintext 用户输入的明文密码
/// @param hash      数据库中存储的 hash 字符串
/// @return true 匹配，false 不匹配
bool verifyPassword(const std::string& plaintext, const std::string& hash);

}  // namespace common
