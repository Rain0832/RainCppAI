#include <gtest/gtest.h>

#include "storage/DbConnectionPool.h"

TEST(DbConnectionPoolTest, SingletonExists)
{
    auto& pool = storage::DbConnectionPool::getInstance();
    // Pool exists even without init (just verify no crash)
    EXPECT_TRUE(true);
}
