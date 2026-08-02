#include "storage/DbConnectionPool.h"

#include "Common/Logging/Logger.h"
#include "storage/DbException.h"

namespace storage
{

void DbConnectionPool::init(const std::string& host,
                            const std::string& user,
                            const std::string& password,
                            const std::string& database,
                            size_t poolSize)
{
    // 连接池会被多个线程访问，所以操作其成员变量时需要加锁
    std::lock_guard<std::mutex> lock(mutex_);
    // 确保只初始化一次
    if (initialized_)
    {
        return;
    }

    host_ = host;
    user_ = user;
    password_ = password;
    database_ = database;

    // 创建连接
    for (size_t i = 0; i < poolSize; ++i)
    {
        connections_.push(createConnection());
    }

    initialized_ = true;
    SPDLOG_INFO_TAG("DB") << "Database connection pool initialized with " << poolSize << " connections";
}

DbConnectionPool::DbConnectionPool()
{
    checkThread_ = std::thread(&DbConnectionPool::checkConnections, this);
}

DbConnectionPool::~DbConnectionPool()
{
    running_ = false;
    cv_.notify_all();  // 唤醒可能正在 wait 的检查线程
    if (checkThread_.joinable())
    {
        checkThread_.join();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    while (!connections_.empty())
    {
        connections_.pop();
    }
    SPDLOG_INFO_TAG("DB") << "Database connection pool destroyed";
}

// 修改获取连接的函数
std::shared_ptr<DbConnection> DbConnectionPool::getConnection()
{
    std::shared_ptr<DbConnection> conn;
    {
        std::unique_lock<std::mutex> lock(mutex_);

        // 等待超时 3 秒，防止高并发时线程池全部阻塞在此
        const auto timeout = std::chrono::seconds(3);
        bool acquired = cv_.wait_for(lock, timeout, [this] { return !connections_.empty(); });

        if (!acquired)
        {
            throw DbException("Connection pool timeout: no available connection within 3s");
        }
        if (!initialized_)
        {
            throw DbException("Connection pool not initialized");
        }

        conn = connections_.front();
        connections_.pop();
    }  // 释放锁

    try
    {
        // 在锁外检查连接
        if (!conn->ping())
        {
            SPDLOG_WARN_TAG("DB") << "Connection lost, attempting to reconnect...";
            conn->reconnect();
        }

        // 使用自定义删除器：归还连接到池而非 delete
        // 注意：lambda 按值捕获 conn，确保 DbConnection 对象在归还前不被析构
        auto connHolder = conn;  // 转移所有权到 lambda
        return std::shared_ptr<DbConnection>(connHolder.get(),
                                             [this, connHolder](DbConnection*)
                                             {
                                                 std::lock_guard<std::mutex> lock(mutex_);
                                                 connections_.push(connHolder);
                                                 cv_.notify_one();
                                             });
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR_TAG("DB") << "Failed to get connection: " << e.what();
        // 故障连接不放回池中，由 shared_ptr 析构自动释放
        throw;
    }
}

std::shared_ptr<DbConnection> DbConnectionPool::createConnection()
{
    return std::make_shared<DbConnection>(host_, user_, password_, database_);
}

// 修改检查连接的函数
void DbConnectionPool::checkConnections()
{
    while (running_.load())
    {
        try
        {
            std::vector<std::shared_ptr<DbConnection>> connsToCheck;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if (connections_.empty())
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }

                auto temp = connections_;
                while (!temp.empty())
                {
                    connsToCheck.push_back(temp.front());
                    temp.pop();
                }
            }

            // 在锁外检查连接
            for (auto& conn : connsToCheck)
            {
                if (!conn->ping())
                {
                    try
                    {
                        conn->reconnect();
                    }
                    catch (const std::exception& e)
                    {
                        SPDLOG_ERROR_TAG("DB") << "Failed to reconnect: " << e.what();
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
        catch (const std::exception& e)
        {
            SPDLOG_ERROR_TAG("DB") << "Error in check thread: " << e.what();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

}  // namespace storage
