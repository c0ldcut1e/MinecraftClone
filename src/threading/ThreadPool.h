#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool
{
public:
    explicit ThreadPool(size_t threadCount);
    ~ThreadPool();

    ThreadPool(const ThreadPool &)            = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

    ThreadPool(ThreadPool &&)            = delete;
    ThreadPool &operator=(ThreadPool &&) = delete;

    void detachTask(std::function<void()> task);

    void wait();

    size_t getThreadCount() const;

private:
    void workerLoop();

    std::vector<std::thread> m_workers;

    std::queue<std::function<void()>> m_tasks;

    mutable std::mutex m_mutex;
    std::condition_variable m_taskCv;
    std::condition_variable m_waitCv;

    bool m_stopping;
    size_t m_activeWorkers;
};
