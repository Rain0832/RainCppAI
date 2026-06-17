#pragma once
#include <string>

#include "DbConnectionPool.h"

namespace storage
{

class MysqlUtil
{
public:
    static void init(const std::string& host, const std::string& user, const std::string& password,
                     const std::string& database, size_t poolSize = 10)
    {
        storage::DbConnectionPool::getInstance().init(host, user, password, database, poolSize);
    }

    template <typename... Args> sql::ResultSet* executeQuery(const std::string& sql, Args&&... args)
    {
        auto conn = storage::DbConnectionPool::getInstance().getConnection();
        return conn->executeQuery(sql, std::forward<Args>(args)...);
    }

    template <typename... Args> int executeUpdate(const std::string& sql, Args&&... args)
    {
        auto conn = storage::DbConnectionPool::getInstance().getConnection();
        return conn->executeUpdate(sql, std::forward<Args>(args)...);
    }

    /// 执行原生 DDL SQL（CREATE TABLE / DROP TABLE 等），走文本协议，绕过 Prepared Statement
    int executeRawSql(const std::string& sql)
    {
        auto conn = storage::DbConnectionPool::getInstance().getConnection();
        return conn->executeRawSql(sql);
    }
};

}  // namespace storage