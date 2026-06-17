#pragma once
#include <cppconn/connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <muduo/base/Logging.h>
#include <mysql/mysql.h>
#include <mysql_driver.h>

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>

#include "DbException.h"

namespace storage
{

class DbConnection
{
public:
    DbConnection(const std::string& host, const std::string& user, const std::string& password,
                 const std::string& database);
    ~DbConnection();

    // 禁止拷贝
    DbConnection(const DbConnection&) = delete;
    DbConnection& operator=(const DbConnection&) = delete;

    bool isValid();
    void reconnect();
    void cleanup();

    template <typename... Args> sql::ResultSet* executeQuery(const std::string& sql, Args&&... args)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        try
        {
            std::unique_ptr<sql::PreparedStatement> stmt(conn_->prepareStatement(sql));
            bindParams(stmt.get(), 1, std::forward<Args>(args)...);
            return stmt->executeQuery();
        }
        catch (const sql::SQLException& e)
        {
            LOG_ERROR << "Query failed: " << e.what() << ", SQL: " << sql;
            throw DbException(e.what());
        }
    }

    template <typename... Args> int executeUpdate(const std::string& sql, Args&&... args)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        try
        {
            std::unique_ptr<sql::PreparedStatement> stmt(conn_->prepareStatement(sql));
            bindParams(stmt.get(), 1, std::forward<Args>(args)...);
            return stmt->executeUpdate();
        }
        catch (const sql::SQLException& e)
        {
            LOG_ERROR << "Update failed: " << e.what() << ", SQL: " << sql;
            throw DbException(e.what());
        }
    }

    bool ping();

    /// 执行原生 SQL 文本（走 sql::Statement 文本协议），
    /// 专用于 DDL（CREATE TABLE / DROP TABLE），绕过 Prepared Statement 二进制协议避免 Malformed packet
    int executeRawSql(const std::string& sql);

private:
    // 辅助函数：递归终止条件
    void bindParams(sql::PreparedStatement*, int) {}

    // 通用模板：转换为字符串绑定（fallback for unknown types）
    template <typename T, typename... Args>
    void bindParams(sql::PreparedStatement* stmt, int index, T&& value, Args&&... args)
    {
        stmt->setString(index, std::to_string(std::forward<T>(value)));
        bindParams(stmt, index + 1, std::forward<Args>(args)...);
    }

    // 特化 std::string (non-const lvalue)
    template <typename... Args>
    void bindParams(sql::PreparedStatement* stmt, int index, std::string& value, Args&&... args)
    {
        stmt->setString(index, value);
        bindParams(stmt, index + 1, std::forward<Args>(args)...);
    }

    // 特化 std::string (const lvalue)
    template <typename... Args>
    void bindParams(sql::PreparedStatement* stmt, int index, const std::string& value, Args&&... args)
    {
        stmt->setString(index, value);
        bindParams(stmt, index + 1, std::forward<Args>(args)...);
    }

    // 特化 const char*
    template <typename... Args>
    void bindParams(sql::PreparedStatement* stmt, int index, const char* value, Args&&... args)
    {
        stmt->setString(index, value);
        bindParams(stmt, index + 1, std::forward<Args>(args)...);
    }

    // 特化 int
    template <typename... Args> void bindParams(sql::PreparedStatement* stmt, int index, int value, Args&&... args)
    {
        stmt->setInt(index, value);
        bindParams(stmt, index + 1, std::forward<Args>(args)...);
    }

    // 特化 long long
    template <typename... Args>
    void bindParams(sql::PreparedStatement* stmt, int index, long long value, Args&&... args)
    {
        stmt->setInt64(index, value);
        bindParams(stmt, index + 1, std::forward<Args>(args)...);
    }

    // 特化 bool — 用于 is_deleted TINYINT(1)
    template <typename... Args> void bindParams(sql::PreparedStatement* stmt, int index, bool value, Args&&... args)
    {
        stmt->setBoolean(index, value);
        bindParams(stmt, index + 1, std::forward<Args>(args)...);
    }

    // 特化 std::nullptr_t — 用于 payload JSON NULL
    template <typename... Args> void bindParams(sql::PreparedStatement* stmt, int index, std::nullptr_t, Args&&... args)
    {
        stmt->setNull(index, sql::DataType::VARCHAR);
        bindParams(stmt, index + 1, std::forward<Args>(args)...);
    }

private:
    std::shared_ptr<sql::Connection> conn_;
    std::string host_;
    std::string user_;
    std::string password_;
    std::string database_;
    std::mutex mutex_;
};

}  // namespace storage