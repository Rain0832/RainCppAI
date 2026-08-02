#include "storage/DbConnection.h"

#include "Common/Logging/Logger.h"
#include "storage/DbException.h"

namespace storage
{

DbConnection::DbConnection(const std::string& host,
                           const std::string& user,
                           const std::string& password,
                           const std::string& database)
    : host_(host), user_(user), password_(password), database_(database)
{
    try
    {
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
        conn_.reset(driver->connect(host_, user_, password_));
        if (conn_)
        {
            conn_->setSchema(database_);

            // 设置连接属性
            conn_->setClientOption("OPT_CONNECT_TIMEOUT", "10");
            conn_->setClientOption("multi_statements", "false");

            // 设置字符集
            std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
            stmt->execute("SET NAMES utf8mb4");

            SPDLOG_INFO_TAG("DB") << "Database connection established";
        }
    }
    catch (const sql::SQLException& e)
    {
        SPDLOG_ERROR_TAG("DB") << "Failed to create database connection: " << e.what();
        throw DbException(e.what());
    }
}

DbConnection::~DbConnection()
{
    try
    {
        cleanup();
    }
    catch (...)
    {
        // 析构函数中不抛出异常
    }
    SPDLOG_INFO_TAG("DB") << "Database connection closed";
}

bool DbConnection::ping()
{
    try
    {
        std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery("SELECT 1"));
        return true;
    }
    catch (const sql::SQLException& e)
    {
        SPDLOG_ERROR_TAG("DB") << "Ping failed: " << e.what();
        return false;
    }
}

int DbConnection::executeRawSql(const std::string& sql)
{
    std::lock_guard<std::mutex> lock(mutex_);
    try
    {
        std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
        stmt->execute(sql);
        return 0;
    }
    catch (const sql::SQLException& e)
    {
        SPDLOG_ERROR_TAG("DB") << "Raw SQL failed: " << e.what() << ", SQL: " << sql;
        throw DbException(e.what());
    }
}

bool DbConnection::isValid()
{
    try
    {
        if (!conn_) return false;
        std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
        stmt->execute("SELECT 1");
        return true;
    }
    catch (const sql::SQLException&)
    {
        return false;
    }
}

void DbConnection::reconnect()
{
    // 加锁防止多线程同时调用 reconnect() 导致 conn_ 竞态
    // (checkConnections 线程 + getConnection 使用方可能持有同一对象的 shared_ptr 副本)
    std::lock_guard<std::mutex> lock(mutex_);

    // 先关闭并释放旧连接
    if (conn_)
    {
        try
        {
            conn_->close();
        }
        catch (...)
        {
            // 忽略关闭时的错误
        }
        conn_.reset();  // 立即释放，防止 close 后状态残留
    }

    try
    {
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
        conn_.reset(driver->connect(host_, user_, password_));
        conn_->setSchema(database_);
        std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
        stmt->execute("SET NAMES utf8mb4");
    }
    catch (const sql::SQLException& e)
    {
        // 连接失败时 conn_ 已通过上面 reset() 置空，避免持有无效句柄
        conn_.reset();
        SPDLOG_ERROR_TAG("DB") << "Reconnect failed: " << e.what();
        throw DbException(e.what());
    }
}

void DbConnection::cleanup()
{
    std::lock_guard<std::mutex> lock(mutex_);
    try
    {
        if (conn_)
        {
            // 确保所有事务都已完成
            if (!conn_->getAutoCommit())
            {
                conn_->rollback();
                conn_->setAutoCommit(true);
            }

            // 清理所有未处理的结果集
            std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
            while (stmt->getMoreResults())
            {
                auto result = stmt->getResultSet();
                while (result && result->next())
                {
                    // 消费所有结果
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        SPDLOG_WARN_TAG("DB") << "Error cleaning up connection: " << e.what();
        try
        {
            reconnect();
        }
        catch (...)
        {
            // 忽略重连错误
        }
    }
}

}  // namespace storage
