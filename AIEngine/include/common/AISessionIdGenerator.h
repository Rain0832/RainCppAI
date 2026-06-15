#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>

/**
 * @brief 会话 ID 生成器 — 基于雪花算法（Snowflake）
 *
 * 64 位 ID 结构：
 *   - 41 bit 时间戳（毫秒级，从自定义起始时间算起，可支撑约 69 年）
 *   - 10 bit 机器 ID（0~1023）
 *   - 12 bit 序列号（0~4095，同一毫秒内递增）
 *
 * 线程安全，趋势递增，对 InnoDB B+Tree 聚簇索引友好。
 */
class AISessionIdGenerator
{
public:
    /// @param worker_id 机器 ID（0~1023），默认 1
    explicit AISessionIdGenerator(uint16_t worker_id = 1);

    /// 生成一个全新会话 ID（uint64 → string）
    std::string generate();

private:
    uint64_t nextId();
    uint64_t currentMillis() const;

    static constexpr uint64_t kEpoch = 1700000000000ULL;  // 自定义纪元 (2023-11-14)
    static constexpr uint8_t kWorkerBits = 10;
    static constexpr uint8_t kSequenceBits = 12;
    static constexpr uint16_t kMaxWorker = (1 << kWorkerBits) - 1;      // 1023
    static constexpr uint16_t kMaxSequence = (1 << kSequenceBits) - 1;  // 4095
    static constexpr uint8_t kTimestampShift = kWorkerBits + kSequenceBits;
    static constexpr uint8_t kWorkerShift = kSequenceBits;

    uint16_t worker_id_;
    std::atomic<uint64_t> sequence_;
    uint64_t last_timestamp_;
    std::mutex mutex_;
};