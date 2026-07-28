#include <gtest/gtest.h>

#include "Common/Config/ConfigManager.h"

TEST(ConfigManagerTest, LoadAndGet)
{
    auto& cfg = common::ConfigManager::instance();
    // Test default fallback (no config file loaded)
    EXPECT_EQ(cfg.get("server.port", "80"), "80");
    EXPECT_EQ(cfg.getInt("server.threads", 4), 4);
}

TEST(ConfigManagerTest, DefaultValues)
{
    auto& cfg = common::ConfigManager::instance();
    EXPECT_EQ(cfg.get("nonexistent.key", "default"), "default");
    EXPECT_EQ(cfg.getInt("nonexistent.key", 999), 999);
}
