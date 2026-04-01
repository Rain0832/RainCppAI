#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <stdexcept>

namespace http
{

    /**
     * @brief 通用线程池
     *
     * 支持提交任意可调用对象，返回 std::future 获取结果。
     * 用于将阻塞任务（如 AI API 调用）从 IO 线程卸载到独立线程执行。
     */
    class ThreadPool
    {
    public:
        explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency())
            : stop_(false)
        {
            for (size_t i = 0; i < numThreads; ++i)
            {
                workers_.emplace_back([this]
                                      {
                    while (true) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(mutex_);
                            cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                            if (stop_ && tasks_.empty()) return;
                            task = std::move(tasks_.front());
                            tasks_.pop();
                        }
                        task();
                    } });
            }
        }

        ~ThreadPool()
        {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stop_ = true;
            }
            cv_.notify_all();
            for (auto &t : workers_)
            {
                if (t.joinable())
                    t.join();
            }
        }

        /**
         * @brief 提交任务到线程池
         * @param f 可调用对象
         * @param args 参数
         * @return std::future<返回类型>
         */
        template <typename F, typename... Args>
        auto submit(F &&f, Args &&...args)
            -> std::future<typename std::invoke_result<F, Args...>::type>
        {
            using ReturnType = typename std::invoke_result<F, Args...>::type;

            auto task = std::make_shared<std::packaged_task<ReturnType()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...));

            std::future<ReturnType> future = task->get_future();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (stop_)
                    throw std::runtime_error("submit on stopped ThreadPool");
                tasks_.emplace([task]()
                               { (*task)(); });
            }
            cv_.notify_one();
            return future;
        }

        size_t queueSize() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return tasks_.size();
        }

        size_t threadCount() const { return workers_.size(); }

    private:
        std::vector<std::thread> workers_;
        std::queue<std::function<void()>> tasks_;
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        std::atomic<bool> stop_;
    };

} // namespace http
