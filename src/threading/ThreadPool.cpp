#include "ThreadPool.h"

#include <utility>

#include "ThreadStorage.h"

ThreadPool::ThreadPool(size_t threadCount) : m_stopping(false), m_activeWorkers(0)
{
    if (threadCount == 0)
    {
        threadCount = 1;
    }

    m_workers.reserve(threadCount);
    for (size_t i = 0; i < threadCount; i++)
    {
        m_workers.emplace_back([this] { workerLoop(); });
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_stopping = true;
    }

    m_taskCv.notify_all();

    for (std::thread &worker : m_workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
}

void ThreadPool::detachTask(std::function<void()> task)
{
    if (!task)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_stopping)
        {
            return;
        }

        m_tasks.push(task);
    }

    m_taskCv.notify_one();
}

void ThreadPool::wait()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    m_waitCv.wait(lock, [this] { return m_tasks.empty() && m_activeWorkers == 0; });
}

size_t ThreadPool::getThreadCount() const { return m_workers.size(); }

void ThreadPool::workerLoop()
{
    ThreadStorage::createNewThreadStorage();

    while (true)
    {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_taskCv.wait(lock, [this] { return m_stopping || !m_tasks.empty(); });

            if (m_stopping && m_tasks.empty())
            {
                break;
            }

            task = std::move(m_tasks.front());
            m_tasks.pop();
            m_activeWorkers++;
        }

        task();

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_activeWorkers--;
            if (m_tasks.empty() && m_activeWorkers == 0)
            {
                m_waitCv.notify_all();
            }
        }
    }

    ThreadStorage::releaseThreadStorage();
}
