#include "common/AISessionIdGenerator.h"

AISessionIdGenerator::AISessionIdGenerator(uint16_t worker_id)
    : worker_id_(worker_id % (kMaxWorker + 1)), sequence_(0), last_timestamp_(0)
{
}

std::string AISessionIdGenerator::generate()
{
    return std::to_string(nextId());
}

uint64_t AISessionIdGenerator::nextId()
{
    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t timestamp = currentMillis();

    if (timestamp == last_timestamp_)
    {
        // 同一毫秒内序列号自增
        sequence_ = (sequence_.load() + 1) & kMaxSequence;
        if (sequence_.load() == 0)
        {
            // 当前毫秒序列号已用完，等待下一毫秒
            while (timestamp <= last_timestamp_)
            {
                timestamp = currentMillis();
            }
        }
    }
    else
    {
        sequence_ = 0;
    }

    last_timestamp_ = timestamp;

    return ((timestamp - kEpoch) << kTimestampShift) | (static_cast<uint64_t>(worker_id_) << kWorkerShift) |
           sequence_.load();
}

uint64_t AISessionIdGenerator::currentMillis() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
}